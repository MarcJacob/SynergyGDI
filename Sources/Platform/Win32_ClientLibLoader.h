#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <SynergyClient.h>

HMODULE LoadClientModule(SynergyClientAPI& APIStruct);
void UnloadClientModule(HMODULE ClientModule);