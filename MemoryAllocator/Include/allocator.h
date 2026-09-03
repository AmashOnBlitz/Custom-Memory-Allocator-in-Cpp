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

/*
Style Taken : BlockHeader.size = size of user mem (dont add header area)
			  and BlockHeader will point to start of the block , the start of header 
			  not user mem data;
*/

struct BlockHeader {
	SIZE_T size;
	bool free;
	BlockHeader* next;
};

class Allocator
{
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
	BlockHeader* mHeadMemBlock;
};
