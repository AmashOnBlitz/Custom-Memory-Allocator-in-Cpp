#include <iostream>
#include <Windows.h>
#include <allocator.h>

#define print(x) std::cout << x << "\n";

int main(int argc, char** argv)
{
	print("LinkPrevious =========================\n");

	Allocator<CoalesceAlgorithm::LinkPrevious> allocator(StandardMemoryUnits::KB);

	print("Initial:");
	print(allocator.DebugBlocks());

	print("Allocate A (int)");
	int* a = allocator.Allocate<int>(sizeof(int));
	*a = 10;
	print(allocator.DebugBlocks());

	print("Allocate B (int)");
	int* b = allocator.Allocate<int>(sizeof(int));
	*b = 20;
	print(allocator.DebugBlocks());

	print("Allocate C (double)");
	double* c = allocator.Allocate<double>(sizeof(double));
	*c = 30.5;
	print(allocator.DebugBlocks());

	print("Free B");
	allocator.Free(b);
	print(allocator.DebugBlocks());

	print("Free A -> should coalesce A + B");
	allocator.Free(a);
	print(allocator.DebugBlocks());

	print("Allocate D (int) -> should reuse coalesced block");
	int* d = allocator.Allocate<int>(sizeof(int));
	*d = 40;
	print(allocator.DebugBlocks());

	print("Free C");
	allocator.Free(c);
	print(allocator.DebugBlocks());

	print("Free D -> should coalesce everything");
	allocator.Free(d);
	print(allocator.DebugBlocks());

	print("Allocate E (int)");
	int* e = allocator.Allocate<int>(sizeof(int));
	*e = 50;
	print(allocator.DebugBlocks());

	print("Allocate F (int)");
	int* f = allocator.Allocate<int>(sizeof(int));
	*f = 60;
	print(allocator.DebugBlocks());

	print("Free E");
	allocator.Free(e);
	print(allocator.DebugBlocks());

	print("Free F -> should coalesce E + F");
	allocator.Free(f);
	print(allocator.DebugBlocks());

	print("Values check:");
	print(*e);
	print(*f);

	print("========================================");

	return 0;
}