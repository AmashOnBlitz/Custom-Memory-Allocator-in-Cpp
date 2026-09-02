#include "allocator.h"
#include <stdexcept>
#include <string>

#define MSG_PREFIX "[Allocator]"
#define BUILD_RUNTIME_ERR_MSG(err) MSG_PREFIX + std::string("Error:") + std::string(err)

Allocator::Allocator(SIZE_T bytes) :
	mBytes(bytes + sizeof(BlockHeader)),
	memory(nullptr),
	mBase(nullptr),
	mCapacity(0),
	mFirstBlock(nullptr)
{
	if (bytes == 0)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate 0 bytes"));
	memory = ::VirtualAlloc(
		nullptr,
		mBytes,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);
	if (!memory)
		throw std::runtime_error(BUILD_RUNTIME_ERR_MSG("Cannot Allocate Memory, VirtualAlloc Failed!"));

	mBase = static_cast<uint8_t*>(memory);
	mCapacity = mBytes;

	mFirstBlock = reinterpret_cast<BlockHeader*>(mBase);
	mFirstBlock->size = mBytes - sizeof(BlockHeader);  // usable space
	mFirstBlock->free = true;
	mFirstBlock->next = nullptr;
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
	BlockHeader* current = mFirstBlock;

	while (current != nullptr) {
		if (current->free && current->size >= bytes) {
			current->free = false;
			return reinterpret_cast<void*>(current + 1);
			// first move one indice forward (one type (BlockHeader in this case, other possible types can be int, float
			// etc) forward and make void point to that address (user memory area) 
			// moved 1 indice forward so that pointer points to user mem area not header
			// used reinterpret_cast as there is no relation b/w void and BlockHeader 
			// it tells compiler just do it, we know what we are doing
		}
		current = current->next;
	}

	return nullptr;
}

void Allocator::Free(void* memory)
{
	if (!memory) return;
	BlockHeader* header = reinterpret_cast<BlockHeader*>(memory) - 1;
	// make this header point to mem addr (i.e 0x100) and move 1 indice backward (1 blockheader backward) 
	// to point at header of mem
	header->free = true;
}