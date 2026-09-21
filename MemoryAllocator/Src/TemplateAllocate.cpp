// NOTE (for self compilers): Dont Include this file in your build this is just template definition and will be added in allocate.header auto...

#include "allocator.h"
#include <stdexcept>
#include <string>

#define MSG_PREFIX "[Allocator]"
#define BUILD_RUNTIME_ERR_MSG(err) MSG_PREFIX + std::string("Error:") + std::string(err)
#define RETURN_CONSTRUCTED_MEMORY(DataType, voidMem) return (DataType*)(voidMem)

template<CoalesceAlgorithm CoalesceAlgo>
template<typename DataType>
DataType* Allocator<CoalesceAlgo>::Allocate(SIZE_T requiredSize)
{
	if (requiredSize == 0)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate 0 bytes"));

	if (mArenaCapacity < sizeof(RoutedBlockHeader) || requiredSize >(mArenaCapacity - sizeof(RoutedBlockHeader)))
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena too small to allocate"));

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

		RETURN_CONSTRUCTED_MEMORY(DataType, reinterpret_cast<void*>(aligned));
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
			if (std::get<0>(bestBlock) == nullptr || sizeExceed < std::get<1>(bestBlock)) {
				bestBlock = { currentBlock, sizeExceed, offset };
			}
		}
		previousBlock = currentBlock;
		currentBlock = currentBlock->next;
	}

	if (std::get<0>(bestBlock)) {
		SIZE_T originalSize = std::get<0>(bestBlock)->size;
		SIZE_T usedSize = Align(std::get<2>(bestBlock) + requiredSize, alignof(RoutedBlockHeader));

		if (usedSize <= originalSize) {
			SIZE_T remaining = originalSize - usedSize;
			if (remaining >= sizeof(RoutedBlockHeader) + 1) {
				RoutedBlockHeader* oldNext = std::get<0>(bestBlock)->next;
				std::get<0>(bestBlock)->size = usedSize;
				UINT8* newBlockArea = reinterpret_cast<UINT8*>(std::get<0>(bestBlock) + 1) + std::get<0>(bestBlock)->size;
				RoutedBlockHeader* newBlock = reinterpret_cast<RoutedBlockHeader*>(newBlockArea);
				newBlock->free = true;
				newBlock->size = remaining - sizeof(RoutedBlockHeader);
				newBlock->next = std::get<0>(bestBlock)->next;
				if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious) {
					newBlock->prev = std::get<0>(bestBlock);
					if (oldNext)
						oldNext->prev = newBlock;
				}
				std::get<0>(bestBlock)->next = newBlock;
			}
		}

		std::get<0>(bestBlock)->free = false;

		UINT8* rawAddress = reinterpret_cast<UINT8*>(std::get<0>(bestBlock) + 1);
		UINT8* aligned = rawAddress + std::get<2>(bestBlock);
		*reinterpret_cast<SIZE_T*>(aligned - sizeof(SIZE_T)) = std::get<2>(bestBlock);
		RETURN_CONSTRUCTED_MEMORY(DataType, reinterpret_cast<void*>(aligned));
	}

	UINT8* newBlockArea = reinterpret_cast<UINT8*>(previousBlock + 1) + previousBlock->size;
	RoutedBlockHeader* newBlock = reinterpret_cast<RoutedBlockHeader*>(newBlockArea);
	uintptr_t rawAddress = reinterpret_cast<uintptr_t>(newBlock + 1);
	uintptr_t ptrNewBlockArOffset = rawAddress + sizeof(SIZE_T);
	UINT8* alignedNewBlockArea = reinterpret_cast<UINT8*>(
		Align(ptrNewBlockArOffset, alignof(DataType))
		);
	UINT8* arenaEnd = reinterpret_cast<UINT8*>(mBase) + mArenaCapacity;
	if (alignedNewBlockArea + requiredSize <= arenaEnd) {
		newBlock->free = false;
		newBlock->next = nullptr;

		SIZE_T offset = static_cast<SIZE_T>(
			reinterpret_cast<uintptr_t>(alignedNewBlockArea) -
			rawAddress
			);
		newBlock->size = requiredSize + offset;

		if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious)
			newBlock->prev = previousBlock;

		previousBlock->next = newBlock;
		SIZE_T* slot = reinterpret_cast<SIZE_T*>(alignedNewBlockArea - sizeof(SIZE_T));
		*slot = offset;

		RETURN_CONSTRUCTED_MEMORY(DataType, alignedNewBlockArea);
	}

	throw std::runtime_error(
		BUILD_RUNTIME_ERR_MSG("Cannot Allocate A Free Block Or Create New One In This Arena!\nOut Of Memory In Arena")
	);
}