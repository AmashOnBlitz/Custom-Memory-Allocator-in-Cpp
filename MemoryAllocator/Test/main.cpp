#include <iostream>
#include <Windows.h>
#include <allocator.h>

#define print(x) std::cout << x << "\n";

int main(int argc, char** argv[]) {

	Allocator allocator(sizeof(int));
	int* intOne = (int*) allocator.Allocate(sizeof(int));
	*intOne = 10;
	print("Int One: " << *intOne);
	print("Int One Addr: " << intOne);
	allocator.Free((void*)(intOne));

	int* intTwo = (int*) allocator.Allocate(sizeof(int));
	*intTwo = 20;
	print("Int Two: " << *intTwo);
	print("Int Two Addr: " << intTwo);
	allocator.Free((void*)(intTwo));

	for (;;) {};
	return EXIT_SUCCESS;
}