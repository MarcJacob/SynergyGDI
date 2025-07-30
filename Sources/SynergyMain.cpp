#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "SynergyClient.h"

#include <iostream>

#define CLIENT_MODULE_FILENAME ".\\SynergyClientLib"
#define WCLIENT_MODULE_FILENAME L".\\SynergyClientLib"

struct Win32AppContext
{
	HINSTANCE ProgramInstance;
	HWND MainWindow;

	bool bRunning = false;

	bool bUsingConsole = true;

	// Synergy Client dll module
	HMODULE ClientModule;
};

struct SynergyClientAPI
{
	decltype(Hello)* Hello = nullptr;
};

static Win32AppContext Win32App;
static SynergyClientAPI ClientAPI;

void LoadClientModule()
{
	Win32App.ClientModule = LoadLibrary(WCLIENT_MODULE_FILENAME);
	if (Win32App.ClientModule == nullptr)
	{
		std::cerr << "Error: Couldn't load Client Library. Make sure \"" << CLIENT_MODULE_FILENAME << "\" exists.\n";
		return;
	}

	// Load Client API functions.
	ClientAPI = {};
	ClientAPI.Hello = reinterpret_cast<decltype(ClientAPI.Hello)>(GetProcAddress(Win32App.ClientModule, "Hello"));
}

void UnloadClientModule()
{
	FreeLibrary(Win32App.ClientModule);

	ClientAPI = {};
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT messageType, WPARAM wParam, LPARAM lParam)
{
	switch (messageType)
	{
	case(WM_CLOSE):
		Win32App.bRunning = false;
		break;
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

	ShowWindow(Win32App.MainWindow, 1);
}

void CleanupMainWindow()
{

}

void CreateConsole()
{
	AllocConsole();
	
	// Redirect standard out to allocated console.
	FILE* fDummy;
	freopen_s(&fDummy, "CONOUT$", "w", stdout);

	// Redirect error out to allocated console.
	freopen_s(&fDummy, "CONOUT$", "w", stderr);
}

void CloseConsole()
{
	FreeConsole();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevious, LPSTR pCmdLine, int nCmdShow)
{
	if (Win32App.bUsingConsole)
	{
		CreateConsole();
	}

	LoadClientModule();

	CreateMainWindow();

	if (Win32App.MainWindow != nullptr && Win32App.ClientModule != nullptr)
	{
		Win32App.bRunning = true;

		ClientAPI.Hello();
	}

	MSG message;
	while (Win32App.bRunning && GetMessage(&message, Win32App.MainWindow, NULL, NULL))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	if (Win32App.bUsingConsole)
	{
		CloseConsole();
	}

	return 0;
}