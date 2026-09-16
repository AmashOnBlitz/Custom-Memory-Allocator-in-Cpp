#include "allocator.h"
#include <stdexcept>
#include <string>

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
}

template<CoalesceAlgorithm CoalesceAlgo>
template<typename DataType>
void* Allocator<CoalesceAlgo>::Allocate(SIZE_T requiredSize)
{
	if (requiredSize == 0)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate 0 bytes"));

	if (mArenaCapacity < sizeof(RoutedBlockHeader) ||  requiredSize > (mArenaCapacity - sizeof(RoutedBlockHeader)))
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena to small to allocate"));

	if (!mHeadMemBlock) {
		mHeadMemBlock = reinterpret_cast<RoutedBlockHeader*>(mBase);

		uintptr_t rawAddress = reinterpret_cast<uintptr_t>(mHeadMemBlock + 1);
		uintptr_t addrSlot = rawAddress + sizeof(SIZE_T);
		uintptr_t aligned = Align(addrSlot, alignof(DataType));
		SIZE_T offset = static_cast<SIZE_T>(aligned - rawAddress);

		if (mArenaCapacity - sizeof(RoutedBlockHeader) < requiredSize + offset)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena to small to allocate"));

		mHeadMemBlock->size = requiredSize + offset;
		mHeadMemBlock->free = false;
		mHeadMemBlock->next = nullptr;
		if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious)
			mHeadMemBlock->prev = nullptr;
		SIZE_T* slot = reinterpret_cast<SIZE_T*>(aligned -sizeof(SIZE_T));
		*slot = offset;

		return reinterpret_cast<void*>(aligned);
	}

	RoutedBlockHeader* previousBlock = mHeadMemBlock;
	RoutedBlockHeader* currentBlock = mHeadMemBlock;

	std::tuple<RoutedBlockHeader*, SIZE_T, SIZE_T> bestBlock = { nullptr, 0, 0 };

	while (currentBlock) {
		SIZE_T totalDataSize = currentBlock->size;
		if (currentBlock->free && totalDataSize >= requiredSize) {
			uintptr_t rawAddress = reinterpret_cast<uintptr_t>(currentBlock + 1);
			uintptr_t addrSlot = rawAddress + sizeof(SIZE_T);
			uintptr_t aligned = Align(addrSlot, alignof(DataType));
			SIZE_T offset = static_cast<SIZE_T>(aligned - rawAddress);
			if ((aligned + requiredSize) > rawAddress + currentBlock->size) {
				previousBlock = currentBlock;
				currentBlock = currentBlock->next;
				continue;
			}

			SIZE_T sizeExceed = currentBlock->size - (offset + requiredSize);
			if (bestBlock.first == nullptr || sizeExceed < bestBlock.second) {
				bestBlock = { currentBlock, sizeExceed, offset};
			}
		}
		previousBlock = currentBlock;
		currentBlock = currentBlock->next;
	}

	if (bestBlock.first) {
		SIZE_T originalSize = bestBlock.first->size;
		SIZE_T usedSize = Align(bestBlock.third + requiredSize, alignof(RoutedBlockHeader));
		SIZE_T remaining = originalSize - usedSize;
		if (remaining >= sizeof(RoutedBlockHeader) + 1) {
			RoutedBlockHeader* oldNext = bestBlock.first->next;
			bestBlock.first->size = usedSize;
			UINT8* newBlockArea = reinterpret_cast<UINT8*>(bestBlock.first + 1) + bestBlock.first->size;
			RoutedBlockHeader* newBlock = reinterpret_cast<RoutedBlockHeader*>(newBlockArea);
			newBlock->free = true;
			newBlock->size = remaining - sizeof(RoutedBlockHeader);
			newBlock->next = bestBlock.first->next;
			if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious) {
				newBlock->prev = bestBlock.first;
				if (oldNext)
					oldNext->prev = newBlock;
			}
			bestBlock.first->next = newBlock;
		}

		bestBlock.first->free = false;

		UINT8* rawAddress = reinterpret_cast<UINT8*>(bestBlock.first + 1);
		UINT8* aligned = rawAddress + bestBlock.third;
		*reinterpret_cast<SIZE_T*>(aligned - sizeof(SIZE_T)) = bestBlock.third;
		return reinterpret_cast<void*>(aligned);
	}

	UINT8* newBlockArea = reinterpret_cast<UINT8*>(previousBlock + 1) + previousBlock->size;
	uintptr_t ptrNewBlockArea = reinterpret_cast<uintptr_t>(newBlockArea);
	uintptr_t ptrNewBlockArOffset = ptrNewBlockArea + sizeof(SIZE_T);
	UINT8* alignedNewBlockArea = reinterpret_cast<UINT8*>(
		Align(ptrNewBlockArOffset, alignof(DataType))
		);
	UINT8* arenaEnd = reinterpret_cast<UINT8*>(mBase) + mArenaCapacity;
	if ((alignedNewBlockArea + sizeof(SIZE_T) + requiredSize) <= arenaEnd) {

		if ((alignedNewBlockArea + requiredSize) > arenaEnd) {
			throw std::runtime_error(
				BUILD_RUNTIME_ERR_MSG("Cannot Allocate A Free Block Or Create New One In This Arena!\nOut Of Memory In Arena")
			);
		}

		RoutedBlockHeader* newBlock = reinterpret_cast<RoutedBlockHeader*>(newBlockArea);
		newBlock->free = false;
		newBlock->next = nullptr;

		SIZE_T offset = static_cast<SIZE_T>(
			reinterpret_cast<uintptr_t>(alignedNewBlockArea) -
			reinterpret_cast<uintptr_t>(newBlockArea)
			);
		newBlock->size = requiredSize + offset;

		if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious)
			newBlock->prev = previousBlock;

		previousBlock->next = newBlock;
		SIZE_T* slot = reinterpret_cast<SIZE_T*>(alignedNewBlockArea - sizeof(SIZE_T));
		*slot = offset;

		return reinterpret_cast<void*>(alignedNewBlockArea);
	}

	throw std::runtime_error(
		BUILD_RUNTIME_ERR_MSG("Cannot Allocate A Free Block Or Create New One In This Arena!\nOut Of Memory In Arena")
	);
}

template<CoalesceAlgorithm CoalesceAlgo>
void Allocator<CoalesceAlgo>::Deallocate(void* memory)
{
	if (!memory)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Deallocate/Free nullptr"));

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

template<CoalesceAlgorithm CoalesceAlgo>
void Allocator<CoalesceAlgo>::Free(void* memory) {
	Deallocate(memory);
}

template<CoalesceAlgorithm CoalesceAlgo>
std::string Allocator<CoalesceAlgo>::DebugBlocks()
{
	std::string debugStr = "";
	if (!mHeadMemBlock) return debugStr;
	if (mHeadMemBlock->size == 0) return debugStr;

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

	return debugStr;
}

template<CoalesceAlgorithm CoalesceAlgo>
uintptr_t Allocator<CoalesceAlgo>::Align(uintptr_t rawAddr, uintptr_t alignment)
{
	return uintptr_t((rawAddr + alignment - 1) & ~(alignment - 1));
}


template class Allocator<CoalesceAlgorithm::LinkPrevious>;
template class Allocator<CoalesceAlgorithm::SearchFromHead>;