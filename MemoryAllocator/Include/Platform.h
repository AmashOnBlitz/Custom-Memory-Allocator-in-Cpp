#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif 

#if defined(__linux__) || defined(__APPLE__)
#include <cstddef>
#include <cstdint>

using SIZE_T = std::size_t;
using UINT8 = std::uint8_t;
using BYTE = UINT8;
using DWORD = std::uint32_t;
using BOOL = bool;
using LPVOID = void*;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#endif

#ifdef __linux__
#endif

#ifdef __APPLE__
#endif


void* AllocateMemory(void* addr, SIZE_T size);

// Just like Win use size = 0 for freeing whole mem block
// In Windows platform size will be ignored and 0 will be taken
bool FreeMemory(void* addr, SIZE_T size = 0);