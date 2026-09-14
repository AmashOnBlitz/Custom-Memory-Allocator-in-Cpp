#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace StandardMemoryUnits {
	constexpr size_t B = 1;
	constexpr size_t KB = B * 1024;
	constexpr size_t MB = KB * 1024;
}

// Link Prev adds additional 8 bytes to memory block header (which becomes 24 + 8 = 32 bytes)
// Search From Head searches linearly from head list to prev list so O(n)
// Link Prev takes less time while Search from Head wastes less memory
// On 10k allocations test, LinkPrev was approx 36.8% fast (64 bit Windows)
enum class CoalesceAlgorithm {
	LinkPrevious,
	SearchFromHead
};

/*
Style Taken : BlockHeader.size = size of user mem + slot area (dont add header area)
			  and BlockHeader will point to start of the block , the start of header
			  not user mem data;
*/

template <CoalesceAlgorithm CoalesceAlgo>
struct BlockHeader {
	SIZE_T size;
	bool free;
	BlockHeader* next;
	//SIZE_T alignmentOffset;
};

template<>
struct BlockHeader<CoalesceAlgorithm::LinkPrevious> {
	SIZE_T size;
	bool free;
	BlockHeader* next;
	BlockHeader* prev;
	//SIZE_T alignmentOffset;
};

template<CoalesceAlgorithm CoalesceAlgo>
class Allocator
{
	using RoutedBlockHeader = BlockHeader<CoalesceAlgo>;

public:
	Allocator(SIZE_T arenaSize);
	~Allocator();

	template<typename DataType>
	void* Allocate(SIZE_T requiredSize);
	void Deallocate(void* memory);
	void Free(void* memory);
	std::string DebugBlocks();

private:
	uintptr_t Align(uintptr_t rawAddr, uintptr_t alignment); // uintptr_t to do int maths on pointer 
private:
	void* mMemoryArena;
	UINT8* mBase; //UINT cuz its pointer to individual bytes (in this case base of mem i.e 0x100)
	SIZE_T mArenaCapacity;
	RoutedBlockHeader* mHeadMemBlock;
};
