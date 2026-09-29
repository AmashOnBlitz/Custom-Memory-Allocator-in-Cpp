#include <Platform.h>
#ifdef _WIN32

void* AllocateMemory(void* addr, SIZE_T size) {
	return ::VirtualAlloc(
		addr,
		size,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);
}

bool FreeMemory(void* addr, SIZE_T size) {
	return ::VirtualFree(
		addr,
		0,
		MEM_RELEASE
	);
}

#endif 