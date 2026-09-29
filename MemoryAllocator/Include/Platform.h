#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif 


#if defined(__linux__) || defined(__APPLE__)
#include <cstddef>
#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>

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
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

// Current API promises to provide addr as a hint in Unix systems
// and does not flag MAP_FIXED so ONLY USE nullptr/NULL in unix systems 
void* AllocateMemory(void* addr, SIZE_T size);

// Provide size EXPLICICTLY as in unix system we needa pass it 
bool FreeMemory(void* addr, SIZE_T size);