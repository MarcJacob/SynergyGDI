SOURCE_INC_FILE()

// Name of temporary folder where data only relevant to the current program execution is stored. Gets deleted and recreated by any subsequent launches.
#define WIN32_TEMP_DATA_FOLDER "Temp"

// Symbol definitions for managing hard drive files for the Win32 platform.

bool Win32_CreateTempCopyFile(const std::string& SourcePath, const std::string& DestPath)
{
	return CopyFileA(SourcePath.c_str(), Win32_ConvertTempPathToRelativePath(DestPath).c_str(), FALSE);
}

void Win32_DeleteTempFile(const std::string& FilePath)
{
	// If provided file is not relative to temp data folder already, convert it.
	std::string relativePath;
	if (FilePath.rfind(WIN32_TEMP_DATA_FOLDER, 0) != 0)
	{
		relativePath = Win32_ConvertTempPathToRelativePath(FilePath);
	}
	else
	{
		relativePath = FilePath;
	}

	WIN32_FIND_DATAA findData;
	HANDLE file = FindFirstFileA(relativePath.c_str(), &findData);

	if (file == INVALID_HANDLE_VALUE)
	{
		return;
	}

	if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		// Delete every file inside this folder then delete the folder itself.
		HANDLE folder = file;
		std::string subFolderFilePattern = relativePath + "\\*.*";
		file = FindFirstFileA(subFolderFilePattern.c_str(), &findData);
		do
		{
			// Ignore current and parent directories
			if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
			{
				continue;
			}

			std::string deletedSubFilePath = relativePath.c_str();
			deletedSubFilePath += '\\';
			deletedSubFilePath += findData.cFileName;

			Win32_DeleteTempFile(deletedSubFilePath);

			// Set the Find Data back to search pattern for the folder.
			strcpy_s(findData.cFileName, sizeof(findData.cFileName), subFolderFilePattern.c_str());

		} while (FindNextFileA(file, &findData));
		RemoveDirectoryA(relativePath.c_str());
	}
	else
	{
		// Delete the file.
		DeleteFileA(relativePath.c_str());
	}
}

void Win32_ResetTempDataFolder()
{
	Win32_DeleteTempFile("");
	if (CreateDirectoryA(WIN32_TEMP_DATA_FOLDER, NULL))
	{
		std::cout << "Temp folder cleared and re-created at \"" << WIN32_TEMP_DATA_FOLDER << "\"\n";
	}
	else
	{
		std::cerr << "Failed to create temporary data folder at \"" << WIN32_TEMP_DATA_FOLDER << "\" !\n";
	}
}

std::string Win32_ConvertTempPathToRelativePath(const std::string& TempPath)
{
	return TempPath.empty() ? WIN32_TEMP_DATA_FOLDER : WIN32_TEMP_DATA_FOLDER "\\" + TempPath;
}