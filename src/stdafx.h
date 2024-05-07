#pragma once

#ifdef _WIN32
#include <ntstatus.h>
#define WIN32_NO_STATUS
#include "foobar2000/SDK/foobar2000.h"
#undef WIN32_NO_STATUS

#else
#include "foobar2000/SDK/foobar2000.h"
#endif

#define PROJECT_HOST_REPO "https://github.com/pnck/foo_input_ncm"
