// Shared symbols among the Win32 Platform implementation files.

#ifndef WIN32_PLATFORM_INCLUDED
#define WIN32_PLATFORM_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <iostream>

#define HOTRELOAD_SUPPORTED 1

struct SynergyClientAPI;

void LoadClientModule(SynergyClientAPI& APIStruct, std::string LibNameOverride = "");
void UnloadClientModule(SynergyClientAPI& API);

#if HOTRELOAD_SUPPORTED
// Checks if a new Client library version is available for hotreload, and if there is, do it immediately. Returns whether hotreload was successful.
bool TryHotreloadClientModule(SynergyClientAPI& API, bool bForce = false);

// Cleans up the current iteration of hot reloaded client module files from working directory. To be called on program exit.
void CleanupHotreloadFiles();
#endif


struct DrawCall;
enum class DrawCallType;

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

/*
	Contains all draw calls emitted by the client over a single frame.
	TODO IMPORTANT This needs to be overhauled into a system that puts more power into the platform's hands when it comes to creating and managing draw calls.
	The platform should probably supply functions to create draw calls, and then manage them however it pleases. That way fewer symbols need to be exported or
	defined in header directly.
*/
struct ClientFrameDrawCallBuffer
{
	/*
		To be called before writing into the buffer. Zeroes out the buffer and puts the buffer object into a writeable state.
		Returns whether the buffer is writeable.
	*/
	bool BeginWrite();

	/*
		Provided the buffer isn't full, returns the memory address where a draw call of the passed type can be built.
		Make sure to call BeginWrite() before the first call to NewDrawCall().
		Returns nullptr if the buffer is too small to allocate the given draw call type.
	*/
	DrawCall* NewDrawCall(DrawCallType Type);

	/*
		To be called before reading through the buffer. Puts the buffer object into a readable state.
		Returns whether the buffer is readable.
	*/
	bool BeginRead();

	/*
		Returns next draw call in the buffer.
		Make sure to call BeginRead() before the first call to GetNext().
		Advances the Cursor to the first byte of the next call, meaning it should be equal to BufferSize when reading the entire buffer is done.
		Returns nullptr for any error or reaching the end of the buffer.
	*/
	DrawCall* GetNext();

	// Pre-allocated memory for holding draw call structures.
	uint8_t* Buffer = nullptr;

	// Buffer size in BYTES.
	size_t BufferSize = 0;

	// When filling the buffer in, is the write cursor. When reading the buffer, is the read cursor.
	size_t CursorPosition = 0;
};


#endif