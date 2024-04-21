#pragma once

#ifdef _WIN32
#include <ntstatus.h>
#define WIN32_NO_STATUS
#include "sdk/foobar2000/SDK/foobar2000.h"
#undef WIN32_NO_STATUS

#else
#include "sdk/foobar2000/SDK/foobar2000.h"

#endif

#include "common/consts.hpp"

#if (defined _DEBUG) || (defined DEBUG)
#define DEBUG_LOG(...) FB2K_console_print(__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif
