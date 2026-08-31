#include <iostream>
#include <Windows.h>
#include <allocator.h>

#define print(x) std::cout << x << "\n";

int main(int argc, char** argv[]) {
	Allocator allocator(1024 * StandardMemoryUnits::MB);
	int intCount = (1024 * StandardMemoryUnits::MB) / sizeof(int);

	for (int i = { 0 }; i < intCount; i++) {
		int* x = (int*)allocator.Allocate(sizeof(int));
		*x = 12345;

		if (i % (intCount/10) == 0) {
			print("==================================");
			print("Intergers Allocated: " << i + 1);
			print("Memory Free(MB): " << (allocator.GetFreeBytes() / StandardMemoryUnits::MB));
			print("Memory Used(MB): " << (allocator.GetOccupiedBytes() / StandardMemoryUnits::MB));
			print("===============XXX=================")
		}
	}

	print("Total Integers Created: " << intCount);

	for (;;) {};
	return EXIT_SUCCESS;
}