#include "allocator.h"
#include <stdexcept>
#include <string>

#define MSG_PREFIX "[Allocator]"
#define BUILD_RUNTIME_ERR_MSG(err) MSG_PREFIX + std::string("Error:") + std::string(err)

Allocator::Allocator(SIZE_T bytes) :
	mBytes(bytes),
	memory(nullptr),
	mBase(nullptr),
	mCapacity(0),
	mOffset(0)
{
	if (bytes == 0)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate 0 bytes"));
	memory = ::VirtualAlloc(
		nullptr,
		bytes,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);
	if (!memory)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate Memory, VirtualAlloc Failed!"));

	mBase = static_cast<uint8_t*>(memory);
	mCapacity = mBytes;
}

Allocator::~Allocator()
{
	if (memory) {
		VirtualFree(
			memory,
			0,
			MEM_RELEASE
		);
	}
}

void* Allocator::Allocate(SIZE_T bytes)
{
	if (mOffset + bytes > mCapacity)
		throw std::runtime_error(
			BUILD_RUNTIME_ERR_MSG(
				"Out Of Memory In Allocator! Used: " +
				std::to_string(mOffset) +
				" Capacity: " +
				std::to_string(mCapacity)
			)
		);
	void* result = mBase + mOffset;
	mOffset += bytes;
	return result;
}

SIZE_T Allocator::GetOccupiedBytes()
{
	return mOffset;
}

SIZE_T Allocator::GetFreeBytes()
{
	return mCapacity - mOffset;
}
