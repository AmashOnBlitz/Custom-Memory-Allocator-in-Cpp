#include <iostream>

int main(int argc, char** argv[]) {
	int* pInt = (int*)malloc(sizeof(int));
	*pInt = 20;
	std::cout << "Int: " << *pInt << "\n";
	std::cout << "Address: " << pInt << "\n";
	for (;;) {};
	return EXIT_SUCCESS;
}