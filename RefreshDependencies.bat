@echo off

:: Note: We assume that the Synergy project is located in a folder that is a "sibling" of the SynergyGDI root folder. 
:: If it isn't, adapt this variable.
set SynergyProjectPath=..\Synergy\

:: Path to Client Lib
set ClientLibPath=%SynergyProjectPath%Build\SynergyClientLib\SynergyClientLib.dll 

:: We depend on Client includes and Core includes. Cache the path to both.
set CoreIncludesPath=%SynergyProjectPath%SynergyCoreLib\Includes\Public\
set ClientIncludesPath=%SynergyProjectPath%SynergyClientLib\Includes\Public\

:: Copy dependency includes and libraries to Dependencies folder after removing all of its contents.
rmdir /Q /S %__CD__%Dependencies\

:: Client includes and shared library.
xcopy %ClientIncludesPath% %__CD__%Dependencies\SynergyClient\ /E /Y
xcopy %ClientLibPath% %__CD__%Dependencies\SynergyClient\ /E /Y

:: Core includes (Core library itself is statically linked inside Client library).
xcopy %CoreIncludesPath% %__CD__%Dependencies\SynergyCore\ /E /Y

:: Copy the Client DLL to the Build folder so the exe finds it on launch or reload.
xcopy %__CD__%Dependencies\SynergyClient\SynergyClientLib.dll %__CD__%\Build\ /E /Y