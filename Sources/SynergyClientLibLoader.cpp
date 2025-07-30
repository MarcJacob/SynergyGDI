#include <SynergyClientLibLoader.h>

#include <iostream>
#include <shellapi.h>

#define CLIENT_MODULE_FILENAME ".\\SynergyClientLib"
#define WCLIENT_MODULE_FILENAME L".\\SynergyClientLib"

HMODULE LoadClientModule(SynergyClientAPI& APIStruct)
{
	HINSTANCE reloadProgram = ShellExecute(NULL, L"open", L"UpdateClientLib.bat", L"", L"", 0);

	// Wait for reload program to do its thing.
	// TODO Let's use ShellExecuteEx and wait for the program to end properly. 
	Sleep(250);

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
}