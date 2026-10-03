#include "allocator.h"
#include <stdexcept>
#include <string>
#include <iostream>

#define MSG_PREFIX "[Allocator]"
#define BUILD_RUNTIME_ERR_MSG(err) MSG_PREFIX + std::string("Error:") + std::string(err)

template<CoalesceAlgorithm CoalesceAlgo>
Allocator<CoalesceAlgo>::Allocator(SIZE_T arenaSize) :
	mMemoryArena(nullptr),
	mArenaCapacity(arenaSize),
	mHeadMemBlock(nullptr)
{
	if (mArenaCapacity == 0)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Reserve 0 bytes"));

	mMemoryArena = ::AllocateMemory(nullptr, mArenaCapacity);

	if (!mMemoryArena)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Reserve Memory, VirtualAlloc Failed!"));

	mBase = static_cast<UINT8*>(mMemoryArena);
}

template<CoalesceAlgorithm CoalesceAlgo>
Allocator<CoalesceAlgo>::~Allocator()
{
	if (mMemoryArena) {
		::FreeMemory(mMemoryArena, 0);
		mMemoryArena = nullptr;
		mBase = nullptr;
	}
	if constexpr (CoalesceAlgo == CoalesceAlgorithm::FixedSize_NoHeader) {
#if ALLOCATOR_ENABLE_BITMAP
		if (mAlgoSpecificData.mMemoryMetaDataArena) {
			::FreeMemory(mAlgoSpecificData.mMemoryMetaDataArena, 0);
			mAlgoSpecificData.mMemoryMetaDataArena = nullptr;
			mAlgoSpecificData.mMetaDataBase = nullptr;
		}
#endif
	}
	if (mAlgoSpecificData.mFreeStackArena) {
		::FreeMemory(mAlgoSpecificData.mFreeStackArena, 0);
		mAlgoSpecificData.mFreeStackArena = nullptr;
		mAlgoSpecificData.mFreeStackBase = nullptr;
	}
}

template<CoalesceAlgorithm CoalesceAlgo>
void Allocator<CoalesceAlgo>::Deallocate(void* memory)
{
	if (!memory)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Deallocate/Free nullptr"));

	if constexpr (CoalesceAlgo == CoalesceAlgorithm::FixedSize_NoHeader) {
		UINT8* targetMem = reinterpret_cast<UINT8*>(memory);
		if (targetMem < mBase + mAlgoSpecificData.memPrefixAlignment || targetMem >= mBase + mArenaCapacity)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Invalid memory address"));
		SIZE_T actualOffset = targetMem - mBase - mAlgoSpecificData.memPrefixAlignment;
		if (actualOffset % mAlgoSpecificData.slotSize != 0)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Invalid memory address"));
		SIZE_T index = actualOffset / mAlgoSpecificData.slotSize;
		if (index >= mAlgoSpecificData.allocationIt)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Invalid memory address"));

#if ALLOCATOR_ENABLE_BITMAP
		SIZE_T byteIndex = index / 8;
		UINT8 bitMask = static_cast<UINT8>(1u << (index % 8));
		if ((mAlgoSpecificData.mMetaDataBase[byteIndex] & bitMask) == 0)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Deallocate/Free already free memory"));
		mAlgoSpecificData.mMetaDataBase[byteIndex] &= static_cast<UINT8>(~bitMask);
#endif

		mAlgoSpecificData.mFreeStackBase[mAlgoSpecificData.freeStackTop++] = index;
	}
	else {
		uintptr_t slotArea = reinterpret_cast<uintptr_t>(memory) - sizeof(SIZE_T);
		SIZE_T* slot = reinterpret_cast<SIZE_T*>(slotArea);
		uintptr_t memPtr = reinterpret_cast<uintptr_t>(memory) - *slot;
		UINT8* beforeInternalFrag = reinterpret_cast<UINT8*>(memPtr);

		RoutedBlockHeader* block = reinterpret_cast<RoutedBlockHeader*>(beforeInternalFrag) - 1;
		if (block->free)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Deallocate/Free already free memory"));
		block->free = true;

		RoutedBlockHeader* nxt = block->next;
		RoutedBlockHeader* prev = nullptr;

		if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious) {
			prev = block->prev;
		}
		else {
			RoutedBlockHeader* iPrev = nullptr;
			RoutedBlockHeader* current = mHeadMemBlock;
			while (current) {
				if (current == block) {
					prev = iPrev;
					break;
				}
				iPrev = current;
				current = current->next;
			}
		}

		RoutedBlockHeader* backCoalescedBlock = block;

		if (prev && prev->free) {
			RemoveFreeEntry(prev);
			prev->size += (block->size + sizeof(RoutedBlockHeader));
			prev->next = block->next;
			prev->free = true;
			backCoalescedBlock = prev;
			if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious) {
				if (backCoalescedBlock->next)
					backCoalescedBlock->next->prev = prev;
			}
		}

		if (nxt && nxt->free) {
			RemoveFreeEntry(nxt);
			backCoalescedBlock->size += (nxt->size + sizeof(RoutedBlockHeader));
			backCoalescedBlock->free = true;
			backCoalescedBlock->next = nxt->next;
			if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious) {
				if (backCoalescedBlock->next)
					backCoalescedBlock->next->prev = backCoalescedBlock;
			}
		}
		PushFreeEntry(backCoalescedBlock);
	}
}

