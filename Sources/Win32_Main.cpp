#define TRANSLATION_UNIT Win32_Main

#include "SynergyClientAPI.h"
#include "Platform/Win32_Platform.h"

#include <vector>

// Source includes
#include "Platform/Win32_ClientLibLoader_INC.cpp"
#include "Platform/Win32_Drawing_INC.cpp"
#include "Platform/Win32_FileManagement_INC.cpp"

/* 
	Viewport structure for the Win32 Platform, created by request of the Client.
	For now every viewport should spawn a separate window, and closing any of them should end the program.
	Once a more robust viewport management system with parent / child relationships and events is in, it will be
	possible to complexify their behavior.
*/
struct Win32Viewport
{
	// Unique identifier for this viewport, used by the client to reference it.
	ViewportID ID = VIEWPORT_ERROR_ID;

	// Dimensions requested at creation. The client will assume this is the size of the viewport so the platform should leave it alone.
	Vector2s Dimensions = {};
	
	// Name used by viewport if real name can't be assigned or retrieved for any reason.
	static constexpr WCHAR* ERROR_NAME = L"VIEWPORT_NAME_ERROR"; 
	
	// Display name of the viewport.
	WCHAR* Name = ERROR_NAME; 

	// Window & Bitmap data
	HWND Win32WindowHandle = NULL;
	HDC Win32WindowDC = NULL;
	HBITMAP DrawingBitmap = NULL;
	HDC DrawingBitmapDC = NULL;

	// Render Pixel data
	Win32PixelRGBA* PixelBuffer = nullptr;
	uint16_t PixelBufferWidth = 0;
	uint16_t PixelBufferHeight = 0;

	// Draw Call buffer, filled in via client requests.
	Win32DrawCallBuffer ClientDrawCallBuffer;
};

// Buffer for holding Action inputs recorded by the Win32 platform.
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
	HINSTANCE ProgramInstance = NULL;

	// Whether the app is actively running client frames.
	bool bRunning = false;

	// Active Viewports
	std::vector<Win32Viewport> Viewports;

	// Client context & frame data.
	ClientSessionData ClientRunningContext = {};
	ClientFrameRequestData ClientFrameRequestData = {};

	// Input buffer currently being filled in.
	Win32ActionInputBuffer* InputBackbuffer = nullptr;

	// Input buffer currently in use by frame or about to be used by next frame.
	Win32ActionInputBuffer* InputFrontbuffer = nullptr;

	// Latent input state, used to add extra data to input events.
	Vector2s CursorCoordinates = {};
	bool bCtrlPressed = false;
	bool bShiftPressed = false;
	bool bAltPressed = false;
};

// Main Win32 Static Application Context.
static Win32AppContext Win32App;

