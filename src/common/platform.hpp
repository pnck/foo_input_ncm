#pragma once

#ifndef _WIN32
#include <array>

struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    std::array<uint8_t, 8> Data4;
};
#endif

#ifdef _WIN32


#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS

#include <winerror.h>
#include <ntstatus.h>

typedef NTSTATUS STATUS;
typedef DWORD DWORD;

#else
typedef uint32_t STATUS;
typedef uint32_t DWORD;
inline auto GetLastError() {
    return errno;
}
#endif