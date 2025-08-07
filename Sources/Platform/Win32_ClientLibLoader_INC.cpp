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

// Base name for the Client dynamic library file. The actual file will possibly have a suffix with its version and build time identification.
#define CLIENT_MODULE_FILENAME_BASE "SynergyClientLib"

/*
	Module identifier for the currently loaded Client library module, if any.
*/
HMODULE ClientLibModule = NULL;

#if HOTRELOAD_SUPPORTED

// NOTE(MJ) This whole hotreload system is a little garbage because it's so compiler-specific and goes around the entire build system,
// but I just couldn't find a convenient way to have CMake do something that would work with hotreloading under the constraint that .pdb
// files don't get unloaded when their associated library does...
//
// So for now let's consider hot reloading a very particular feature that has to be setup locally. As long as I'm working alone on this using
// the MSVC compiler I'm fine, but the second this changes we'll need to move the entire hotreload system configuration to a file or something.

// Script ran on each hot reload compile, triggering a simplified build pipeline on client code that has to output .dll and .pdb files
// compatible with hotreloading (IE different name per iteration).
#define CLIENT_MODULE_HOTRELOAD_COMPILE_SCRIPT "..\\Scripts\\Win32Dev\\CompileClientForHotreload.bat"

// Folder where new versions of the client library can be retrieved and hotreloaded as the program is running.
#define CLIENT_MODULE_SOURCE_PATH "Dependencies\\Synergy\\SynergyClientLib\\"

/*
	Iteration number of the client library and debug symbols being used. Gets increment on each hotreload that involves a successful copy of
	the dll. When equal to 0, the base library is used and is NOT retrieved from the source path.
*/
uint8_t ClientLibHotReloadIteration = 0;

// Context properties of the Hot Reloadsystem.
struct Win32HotreloadSystemContext
{
	/* 
		Full filename of the hotreloaded library as it was copied into working directory.
		Used to clean it up on program shutdown or hotreloading.
	*/
	std::string LibFilename = "";
	
	/*
		Full filename of the hotreloaded library symbols as they were copied into working directory.
		Used to clean it up on program shutdown or hotreloading.
	*/
	std::string SymbolsFilename = "";

	/*
		Whether the currently loaded library is a Base library or Hotreloaded.Hotreloaded libraries are dynamically copied from source
		and need to be cleaned up.
	*/
	bool bIsHotreloaded = false;

	/*
		Windows File Write Timestamp of the last Client library file that was loaded, for automatically detecting new versions.
	*/
	FILETIME LastLoadedClientLibraryFileWriteTime = {};

	/*
		Whether the Hot Reload Compile Setup script has been ran on not.
	*/
	bool bCompileSetupScriptRan = false;
};

static Win32HotreloadSystemContext Win32HotreloadContext;

#endif

void Win32_LoadClientModule(SynergyClientAPI& APIStruct, std::string LibNameOverride)
{
	APIStruct = {};

	std::string LibName = CLIENT_MODULE_FILENAME_BASE;
	if (!LibNameOverride.empty())
	{
		LibName = LibNameOverride;
	}

	std::string LibFilename = LibName + ".dll";

	ClientLibModule = LoadLibraryA(LibFilename.c_str());
	if (ClientLibModule == nullptr)
	{
		std::cerr << "Error: Couldn't load Client Library. Make sure \"" << LibFilename << "\" exists in working directory.\n";
		return;
	}

	// Load Client API functions.
	APIStruct = {};

	APIStruct.Hello = decltype(APIStruct.Hello)(GetProcAddress(ClientLibModule, "Hello"));
	if (APIStruct.Hello == nullptr)
	{
		std::cerr << "Error: Missing symbol \"Hello\" in Client library.\n";
	}

	APIStruct.StartClient = decltype(APIStruct.StartClient)(GetProcAddress(ClientLibModule, "StartClient"));
	if (APIStruct.StartClient == nullptr)
	{
		std::cerr << "Error: Missing symbol \"CreateClientContext\" in Client library.\n";
	}

	APIStruct.RunClientFrame = decltype(APIStruct.RunClientFrame)(GetProcAddress(ClientLibModule, "RunClientFrame"));
	if (APIStruct.RunClientFrame == nullptr)
	{
		std::cerr << "Error: Missing symbol \"RunClientFrame \" in Client library.\n";
	}

	APIStruct.ShutdownClient = decltype(APIStruct.ShutdownClient)(GetProcAddress(ClientLibModule, "ShutdownClient"));
	if (APIStruct.ShutdownClient == nullptr)
	{
		std::cerr << "Error: Missing symbol \"ShutdownClient \" in Client library.\n";
	}

	if (APIStruct.APISuccessfullyLoaded())
	{
		std::cout << "Successfully loaded client library from '" << LibFilename << "'.\n";
	}
}

void Win32_UnloadClientModule(SynergyClientAPI& API)
{
	FreeLibrary(ClientLibModule);
	ClientLibModule = NULL;

	/*
		Assign "stub" lambdas to all API functions so they do not crash the program if called mistakenly.
		This can happen specifically during forced platform shutdown happening on a different thread.
	*/

	API.Hello = []() {};
	API.RunClientFrame = [](ClientContext& Context, ClientFrameData& FrameData) {};
	API.StartClient = [](ClientContext& Context) {};
	API.ShutdownClient = [](ClientContext& Context) {};
}

