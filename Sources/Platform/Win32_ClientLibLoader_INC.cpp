// Synergy Client Module & API Loading implementation. The symbols are referenced and used in Win32_Main.cpp.

#include "Platform/Win32_Platform.h"

#include <SynergyClient.h>

#include <iostream>
#include <shellapi.h>

#include <string>

#ifndef TRANSLATION_UNIT
static_assert(0, "INC File " __FILE__ " must be included within a translation unit and NOT compiled on its own !");
#endif

#ifndef WIN32_CLIENTLIBLOADER_INCLUDED
#define WIN32_CLIENTLIBLOADER_INCLUDED
#else
static_assert(0, "INC File " __FILE__ " has been included twice !");
#endif

#define CLIENT_MODULE_FILENAME "SynergyClientLib"
#define CLIENT_MODULE_SOURCE_PATH "Dependencies\\Synergy\\SynergyClientLib\\"
/*
	Iteration number of the client library and debug symbols being used. Gets increment on each hotreload that involves a successful copy of
	the dll.
*/
uint8_t ClientLibHotReloadIteration = 0;

void GetModuleAndSymbolsName(std::string& OutModuleName, std::string& OutSymbolsName, uint8_t HotreloadIteration)
{
	OutModuleName = CLIENT_MODULE_FILENAME;
	OutSymbolsName = CLIENT_MODULE_FILENAME;
	if (HotreloadIteration > 0)
	{
		OutModuleName.append("_HOTRELOAD_" + std::to_string(HotreloadIteration));
		OutSymbolsName.append("_HOTRELOAD_" + std::to_string(HotreloadIteration));
	}

	OutModuleName.append(".dll");
	OutSymbolsName.append(".pdb");
}

HMODULE LoadClientModule(SynergyClientAPI& APIStruct)
{
	APIStruct = {};

	// Locate library file. Copy it to a temporary target in the working directory and load it.
	std::string FinalClientModuleName;
	std::string FinalClientSymbolsName;
	GetModuleAndSymbolsName(FinalClientModuleName, FinalClientSymbolsName, ClientLibHotReloadIteration);

	if (!CopyFileA(CLIENT_MODULE_SOURCE_PATH CLIENT_MODULE_FILENAME ".dll", FinalClientModuleName.c_str(), FALSE))
	{
		std::cerr << "ERROR: Failed to copy up to date client module from Dependencies folder. Make sure the client library has been built.\n"
			<< "Searched path = " << CLIENT_MODULE_SOURCE_PATH CLIENT_MODULE_FILENAME ".dll" << "\n";
	}
	else
	{
		// Copy .pdb symbols.
		if (!CopyFileA(CLIENT_MODULE_SOURCE_PATH CLIENT_MODULE_FILENAME ".pdb", FinalClientSymbolsName.c_str(), FALSE))
		{
			std::cerr << "WARNING: Failed to copy up to date client module debug symbols from Dependencies folder. Make sure the client library symbols have been produced.\n"
				<< "Searched path = " << CLIENT_MODULE_SOURCE_PATH  CLIENT_MODULE_FILENAME ".pdb" << "\n";
		}
		else
		{
			ClientLibHotReloadIteration++;
		}
	}

	HMODULE clientModule = LoadLibraryA(FinalClientModuleName.c_str());
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

/*
	Clears all hotreloaded instances of the Client .dll and .pdb files.
*/
void ClearHotreloadedCopies()
{
	for (int iteration = 1; iteration <= ClientLibHotReloadIteration; iteration++)
	{
		// Remove hotreloaded copies, keeping only the original one around.
		std::string HotreloadClientModuleName;
		std::string HotreloadClientSymbolsName;
		GetModuleAndSymbolsName(HotreloadClientModuleName, HotreloadClientSymbolsName, iteration);

		DeleteFileA(HotreloadClientModuleName.c_str());
		DeleteFileA(HotreloadClientSymbolsName.c_str());
	}
}

void UnloadClientModule(HMODULE ClientModule)
{
	FreeLibrary(ClientModule);
	ClientModule = 0;

	if (ClientLibHotReloadIteration > 0)
	{
		ClearHotreloadedCopies();
	}
}