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
#include <cerrno>

#if 0
namespace std
{
    template <typename... T>
    [[nodiscard]] inline auto format(fmt::format_string<T...> fmt, T &&...args) {
        return fmt::format(fmt, std::forward<T>(args)...);
    }
} // namespace std
#endif

#define SUCCESS(Status) ((Status) == kCCSuccess)

typedef CCCryptorStatus STATUS;
typedef uint32_t DWORD;
inline auto GetLastError() {
    return errno;
}
#define KEYSIZE_ERROR kCCKeySizeError
#define COMMON_ERROR kCCUnspecifiedError

#endif

// since we are using spdlog, we can use fmt::format instead of std::format,
// which is still unavailable (as of libc++15, within macos-14 github action runner)
#include "spdlog/fmt/bundled/format.h"