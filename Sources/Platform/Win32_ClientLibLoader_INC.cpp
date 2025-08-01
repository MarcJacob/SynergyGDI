// Synergy Client Module & API Loading implementation. The symbols are referenced and used in Win32_Main.cpp.

#include "Platform/Win32_Platform.h"

#include <SynergyClient.h>

#include <iostream>
#include <shellapi.h>

#ifndef TRANSLATION_UNIT
static_assert(0, "INC File " __FILE__ " must be included within a translation unit and NOT compiled on its own !");
#endif

#ifndef WIN32_CLIENTLIBLOADER_INCLUDED
#define WIN32_CLIENTLIBLOADER_INCLUDED
#else
static_assert(0, "INC File " __FILE__ " has been included twice !");
#endif

#define CLIENT_MODULE_FILENAME ".\\SynergyClientLib"
#define WCLIENT_MODULE_FILENAME L".\\SynergyClientLib"

#define LIB_RELOAD_PROGRAM_PATH L".\\RefreshDependencies.bat" // TODO Make it configurable.

HMODULE LoadClientModule(SynergyClientAPI& APIStruct)
{
	// Attempt to launch Lib Reload Program.
	SHELLEXECUTEINFO execInfo = {};
	execInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	execInfo.lpVerb = L"open";
	execInfo.lpFile = LIB_RELOAD_PROGRAM_PATH;

	if (ShellExecuteEx(&execInfo))
	{
		// Wait for reload program to do its thing.
		// TODO Let's use ShellExecuteEx and wait for the program to end properly. 
		WaitForSingleObject(execInfo.hProcess, 2000);
		CloseHandle(execInfo.hProcess);
	}

	APIStruct = {};

	HMODULE clientModule = LoadLibrary(WCLIENT_MODULE_FILENAME);
	if (clientModule == nullptr)
	{
		std::cerr << "Error: Couldn't load Client Library. Make sure \"" << CLIENT_MODULE_FILENAME << "\" exists.\n";
		return NULL;
	}

	// Load Client API functions.
	APIStruct = {};

	APIStruct.Hello = reinterpret_cast<decltype(APIStruct.Hello)>(GetProcAddress(clientModule, "Hello"));
	if (APIStruct.Hello == nullptr)
	{
		std::cerr << "Error: Missing symbol \"Hello\" in Client library.\n";
	}

	APIStruct.StartClient = reinterpret_cast<decltype(APIStruct.StartClient)>(GetProcAddress(clientModule, "StartClient"));
	if (APIStruct.StartClient == nullptr)
	{
		std::cerr << "Error: Missing symbol \"CreateClientContext\" in Client library.\n";
	}

	APIStruct.RunClientFrame = reinterpret_cast<decltype(APIStruct.RunClientFrame)>(GetProcAddress(clientModule, "RunClientFrame"));
	if (APIStruct.RunClientFrame == nullptr)
	{
		std::cerr << "Error: Missing symbol \"RunClientFrame \" in Client library.\n";
	}

	APIStruct.ShutdownClient = reinterpret_cast<decltype(APIStruct.ShutdownClient)>(GetProcAddress(clientModule, "ShutdownClient"));
	if (APIStruct.ShutdownClient == nullptr)
	{
		std::cerr << "Error: Missing symbol \"ShutdownClient \" in Client library.\n";
	}

	if (!APIStruct.APISuccessfullyLoaded())
	{
		std::cerr << "FAILED TO LOAD CLIENT LIBRARY.\n";
		APIStruct = {};
		return NULL;
	}

	return clientModule;
}

void UnloadClientModule(HMODULE ClientModule)
{
	FreeLibrary(ClientModule);
	ClientModule = 0;
}