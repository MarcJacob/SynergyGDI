#include "Platform/Win32_Platform.h"
#include "SynergyClient.h"
#include "SynergyClientDrawing.h"

#include <vector>

struct Win32Viewport
{
	ViewportID ID;
	Vector2s Dimensions; // Dimensions requested at creation. The client will assume this is the size of the viewport.
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

	ClientFrameDrawCallBuffer ClientDrawCallBuffer;
};

// Global context state for the Win32 application layer.
struct Win32AppContext
{
	HINSTANCE ProgramInstance;
	bool bRunning = false;

	bool bUsingConsole = true;

	// Synergy Client dll module
	HMODULE ClientModule;

	// Memory arena for dynamic memory allocations in the Client app.
	uint8_t* ClientAppMemory;
	size_t ClientAppMemorySize;

	// Active Viewports
	std::vector<Win32Viewport> Viewports;
};

static Win32AppContext Win32App;
static SynergyClientAPI ClientAPI;

void ReloadClientLibrary()
{
	std::cout << "Loading Synergy Client Module.\n";

	if (Win32App.ClientModule != 0)
	{
		UnloadClientModule(Win32App.ClientModule);
	}

	Win32App.ClientModule = LoadClientModule(ClientAPI);

	if (Win32App.ClientModule != 0)
	{
		std::cout << "Synergy Client Module loaded successfully.\n";
		ClientAPI.Hello();
	}
}

bool ViewportIsValid(ViewportID ID)
{
	return Win32App.Viewports.size() > ID && Win32App.Viewports[ID].ID != VIEWPORT_ERROR_ID;
}

