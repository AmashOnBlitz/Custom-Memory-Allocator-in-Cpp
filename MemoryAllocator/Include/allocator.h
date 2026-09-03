#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace StandardMemoryUnits {
	constexpr size_t KB = 1024;
	constexpr size_t MB = KB * 1024;
}

// Link Previous adds additional 8 bytes to memory block header (which is 24 + 8 = 32 bytes)
// Search From Head seaches linearly from head list to previous list when coalescing which is O(n)
// Allocation of 4 byte int :
// Without Previous : 24 + 4 = 28 byte 
// With Previous : 28 + 4 = 32 byte
// Your choice: Less Memory Waste with More Processing Time,
// or More Memory Waste with Less Processing Time.
//
// Suggestion: Use More Memory Waste on more memory systems,
// while using Less Memory Waste on memory limited systems.
enum class CoalesceAlgorithm {
	LinkPrevious,
	SearchFromHead
};

/*
Style Taken : BlockHeader.size = size of user mem (dont add header area)
			  and BlockHeader will point to start of the block , the start of header
			  not user mem data;
*/

template <CoalesceAlgorithm CoalesceAlgo>
struct BlockHeader {
	SIZE_T size;
	bool free;
	BlockHeader* next;
};

template<>
struct BlockHeader<CoalesceAlgorithm::LinkPrevious> {
	SIZE_T size;
	bool free;
	BlockHeader* next;
	BlockHeader* prev;
};

template<CoalesceAlgorithm CoalesceAlgo>
class Allocator
{
	using RoutedBlockHeader = BlockHeaderBase<CoalesceAlgo>;

public:
	Allocator(SIZE_T arenaSize);
	~Allocator();

	void* Allocate(SIZE_T requiredSize);
	void Deallocate(void* memory);
	void Free(void* memory);
	std::string DebugBlocks();

private:
	void* mMemoryArena;
	UINT8* mBase; //UINT cuz its pointer to individual bytes (in this case baso of mem i.e 0x100)
	SIZE_T mArenaCapacity;
	RoutedBlockHeader* mHeadMemBlock;
};
