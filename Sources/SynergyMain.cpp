#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "SynergyClient.h"

#include <iostream>

#include <shellapi.h>

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
	bool bLoaded = false;

	decltype(Hello)* Hello = nullptr;

	void* (*CreateClientContext)() = nullptr;
};

static Win32AppContext Win32App;
static SynergyClientAPI ClientAPI;

void LoadClientModule()
{
	HINSTANCE ReloadProgram = ShellExecute(NULL, L"open", L"UpdateClientLib.bat", L"", L"", 0);
	
	// Wait for reload program to do its thing.
	// TODO Let's use ShellExecuteEx and wait for the program to end properly. 
	Sleep(500);

	Win32App.ClientModule = LoadLibrary(WCLIENT_MODULE_FILENAME);
	if (Win32App.ClientModule == nullptr)
	{
		std::cerr << "Error: Couldn't load Client Library. Make sure \"" << CLIENT_MODULE_FILENAME << "\" exists.\n";
		return;
	}

	// Load Client API functions.
	ClientAPI = {};
	
	ClientAPI.Hello = reinterpret_cast<decltype(ClientAPI.Hello)>(GetProcAddress(Win32App.ClientModule, "Hello"));
	if (ClientAPI.Hello == nullptr)
	{
		std::cerr << "Error: Missing symbol \"Hello\" in Client library.\n";
		return;
	}
	
	ClientAPI.CreateClientContext = reinterpret_cast<decltype(ClientAPI.CreateClientContext)>(GetProcAddress(Win32App.ClientModule, "CreateClientContext"));
	if (0 && ClientAPI.CreateClientContext == nullptr)
	{
		std::cerr << "Error: Missing symbol \"CreateClientContext\" in Client library.\n";
		return;
	}

	ClientAPI.bLoaded = true;
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
	case(WM_KEYDOWN):
		std::cout << "Reloading Client Module.\n";
		UnloadClientModule();
		LoadClientModule();
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
	return Win32App.MainWindow != nullptr && ClientAPI.bLoaded;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevious, LPSTR pCmdLine, int nCmdShow)
{
	if (Win32App.bUsingConsole)
	{
		CreateConsole();
	}

	LoadClientModule();

	CreateMainWindow();

	if (AppContextInitSuccessful())
	{
		Win32App.bRunning = true;

		// Output client Hello.
		ClientAPI.Hello();
	}

	MSG message;
	while (Win32App.bRunning && GetMessage(&message, Win32App.MainWindow, NULL, NULL))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	CleanupMainWindow();

	UnloadClientModule();

	if (Win32App.bUsingConsole)
	{
		system("pause");
		CloseConsole();
	}

	return 0;
}