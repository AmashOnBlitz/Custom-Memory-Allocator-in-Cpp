// NOTE (for self compilers): Dont Include this file in your build this is just template definition and will be added in allocate.header auto...

#include "allocator.h"
#include <stdexcept>
#include <string>
#include <bit> //reason for min support C++ 20
#include <tuple>

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
		bool& isFirstit = mAlgoSpecificData.isFirstit;
		if (isFirstit) {
			mAlgoSpecificData.alignment = alignof(DataType);
			mAlgoSpecificData.size = sizeof(DataType);

			if (alignof(DataType) > sizeof(DataType)) {
				mAlgoSpecificData.slotSize = alignof(DataType);
			}
			else {
				mAlgoSpecificData.slotSize = sizeof(DataType);
			}

			isFirstit = !isFirstit;

			SIZE_T& buffer = mAlgoSpecificData.buffer;
			uintptr_t freeAddr = reinterpret_cast<uintptr_t>(mBase) + buffer;;
			freeAddr = Align(freeAddr, alignof(DataType));
			SIZE_T alignedBuff = (freeAddr - reinterpret_cast<uintptr_t>(mBase));
			if (alignedBuff >= mArenaCapacity)
				throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena too small to allocate"));
			mAlgoSpecificData.memPrefixAlignment = alignedBuff;
			mAlgoSpecificData.maxAllocations = (mArenaCapacity - alignedBuff) / mAlgoSpecificData.slotSize;

#if ALLOCATOR_ENABLE_BITMAP
			SIZE_T metaDataArenaCapacity = (mAlgoSpecificData.maxAllocations + 7) / 8;
			mAlgoSpecificData.mMemoryMetaDataArena = ::AllocateMemory(nullptr, metaDataArenaCapacity);

			if (!mAlgoSpecificData.mMemoryMetaDataArena)
				throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot prepare arena to allocate memory"));

			mAlgoSpecificData.mMetaDataBase = static_cast<UINT8*>(mAlgoSpecificData.mMemoryMetaDataArena);
#endif
			SIZE_T freeStackCapacity = mAlgoSpecificData.maxAllocations * sizeof(SIZE_T);
			mAlgoSpecificData.mFreeStackArena = ::AllocateMemory(nullptr, freeStackCapacity);

			if (!mAlgoSpecificData.mFreeStackArena)
				throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot prepare arena to allocate memory"));

			mAlgoSpecificData.mFreeStackBase = static_cast<SIZE_T*>(mAlgoSpecificData.mFreeStackArena);
		}
		else {
			SIZE_T slotSize;
			if (alignof(DataType) > sizeof(DataType)) {
				slotSize = alignof(DataType);
			}
			else {
				slotSize = sizeof(DataType);
			}

			if (alignof(DataType) != mAlgoSpecificData.alignment ||
				sizeof(DataType) != mAlgoSpecificData.size ||
				slotSize != mAlgoSpecificData.slotSize
				)
				throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Trying to allocate memory to different data types!\n \
															through FixedSize algorithm"));
		}

		if (mAlgoSpecificData.freeStackTop > 0) {
			SIZE_T i = mAlgoSpecificData.mFreeStackBase[--mAlgoSpecificData.freeStackTop];

#if ALLOCATOR_ENABLE_BITMAP
			mAlgoSpecificData.mMetaDataBase[i / 8] |= static_cast<UINT8>(1u << (i % 8));
#endif
			void* mem = reinterpret_cast<void*>(mBase + mAlgoSpecificData.memPrefixAlignment + i * mAlgoSpecificData.slotSize);
			RETURN_CONSTRUCTED_MEMORY(DataType, mem);
		}

		SIZE_T& buffer = mAlgoSpecificData.buffer;
		uintptr_t freeAddr = reinterpret_cast<uintptr_t>(mBase) + buffer;
		freeAddr = Align(freeAddr, alignof(DataType));
		SIZE_T alignedBuff = (freeAddr - reinterpret_cast<uintptr_t>(mBase)) + mAlgoSpecificData.slotSize;

		if (alignedBuff > mArenaCapacity)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena too small to allocate"));
		if (mAlgoSpecificData.allocationIt >= mAlgoSpecificData.maxAllocations)
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena too small to allocate"));

		buffer = alignedBuff;

