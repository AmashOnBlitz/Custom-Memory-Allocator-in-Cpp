#include <iostream>
#include <Windows.h>
#include <allocator.h>

#define print(x) std::cout << x << "\n";

int main(int argc, char** argv)
{

    print("LinkedPrevious ================== = \n");

    Allocator<CoalesceAlgorithm::LinkPrevious> linkedAllocator(StandardMemoryUnits::KB);

    int* a = (int*)linkedAllocator.Allocate(sizeof(int));
    double* b = (double*)linkedAllocator.Allocate(64);
    int* c = (int*)linkedAllocator.Allocate(sizeof(int));

    *a = 10;
    *b = 20;
    *c = 30;

    print("A value: " << *a);
    print("A address: " << a);
    print("B value: " << *b);
    print("B address: " << b);
    print("C value: " << *c);
    print("C address: " << c);

    print("\n--- LinkedPrevious BEFORE FREE ---");
    print(linkedAllocator.DebugBlocks());

    linkedAllocator.Free(b);

    print("\n--- LinkedPrevious AFTER FREE(B) ---");
    print(linkedAllocator.DebugBlocks());

    int* d = (int*)linkedAllocator.Allocate(sizeof(int));
    *d = 40;

    print("\n--- LinkedPrevious AFTER ALLOCATING D ---");
    print("D value: " << *d);
    print("D address: " << d);
    print(linkedAllocator.DebugBlocks());

    print("\nSearchFromHead =========================\n");

    Allocator<CoalesceAlgorithm::SearchFromHead> searchAllocator(StandardMemoryUnits::KB);

    int* x = (int*)searchAllocator.Allocate(sizeof(int));
    double* y = (double*)searchAllocator.Allocate(64);
    int* z = (int*)searchAllocator.Allocate(sizeof(int));

    *x = 100;
    *y = 200;
    *z = 300;

    print("X value: " << *x);
    print("X address: " << x);
    print("Y value: " << *y);
    print("Y address: " << y);
    print("Z value: " << *z);
    print("Z address: " << z);

    print("\n--- SearchFromHead BEFORE FREE ---");
    print(searchAllocator.DebugBlocks());

    searchAllocator.Free(y);

    print("\n--- SearchFromHead AFTER FREE(Y) ---");
    print(searchAllocator.DebugBlocks());

    int* w = (int*)searchAllocator.Allocate(sizeof(int));
    *w = 400;

    print("\n--- SearchFromHead AFTER ALLOCATING W ---");
    print("W value: " << *w);
    print("W address: " << w);
    print(searchAllocator.DebugBlocks());

    for (;;) {}

    return EXIT_SUCCESS;
}
