// NOTE (for self compilers): Dont Include this file in your build this is just template definition and will be added in allocate.header auto...

#include "allocator.h"
#include <stdexcept>
#include <string>
#include <bit> //reason for min support C++ 20

#define MSG_PREFIX "[Allocator]"
#define BUILD_RUNTIME_ERR_MSG(err) MSG_PREFIX + std::string("Error:") + std::string(err)
#define RETURN_CONSTRUCTED_MEMORY(DataType, voidMem) return (DataType*)(voidMem)

template<CoalesceAlgorithm CoalesceAlgo>
template<typename DataType>
DataType* Allocator<CoalesceAlgo>::Allocate(SIZE_T requiredSize)
{
	if (requiredSize == 0)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate 0 bytes"));

	if constexpr (CoalesceAlgo == CoalesceAlgorithm::FixedSize_NoHeader) {
		bool& isFirstit = algoSpecificData.isFirstit;
		if (isFirstit) {
			algoSpecificData.alignment = alignof(DataType);
			algoSpecificData.size = sizeof(DataType);

			// If align is big, always return aligned mem and if size is big then return size
			// to avoid further align() after first align()
			if (alignof(DataType) > sizeof(DataType)) {
				algoSpecificData.slotSize = alignof(DataType);
			}
			else {
				algoSpecificData.slotSize = sizeof(DataType);
			}

			isFirstit = !isFirstit;

			SIZE_T& buffer = algoSpecificData.buffer;
			uintptr_t freeAddr = reinterpret_cast<uintptr_t>(mBase) + buffer;;
			freeAddr = Align(freeAddr, alignof(DataType));
			SIZE_T alignedBuff = (freeAddr - reinterpret_cast<uintptr_t>(mBase));
			if (alignedBuff >= mArenaCapacity)
				throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena too small to allocate"));
			algoSpecificData.memPrefixAlignment = alignedBuff;
			algoSpecificData.maxAllocations = (mArenaCapacity - alignedBuff) / algoSpecificData.slotSize;
			SIZE_T metaDataArenaCapacity = (algoSpecificData.maxAllocations + 7) / 8;
			algoSpecificData.mMemoryMetaDataArena = ::VirtualAlloc(
				nullptr,
				metaDataArenaCapacity,
				MEM_COMMIT | MEM_RESERVE,
				PAGE_READWRITE
			);

			if (!algoSpecificData.mMemoryMetaDataArena)
				throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot prepare arena to allocate memory"));

			algoSpecificData.mMetaDataBase = static_cast<UINT8*>(algoSpecificData.mMemoryMetaDataArena);
		}
		else {
			SIZE_T slotSize;
			if (alignof(DataType) > sizeof(DataType)) {
				slotSize = alignof(DataType);
			}
			else {
				slotSize = sizeof(DataType);
			}

			if (alignof(DataType) != algoSpecificData.alignment ||
				sizeof(DataType) != algoSpecificData.size ||
				slotSize != algoSpecificData.slotSize
				)
				throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Trying to allocate memory to different data types!\n \
																through FixedSize algorithm"));
		}

		SIZE_T& buffer = algoSpecificData.buffer;
		uintptr_t freeAddr = reinterpret_cast<uintptr_t>(mBase) + buffer;
		freeAddr = Align(freeAddr, alignof(DataType));
		SIZE_T alignedBuff = (freeAddr - reinterpret_cast<uintptr_t>(mBase)) + algoSpecificData.slotSize;
		SIZE_T& allocationIt = algoSpecificData.allocationIt;
		for (SIZE_T byteIndex = 0; byteIndex < (allocationIt + 7) / 8; byteIndex++) {
			UINT8 byte = algoSpecificData.mMetaDataBase[byteIndex];
			if (byte == 0xFF)
				continue;
			SIZE_T bit = std::countr_zero(static_cast<unsigned>(~byte));
			SIZE_T i = byteIndex * 8 + bit;

			if (i < allocationIt) {
				UINT8 bitMask = static_cast<UINT8>(1u << bit);
				algoSpecificData.mMetaDataBase[byteIndex] |= bitMask;
				void* mem = reinterpret_cast<void*>(mBase + algoSpecificData.memPrefixAlignment + i * algoSpecificData.slotSize);
				RETURN_CONSTRUCTED_MEMORY(DataType, mem);
			}
		}

		if (alignedBuff > mArenaCapacity)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena too small to allocate"));
		if (algoSpecificData.allocationIt >= algoSpecificData.maxAllocations)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena too small to allocate"));

		buffer = alignedBuff;

		SIZE_T byteIndex = algoSpecificData.allocationIt / 8;
		UINT8 bitMask = static_cast<UINT8>(1u << (algoSpecificData.allocationIt % 8));
		algoSpecificData.mMetaDataBase[byteIndex] |= bitMask;

		algoSpecificData.allocationIt += 1;
		RETURN_CONSTRUCTED_MEMORY(DataType, reinterpret_cast<void*>(freeAddr));
	}
	else {
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
				SIZE_T usedSize = Align(offset + requiredSize, alignof(RoutedBlockHeader));
				if (usedSize > currentBlock->size) {
					previousBlock = currentBlock;
					currentBlock = currentBlock->next;
					continue;
				}

				SIZE_T sizeExceed = currentBlock->size - usedSize;
				if (std::get<0>(bestBlock) == nullptr || sizeExceed < std::get<1>(bestBlock)) {
					bestBlock = { currentBlock, sizeExceed, offset };
					if (sizeExceed == 0)
						break;
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
	}

	throw std::runtime_error(
		BUILD_RUNTIME_ERR_MSG("Cannot Allocate A Free Block Or Create New One In This Arena!\nOut Of Memory In Arena")
	);
}
