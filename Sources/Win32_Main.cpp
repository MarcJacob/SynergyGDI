#include "Platform/Win32_Platform.h"
#include "SynergyClient.h"
#include "SynergyClientDrawBuffer.h"

#include <iostream>

// Global context state for the Win32 application layer.
struct Win32AppContext
{
	HINSTANCE ProgramInstance;
	bool bRunning = false;

	bool bUsingConsole = true;

	// Synergy Client dll module
	HMODULE ClientModule;

	// Window & Bitmap data
	HWND MainWindow;
	HDC MainWindowDC;
	HBITMAP MainWindowBitmap;
	HDC MainWindowBitmapDC;

	// Render Pixel data
	PixelRGBA* PixelBuffer;
	uint16_t PixelBufferWidth;
	uint16_t PixelBufferHeight;

	// Memory arena for dynamic memory allocations in the Client app.
	uint8_t* ClientAppMemory;
	size_t ClientAppMemorySize;
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

LRESULT CALLBACK MainWindowProc(HWND window, UINT messageType, WPARAM wParam, LPARAM lParam)
{
	BITMAPINFO bitmapInfo = {};

	switch (messageType)
	{
	case(WM_CLOSE):
		Win32App.bRunning = false;
		break;
	case(WM_SIZE):

		// Do not do anything if Window got minimized.
		if (wParam == SIZE_MINIMIZED)
		{
			break;
		}

		// Re allocate render bitmap if size actually changed.
		int16_t newWidth, newHeight;
		newWidth = LOWORD(lParam);
		newHeight = HIWORD(lParam);

		if (Win32App.PixelBuffer != nullptr 
			&& Win32App.PixelBufferWidth == newWidth
			&& Win32App.PixelBufferHeight == newHeight)
		{
			break;
		}

		Win32App.PixelBufferWidth = newWidth;
		Win32App.PixelBufferHeight = newHeight;
		Win32App.PixelBuffer = nullptr;
			
		bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo);
		bitmapInfo.bmiHeader.biWidth = newWidth;
		bitmapInfo.bmiHeader.biHeight = -newHeight; // Let's stick to upper-left origin.
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 32;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;

		Win32App.MainWindowBitmap = CreateDIBSection(Win32App.MainWindowDC, &bitmapInfo,
			DIB_RGB_COLORS, reinterpret_cast<void**>(&Win32App.PixelBuffer), NULL, NULL);

		if (Win32App.MainWindowBitmap == 0 || Win32App.PixelBuffer == nullptr)
		{
			std::cerr << "ERROR: Failed to allocate bitmap if size " << newWidth << " x " << newHeight << " !\n";
			break;
		}

		Win32App.MainWindowBitmapDC = CreateCompatibleDC(Win32App.MainWindowDC);
		SelectObject(Win32App.MainWindowBitmapDC, Win32App.MainWindowBitmap);

		break;
	case(WM_KEYDOWN):
		ReloadClientLibrary();
	default:
		break;
	}
	return DefWindowProc(window, messageType, wParam, lParam);
}

void CreateMainWindow()
{
	static const LPCWSTR MAIN_WINDOW_CLASS_NAME = L"Synergy Main Window Class";

	// Register Main Window Class
	{
		WNDCLASS mainWindowClass = {};

		mainWindowClass.hInstance = Win32App.ProgramInstance;
		mainWindowClass.lpszClassName = MAIN_WINDOW_CLASS_NAME;
		mainWindowClass.lpfnWndProc = MainWindowProc;
		mainWindowClass.style = CS_OWNDC;

		RegisterClass(&mainWindowClass);
	}
	
	Win32App.MainWindow = CreateWindow(MAIN_WINDOW_CLASS_NAME, L"Synergy", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
		NULL, NULL, Win32App.ProgramInstance, NULL);

	if (Win32App.MainWindow == NULL)
	{
		std::cerr << "Failed to create main window. Error Code = " << GetLastError() << "\n";
		return;
	}

	Win32App.MainWindowDC = GetDC(Win32App.MainWindow);

	ShowWindow(Win32App.MainWindow, 1);
}