// Main instance of loaded symbols from the Client dynamic library.
static SynergyClientAPI Win32ClientAPI;

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
void RecordActionInputForViewport(Win32Viewport& viewport, uint64_t Keycode, bool bRelease)
{
	// Check that there is a backbuffer ready and that it is not full.
	if (Win32App.InputBackbuffer == nullptr
		|| Win32App.InputBackbuffer->EventCount >= Win32App.InputBackbuffer->MaxEventCount)
	{
		return;
	}

	ActionInputEvent& event = Win32App.InputBackbuffer->Buffer[Win32App.InputBackbuffer->EventCount];

	// Determine keycode then fill in the input event.
	ActionKey key = ActionKey::ACTION_KEY_NONE;

	// Numbers
	if (Keycode >= '0' && Keycode <= '9')
	{
		key = (ActionKey)((Keycode)-'0' + (uint8_t)(ActionKey::NUMBERS_START));
	}
	// Letters
	else if (Keycode >= 'A' && Keycode <= 'Z')
	{
		key = (ActionKey)((Keycode)-'A' + (uint8_t)(ActionKey::LETTERS_START));
	}
	// Arrow keys
	else if (Keycode >= VK_LEFT && Keycode <= VK_DOWN)
	{
		key = (ActionKey)((Keycode) - VK_LEFT + (uint8_t)(ActionKey::ARROW_KEYS_START));
	}
	// Mouse buttons
	else if (Keycode >= VK_LBUTTON && Keycode <= VK_MBUTTON)
	{
		switch(Keycode)
		{
			case(VK_LBUTTON):
				key = ActionKey::MOUSE_LEFT;
				break;
			case(VK_RBUTTON):
				key = ActionKey::MOUSE_RIGHT;
				break;
			case(VK_MBUTTON):
				key = ActionKey::MOUSE_MIDDLE;
				break;
			default:
			break;
		}
	}
	// Function keys
	else if (Keycode >= VK_F1 && Keycode <= VK_F12)
	{
		key = (ActionKey)(Keycode - VK_F1 + (uint8_t)ActionKey::FUNCTION_KEYS_START);
	}
	else if (Keycode == VK_SPACE)
	{
		key = ActionKey::KEY_SPACE;
	}

	if (key == ActionKey::ACTION_KEY_NONE)
	{
		// Unsupported input.
		return;
	}

	// PLATFORM INTERACTION HOTKEYS
	// First 6 function keys are reserved by the client.
	if (key == ActionKey::KEY_FUNC7 && !bRelease)
	{
		// Force a hot reload of the client module if hot reloading is supported.
#if HOTRELOAD_SUPPORTED
		RunHotreloadCompileProgram();
		Win32_TryHotreloadClientModule(Win32ClientAPI, true);
#endif
	}
	else if (key == ActionKey::KEY_FUNC8 && !bRelease)
	{
		// Log info about the current state of the platform.
		std::cout << "WIN32 PLATFORM INFO:\n" <<
			"\tMouse Coordinates: " << Win32App.CursorCoordinates.x << " | " << Win32App.CursorCoordinates.y << "\n";
	}

	// Determine modifier key states for this input.
	event.modifiers.modifiersBitmask |= Win32App.bCtrlPressed << 0;
	event.modifiers.modifiersBitmask |= Win32App.bShiftPressed << 1;
	event.modifiers.modifiersBitmask |= Win32App.bAltPressed << 2;

	// Fill in other properties.
	event.key = key;
	event.bRelease = bRelease;
	event.timeNormalized = 0.f;
	event.viewport = viewport.ID;
	event.cursorLocation = Win32App.CursorCoordinates;

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
		// Close the whole app on closing any viewport window.
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
			DIB_RGB_COLORS, (void**)(&viewport->PixelBuffer), NULL, NULL);

		if (viewport->DrawingBitmap == 0 || viewport->PixelBuffer == nullptr)
		{
			std::cerr << "ERROR: Failed to allocate bitmap if size " << newWidth << " x " << newHeight << " !\n";
			break;
		}
		
		// Retrieve Bitmap DC to be used to copy the bitmap memory onto the viewport's window.
		viewport->DrawingBitmapDC = CreateCompatibleDC(viewport->Win32WindowDC);
		SelectObject(viewport->DrawingBitmapDC, viewport->DrawingBitmap);

		break;

	// MOUSE INPUT
	case(WM_MOUSEMOVE):
		if (viewport != nullptr)
		{
			// Update cursor viewport and position.
			Win32App.CursorCoordinates.x = LOWORD(lParam);
			Win32App.CursorCoordinates.y = HIWORD(lParam);
		}
		break;
	case(WM_LBUTTONDOWN):
		if (viewport != nullptr)
		{
			RecordActionInputForViewport(*viewport, VK_LBUTTON, false);
		}
		break;
	case(WM_RBUTTONDOWN):
		if (viewport != nullptr)
		{
			RecordActionInputForViewport(*viewport, VK_RBUTTON, false);
		}
		break;
	case (WM_MBUTTONDOWN):
		if (viewport != nullptr)
		{
			RecordActionInputForViewport(*viewport, VK_MBUTTON, false);
		}
		break;
	case(WM_LBUTTONUP):
		if (viewport != nullptr)
		{
			RecordActionInputForViewport(*viewport, VK_LBUTTON, true);
		}
		break;
	case(WM_RBUTTONUP):
		if (viewport != nullptr)
		{
			RecordActionInputForViewport(*viewport, VK_RBUTTON, true);
		}
		break;
	case (WM_MBUTTONUP):
		if (viewport != nullptr)
		{
			RecordActionInputForViewport(*viewport, VK_MBUTTON, true);
		}
		break;
	// KEYBOARD INPUT
	case(WM_KEYDOWN):
		if (viewport != nullptr)
		{
			// Modifier keys are handled separately. 
			if (wParam == VK_CONTROL)
			{
				Win32App.bCtrlPressed = true;
			}
			else if (wParam == VK_SHIFT)
			{
				Win32App.bShiftPressed = true;
			}
			// Alt is handled in a different message.
			// All other keys go through the normal action input processing.
			else
			{
				RecordActionInputForViewport(*viewport, wParam, false);
			}
		}
		break;
	case(WM_KEYUP):
		if (viewport != nullptr)
		{
			// Modifier keys are handled separately. 
			if (wParam == VK_CONTROL)
			{
				Win32App.bCtrlPressed = false;
			}
			else if (wParam == VK_SHIFT)
			{
				Win32App.bShiftPressed = false;
			}
			// Alt is handled in a different message.
			// All other keys go through the normal action input processing.
			else
			{
				RecordActionInputForViewport(*viewport, wParam, true);
			}
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
	
	// Find ID for viewport, allocate a new slot if necessary.
	ViewportID newViewportID;
	{
		// Find an empty spot in the Viewports array or create a new one if none are available.
		
		for (newViewportID = 0; newViewportID < Win32App.Viewports.size(); newViewportID++)
		{
			if (!ViewportIsValid(newViewportID))
			{
				break; // Take empty spot
			}
		}

		if (newViewportID == Win32App.Viewports.size())
		{
			// Failed to find available spot. Allocate new viewport slot. The ID should already correspond.
			Win32App.Viewports.emplace_back();
		}
	}

	Win32Viewport& newViewport = Win32App.Viewports[newViewportID];
	newViewport = {};
	newViewport.ID = newViewportID;

	newViewport.Dimensions = Dimensions;

	// Convert provided ANSI viewport name to UNICODE.
	size_t nameLength = strlen(Name) + 1;
	newViewport.Name = (WCHAR*)(malloc(nameLength * 2));

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
	Win32DrawCallBuffer frameDrawBuffer = Win32DrawCallBuffer();
	frameDrawBuffer.Buffer = (uint8_t*)(malloc(64000));
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
	return Win32ClientAPI.APISuccessfullyLoaded();
}

