SOURCE_INC_FILE()

// Synergy Client Module & API Loading implementation. The symbols are referenced and used in Win32_Main.cpp.

#include "SynergyClientAPI.h"
#include "Platform/Win32_Platform.h"

#include <iostream>
#include <shellapi.h>

#include <string>

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

	ClientLibModule = LoadLibraryA(LibName.c_str());
	if (ClientLibModule == nullptr)
	{
		std::cerr << "Error: Couldn't load Client Library. Make sure \"" << LibName << "\" exists in working directory.\n";
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
		std::cout << "Successfully loaded client library from '" << LibName << "'.\n";
		APIStruct.Hello();
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
	API.RunClientFrame = [](ClientSessionData& Context, ClientFrameRequestData& FrameData) {};
	API.StartClient = [](ClientSessionData& Context) {};
	API.ShutdownClient = [](ClientSessionData& Context) {};
}

#if HOTRELOAD_SUPPORTED

void Win32_CleanupHotreloadFiles()
{
	DeleteFileA((Win32HotreloadContext.LibFilename).c_str());
	DeleteFileA((Win32HotreloadContext.SymbolsFilename).c_str());
}

void HotreloadClientModule(SynergyClientAPI& API, std::string sourceLibFilePath)
{
	std::cout << "Hotreloading Synergy Client Module.\n";

	// Retrieve file name from path.
	size_t lastFolderSeparatorIndex = sourceLibFilePath.find_last_of('\\');

	std::string sourceFileName;
	std::string sourceFolder;
	if (lastFolderSeparatorIndex != std::string::npos)
	{
		sourceFileName = sourceLibFilePath.substr(lastFolderSeparatorIndex + 1);
		sourceFolder = sourceLibFilePath;
		sourceFolder.erase(lastFolderSeparatorIndex + 1);
	}
	else
	{
		std::cerr << "Failed to hotreload client module from file '" << sourceLibFilePath << "' !\n";
		return;
	}

	// Strip any file extension from candidate name.
	size_t dotPos = sourceFileName.find(".", 0);
	if (dotPos != std::string::npos)
	{
		sourceFileName.erase(dotPos);
	}

	// Determine "candidate" file names and paths which will be the targets of copying and loading.
	std::string candidateLibFileName = sourceFileName + ".dll";
	std::string candidateSymbolsFilename = sourceFileName + ".pdb";

	std::string candidateLibFilePath = WIN32_TEMP_DATA_FOLDER + candidateLibFileName;
	std::string candidateSymbolsFilePath = WIN32_TEMP_DATA_FOLDER + candidateSymbolsFilename;

	// Assume that a .pdb file with the same file name as the source file will be found in the same folder.
	std::string sourceSymbolsFilePath = sourceFolder + candidateSymbolsFilename;

	bool bCopyFailed = false;

	// Copy .dll
	if (!CopyFileA(sourceLibFilePath.c_str(), candidateLibFilePath.c_str(), FALSE))
	{
		if (GetLastError() != ERROR_SHARING_VIOLATION)
		{
			std::cerr << "ERROR: Failed to copy client module from Dependencies folder. Make sure the client library has been built.\n"
				<< "Searched path = " << sourceLibFilePath << "\nError Code = " << GetLastError() << "\n";
		}
		
		bCopyFailed = true;
	}
	// Copy .pdb symbols.
	else if (!CopyFileA(sourceSymbolsFilePath.c_str(), candidateSymbolsFilePath.c_str(), FALSE))
	{
		if (GetLastError() != ERROR_SHARING_VIOLATION)
		{
			std::cerr << "WARNING: Failed to find client module debug symbols from Dependencies folder. Make sure the client library symbols have been produced.\n"
				<< "Searched path = " << sourceSymbolsFilePath << "\nError Code = " << GetLastError() << "\n";
		}
		// bCopyFailed = true; (Copying the symbols over successfully is not a critical necessity, uncomment this line if this changes).
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
	Win32_LoadClientModule(API, candidateLibFilePath);

	if (API.APISuccessfullyLoaded())
	{
		std::cout << "Synergy Client Module hot-reloaded successfully.\n";
		
		// Cache the last write time of the file for automated change detection and filename.
		WIN32_FIND_DATAA fileFindData;
		FindFirstFileA(candidateLibFilePath.c_str(), &fileFindData);

		Win32HotreloadContext.LibFilename = candidateLibFilePath;
		Win32HotreloadContext.LastLoadedClientLibraryFileWriteTime = fileFindData.ftLastWriteTime;

		// Cache symbols file name.
		Win32HotreloadContext.SymbolsFilename = candidateSymbolsFilePath;
		Win32HotreloadContext.bIsHotreloaded = true;
	}
	else
	{
		std::cout << "Synergy Client Module hotreload was unsuccessful. Unloading...\nProvide a new library file at the source folder or restart the app.\n";
		Win32_UnloadClientModule(API);
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

	// Check that the hotreload is forced or that the file found is more recent than the one currently loaded.
	if (!bForce 
		&& (!CompareFileTime(&Win32HotreloadContext.LastLoadedClientLibraryFileWriteTime, &sourceFileFindData.ftLastWriteTime) 
		|| Win32HotreloadContext.LibFilename.compare(sourceFileFindData.cFileName) == 0))
	{
		return false;
	}

	std::string sourceFilePath = CLIENT_MODULE_SOURCE_PATH;
	sourceFilePath += sourceFileFindData.cFileName;

	// Check that it is possible to open the file.
	HANDLE createTestHandle = CreateFileA(sourceFilePath.c_str(), GENERIC_READ, NULL, NULL, OPEN_EXISTING, NULL, NULL);
	if (createTestHandle == INVALID_HANDLE_VALUE)
	{
		// File is locked, probably already loaded by something else or still under construction.
		return false;
	}
	CloseHandle(createTestHandle);

	// We've found a candidate for hotreload !
	HotreloadClientModule(API, sourceFilePath);

	return API.APISuccessfullyLoaded();
}
#endif