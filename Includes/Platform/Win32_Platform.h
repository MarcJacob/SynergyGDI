// Shared symbols among the Win32 Platform implementation files.

#ifndef WIN32_PLATFORM_INCLUDED
#define WIN32_PLATFORM_INCLUDED

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <iostream>

// WIN32 PLATFORM LAYER COMPILATION FLAGS

#define DEBUG_CONSOLE 1

#define HOTRELOAD_SUPPORTED 1

#if HOTRELOAD_SUPPORTED

// NOTE(MJ) This whole hotreload system is a little garbage because it's so compiler-specific and goes around the entire build system,
// but I just couldn't find a convenient way to have CMake do something that would work with hotreloading under the constraint that .pdb
// files don't get unloaded when their associated library does...
//
// So for now let's consider hot reloading a very particular feature that has to be setup locally. As long as I'm working alone on this using
// the MSVC compiler I'm fine, but the second this changes we'll need to move the entire hotreload system configuration to a file or something.

// Script ran on each hot reload compile, triggering a simplified build pipeline on client code that has to output .dll and .pdb files
// compatible with hotreloading (IE different name per iteration).
#define CLIENT_MODULE_HOTRELOAD_COMPILE_SCRIPT "..\\Scripts\\Win32Dev\\CompileClientForHotreload.bat"

// Folder where new versions of the client library can be retrieved and hotreloaded as the program is running.
#define CLIENT_MODULE_SOURCE_PATH "Dependencies\\Synergy\\SynergyClientLib\\"

#endif // HOTRELOAD_SUPPORTED

#define CLIENT_FRAMES_PER_SECOND (60)
#define CLIENT_FRAME_TIME (1.f / CLIENT_FRAMES_PER_SECOND)

// --------------------------------------

// CLIENT LOADING & API

struct SynergyClientAPI;

void Win32_LoadClientModule(SynergyClientAPI& APIStruct, std::string LibNameOverride = "");
void Win32_UnloadClientModule(SynergyClientAPI& API);

#if HOTRELOAD_SUPPORTED
/*
	Checks if a new Client library version is available for hotreload, and if there is, do it immediately.Returns whether hotreload was successful.
	Uses the CLIENT_MODULE_SOURCE_PATH folder to find a new lib file to load, and if successful, copies it and its symbols to the temp data folder.
*/
bool Win32_TryHotreloadClientModule(SynergyClientAPI& API, bool bForce = false);

// Cleans up the current iteration of hot reloaded client module files from working directory.
void Win32_CleanupHotreloadFiles();
#endif

// -----------------------------

// DRAWING

struct DrawCall;
enum class DrawCallType;

union Win32PixelRGBA
{
	Win32PixelRGBA(uint32_t bytes): full(bytes) {}

	struct
	{
		uint8_t a, r, g, b;
	};

	uint32_t full;
};

typedef Win32PixelRGBA* Win32PixelBuffer;

void Win32_ClearPixelBuffer(Win32PixelRGBA PixelColor, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight);

void Win32_ProcessDrawCall(DrawCall& Call, Win32PixelBuffer& PixelBuffer, uint16_t BufferWidth, uint16_t BufferHeight);

/*
	Contains all draw calls emitted by the client over a single frame.
*/
struct Win32DrawCallBuffer
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

// FILE MANAGEMENT

/*
	Creates a copy of any file passed as SOURCE into the Temp Data folder at the provided relative destination path.
	SourcePath is absolute or relative to current working directory.
	DestPath is relative to Temp Data folder.

	Returns whether the copy was successful.
*/
bool Win32_CreateTempCopyFile(const std::string& SourcePath, const std::string& DestPath);

/*
	Deletes a file from the Temp Data folder with the given relative path.
	FilePath is relative to Temp Data folder.
*/
void Win32_DeleteTempFile(const std::string& FilePath);

/*
	Deletes the entirety of the Temp Data folder and recreates it. Done at platform initialization.
*/
void Win32_ResetTempDataFolder();

/*
	Converts a passed in Temp Data Folder Relative path into a path relative to the current working directory.
*/
std::string Win32_ConvertTempPathToRelativePath(const std::string& TempPath);

#endif // WIN32_PLATFORM_INCLUDED