/*
	Final program cleanup code ran when the program ends for ANY reason.
	When Force Shutdown is true, a lot of "heavy" or thread-sensitive cleanup operations will be skipped, the priority being to free
	external resources, cleanup temporary files and exit before the OS timeout forces us to.
*/
void OnProgramEnd()
{
	// Deallocate client frame memory
	if (Win32App.ClientFrameRequestData.FrameMemoryBuffer.Memory != nullptr)
	{
		free(Win32App.ClientFrameRequestData.FrameMemoryBuffer.Memory);
		Win32App.ClientFrameRequestData.FrameMemoryBuffer.Memory = nullptr;
		Win32App.ClientFrameRequestData.FrameMemoryBuffer.Size = 0;
	}

	// Deallocate client persistent memory
	if (Win32App.ClientRunningContext.PersistentMemoryBuffer.Memory != nullptr)
	{
		free(Win32App.ClientRunningContext.PersistentMemoryBuffer.Memory);
		Win32App.ClientRunningContext.PersistentMemoryBuffer.Memory = nullptr;
		Win32App.ClientRunningContext.PersistentMemoryBuffer.Size = 0;
	}

	// If Client API was ever successfully loaded, unload it.
	if (Win32ClientAPI.APISuccessfullyLoaded())
	{
		Win32ClientAPI.ShutdownClient(Win32App.ClientRunningContext);
		
		Win32_UnloadClientModule(Win32ClientAPI);
	}

	// Destroy remaining viewports.
	for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
	{
		if (!ViewportIsValid(viewportID)) continue;
		Win32Viewport& viewport = Win32App.Viewports[viewportID];

		DestroyViewport(viewport.ID);
	}

	Win32_CleanupHotreloadFiles();

	if (DEBUG_CONSOLE)
	{
		system("pause");
		CloseConsole();
	}
}

