#include "SynergyClient.h"
#include "SynergyClientDrawing.h"

#include "Platform/Win32_Platform.h"

#include <vector>

#define TRANSLATION_UNIT Win32_Main
#include "Platform/Win32_ClientLibLoader_INC.cpp"
#include "Platform/Win32_Drawing_INC.cpp"

/* 
	Viewport structure for the Win32 Platform, created by request of the Client.
	For now every viewport should spawn a separate window, and closing any of them should end the program.
	Once a more robust viewport management system with parent / child relationships and events is in, it will be
	possible to complexify their behavior.
*/
struct Win32Viewport
{
	// Unique identifier for this viewport, used by the client to reference it.
	ViewportID ID;

	// Dimensions requested at creation. The client will assume this is the size of the viewport so the platform should leave it alone.
	Vector2s Dimensions; 
	
	// Name used by viewport if real name can't be assigned or retrieved for any reason.
	static constexpr WCHAR* ERROR_NAME = L"VIEWPORT_NAME_ERROR"; 
	
	// Display name of the viewport.
	WCHAR* Name; 

	// Window & Bitmap data
	HWND Win32WindowHandle;
	HDC Win32WindowDC;
	HBITMAP DrawingBitmap;
	HDC DrawingBitmapDC;

	// Render Pixel data
	PixelRGBA* PixelBuffer;
	uint16_t PixelBufferWidth;
	uint16_t PixelBufferHeight;

	// Draw Call buffer, filled in via client requests.
	ClientFrameDrawCallBuffer ClientDrawCallBuffer;
};

struct Win32ActionInputBuffer
{
	ActionInputEvent* Buffer;
	size_t EventCount;
	size_t MaxEventCount;
};

// Global context state for the Win32 application layer.
struct Win32AppContext
{
	// Win32 Program Process instance.
	HINSTANCE ProgramInstance;

	// Whether the app is actively running client frames.
	bool bRunning = false;

	// Whether the program should allocate a console and output debug info to it. TODO: Make this configurable.
	bool bUsingConsole = true;

	// Active Viewports
	std::vector<Win32Viewport> Viewports;

	// Client context & frame data.
	ClientContext ClientRunningContext;
	ClientFrameData ClientFrameData;

	// Input buffer currently being filled in.
	Win32ActionInputBuffer* InputBackbuffer;
};

static Win32AppContext Win32App;
static SynergyClientAPI ClientAPI;

bool ViewportIsValid(ViewportID ID)
{
	return Win32App.Viewports.size() > ID && Win32App.Viewports[ID].ID != VIEWPORT_ERROR_ID;
}

Win32Viewport* FindViewportFromWindowHandle(HWND windowHandle)
{
	// Simple linear search - we should never have too many viewports at once on this platform anyway.
	for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
	{
		if (!ViewportIsValid(viewportID)) continue;
		Win32Viewport& viewport = Win32App.Viewports[viewportID];
		if (viewport.Win32WindowHandle == windowHandle)
		{
			return &viewport;
		}
	}
	return nullptr;
}

