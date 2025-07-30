#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "SynergyClient.h"
#include "Win32_ClientLibLoader.h"

#include <iostream>

union PixelRGBA
{
	struct
	{
		uint8_t a, r, g, b;
	};

	uint32_t full;
};

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
	size_t PixelBufferWidth;
	size_t PixelBufferHeight;

	// Memory arena for dynamic memory allocations in the Client app.
	uint8_t* ClientAppMemory;
	size_t ClientAppMemorySize;
};

static Win32AppContext Win32App;
static SynergyClientAPI ClientAPI;

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
		std::cout << "Reloading Client Module.\n";
		UnloadClientModule(Win32App.ClientModule);
		Win32App.ClientModule = LoadClientModule(ClientAPI);
		ClientAPI.Hello();
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

	Win32App.ClientModule = LoadClientModule(ClientAPI);

	CreateMainWindow();

	if (AppContextInitSuccessful())
	{
		Win32App.bRunning = true;

		// Output client Hello.
		ClientAPI.Hello();
	}

	// Create the Client Context, start the client and begin processing frames as fast as possible.

	// Message processing & Drawing loop.
	MSG message;
	while (Win32App.bRunning )
	{
		while (PeekMessage(&message, Win32App.MainWindow, NULL, NULL, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		
		// Clear screen to blue.
		for (int x = 0; x < Win32App.PixelBufferWidth; x++)
		{
			for (int y = 0; y < Win32App.PixelBufferHeight; y++)
			{
				Win32App.PixelBuffer[x * Win32App.PixelBufferHeight + y].full = 0xff0000ff;
			}
		}
		
		BitBlt(Win32App.MainWindowDC, 0, 0, Win32App.PixelBufferWidth, Win32App.PixelBufferHeight, Win32App.MainWindowBitmapDC, 0, 0, SRCCOPY);
	}

	CleanupMainWindow();

	UnloadClientModule(Win32App.ClientModule);

	if (Win32App.bUsingConsole)
	{
		system("pause");
		CloseConsole();
	}

	return 0;
}