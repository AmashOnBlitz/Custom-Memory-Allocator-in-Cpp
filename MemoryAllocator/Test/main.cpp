#include <iostream>
#include <Windows.h>
#include <allocator.h>

#define print(x) std::cout << x << "\n";

int main(int argc, char** argv[]) {

	Allocator allocator(StandardMemoryUnits::KB);

    int* a = (int*)allocator.Allocate(sizeof(int));
    double* b = (double*)allocator.Allocate(64);
    int* c = (int*)allocator.Allocate(sizeof(int));
    *a = 10;
    *b = 20;
    *c = 30;

    print("A value: " << *a);
    print("A address: " << a);
    print("B value: " << *b);
    print("B address: " << b);
    print("C value: " << *c);
    print("C address: " << c);

    print("\n--- BEFORE FREE ---");
    print(allocator.DebugBlocks());
    allocator.Free(b);

    print("\n--- AFTER FREE(B) ---");
    print(allocator.DebugBlocks());

    int* d = (int*)allocator.Allocate(sizeof(int));
    *d = 40;

    print("\n--- AFTER ALLOCATING D ---");
    print("D value: " << *d);
    print("D address: " << d);
    print(allocator.DebugBlocks());

	for (;;) {};
	return EXIT_SUCCESS;
}