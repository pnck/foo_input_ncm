#pragma once

#if defined WIN32 && !defined _WIN32
#define _WIN32
#endif

#ifdef _WIN32
#ifndef NT_SUCCESS
#define SUCCESS(Status) (((STATUS)(Status)) >= 0)
#else
#define SUCCESS(Status) NT_SUCCESS(Status)
#endif

#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS

#include <winerror.h>
#include <ntstatus.h>

typedef NTSTATUS STATUS;
typedef DWORD DWORD;
#define KEYSIZE_ERROR STATUS_INVALID_PARAMETER
#define COMMON_ERROR STATUS_UNSUCCESSFUL

#elif defined __APPLE__
#include <CommonCrypto/CommonCryptoError.h>
#define SUCCESS(Status) ((Status) == kCCSuccess)

typedef CCCryptorStatus STATUS;
typedef uint32_t DWORD;
inline auto GetLastError() {
    return errno;
}
#define KEYSIZE_ERROR kCCKeySizeError
#define COMMON_ERROR kCCUnspecifiedError

#endif