/* Builds and records an action input for the given keyboard key code and viewport, putting it in whatever buffer(s) are appropriate. */
void RecordActionInputForViewport(Win32Viewport& Viewport, uint64_t Keycode, bool bRelease)
{
	// Check that there is a backbuffer ready.
	if (Win32App.InputBackbuffer == nullptr
		&& Win32App.InputBackbuffer->EventCount >= Win32App.InputBackbuffer->MaxEventCount)
	{
		return;
	}

	ActionInputEvent& event = Win32App.InputBackbuffer->Buffer[Win32App.InputBackbuffer->EventCount];

	// Determine keycode then fill in the input event.
	ActionKey key = ActionKey::ACTION_KEY_NONE;

	// Numbers
	if (Keycode >= '0' && Keycode <= '9')
	{
		key = static_cast<ActionKey>((Keycode) - '0' + static_cast<uint8_t>(ActionKey::NUMBERS_START));
	}
	// Letters
	else if (Keycode >= 'A' && Keycode <= 'Z')
	{
		key = static_cast<ActionKey>((Keycode) - 'A' + static_cast<uint8_t>(ActionKey::LETTERS_START));

		if (key == ActionKey::KEY_R && !bRelease)
		{
			ReloadClientModule(ClientAPI);
		}
	}
	// Arrow keys
	else if (Keycode >= VK_LEFT && Keycode <= VK_DOWN)
	{
		key = static_cast<ActionKey>((Keycode) - VK_LEFT + static_cast<uint8_t>(ActionKey::ARROW_KEYS_START));
	}
	// TODO Handle all other action keys.

	if (key == ActionKey::ACTION_KEY_NONE)
	{
		// Unsupported input.
		return;
	}

	// Fill in other properties.
	event.Key = key;
	event.bRelease = bRelease;
	event.TimeNormalized = 0.f;
	event.Viewport = Viewport.ID;

	// Increment number of events in the buffer.
	Win32App.InputBackbuffer->EventCount++;
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT messageType, WPARAM wParam, LPARAM lParam)
{
	BITMAPINFO bitmapInfo = {};
	Win32Viewport* viewport = FindViewportFromWindowHandle(window);

	switch (messageType)
	{
	case(WM_CLOSE):
		// Close the whole app on closing any viewport window. TODO don't do this if the viewport was destroyed from a client request,
		// or consider making it configurable per viewport.
		Win32App.bRunning = false;
		break;
	case(WM_SIZE):

		// Ignore message if window isn't tied to a viewport.
		if (viewport == nullptr)
		{
			break;
		}

		// Do not do anything if Window got minimized.
		if (wParam == SIZE_MINIMIZED)
		{
			break;
		}

		// Re allocate render bitmap if size actually changed.
		int16_t newWidth, newHeight;
		newWidth = LOWORD(lParam);
		newHeight = HIWORD(lParam);

		if (viewport->PixelBuffer != nullptr 
			&& viewport->PixelBufferWidth == newWidth
			&& viewport->PixelBufferHeight == newHeight)
		{
			break;
		}

		// Update viewport Buffer data. Leave Dimensions as is as it will keep being used by the client.
		viewport->PixelBufferWidth = newWidth;
		viewport->PixelBufferHeight = newHeight;
		viewport->PixelBuffer = nullptr;
			
		// Init bitmap info for 32 bits RGBA format pixels.
		bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo);
		bitmapInfo.bmiHeader.biWidth = newWidth;
		bitmapInfo.bmiHeader.biHeight = -newHeight; // Let's stick to upper-left origin.
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 32;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;

		// Create Device-Independent Bitmap section and link the viewport's buffer memory to it.
		viewport->DrawingBitmap = CreateDIBSection(viewport->Win32WindowDC, &bitmapInfo,
			DIB_RGB_COLORS, reinterpret_cast<void**>(&viewport->PixelBuffer), NULL, NULL);

		if (viewport->DrawingBitmap == 0 || viewport->PixelBuffer == nullptr)
		{
			std::cerr << "ERROR: Failed to allocate bitmap if size " << newWidth << " x " << newHeight << " !\n";
			break;
		}
		
		// Retrieve Bitmap DC to be used to copy the bitmap memory onto the viewport's window.
		viewport->DrawingBitmapDC = CreateCompatibleDC(viewport->Win32WindowDC);
		SelectObject(viewport->DrawingBitmapDC, viewport->DrawingBitmap);

		break;
	case(WM_KEYDOWN):
		if (viewport != nullptr)
		{
			RecordActionInputForViewport(*viewport, wParam, false);			
		}
		break;
	case(WM_KEYUP):
		if (viewport != nullptr)
		{
			RecordActionInputForViewport(*viewport, wParam, true);			
		}
		break;
	default:
		break;
	}
	return DefWindowProc(window, messageType, wParam, lParam);
}

