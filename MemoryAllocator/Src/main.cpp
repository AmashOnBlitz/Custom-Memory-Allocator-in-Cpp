#include <iostream>
#include <Windows.h>

#define print(x) std::cout << x << "\n";
 
struct ExampleStruct {
	int count = 0; // 4 bytes
	char character = 'A'; // 1 byte
	bool bSwitch = true; // 1 byte
};
// Total = 4 + 1 + 1 = 6 bytes 
// alignment will be 4 bytes so total size = 8 bytes (6 + 2 padding)

int main(int argc, char** argv[]) {
	int* pInt = (int*)malloc(sizeof(int));
	*pInt = 20;
	print("Int: " << *pInt);
	print("Address: " << pInt);

	SYSTEM_INFO sysInfo;
	::GetSystemInfo(&sysInfo);
	DWORD pageSize = sysInfo.dwPageSize;
	LPVOID minAppMemAddr = sysInfo.lpMinimumApplicationAddress;
	LPVOID maxAppMemAddr = sysInfo.lpMaximumApplicationAddress;

	print("Page Size: " << pageSize);
	print("Minimum App Memory Address: " << minAppMemAddr);
	print("Maximum App Memory Address: " << maxAppMemAddr);

	/*---- - OUTPUT------
	Page Size: 4096
	Minimum App Memory Address: 0000000000010000
	Maximum App Memory Address: 00007FFFFFFEFFFF 
	*/

	print("Size Of Struct: " << sizeof(ExampleStruct));
	print("Alignment of Struct: " << alignof(ExampleStruct));

	void* memArea = VirtualAlloc(
		nullptr,
		2*sizeof(ExampleStruct),
		MEM_RESERVE,
		PAGE_READWRITE
	);
	
	memArea = VirtualAlloc(
		memArea,
		sizeof(ExampleStruct),
		MEM_COMMIT,
		PAGE_READWRITE
	);

	// reserved mem size for 2 example struct <---- not backed by physical ram, just virt space reserved
	// commited area for 1 example stuct <-- asked windows to back it ram of size of only 1 example struct 

	ExampleStruct* pStruct = (ExampleStruct*)(memArea);
	print("Address of Instance of pStruct: " << pStruct);

	VirtualFree(
		memArea,
		0, // <--- 0 means doing for whole reserved reigon
		MEM_DECOMMIT
	);
	print("Address of Instance of pStruct(After Decommit): " << pStruct);

	VirtualFree(
		memArea,
		0,
		MEM_RELEASE // <- gave up the reserved virt range too
	);
	print("Address of Instance of pStruct(After Release): " << pStruct);

	// even after decommit or release it would point to same addr (i.e 0x001) bcz 
	// we didnt change the pointer to point to new location, but doing 
	// pStruct->count = 20; would cause an error bcz that memory area is empty 
	// or in other cases used by some another thing

	return EXIT_SUCCESS;
}