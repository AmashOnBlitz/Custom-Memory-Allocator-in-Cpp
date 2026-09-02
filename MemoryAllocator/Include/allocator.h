#pragma once
#include <Windows.h>
#include <cstdint>

namespace StandardMemoryUnits {
	constexpr size_t KB = 1024;
	constexpr size_t MB = KB * 1024;
}

struct BlockHeader {
	SIZE_T size;
	bool free;
	BlockHeader* next;
};

class Allocator
{
public:
	Allocator(SIZE_T bytes);
	~Allocator();

	void* Allocate(SIZE_T bytes);
	void Free(void* memory);

private:
	SIZE_T mBytes;
	void* memory;
	uint8_t* mBase; // uint for pointer arithetic (i.e moving n bytes ahead)
	SIZE_T mCapacity;
	BlockHeader* mFirstBlock;
};
