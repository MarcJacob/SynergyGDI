#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "SynergyClient.h"

#include <iostream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevious, LPSTR pCmdLine, int nCmdShow)
{
	AllocConsole();

	// Redirect standard out to allocated console.
	FILE* fDummy;
	freopen_s(&fDummy, "CONOUT$", "w", stdout);

	// TEST Hot reload the client library on a loop
	while (1)
	{
		decltype(OutputHelloWorldFromCore)* Func;
		HMODULE ClientLibModule = LoadLibrary(L"SynergyClientLib.dll");
		if (ClientLibModule == nullptr)
		{
			return 1;
		}

		Func = reinterpret_cast<decltype(Func)>(GetProcAddress(ClientLibModule, "OutputHelloWorldFromCore"));
		Func();

		FreeLibrary(ClientLibModule);

		system("pause");
	}

	return 0;
}