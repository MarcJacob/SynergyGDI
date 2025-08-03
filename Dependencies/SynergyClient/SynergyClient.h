// Contains public-facing declarations for Client API structures.

#ifndef SYNERGY_CLIENT_INCLUDED
#define SYNERGY_CLIENT_INCLUDED

#include <SynergyCore.h>

#include "SynergyClientInput.h"

#include <stdint.h>

// Type definitions and forward declarations

// Base type for Client drawcall. Always actually contains an underlying datatype defined in drawing code.
struct DrawCall; 
// Specific Type of a Client drawcall. Allows finding out which data structure to cast the DrawCall to.
enum class DrawCallType; 

// Base type for an Input Event. Much like drawcalls they always actually contain an underlying full datatype defined in input management code.
struct ActionInputEvent;

// Data associated with a single frame of the Client's execution, during which it should integrate the passage of time, react to inputs
// and output draw calls and audio samples.
struct ClientFrameData
{
	size_t FrameNumber;
	float FrameTime;

	// General-purpose Memory for this specific frame. Anything allocated here should be wiped automatically at the end of the frame by the platform.
	struct
	{
		uint8_t* Memory;
		size_t Size;
	} FrameMemory;

	// Input events to be processed during this frame. It is assumed the platform will have sorted the buffer from oldest to newest event.
	struct
	{
		ActionInputEvent* Buffer;
		size_t EventCount;
	} InputEvents;

	// Requests the allocation of a new draw call for this frame, to be processed by the platform usually at the end of the frame.
	// If successful returns a pointer to a base DrawCall structure with the correct underlying data type according to the passed type.
	// If it fails for any reason, returns nullptr.
	DrawCall* (*NewDrawCall)(ViewportID TargetViewportID, DrawCallType Type);
};

// Collection of platform functions that can be called from Client code.
struct PlatformAPI
{
	/*
		Synchronously requests the allocation of a new Viewport with the given properties.
		Returns ID of new Viewport or VIEWPORT_ERROR_ID if unsuccessful.
	*/
	ViewportID (*AllocateViewport)(const char* DisplayName, Vector2s Dimensions) = nullptr;

	/*
		Synchronously requests the destruction of the viewport with the given ID.
		The viewport will no longer provide inputs from the next frame onward and further output calls targeting it will be ignored.
	*/
	void (*DestroyViewport)(ViewportID ViewportToDestroy) = nullptr;
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

	// Memory guaranteed to be persistent from the moment the client starts to when it shuts down.
	struct
	{
		uint8_t* Memory;
		size_t Size;
	} PersistentMemory;
	

	// Current size in pixels of the Viewport, which is the virtual or real (depending on Platform implementation) surface the client uses
	// as reference to build Draw calls.
	Vector2s ViewportSize;

	// Underlying Platform API, usable at any point by the client and guaranteed to be thread-safe when relevant.
	PlatformAPI Platform;
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