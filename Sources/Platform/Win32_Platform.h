// Shared symbols among the Win32 Platform implementation files.

#ifndef WIN32_PLATFORM_INCLUDED
#define WIN32_PLATFORM_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>

struct SynergyClientAPI;

HMODULE LoadClientModule(SynergyClientAPI& APIStruct);
void UnloadClientModule(HMODULE ClientModule);

struct DrawCall;

union PixelRGBA
{
	PixelRGBA(uint32_t bytes): full(bytes) {}

	struct
	{
		uint8_t a, r, g, b;
	};

	uint32_t full;
};

typedef PixelRGBA* Win32PixelBuffer;

void ClearPixelBuffer(PixelRGBA PixelColor, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight);

void ProcessDrawCall(DrawCall& Call, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight);

#endif