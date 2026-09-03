#include "allocator.h"
#include <stdexcept>
#include <string>

#define MSG_PREFIX "[Allocator]"
#define BUILD_RUNTIME_ERR_MSG(err) MSG_PREFIX + std::string("Error:") + std::string(err)

Allocator::Allocator(SIZE_T arenaSize) :
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


Allocator::~Allocator()
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

void* Allocator::Allocate(SIZE_T requiredSize)
{
	if (requiredSize == 0)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate 0 bytes"));

	if (requiredSize > (mArenaCapacity - sizeof(BlockHeader)))
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Arena to small to allocate"));

	if (!mHeadMemBlock) {
		mHeadMemBlock = reinterpret_cast<BlockHeader*>(mBase);
		mHeadMemBlock->size = requiredSize;
		mHeadMemBlock->free = false;
		mHeadMemBlock->next = nullptr;
		return reinterpret_cast<void*>(mHeadMemBlock + 1);
	}

	BlockHeader* previousBlock = mHeadMemBlock;
	BlockHeader* currentBlock = mHeadMemBlock;

	std::vector<std::pair<BlockHeader*, SIZE_T>> suitableBlocks;

	while (currentBlock) {
		if (currentBlock->free && currentBlock->size >= requiredSize) {
			SIZE_T sizeExceed = currentBlock->size - requiredSize;
			suitableBlocks.push_back({ currentBlock,sizeExceed });
		}
		previousBlock = currentBlock;
		currentBlock = currentBlock->next;
	}

	if (suitableBlocks.size() != 0) {
		std::pair<BlockHeader*, SIZE_T> bestBlock = suitableBlocks[0];
		for (const auto& block : suitableBlocks) {
			if (block.second < bestBlock.second) {
				bestBlock = block;
			}
		}

		SIZE_T remaining = bestBlock.first->size - requiredSize;
		if (remaining >= sizeof(BlockHeader) + 1) {
			bestBlock.first->size = requiredSize;
			UINT8* newBlockArea = reinterpret_cast<UINT8*>(bestBlock.first + 1) + bestBlock.first->size;
			BlockHeader* newBlock = reinterpret_cast<BlockHeader*>(newBlockArea);
			newBlock->free = true;
			newBlock->size = remaining - sizeof(BlockHeader);
			newBlock->next = bestBlock.first->next;
			bestBlock.first->next = newBlock;
		}
		bestBlock.first->free = false;
		return reinterpret_cast<void*>(bestBlock.first + 1);
	}

	UINT8* newBlockArea = reinterpret_cast<UINT8*>(previousBlock + 1) + previousBlock->size;
	if ((newBlockArea + sizeof(BlockHeader) + requiredSize) <= mBase + mArenaCapacity) {
		BlockHeader* newBlock = reinterpret_cast<BlockHeader*>(newBlockArea);
		newBlock->free = false;
		newBlock->next = nullptr;
		newBlock->size = requiredSize;
		previousBlock->next = newBlock;
		return reinterpret_cast<void*>(newBlock + 1);
	}

	throw std::runtime_error(
		BUILD_RUNTIME_ERR_MSG("Cannot Allocate A Free Block Or Create New One In This Arena!\nOut Of Memory In Arena")
	);
}

void Allocator::Deallocate(void* memory)
{
	if (!memory)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Deallocate/Free nullptr"));

	BlockHeader* block = reinterpret_cast<BlockHeader*>(memory) - 1;
	block->free = true;
}

void Allocator::Free(void* memory) {
	Deallocate(memory);
}

std::string Allocator::DebugBlocks()
{
	std::string debugStr = "";
	if (!mHeadMemBlock) return debugStr;
	if (mHeadMemBlock->size == 0) return debugStr;

	BlockHeader* currentBlock = mHeadMemBlock;
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