/*
	Returns a valid Client Session Data structure which can be used to start and run a Client with.
*/
ClientSessionData InitializeClientSessionData(size_t PersistentMemorySize)
{
	ClientSessionData sessionData = {};
	sessionData.PersistentMemoryBuffer.Memory = (uint8_t*)(malloc(PersistentMemorySize));
	sessionData.PersistentMemoryBuffer.Size = PersistentMemorySize;

	sessionData.Platform.AllocateViewport = AllocateViewport;
	sessionData.Platform.DestroyViewport = DestroyViewport;
	
	return sessionData;
}

/*
	Returns a valid Frame Request Data structure which can be used to run a Client frame with.
*/
ClientFrameRequestData InitializeFrameRequestData(size_t FrameNumber, size_t FrameMemorySize)
{
	ClientFrameRequestData frameData = {};

	frameData.FrameMemoryBuffer.Memory = (uint8_t*)(malloc(FrameMemorySize));
	frameData.FrameMemoryBuffer.Size = FrameMemorySize;
	frameData.FrameNumber = FrameNumber;
	frameData.FrameTime = CLIENT_FRAME_TIME;

	if (frameData.FrameMemoryBuffer.Memory == nullptr)
	{
		std::cerr << "FATAL ERROR: Failed to allocate memory for Frame Memory !\n";
		return {};
	}

	memset(frameData.FrameMemoryBuffer.Memory, 0, FrameMemorySize);

	// Assign Frame System Calls
	frameData.NewDrawCall = [](ViewportID TargetViewportID, DrawCallType Type)
		{
			// Simply redirect the call directly to whichever draw buffer is assigned to the target viewport.
			return Win32App.Viewports[TargetViewportID].ClientDrawCallBuffer.NewDrawCall(Type);
		};

	// Note cursor location & viewport ID as the frame is about to start.
	frameData.CursorLocation = Win32App.CursorCoordinates;
	frameData.CursorViewport = 0;

	// Assign input buffer.
	frameData.ActionInputEvents.Buffer = Win32App.InputFrontbuffer->Buffer;
	frameData.ActionInputEvents.EventCount = Win32App.InputFrontbuffer->EventCount;

	return frameData;
}

/*
	Frees up the resources taken by a Frame Request Data structure.
*/
void FreeFrameRequestData(ClientFrameRequestData& FrameData)
{
	// Free frame memory and perform a full reset of its properties.
	free(FrameData.FrameMemoryBuffer.Memory);

	FrameData = {};
}

