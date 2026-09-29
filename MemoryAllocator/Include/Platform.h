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
#endif

#ifdef __linux__
#endif

#ifdef __APPLE__
#endif