#if ALLOCATOR_ENABLE_BITMAP
		SIZE_T byteIndex = mAlgoSpecificData.allocationIt / 8;
		UINT8 bitMask = static_cast<UINT8>(1u << (mAlgoSpecificData.allocationIt % 8));
		mAlgoSpecificData.mMetaDataBase[byteIndex] |= bitMask;
#endif

		mAlgoSpecificData.allocationIt += 1;
		RETURN_CONSTRUCTED_MEMORY(DataType, reinterpret_cast<void*>(freeAddr));
	}
	else {
		if (mArenaCapacity < sizeof(RoutedBlockHeader) || requiredSize >(mArenaCapacity - sizeof(RoutedBlockHeader)))
			throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena too small to allocate"));

		bool& isFirstit = mAlgoSpecificData.isFirstit;
		if (isFirstit) {
			mAlgoSpecificData.maxAllocations = mArenaCapacity / (sizeof(RoutedBlockHeader) + 1);
			SIZE_T freeStackCapacity = (mAlgoSpecificData.maxAllocations * 2) * sizeof(SIZE_T);
			mAlgoSpecificData.mFreeStackArena = ::AllocateMemory(nullptr, freeStackCapacity);
			if (!mAlgoSpecificData.mFreeStackArena)
				throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot prepare arena to allocate memory"));
			isFirstit = !isFirstit;
			mAlgoSpecificData.mFreeStackBase = static_cast<SIZE_T*>(mAlgoSpecificData.mFreeStackArena);
		}

		if (!mHeadMemBlock) {
			mHeadMemBlock = reinterpret_cast<RoutedBlockHeader*>(mBase);

			uintptr_t rawAddress = reinterpret_cast<uintptr_t>(mHeadMemBlock + 1);
			uintptr_t addrSlot = rawAddress + sizeof(SIZE_T);
			uintptr_t aligned = Align(addrSlot, alignof(DataType));
			SIZE_T offset = static_cast<SIZE_T>(aligned - rawAddress);
			SIZE_T blockSize = Align(offset + requiredSize, alignof(RoutedBlockHeader));

			if (mArenaCapacity - sizeof(RoutedBlockHeader) < blockSize)
				throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena to small to allocate"));

			mHeadMemBlock->size = blockSize;
			mHeadMemBlock->free = false;
			mHeadMemBlock->next = nullptr;
			if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious)
				mHeadMemBlock->prev = nullptr;
			mTailMemBlock = mHeadMemBlock;
			SIZE_T* slot = reinterpret_cast<SIZE_T*>(aligned - sizeof(SIZE_T));
			*slot = offset;

			RETURN_CONSTRUCTED_MEMORY(DataType, reinterpret_cast<void*>(aligned));
		}

		std::tuple<RoutedBlockHeader*, SIZE_T, SIZE_T> bestBlock = { nullptr, 0, 0 };

		for (SIZE_T arrIndex = mAlgoSpecificData.freeStackTop; arrIndex-- > 0; ) {
			SIZE_T addrIndex = arrIndex * 2;
			SIZE_T capacity = mAlgoSpecificData.mFreeStackBase[addrIndex + 1];

			if (capacity < sizeof(RoutedBlockHeader) + requiredSize)
				continue;

			RoutedBlockHeader* currentBlock = reinterpret_cast<RoutedBlockHeader*>(mAlgoSpecificData.mFreeStackBase[addrIndex]);
			uintptr_t rawAddress = reinterpret_cast<uintptr_t>(currentBlock + 1);
			uintptr_t addrSlot = rawAddress + sizeof(SIZE_T);
			uintptr_t aligned = Align(addrSlot, alignof(DataType));
			SIZE_T offset = static_cast<SIZE_T>(aligned - rawAddress);
			SIZE_T usedSize = Align(offset + requiredSize, alignof(RoutedBlockHeader));

			if (usedSize > currentBlock->size)
				continue;

			SIZE_T sizeExceed = currentBlock->size - usedSize;
			if (std::get<0>(bestBlock) == nullptr || sizeExceed < std::get<1>(bestBlock)) {
				bestBlock = { currentBlock, sizeExceed, offset };
				if (sizeExceed < sizeof(RoutedBlockHeader) + 1)
					break;
			}
		}

		if (std::get<0>(bestBlock)) {
			RemoveFreeEntry(std::get<0>(bestBlock));
			SIZE_T originalSize = std::get<0>(bestBlock)->size;
			SIZE_T usedSize = Align(std::get<2>(bestBlock) + requiredSize, alignof(RoutedBlockHeader));

			if (usedSize <= originalSize) {
				SIZE_T remaining = originalSize - usedSize;
				if (remaining >= sizeof(RoutedBlockHeader) + 1) {
					RoutedBlockHeader* oldNext = std::get<0>(bestBlock)->next;
					std::get<0>(bestBlock)->size = usedSize;
					UINT8* newBlockArea = reinterpret_cast<UINT8*>(std::get<0>(bestBlock) + 1) + usedSize;
					RoutedBlockHeader* newBlock = reinterpret_cast<RoutedBlockHeader*>(newBlockArea);
					newBlock->free = true;
					newBlock->size = remaining - sizeof(RoutedBlockHeader);
					newBlock->next = oldNext;
					if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious) {
						newBlock->prev = std::get<0>(bestBlock);
						if (oldNext)
							oldNext->prev = newBlock;
					}
					if (!oldNext)
						mTailMemBlock = newBlock;
					std::get<0>(bestBlock)->next = newBlock;
					PushFreeEntry(newBlock);
				}
			}

			std::get<0>(bestBlock)->free = false;

			UINT8* rawAddress = reinterpret_cast<UINT8*>(std::get<0>(bestBlock) + 1);
			UINT8* aligned = rawAddress + std::get<2>(bestBlock);
			*reinterpret_cast<SIZE_T*>(aligned - sizeof(SIZE_T)) = std::get<2>(bestBlock);
			RETURN_CONSTRUCTED_MEMORY(DataType, reinterpret_cast<void*>(aligned));
		}

		RoutedBlockHeader* previousBlock = mTailMemBlock;
		UINT8* arenaEnd = reinterpret_cast<UINT8*>(mBase) + mArenaCapacity;
		UINT8* newBlockArea = reinterpret_cast<UINT8*>(previousBlock + 1) + previousBlock->size;

		if (newBlockArea + sizeof(RoutedBlockHeader) <= arenaEnd) {
			RoutedBlockHeader* newBlock = reinterpret_cast<RoutedBlockHeader*>(newBlockArea);
			uintptr_t rawAddress = reinterpret_cast<uintptr_t>(newBlock + 1);
			uintptr_t aligned = Align(rawAddress + sizeof(SIZE_T), alignof(DataType));
			SIZE_T offset = static_cast<SIZE_T>(aligned - rawAddress);
			SIZE_T blockSize = Align(offset + requiredSize, alignof(RoutedBlockHeader));

			if (rawAddress + blockSize <= reinterpret_cast<uintptr_t>(arenaEnd)) {
				newBlock->free = false;
				newBlock->next = nullptr;
				newBlock->size = blockSize;

				if constexpr (CoalesceAlgo == CoalesceAlgorithm::LinkPrevious)
					newBlock->prev = previousBlock;

				previousBlock->next = newBlock;
				mTailMemBlock = newBlock;
				*reinterpret_cast<SIZE_T*>(aligned - sizeof(SIZE_T)) = offset;

				RETURN_CONSTRUCTED_MEMORY(DataType, reinterpret_cast<void*>(aligned));
			}
		}
	}
	throw std::runtime_error(
		BUILD_RUNTIME_ERR_MSG("Cannot Allocate A Free Block Or Create New One In This Arena!\nOut Of Memory In Arena")
	);
}
