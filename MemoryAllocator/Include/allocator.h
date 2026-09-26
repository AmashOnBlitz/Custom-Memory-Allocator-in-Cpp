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
// Fixed size eliminates block header but you can only store same data type (or more
// specifically data types of same size and alignment) in it.
enum class CoalesceAlgorithm {
	LinkPrevious,
	SearchFromHead,
	FixedSize_NoHeader
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

template<CoalesceAlgorithm Algo>
struct AlgoSpecificData {

};

template<>
struct AlgoSpecificData<CoalesceAlgorithm::FixedSize_NoHeader> {
	SIZE_T buffer = 0;
	SIZE_T alignment = 0;
	SIZE_T size = 0;
	SIZE_T allocationIt = 0;
	SIZE_T memPrefixAlignment = 0;
	bool isFirstit = true;
	// Convection Used: bit == 1 will be used and bit == 0 will be free
	void* mMemoryMetaDataArena = nullptr;
	UINT8* mMetaDataBase = nullptr;
	UINT8* maxAllocations = 0;
};

template<CoalesceAlgorithm CoalesceAlgo>
class Allocator
{
	using RoutedBlockHeader = BlockHeader<CoalesceAlgo>;

public:
	Allocator(SIZE_T arenaSize);
	~Allocator();

	// Custom required sizes will be ignored in FixedSize Algo
	// it will return the memory equal to sizeof data type it was bound to
	// and its alignment will be alignof the same data type
	template<typename DataType>
	DataType* Allocate(SIZE_T requiredSize = sizeof(DataType));
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
	AlgoSpecificData<CoalesceAlgo> algoSpecificData;
};

#include "..\Src\TemplateAllocate.cpp"