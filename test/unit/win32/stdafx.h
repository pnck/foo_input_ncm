//
// pch.h
// Header for standard system include files.
//

#pragma once


#include <ntstatus.h>
#define WIN32_NO_STATUS
#include "gtest/gtest.h"
#include <Windows.h>
#undef WIN32_NO_STATUS

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status)          (((NTSTATUS)(Status)) >= 0)
#endif

