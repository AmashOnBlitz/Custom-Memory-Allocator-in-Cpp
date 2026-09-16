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