// Cleans up resources associated with the Main Window. Closes it first if it wasn't closed already.
void DestroyViewport(ViewportID ID)
{
	if (ViewportIsValid(ID))
	{
		Win32Viewport& viewport = Win32App.Viewports[ID];

		// If Win32 window exists for this viewport, close it.
		if (viewport.Win32WindowHandle != nullptr)
		{
			CloseWindow(viewport.Win32WindowHandle);
			viewport.Win32WindowHandle = nullptr;
		}

		// Free Viewport name if it is not using Error name.
		if (viewport.Name != nullptr && lstrcmp(viewport.Name, Win32Viewport::ERROR_NAME) != 0)
		{
			free(viewport.Name);
			viewport.Name = nullptr;
		}

		// Free Draw Buffer.
		if (viewport.ClientDrawCallBuffer.Buffer != nullptr)
		{
			free(viewport.ClientDrawCallBuffer.Buffer);
			viewport.ClientDrawCallBuffer.Buffer = nullptr;
		}

		// Free associated bitmap.
		if (viewport.DrawingBitmap > 0)
		{
			DeleteObject(viewport.DrawingBitmap);
			viewport.DrawingBitmap = NULL;
			viewport.DrawingBitmapDC = NULL;
		}
		
		// Reset viewport and give it the Error ID.
		viewport = {};
		viewport.ID = VIEWPORT_ERROR_ID;
	}
}

ViewportID AllocateViewport(const char* Name, Vector2s Dimensions)
{
	static const LPCWSTR MAIN_WINDOW_CLASS_NAME = L"Synergy Main Window Class";
	static bool MAIN_WINDOW_CLASS_REGISTERED = false;

	// Register Main Window Class
	if (!MAIN_WINDOW_CLASS_REGISTERED)
	{
		WNDCLASS mainWindowClass = {};

		mainWindowClass.hInstance = Win32App.ProgramInstance;
		mainWindowClass.lpszClassName = MAIN_WINDOW_CLASS_NAME;
		mainWindowClass.lpfnWndProc = MainWindowProc;
		mainWindowClass.style = CS_OWNDC;

		RegisterClass(&mainWindowClass);
		MAIN_WINDOW_CLASS_REGISTERED = true;
	}
	
	// Initialize new viewport.
	// TODO Recycle dead viewport IDs.
	Win32App.Viewports.emplace_back();
	Win32Viewport& newViewport = Win32App.Viewports.back();
	newViewport = {};
	newViewport.ID = static_cast<ViewportID>(Win32App.Viewports.size()) - 1;
	newViewport.Dimensions = Dimensions;

	// Convert provided ANSI viewport name to UNICODE.
	size_t nameLength = strlen(Name) + 1;
	newViewport.Name = static_cast<WCHAR*>(malloc(nameLength * 2));

	if (newViewport.Name != nullptr)
	{
		memset(newViewport.Name, 0, nameLength);
		size_t charactersConverted = 0;

		if (mbstowcs_s(&charactersConverted, newViewport.Name, nameLength, Name, nameLength - 1) != 0)
		{
			newViewport.Name = nullptr;
		}
	}
	
	// Use Error Name if Name is still null for any reason.
	if (newViewport.Name == nullptr)
	{
		newViewport.Name = Win32Viewport::ERROR_NAME;
	}

	// Create Win32 Window.
	newViewport.Win32WindowHandle = CreateWindow(MAIN_WINDOW_CLASS_NAME, newViewport.Name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, Dimensions.x, Dimensions.y,
		NULL, NULL, Win32App.ProgramInstance, NULL);

	if (newViewport.Win32WindowHandle == NULL)
	{
		std::cerr << "Failed to create viewport window. Error Code = " << GetLastError() << "\n";
		DestroyViewport(newViewport.ID);
		return VIEWPORT_ERROR_ID;
	}

	// Cache Window Device Context. It will be used to tie Bitmaps created on Size events to the window.
	newViewport.Win32WindowDC = GetDC(newViewport.Win32WindowHandle);

	// Allocate Frame Buffer for the viewport.
	ClientFrameDrawCallBuffer frameDrawBuffer = ClientFrameDrawCallBuffer();
	frameDrawBuffer.Buffer = static_cast<uint8_t*>(malloc(64000));
	frameDrawBuffer.BufferSize = 64000;

	newViewport.ClientDrawCallBuffer = frameDrawBuffer;

	// Show Window immediately and return the viewport ID.
	ShowWindow(newViewport.Win32WindowHandle, 1);
	return newViewport.ID;
}

// Allocates a Windows console and redirects Standard Out and Standard Error to it.
void CreateConsole()
{
	AllocConsole();
	
	// Redirect standard out to allocated console.
	FILE* fDummy;
	freopen_s(&fDummy, "CONOUT$", "w", stdout);

	// Redirect error out to allocated console.
	freopen_s(&fDummy, "CONOUT$", "w", stderr);
}

