// Dont Include this file in your build this is just template definition and will be added in allocate.header auto...
#include "allocator.h"
#include <stdexcept>
#include <string>

#define MSG_PREFIX "[Allocator]"
#define BUILD_RUNTIME_ERR_MSG(err) MSG_PREFIX + std::string("Error:") + std::string(err)

template<CoalesceAlgorithm CoalesceAlgo>
template<typename DataType>
void* Allocator<CoalesceAlgo>::Allocate(SIZE_T requiredSize)
{
	if (requiredSize == 0)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate 0 bytes"));

	if (mArenaCapacity < sizeof(RoutedBlockHeader) || requiredSize >(mArenaCapacity - sizeof(RoutedBlockHeader)))
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
		SIZE_T* slot = reinterpret_cast<SIZE_T*>(aligned - sizeof(SIZE_T));
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
				bestBlock = { currentBlock, sizeExceed, offset };
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
	if (alignedNewBlockArea + requiredSize <= arenaEnd) {
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
