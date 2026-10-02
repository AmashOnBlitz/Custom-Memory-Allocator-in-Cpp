#include <Platform.h>

#if defined(__linux__) || defined(__APPLE__)

static const SIZE_T GetPageSize()
{

    static const SIZE_T page = (SIZE_T)sysconf(_SC_PAGESIZE);
    return page;
}

void* AllocateMemory(void* addr, SIZE_T size)
{
    (void)addr;
    if (size == 0)
        return nullptr;

    const SIZE_T page = GetPageSize();
    const SIZE_T total = size + page;

    void* mem = mmap(
        nullptr,
        total,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);

    if (mem == MAP_FAILED)
        return nullptr;

    *static_cast<SIZE_T*>(mem) = total;
    // could have used uintptr maths but char does same in short
    return static_cast<char*>(mem) + page;
}

bool FreeMemory(void* addr, SIZE_T size)
{
    (void)size;
    if (!addr)
        return false;
    void* usrAddr = static_cast<char*>(addr) - GetPageSize();
    SIZE_T total = *static_cast<SIZE_T*>(usrAddr);
    return munmap(usrAddr, total) == 0;
}

#endif