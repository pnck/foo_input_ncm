#pragma once

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