int WINAPI WinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd)
{
	Win32App.ProgramInstance = hInstance;

	if (DEBUG_CONSOLE)
	{
		CreateConsole();
	}

	// Reset Temp folder which serves as a staging area for all files that are only relevant while the program runs.
	Win32_ResetTempDataFolder();

	// If HOT RELOAD is supported, then first try to use that instead of loading whatever is inside the executable directory itself.
#if HOTRELOAD_SUPPORTED
	Win32_TryHotreloadClientModule(Win32ClientAPI);
	if (!Win32ClientAPI.APISuccessfullyLoaded())
	{
		std::cerr << "Failed to load Client library from Client Source folder. Attempting to load client library from executable folder...\n";
		Win32_LoadClientModule(Win32ClientAPI);
	}
#else
	Win32_LoadClientModule(Win32ClientAPI);
#endif

	if (!AppContextInitSuccessful())
	{
		std::cerr << "FATAL ERROR: Platform initialization failed ! Ending program.\n";
		OnProgramEnd();
		return 1;
	}

	// Initialize Client Context & Run Client Start, if the app initialized successfully.
	Win32App.ClientRunningContext = InitializeClientSessionData(1024 * 68); // 68kB Persistent memory

	// Start the client
	Win32ClientAPI.StartClient(Win32App.ClientRunningContext);

	// Input buffers
	Win32ActionInputBuffer inputBuffers[2];
	inputBuffers[0].Buffer = (ActionInputEvent*)(malloc(sizeof(ActionInputEvent) * 64));
	memset(inputBuffers[0].Buffer, 0, sizeof(ActionInputEvent) * 64);
	inputBuffers[0].EventCount = 0;
	inputBuffers[0].MaxEventCount = 64;

	inputBuffers[1].Buffer = (ActionInputEvent*)(malloc(sizeof(ActionInputEvent) * 64));
	memset(inputBuffers[1].Buffer, 0, sizeof(ActionInputEvent) * 64);
	inputBuffers[1].EventCount = 0;
	inputBuffers[1].MaxEventCount = 64;

	// Set first backbuffer to index 0. It will be the first to be filled in with input events and used for the first client frame.
	Win32App.InputBackbuffer = &inputBuffers[0];
	Win32App.InputFrontbuffer = &inputBuffers[1];

	// Frame & Time tracking
	size_t frameCounter = 0;

	// Let the party begin
	Win32App.bRunning = true;
	while (Win32App.bRunning)
	{
#if HOTRELOAD_SUPPORTED
		Win32_TryHotreloadClientModule(Win32ClientAPI);
#endif

		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			
			// Message processing & Drawing loop.
			MSG message;
			while (PeekMessage(&message, Win32App.Viewports[viewportID].Win32WindowHandle, NULL, NULL, PM_REMOVE))
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
		}

		// Switch input buffers so backbuffer that was just filled in with messages will become front buffer and be used by the frame.
		Win32ActionInputBuffer* swapTemp = Win32App.InputBackbuffer;
		Win32App.InputBackbuffer = Win32App.InputFrontbuffer;
		Win32App.InputFrontbuffer = swapTemp;

		// Reset new backbuffer and assign it to application context.
		Win32App.InputBackbuffer->EventCount = 0;
		memset(Win32App.InputBackbuffer->Buffer, 0, Win32App.InputBackbuffer->MaxEventCount * sizeof(ActionInputEvent));
		
		// Prepare frame data for next client frame.
		Win32App.ClientFrameRequestData = InitializeFrameRequestData(frameCounter, 1024 * 16); // 16kB frame memory

		// Put the draw buffers in write mode.
		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			if (!Win32App.Viewports[viewportID].ClientDrawCallBuffer.BeginWrite())
			{
				// If the buffer can't be written into for any reason, unlink Draw Call function.
				// This will effectively disable drawing for this frame.
				std::cerr << "ERROR: Could not set draw buffer to write mode for frame " << Win32App.ClientFrameRequestData.FrameNumber << "\n";
				Win32App.ClientFrameRequestData.NewDrawCall = nullptr;
			}
		}

		// Run Client Frame
		Win32ClientAPI.RunClientFrame(Win32App.ClientRunningContext, Win32App.ClientFrameRequestData);

		// Drawing pass - rasterize all incoming draw calls after clearing the screen to black.

		// Read draw calls and process them.
		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			Win32Viewport& viewport = Win32App.Viewports[viewportID];
			
			// Clear screen to blue.
			Win32_ClearPixelBuffer(0xFF000000, viewport.PixelBuffer, viewport.PixelBufferWidth, viewport.PixelBufferHeight);
			
			if (!viewport.ClientDrawCallBuffer.BeginRead())
			{
				std::cerr << "ERROR: Invalid client draw call buffer for frame " << Win32App.ClientFrameRequestData.FrameNumber << " skipping drawing stage.\n";
				continue;
			}

			DrawCall* nextDrawCall = nullptr;
			while ((nextDrawCall = Win32App.Viewports[viewportID].ClientDrawCallBuffer.GetNext()) != nullptr)
			{
				Win32_ProcessDrawCall(*nextDrawCall, viewport.PixelBuffer, viewport.PixelBufferWidth, viewport.PixelBufferHeight);
			}
		}

		// Blit updated pixels onto each Viewport's window.
		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			Win32Viewport& viewport = Win32App.Viewports[viewportID];
			
			BitBlt(viewport.Win32WindowDC, 0, 0, viewport.PixelBufferWidth, viewport.PixelBufferHeight, viewport.DrawingBitmapDC, 0, 0, SRCCOPY);
		}

		// Free resources taken by Client frame.
		FreeFrameRequestData(Win32App.ClientFrameRequestData);
	}

	OnProgramEnd();
	return 0;
}