#if HOTRELOAD_SUPPORTED

void Win32_CleanupHotreloadFiles()
{
	DeleteFileA((Win32HotreloadContext.LibFilename).c_str());
	DeleteFileA((Win32HotreloadContext.SymbolsFilename).c_str());
}

void HotreloadClientModule(SynergyClientAPI& API, std::string candidateName)
{
	std::cout << "Hotreloading Synergy Client Module.\n";

	// Strip any file extension from candidate name.
	size_t dotPos = candidateName.find(".", 0);
	if (dotPos != std::string::npos)
	{
		candidateName.erase(dotPos);
	}

	std::string candidateLibFileName = candidateName + ".dll";
	std::string candidateSymbolsFilename = candidateName + ".pdb";

	std::string candidateLibFilePath = CLIENT_MODULE_SOURCE_PATH + candidateLibFileName;
	std::string candidateSymbolsFilePath = CLIENT_MODULE_SOURCE_PATH + candidateSymbolsFilename;

	bool bCopyFailed = false;

	// Copy .dll
	if (!CopyFileA(candidateLibFilePath.c_str(), candidateLibFileName.c_str(), FALSE))
	{
		if (GetLastError() != ERROR_SHARING_VIOLATION)
		{
			std::cerr << "ERROR: Failed to copy client module from Dependencies folder. Make sure the client library has been built.\n"
				<< "Searched path = " << candidateLibFilePath << "\nError Code = " << GetLastError() << "\n";
		}
		
		bCopyFailed = true;
	}
	// Copy .pdb symbols.
	else if (!CopyFileA(candidateSymbolsFilePath.c_str(), candidateSymbolsFilename.c_str(), FALSE))
	{
		if (GetLastError() != ERROR_SHARING_VIOLATION)
		{
			std::cerr << "WARNING: Failed to find client module debug symbols from Dependencies folder. Make sure the client library symbols have been produced.\n"
				<< "Searched path = " << candidateSymbolsFilePath << "\nError Code = " << GetLastError() << "\n";
		}
		bCopyFailed = true;
	}

	if (bCopyFailed)
	{
		return;
	}

	// Unload client module, which will either unload the base library if this is the first hotreload or unload and delete the previous hotreload iteration.
	Win32_UnloadClientModule(API);

	// If we were using a hotreloading iteration, delete it.
	if (Win32HotreloadContext.bIsHotreloaded)
	{
		Win32_CleanupHotreloadFiles();
	}
	

	// Load client module with the new file names.
	Win32_LoadClientModule(API, candidateName);

	if (API.APISuccessfullyLoaded())
	{
		std::cout << "Synergy Client Module hot-reloaded successfully.\n";
		
		// Cache the last write time of the file for automated change detection and filename.
		WIN32_FIND_DATAA fileFindData;
		FindFirstFileA(candidateLibFileName.c_str(), &fileFindData);

		Win32HotreloadContext.LibFilename = candidateLibFileName;
		Win32HotreloadContext.LastLoadedClientLibraryFileWriteTime = fileFindData.ftLastWriteTime;

		// Cache symbols file name.
		Win32HotreloadContext.SymbolsFilename = candidateSymbolsFilename;
		Win32HotreloadContext.bIsHotreloaded = true;
	}
}

void RunHotreloadCompileProgram()
{
	// Run Hotreload script if one is defined.
#ifdef CLIENT_MODULE_HOTRELOAD_COMPILE_SCRIPT
	SHELLEXECUTEINFOA execInfo = {};
	execInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	execInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
	execInfo.lpVerb = "open";
	execInfo.lpFile = CLIENT_MODULE_HOTRELOAD_COMPILE_SCRIPT;
	execInfo.lpParameters = "..\\";

	if (ShellExecuteExA(&execInfo) && execInfo.hProcess != 0)
	{
		std::cout << "Running Client Hotreload Recompile script...\n";
		WaitForSingleObject(execInfo.hProcess, INFINITE);
		std::cout << "Done.\n";
	}
#endif
}

bool Win32_TryHotreloadClientModule(SynergyClientAPI& API, bool bForce)
{
	// Look for a hotreload candidate file at the source path.
	WIN32_FIND_DATAA sourceFileFindData;
	if (FindFirstFileA(CLIENT_MODULE_SOURCE_PATH CLIENT_MODULE_FILENAME_BASE "*.dll", &sourceFileFindData) == INVALID_HANDLE_VALUE)
	{
		// No valid lib file was found at source path.
		return false;
	}

	uint64_t currentTime = *(uint64_t*)(&Win32HotreloadContext.LastLoadedClientLibraryFileWriteTime);
	uint64_t fileTime = *(uint64_t*)(&sourceFileFindData.ftLastWriteTime);

	if (!bForce 
		&& (!CompareFileTime(&Win32HotreloadContext.LastLoadedClientLibraryFileWriteTime, &sourceFileFindData.ftLastWriteTime) 
		|| Win32HotreloadContext.LibFilename.compare(sourceFileFindData.cFileName) == 0))
	{
		return false;
	}

	// We've found a candidate for hotreload !
	HotreloadClientModule(API, sourceFileFindData.cFileName);

	return API.APISuccessfullyLoaded();
}
#endif