Win32Viewport* FindViewportFromWindowHandle(HWND windowHandle)
{
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

LRESULT CALLBACK MainWindowProc(HWND window, UINT messageType, WPARAM wParam, LPARAM lParam)
{
	BITMAPINFO bitmapInfo = {};
	Win32Viewport* viewport = FindViewportFromWindowHandle(window);

	switch (messageType)
	{
	case(WM_CLOSE):
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

		viewport->PixelBufferWidth = newWidth;
		viewport->PixelBufferHeight = newHeight;
		viewport->PixelBuffer = nullptr;
			
		bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo);
		bitmapInfo.bmiHeader.biWidth = newWidth;
		bitmapInfo.bmiHeader.biHeight = -newHeight; // Let's stick to upper-left origin.
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 32;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;

		viewport->DrawingBitmap = CreateDIBSection(viewport->Win32WindowDC, &bitmapInfo,
			DIB_RGB_COLORS, reinterpret_cast<void**>(&viewport->PixelBuffer), NULL, NULL);

		if (viewport->DrawingBitmap == 0 || viewport->PixelBuffer == nullptr)
		{
			std::cerr << "ERROR: Failed to allocate bitmap if size " << newWidth << " x " << newHeight << " !\n";
			break;
		}

		viewport->DrawingBitmapDC = CreateCompatibleDC(viewport->Win32WindowDC);
		SelectObject(viewport->DrawingBitmapDC, viewport->DrawingBitmap);

		break;
	case(WM_KEYDOWN):
		ReloadClientLibrary();
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
		if (Win32App.Viewports[ID].Win32WindowHandle != nullptr)
		{
			CloseWindow(Win32App.Viewports[ID].Win32WindowHandle);
			Win32App.Viewports[ID].Win32WindowHandle = nullptr;
		}

		free(Win32App.Viewports[ID].Name);
		free(Win32App.Viewports[ID].ClientDrawCallBuffer.Buffer);
		Win32App.Viewports[ID] = {};
		Win32App.Viewports[ID].ID = VIEWPORT_ERROR_ID;
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
	
	// TODO Recycle dead viewport IDs.
	Win32App.Viewports.emplace_back();
	Win32Viewport& newViewport = Win32App.Viewports.back();
	newViewport.ID = static_cast<ViewportID>(Win32App.Viewports.size()) - 1;
	newViewport.Dimensions = Dimensions;

	size_t nameLength = strlen(Name);
	newViewport.Name = static_cast<WCHAR*>(malloc(nameLength + 1));
	memset(newViewport.Name, 0, nameLength + 1);

	size_t charactersConverted = 0;
	mbstowcs_s(&charactersConverted, newViewport.Name, nameLength * 2, Name, nameLength);

	newViewport.Win32WindowHandle = CreateWindow(MAIN_WINDOW_CLASS_NAME, newViewport.Name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, Dimensions.x, Dimensions.y,
		NULL, NULL, Win32App.ProgramInstance, NULL);

	if (newViewport.Win32WindowHandle == NULL)
	{
		std::cerr << "Failed to create viewport window. Error Code = " << GetLastError() << "\n";
		DestroyViewport(newViewport.ID);
		return VIEWPORT_ERROR_ID;
	}

	newViewport.Win32WindowDC = GetDC(newViewport.Win32WindowHandle);

	// Allocate Frame Buffer for the viewport.
	ClientFrameDrawCallBuffer frameDrawBuffer = ClientFrameDrawCallBuffer();
	frameDrawBuffer.Buffer = static_cast<uint8_t*>(malloc(64000));
	frameDrawBuffer.BufferSize = 64000;

	newViewport.ClientDrawCallBuffer = frameDrawBuffer;

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
DrawCall* AllocateNewDrawCallOnCurrentFrame(ViewportID TargetViewportID, DrawCallType Type)
{
	return Win32App.Viewports[TargetViewportID].ClientDrawCallBuffer.NewDrawCall(Type);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevious, LPSTR pCmdLine, int nCmdShow)
{
	Win32App.ProgramInstance = hInstance;

	if (Win32App.bUsingConsole)
	{
		CreateConsole();
	}

	ReloadClientLibrary();

	if (AppContextInitSuccessful())
	{
		Win32App.bRunning = true;
	}

	// Create the Client Context, start the client and begin processing frames as fast as possible.
	ClientContext ClientRunningContext = {};
	ClientRunningContext.PersistentMemory.Memory = static_cast<uint8_t*>(malloc(128000));
	ClientRunningContext.PersistentMemory.Size = 128000;

	ClientRunningContext.Platform.AllocateViewport = AllocateViewport;
	ClientRunningContext.Platform.DestroyViewport = DestroyViewport;

	ClientAPI.StartClient(ClientRunningContext);

	// Frame tracking
	size_t frameCounter = 0;

	// Message processing & Drawing loop.
	MSG message;
	while (Win32App.bRunning)
	{
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

		// Prepare frame data for next client frame.
		ClientFrameData frameData = {};
		frameData.FrameMemory.Memory = static_cast<uint8_t*>(malloc(32000));
		frameData.FrameMemory.Size = 32000;
		frameData.FrameNumber = frameCounter++;
		frameData.FrameTime = 0.016f; // TODO: Actual time tracking. Right now we're assuming we'll be running at 60FPS.
		// It would also be possible to artificially pad frames with sleep time to reach that target before anything heavy actually happens
		// in this software.

		frameData.NewDrawCall = AllocateNewDrawCallOnCurrentFrame;

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
		// TODO: Somehow retrieve draw calls, audio samples and whatever other outputs the Client gives us.
		ClientAPI.RunClientFrame(ClientRunningContext, frameData);

		// Drawing pass - rasterize all incoming draw calls after clearing the screen to black.

		// Read draw calls and process them.
		// TODO: This could be multithreaded, once rendering as a whole gets its own thread.
		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			Win32Viewport& viewport = Win32App.Viewports[viewportID];
			
			// Clear screen to blue.
			ClearPixelBuffer(0xFF000000, viewport.PixelBuffer, viewport.PixelBufferWidth, viewport.PixelBufferHeight);
			

			if (!viewport.ClientDrawCallBuffer.BeginRead())
			{
				std::cerr << "ERROR: Invalid client draw call buffer for frame " << frameData.FrameNumber << " skipping drawing stage.\n";
			}
			else
			{
				DrawCall* nextDrawCall = nullptr;
				while ((nextDrawCall = Win32App.Viewports[viewportID].ClientDrawCallBuffer.GetNext()) != nullptr)
				{
					// Process Draw Call
					ProcessDrawCall(*nextDrawCall, viewport.PixelBuffer, viewport.PixelBufferWidth, viewport.PixelBufferHeight);
				}
			}
		}

		for (ViewportID viewportID = 0; viewportID < Win32App.Viewports.size(); viewportID++)
		{
			if (!ViewportIsValid(viewportID)) continue;
			Win32Viewport& viewport = Win32App.Viewports[viewportID];

			// Blit updated pixels onto the Main Window.
			BitBlt(viewport.Win32WindowDC, 0, 0, viewport.PixelBufferWidth, viewport.PixelBufferHeight, viewport.DrawingBitmapDC, 0, 0, SRCCOPY);
		}

		// Deallocate frame memory
		free(frameData.FrameMemory.Memory);

		// TODO Handle incoming WAV audio samples. Think about that system - in the same spirit as draw calls, should audio use an abstracted
		// idea of "sound bytes" instead, wherein the platform & render layer could process those as it pleases, perhaps using its own sounds ?
	}

	if (ClientAPI.APISuccessfullyLoaded())
	{
		ClientAPI.ShutdownClient(ClientRunningContext);
		UnloadClientModule(Win32App.ClientModule);
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

	return 0;
}