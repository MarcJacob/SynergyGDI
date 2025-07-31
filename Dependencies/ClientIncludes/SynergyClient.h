// Contains public-facing declarations for Client API structures.

#ifndef SYNERGY_CLIENT_INCLUDED
#define SYNERGY_CLIENT_INCLUDED

#include <Core/SynergyCore.h>

#include <stdint.h>

// Generic interface for a memory manager usable by the Client for Persistent and Frame memory.
struct ClientMemoryManager
{
	uint8_t* MemoryPtr = nullptr;
	void* (*Allocate)(size_t Size) = nullptr;
	void (*Free)(void* Ptr) = nullptr;
};

// Collection of platform functions that can be called from Client code.
struct PlatformAPI
{
};

// Persistent context data for a single execution of a client. Effectively acts as the Client's static memory.
struct ClientContext
{
	enum State
	{
		INITIALIZED,
		RUNNING,
		ENDED
	};

	State State;

	// Memory manager for persistent memory allocations whose lifetime is managed by the Client.
	ClientMemoryManager PersistentMemory;

	// Current size in pixels of the Viewport, which is the virtual or real (depending on Platform implementation) surface the client uses
	// as reference to build Draw calls.
	Vector2s ViewportSize;
};

struct DrawCall;
enum class DrawCallType;

// Data associated with a single frame of the Client's execution, during which it should integrate the passage of time, react to inputs
// and output draw calls and audio samples.
struct ClientFrameData
{
	size_t FrameNumber;
	float FrameTime;

	// General-purpose Memory for this specific frame. Anything allocated here should be wiped automatically at the end of the frame by the platform.
	ClientMemoryManager FrameMemory;

	// Requests the allocation of a new draw call for this frame, to be processed by the platform usually at the end of the frame.
	// If successful returns a pointer to a base DrawCall structure with the correct underlying data type according to the passed type.
	// If it fails for any reason, returns nullptr.
	DrawCall* (*NewDrawCall)(DrawCallType Type);
};

// Contains function pointers associated with symbol names for easier symbol loading on the platform and to provide a centralized calling
// site for Platform to Client calls.
struct SynergyClientAPI
{
	// Outputs a Hello message with version info on standard output.
	void (*Hello)() = nullptr;

	// Starts a new client session with the given context. The context should be in INITIALIZED state.
	void (*StartClient)(ClientContext& Context) = nullptr;

	// Runs a single frame on the client session associated with the provided context. The context should be in RUNNING state.
	// Frame data needs to be filled in completely.
	void (*RunClientFrame)(ClientContext& Context, ClientFrameData& FrameData) = nullptr;

	// Shuts down the client cleanly.
	void (*ShutdownClient)(ClientContext& Context) = nullptr;

	// Checks that all essential functions have been successfully loaded.
	bool APISuccessfullyLoaded()
	{
		return Hello != nullptr
			&& StartClient != nullptr
			&& RunClientFrame != nullptr
			&& ShutdownClient != nullptr;
	}
};

#endif