// Cleans up resources associated with the Main Window. Closes it first if it wasn't closed already.
void CleanupMainWindow()
{
	if (Win32App.MainWindow != nullptr)
	{
		CloseWindow(Win32App.MainWindow);
		Win32App.MainWindow = nullptr;
	}
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
	return Win32App.MainWindow != nullptr && ClientAPI.APISuccessfullyLoaded();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevious, LPSTR pCmdLine, int nCmdShow)
{
	if (Win32App.bUsingConsole)
	{
		CreateConsole();
	}

	ReloadClientLibrary();

	CreateMainWindow();

	if (AppContextInitSuccessful())
	{
		Win32App.bRunning = true;
	}

	// Create the Client Context, start the client and begin processing frames as fast as possible.
	ClientContext ClientRunningContext = {};
	ClientRunningContext.PersistentMemory.Allocate = malloc;
	ClientRunningContext.PersistentMemory.Free = free;

	ClientAPI.StartClient(ClientRunningContext);

	// Frame tracking
	size_t frameCounter = 0;

	// Message processing & Drawing loop.
	MSG message;
	while (Win32App.bRunning)
	{
		while (PeekMessage(&message, Win32App.MainWindow, NULL, NULL, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		// TODO Process Inputs and other events triggered by the message loop.

		// Prepare frame data for next client frame.
		ClientFrameData frameData = {};
		frameData.FrameMemory = ClientRunningContext.PersistentMemory;
		frameData.FrameNumber = frameCounter++;
		frameData.FrameTime = 0.016f; // TODO: Actual time tracking. Right now we're assuming we'll be running at 60FPS.
		// It would also be possible to artificially pad frames with sleep time to reach that target before anything heavy actually happens
		// in this software.

		ClientFrameDrawCallBuffer frameDrawBuffer = ClientFrameDrawCallBuffer();
		frameDrawBuffer.Buffer = static_cast<uint8_t*>(malloc(64000));
		frameDrawBuffer.BufferSize = 64000;

		frameData.DrawCallBuffer = &frameDrawBuffer;

		// Run Client Frame
		// TODO: Somehow retrieve draw calls, audio samples and whatever other outputs the Client gives us.
		ClientAPI.RunClientFrame(ClientRunningContext, frameData);

		// Drawing pass - rasterize all incoming draw calls after clearing the screen to black.
		{
			// Clear screen to blue.
			ClearPixelBuffer(0xFF000000, Win32App.PixelBuffer, Win32App.PixelBufferWidth, Win32App.PixelBufferHeight);
		}

		// Read draw calls and process them.
		if (!frameDrawBuffer.BeginRead())
		{
			std::cerr << "ERROR: Invalid client draw call buffer for frame " << frameData.FrameNumber << " skipping drawing stage.\n";
		}
		else
		{
			DrawCall* nextDrawCall = nullptr;
			while ((nextDrawCall = frameDrawBuffer.GetNext()) != nullptr)
			{
				// Process Draw Call
				ProcessDrawCall(*nextDrawCall, Win32App.PixelBuffer, Win32App.PixelBufferWidth, Win32App.PixelBufferHeight);
			}
		}

		// Blit updated pixels onto the Main Window.
		BitBlt(Win32App.MainWindowDC, 0, 0, Win32App.PixelBufferWidth, Win32App.PixelBufferHeight, Win32App.MainWindowBitmapDC, 0, 0, SRCCOPY);
		
		// TODO Handle incoming WAV audio samples. Think about that system - in the same spirit as draw calls, should audio use an abstracted
		// idea of "sound bytes" instead, wherein the platform & render layer could process those as it pleases, perhaps using its own sounds ?
	
		// Get rid of frame data. TODO Later is should be saved so a replay system can be built.
		free(frameDrawBuffer.Buffer);
	}

	if (ClientAPI.APISuccessfullyLoaded())
	{
		ClientAPI.ShutdownClient(ClientRunningContext);
		UnloadClientModule(Win32App.ClientModule);
	}

	CleanupMainWindow();

	if (Win32App.bUsingConsole)
	{
		system("pause");
		CloseConsole();
	}

	return 0;
}