// Frees the Windows console.
void CloseConsole()
{
	FreeConsole();
}

// Runs necessary post-init checks to ensure initialization was successful and the app is in a state where it can run.
bool AppContextInitSuccessful()
{
	return ClientAPI.APISuccessfullyLoaded();
}

// Redirector function that uses the current frame draw buffer in the platform context.
DrawCall* AllocateNewDrawCall(ViewportID TargetViewportID, DrawCallType Type)
{
	return Win32App.Viewports[TargetViewportID].ClientDrawCallBuffer.NewDrawCall(Type);
}

/*
	Final program cleanup code ran when the program ends for ANY reason.
	When Force Shutdown is true, a lot of "heavy" or thread-sensitive cleanup operations will be skipped, the priority being to free
	external resources, cleanup temporary files and exit before the OS timeout forces us to.
*/
void OnProgramEnd()
{
	// Deallocate client frame memory
	if (Win32App.ClientFrameData.FrameMemory.Memory != nullptr)
	{
		free(Win32App.ClientFrameData.FrameMemory.Memory);
		Win32App.ClientFrameData.FrameMemory.Memory = nullptr;
		Win32App.ClientFrameData.FrameMemory.Size = 0;
	}

	// Deallocate client persistent memory
	if (Win32App.ClientRunningContext.PersistentMemory.Memory != nullptr)
	{
		free(Win32App.ClientRunningContext.PersistentMemory.Memory);
		Win32App.ClientRunningContext.PersistentMemory.Memory = nullptr;
		Win32App.ClientRunningContext.PersistentMemory.Size = 0;
	}
	

	// If Client API was ever successfully loaded, unload it.
	if (ClientAPI.APISuccessfullyLoaded())
	{
		ClientAPI.ShutdownClient(Win32App.ClientRunningContext);
		
		UnloadClientModule(ClientAPI);
	}

	// Destroy remaining viewports.
	for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
	{
		if (!ViewportIsValid(viewportID)) continue;
		Win32Viewport& viewport = Win32App.Viewports[viewportID];

		DestroyViewport(viewport.ID);
	}

	if (Win32App.bUsingConsole)
	{
		system("pause");
		CloseConsole();
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevious, LPSTR pCmdLine, int nCmdShow)
{
	Win32App.ProgramInstance = hInstance;

	if (Win32App.bUsingConsole)
	{
		CreateConsole();
	}

	ReloadClientModule(ClientAPI);

	if (!AppContextInitSuccessful())
	{
		std::cerr << "FATAL ERROR: Platform initialization failed ! Ending program.\n";
		OnProgramEnd();
		return 1;
	}

	// Initialize Client Context & Run Client Start, if the app initialized successfully.
	ClientContext& clientRunningContext = Win32App.ClientRunningContext;

	// Start the client
	clientRunningContext.PersistentMemory.Memory = static_cast<uint8_t*>(malloc(128000));
	clientRunningContext.PersistentMemory.Size = 128000;

	clientRunningContext.Platform.AllocateViewport = AllocateViewport;
	clientRunningContext.Platform.DestroyViewport = DestroyViewport;

	ClientAPI.StartClient(clientRunningContext);

	// Frame data

	ClientFrameData& frameData = Win32App.ClientFrameData;
	{
		frameData.FrameMemory.Memory = static_cast<uint8_t*>(malloc(32000));
		frameData.FrameMemory.Size = 32000;
		frameData.FrameNumber = 0;
		frameData.FrameTime = 0.016f; // TODO: Actual time tracking. Right now we're assuming we'll be running at 60FPS.
		// It would also be possible to artificially pad frames with sleep time to reach that target before anything heavy actually happens
		// in this software.
	
		// Assign Frame System Calls
		frameData.NewDrawCall = AllocateNewDrawCall;
	}

	// Input buffers
	Win32ActionInputBuffer inputBuffers[2];
	inputBuffers[0].Buffer = static_cast<ActionInputEvent*>(malloc(sizeof(ActionInputEvent) * 64));
	memset(inputBuffers[0].Buffer, 0, sizeof(ActionInputEvent) * 64);
	inputBuffers[0].EventCount = 0;
	inputBuffers[0].MaxEventCount = 64;

	inputBuffers[1].Buffer = static_cast<ActionInputEvent*>(malloc(sizeof(ActionInputEvent) * 64));
	memset(inputBuffers[1].Buffer, 0, sizeof(ActionInputEvent) * 64);
	inputBuffers[1].EventCount = 0;
	inputBuffers[1].MaxEventCount = 64;

	// Set first backbuffer to index 0
	int inputBackbufferIndex = 0;
	Win32App.InputBackbuffer = &inputBuffers[inputBackbufferIndex];

	// Message processing & Drawing loop.
	MSG message;

	// Let the party begin
	Win32App.bRunning = true;
	while (Win32App.bRunning)
	{
		TryRefreshClientModule(ClientAPI);

		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			while (PeekMessage(&message, Win32App.Viewports[viewportID].Win32WindowHandle, NULL, NULL, PM_REMOVE))
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
		}

		// TODO Process Inputs and other events triggered by the message loop.

		// Reset frame data for next client frame.
		{
			// Increment Frame Counter
			frameData.FrameNumber++;
			
			// Reset frame memory
			memset(frameData.FrameMemory.Memory, 0, frameData.FrameMemory.Size);

			// Switch input buffers and assign the new frontbuffer to frame.
			if (inputBackbufferIndex == 0)
			{
				inputBackbufferIndex = 1;
				frameData.InputEvents.Buffer = inputBuffers[0].Buffer;
				frameData.InputEvents.EventCount = inputBuffers[0].EventCount;
			}
			else
			{
				inputBackbufferIndex = 0;
				frameData.InputEvents.Buffer = inputBuffers[1].Buffer;
				frameData.InputEvents.EventCount = inputBuffers[1].EventCount;
			}

			// Reset new backbuffer and assign it to application context.
			inputBuffers[inputBackbufferIndex].EventCount = 0;
			memset(inputBuffers[inputBackbufferIndex].Buffer, 0, inputBuffers[inputBackbufferIndex].MaxEventCount * sizeof(ActionInputEvent));
			Win32App.InputBackbuffer = &inputBuffers[inputBackbufferIndex];

			// TODO Update frame time ?
		}

		// Put the draw buffers in write mode.
		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			if (!Win32App.Viewports[viewportID].ClientDrawCallBuffer.BeginWrite())
			{
				// If the buffer can't be written into for any reason, unlink Draw Call function.
				// This will effectively disable drawing for this frame.
				std::cerr << "ERROR: Could not set draw buffer to write mode for frame " << frameData.FrameNumber << "\n";
				frameData.NewDrawCall = nullptr;
			}
		}

		// Run Client Frame
		ClientAPI.RunClientFrame(clientRunningContext, frameData);

		// Drawing pass - rasterize all incoming draw calls after clearing the screen to black.

		// Read draw calls and process them.
		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			Win32Viewport& viewport = Win32App.Viewports[viewportID];
			
			// Clear screen to blue.
			ClearPixelBuffer(0xFF000000, viewport.PixelBuffer, viewport.PixelBufferWidth, viewport.PixelBufferHeight);
			
			if (!viewport.ClientDrawCallBuffer.BeginRead())
			{
				std::cerr << "ERROR: Invalid client draw call buffer for frame " << frameData.FrameNumber << " skipping drawing stage.\n";
				continue;
			}

			DrawCall* nextDrawCall = nullptr;
			while ((nextDrawCall = Win32App.Viewports[viewportID].ClientDrawCallBuffer.GetNext()) != nullptr)
			{
				ProcessDrawCall(*nextDrawCall, viewport.PixelBuffer, viewport.PixelBufferWidth, viewport.PixelBufferHeight);
			}
		}

		// Blit updated pixels onto each Viewport's window.
		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			Win32Viewport& viewport = Win32App.Viewports[viewportID];
			
			BitBlt(viewport.Win32WindowDC, 0, 0, viewport.PixelBufferWidth, viewport.PixelBufferHeight, viewport.DrawingBitmapDC, 0, 0, SRCCOPY);
		}

		// TODO Handle incoming WAV audio samples.
	}

	OnProgramEnd();
	return 0;
}