template<CoalesceAlgorithm CoalesceAlgo>
void Allocator<CoalesceAlgo>::Free(void* memory) {
	Deallocate(memory);
}

template<CoalesceAlgorithm CoalesceAlgo>
std::string Allocator<CoalesceAlgo>::DebugBlocks()
{
	std::string debugStr = "";

	if constexpr (CoalesceAlgo == CoalesceAlgorithm::FixedSize_NoHeader) {
		if (!mAlgoSpecificData.mFreeStackBase) return debugStr;
		std::vector<bool> isBlockFree(mAlgoSpecificData.allocationIt, false);
		for (SIZE_T s = 0; s < mAlgoSpecificData.freeStackTop; s++)
			isBlockFree[mAlgoSpecificData.mFreeStackBase[s]] = true;
		for (SIZE_T i = 0; i < mAlgoSpecificData.allocationIt; i++) {
			bool isFree = isBlockFree[i];
			uintptr_t currentBlock = reinterpret_cast<uintptr_t>(
				mBase + mAlgoSpecificData.memPrefixAlignment + i * mAlgoSpecificData.slotSize);

			debugStr += "[Block]==============================\n";
			debugStr += "Address: ";
			debugStr += std::to_string(currentBlock);
			debugStr += "\n";
			debugStr += "User Data Address: ";
			debugStr += std::to_string(currentBlock);
			debugStr += "\n";
			debugStr += "Size: ";
			debugStr += std::to_string(mAlgoSpecificData.slotSize);
			debugStr += " bytes\n";
			debugStr += "Free: ";
			debugStr += isFree ? "True\n" : "False\n";
			debugStr += "=====================================\n";
		}
	}
	else {
		if (!mHeadMemBlock) return debugStr;

		RoutedBlockHeader* currentBlock = mHeadMemBlock;

		while (currentBlock) {
			debugStr += "[Block]==============================\n";
			debugStr += "Address: ";
			debugStr += std::to_string(reinterpret_cast<uintptr_t>(currentBlock));
			debugStr += "\n";
			debugStr += "User Data Address: ";
			debugStr += std::to_string(reinterpret_cast<uintptr_t>(currentBlock + 1));
			debugStr += "\n";
			debugStr += "Size: ";
			debugStr += std::to_string(currentBlock->size);
			debugStr += " bytes\n";
			debugStr += "Free: ";
			debugStr += (currentBlock->free) ? "True\n" : "False\n";
			debugStr += "=====================================\n";
			currentBlock = currentBlock->next;
		}
	}

	return debugStr;
}

template<CoalesceAlgorithm CoalesceAlgo>
uintptr_t Allocator<CoalesceAlgo>::Align(uintptr_t rawAddr, uintptr_t alignment)
{
	return uintptr_t((rawAddr + alignment - 1) & ~(alignment - 1));
}

template<CoalesceAlgorithm CoalesceAlgo>
void Allocator<CoalesceAlgo>::PushFreeEntry(RoutedBlockHeader* b) {
	auto& d = mAlgoSpecificData;
	d.mFreeStackBase[d.freeStackTop * 2] = reinterpret_cast<uintptr_t>(b);
	d.mFreeStackBase[d.freeStackTop * 2 + 1] = b->size + sizeof(RoutedBlockHeader);
	d.freeStackTop++;
}

template<CoalesceAlgorithm CoalesceAlgo>
void Allocator<CoalesceAlgo>::RemoveFreeEntry(RoutedBlockHeader* b) {
	auto& d = mAlgoSpecificData;
	for (SIZE_T i = 0; i < d.freeStackTop; ++i) {
		if (d.mFreeStackBase[i * 2] == reinterpret_cast<uintptr_t>(b)) {
			SIZE_T last = --d.freeStackTop;
			d.mFreeStackBase[i * 2] = d.mFreeStackBase[last * 2];
			d.mFreeStackBase[i * 2 + 1] = d.mFreeStackBase[last * 2 + 1];
			return;
		}
	}
}

//template<CoalesceAlgorithm CoalesceAlgo>
//CompressedSIZE_T Allocator<CoalesceAlgo>::CompressSize_T(SIZE_T size)
//{
//	return static_cast<CompressedSIZE_T>(size);
//}
//
//template<CoalesceAlgorithm CoalesceAlgo>
//SIZE_T Allocator<CoalesceAlgo>::DecompressSize_T(CompressedSIZE_T size)
//{
//	return static_cast<SIZE_T>(size);
//}

template class Allocator<CoalesceAlgorithm::LinkPrevious>;
template class Allocator<CoalesceAlgorithm::SearchFromHead>;
template class Allocator<CoalesceAlgorithm::FixedSize_NoHeader>;