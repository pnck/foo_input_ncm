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

