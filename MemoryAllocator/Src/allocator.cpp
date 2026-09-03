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
void* Allocator<CoalesceAlgo>::Allocate(SIZE_T requiredSize)
{
	if (requiredSize == 0)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate 0 bytes"));

	if (mArenaCapacity < sizeof(RoutedBlockHeader) ||  requiredSize > (mArenaCapacity - sizeof(RoutedBlockHeader)))
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena to small to allocate"));

	if (!mHeadMemBlock) {
		mHeadMemBlock = reinterpret_cast<RoutedBlockHeader*>(mBase);
		mHeadMemBlock->size = requiredSize;
		mHeadMemBlock->free = false;
		mHeadMemBlock->next = nullptr;
		if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious)
			mHeadMemBlock->prev = nullptr;
		return reinterpret_cast<void*>(mHeadMemBlock + 1);
	}

	RoutedBlockHeader* previousBlock = mHeadMemBlock;
	RoutedBlockHeader* currentBlock = mHeadMemBlock;

	std::pair<RoutedBlockHeader*, SIZE_T> bestBlock = { nullptr, 0 };

	while (currentBlock) {
		if (currentBlock->free && currentBlock->size >= requiredSize) {
			SIZE_T sizeExceed = currentBlock->size - requiredSize;
			if (bestBlock.first == nullptr) {
				bestBlock = { currentBlock, sizeExceed };
			}
			else {
				if (sizeExceed < bestBlock.second) {
					bestBlock = { currentBlock, sizeExceed };
				}
			}
		}
		previousBlock = currentBlock;
		currentBlock = currentBlock->next;
	}

	if (bestBlock.first) {
		SIZE_T remaining = bestBlock.first->size - requiredSize;
		if (remaining >= sizeof(RoutedBlockHeader) + 1) {
			RoutedBlockHeader* oldNext = bestBlock.first->next;
			bestBlock.first->size = requiredSize;
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
		return reinterpret_cast<void*>(bestBlock.first + 1);
	}

	UINT8* newBlockArea = reinterpret_cast<UINT8*>(previousBlock + 1) + previousBlock->size;
	if ((newBlockArea + sizeof(RoutedBlockHeader) + requiredSize) <= mBase + mArenaCapacity) {
		RoutedBlockHeader* newBlock = reinterpret_cast<RoutedBlockHeader*>(newBlockArea);
		newBlock->free = false;
		newBlock->next = nullptr;
		newBlock->size = requiredSize;
		if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious)
			newBlock->prev = previousBlock;
		previousBlock->next = newBlock;
		return reinterpret_cast<void*>(newBlock + 1);
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

	RoutedBlockHeader* block = reinterpret_cast<RoutedBlockHeader*>(memory) - 1;
	block->free = true;


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


template class Allocator<CoalesceAlgorithm::LinkPrevious>;
template class Allocator<CoalesceAlgorithm::SearchFromHead>;