#include <iostream>
#include <Windows.h>
#include <allocator.h>

#define print(x) std::cout << x << "\n";

int main(int argc, char** argv)
{
	print("LinkedPrevious =========================\n");

	Allocator<CoalesceAlgorithm::LinkPrevious> linkedAllocator(StandardMemoryUnits::KB);

	int* a = (int*)linkedAllocator.Allocate(sizeof(int));
	int* b = (int*)linkedAllocator.Allocate(sizeof(int));
	int* c = (int*)linkedAllocator.Allocate(sizeof(int));
	int* d = (int*)linkedAllocator.Allocate(sizeof(int));
	*a = 10;
	*b = 20;
	*c = 30;
	*d = 40;

	print("A address: " << a);
	print("B address: " << b);
	print("C address: " << c);
	print("D address: " << d);

	print("\n=== BEFORE FREE ===");
	print(linkedAllocator.DebugBlocks());
	linkedAllocator.Free(b);

	print("\n=== AFTER FREE(B) ===");
	print(linkedAllocator.DebugBlocks());
	linkedAllocator.Free(c);

	print("\n=== AFTER FREE(C) ===");
	print(linkedAllocator.DebugBlocks());
	linkedAllocator.Free(a);

	print("\n=== AFTER FREE(A) ===");
	print(linkedAllocator.DebugBlocks());
	linkedAllocator.Free(d);

	print("\n=== AFTER FREE(D) ===");
	print(linkedAllocator.DebugBlocks());


	print("\nSearchFromHead =========================\n");

	Allocator<CoalesceAlgorithm::SearchFromHead> searchAllocator(StandardMemoryUnits::KB);
	int* w = (int*)searchAllocator.Allocate(sizeof(int));
	int* x = (int*)searchAllocator.Allocate(sizeof(int));
	int* y = (int*)searchAllocator.Allocate(sizeof(int));
	int* z = (int*)searchAllocator.Allocate(sizeof(int));
	*w = 100;
	*x = 200;
	*y = 300;
	*z = 400;

	print("W address: " << w);
	print("X address: " << x);
	print("Y address: " << y);
	print("Z address: " << z);

	print("\n=== BEFORE FREE ===");
	print(searchAllocator.DebugBlocks());
	searchAllocator.Free(x);

	print("\n=== AFTER FREE(X) ===");
	print(searchAllocator.DebugBlocks());
	searchAllocator.Free(y);

	print("\n=== AFTER FREE(Y) ===");
	print(searchAllocator.DebugBlocks());
	searchAllocator.Free(w);

	print("\n=== AFTER FREE(W) ===");
	print(searchAllocator.DebugBlocks());
	searchAllocator.Free(z);

	print("\n=== AFTER FREE(Z) ===");
	print(searchAllocator.DebugBlocks());

	print("\n=== ALLOCATE AFTER COALESCING ===");

	int* test = (int*)searchAllocator.Allocate(sizeof(int));
	*test = 500;
	print("Test value: " << *test);
	print("Test address: " << test);
	print(searchAllocator.DebugBlocks());

	for (;;) {}

	return EXIT_SUCCESS;
}