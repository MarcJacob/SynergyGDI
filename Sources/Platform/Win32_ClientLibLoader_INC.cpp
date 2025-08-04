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

#define CLIENT_MODULE_FILENAME "SynergyClientLib.dll"
#define CLIENT_MODULE_DEBUG_SYMBOLS_FILENAME "SynergyClientLib.pdb"

#define CLIENT_MODULE_SOURCE_PATH "Dependencies\\Synergy\\SynergyClientLib\\" CLIENT_MODULE_FILENAME
#define CLIENT_MODULE_DEBUG_SYMBOLS_SOURCE_PATH "Dependencies\\Synergy\\SynergyClientLib\\" CLIENT_MODULE_DEBUG_SYMBOLS_FILENAME

HMODULE LoadClientModule(SynergyClientAPI& APIStruct)
{
	APIStruct = {};

	// Locate library file. Copy it to a temporary target in the working directory and load it.
	if (!CopyFileA(CLIENT_MODULE_SOURCE_PATH, CLIENT_MODULE_FILENAME, FALSE))
	{
		std::cerr << "ERROR: Failed to copy up to date client module from Dependencies folder. Make sure the client library has been built.\n"
			<< "Searched path = " << CLIENT_MODULE_SOURCE_PATH << "\n";
	}
	else
	{
		// Copy .pdb symbols.
		if (!CopyFileA(CLIENT_MODULE_DEBUG_SYMBOLS_SOURCE_PATH, CLIENT_MODULE_DEBUG_SYMBOLS_FILENAME, FALSE))
		{
			std::cerr << "WARNING: Failed to copy up to date client module debug symbols from Dependencies folder. Make sure the client library symbols have been produced.\n"
				<< "Searched path = " << CLIENT_MODULE_DEBUG_SYMBOLS_SOURCE_PATH << "\n";
		}
	}

	HMODULE clientModule = LoadLibraryA(CLIENT_MODULE_FILENAME);
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