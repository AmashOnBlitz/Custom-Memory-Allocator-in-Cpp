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

	mMemoryArena = ::VirtualAlloc(
		nullptr,
		mArenaCapacity,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);

	if (!mMemoryArena)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Reserve Memory, VirtualAlloc Failed!"));

	mBase = static_cast<UINT8*>(mMemoryArena);
}

template<CoalesceAlgorithm CoalesceAlgo>
Allocator<CoalesceAlgo>::~Allocator()
{
	if (mMemoryArena) {
		::VirtualFree(
			mMemoryArena,
			0,
			MEM_RELEASE
		);
		mMemoryArena = nullptr;
		mBase = nullptr;
	}
	if constexpr (CoalesceAlgo == CoalesceAlgorithm::FixedSize_NoHeader) {
		if (algoSpecificData.mMemoryMetaDataArena) {
			::VirtualFree(
				algoSpecificData.mMemoryMetaDataArena,
				0,
				MEM_RELEASE
			);
			algoSpecificData.mMemoryMetaDataArena = nullptr;
			algoSpecificData.mMetaDataBase = nullptr;
		}
	}
}

template<CoalesceAlgorithm CoalesceAlgo>
void Allocator<CoalesceAlgo>::Deallocate(void* memory)
{
	if (!memory)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Deallocate/Free nullptr"));

	if constexpr (CoalesceAlgo == CoalesceAlgorithm::FixedSize_NoHeader) {
		UINT8* targetMem = reinterpret_cast<UINT8*>(memory);
		if (targetMem < mBase + algoSpecificData.memPrefixAlignment || targetMem >= mBase + mArenaCapacity)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Invalid memory address"));
		SIZE_T actualOffset = targetMem - mBase - algoSpecificData.memPrefixAlignment;
		if (actualOffset % algoSpecificData.slotSize != 0)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Invalid memory address"));
		SIZE_T index = actualOffset / algoSpecificData.slotSize;
		if (index >= algoSpecificData.allocationIt)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Invalid memory address"));

		SIZE_T byteIndex = index / 8;
		UINT8 bitMask = static_cast<UINT8>(1u << (index % 8));
		if ((algoSpecificData.mMetaDataBase[byteIndex] & bitMask) == 0)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Deallocate/Free already free memory"));
		algoSpecificData.mMetaDataBase[byteIndex] &= static_cast<UINT8>(~bitMask);
	}
	else {
		uintptr_t slotArea = reinterpret_cast<uintptr_t>(memory) - sizeof(SIZE_T);
		SIZE_T* slot = reinterpret_cast<SIZE_T*>(slotArea);
		uintptr_t memPtr = reinterpret_cast<uintptr_t>(memory) - *slot;
		UINT8* beforeInternalFrag = reinterpret_cast<UINT8*>(memPtr);

		RoutedBlockHeader* block = reinterpret_cast<RoutedBlockHeader*>(beforeInternalFrag) - 1;
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
			backCoalescedBlock->size += (nxt->size + sizeof(RoutedBlockHeader));
			backCoalescedBlock->free = true;
			backCoalescedBlock->next = nxt->next;
			if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious) {
				if (backCoalescedBlock->next)
					backCoalescedBlock->next->prev = backCoalescedBlock;
			}
		}
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
		if (!algoSpecificData.mMetaDataBase) return debugStr;
		for (SIZE_T i = 0; i < algoSpecificData.allocationIt; i++) {
			SIZE_T byteIndex = i / 8;
			UINT8 bitMask = static_cast<UINT8>(1u << (i % 8));
			bool isFree = (algoSpecificData.mMetaDataBase[byteIndex] & bitMask) == 0;
			uintptr_t currentBlock = reinterpret_cast<uintptr_t>(
				mBase + algoSpecificData.memPrefixAlignment + i * algoSpecificData.slotSize);

			debugStr += "[Block]==============================\n";
			debugStr += "Address: ";
			debugStr += std::to_string(currentBlock);
			debugStr += "\n";
			debugStr += "User Data Address: ";
			debugStr += std::to_string(currentBlock);
			debugStr += "\n";
			debugStr += "Size: ";
			debugStr += std::to_string(algoSpecificData.slotSize);
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

template class Allocator<CoalesceAlgorithm::LinkPrevious>;
template class Allocator<CoalesceAlgorithm::SearchFromHead>;
template class Allocator<CoalesceAlgorithm::FixedSize_NoHeader>;