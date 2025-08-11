@ECHO OFF

:: This script effectively bypasses the normal build system to build a new version of the client library with a name and .pdb file association that is compatible with hot reloading on editors that just don't have the technology to unload and unlock the .pdb file when their associated .dll get unloaded.

:: The script should be ran on a machine with the appropriate env variable setup or from a VS console or from Visual Studio itself.

:: Init env vars

FOR /F "tokens=*" %%g IN ('where cl') do (SET CompilerPath="%%g")
if [%CompilerPath%] == [] call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
set COMPILER=cl

:: Get Project Root from first parameter if any, otherwise set it to two folders above working directory.
set PROJECT_ROOT=%1
if [%PROJECT_ROOT%] == [] (set PROJECT_ROOT=..\..\)

:: Define Sources

set CLIENT_SOURCES_PATH=%PROJECT_ROOT%Dependencies\Synergy\SynergyClientLib\Sources\
set CORE_SOURCES_PATH=%PROJECT_ROOT%Dependencies\Synergy\SynergyCoreLib\Sources\

:: Define Includes

set CLIENT_INCLUDES_PATH=%PROJECT_ROOT%Dependencies\Synergy\SynergyClientLib\Includes\
set CORE_INCLUDES_PATH=%PROJECT_ROOT%Dependencies\Synergy\SynergyCoreLib\Includes\Public\

:: Define libraries
set CORE_LIBRARY_PATH=%PROJECT_ROOT%Build\Dependencies\Synergy\SynergyCoreLib\SynergyCoreLib.lib

:: Define Targets

set CORE_OBJ_PATH=%PROJECT_ROOT%Build\Dependencies\Synergy\SynergyCoreLib\SynergyCoreLib.o
set TARGET_PATH=%PROJECT_ROOT%Build\Dependencies\Synergy\SynergyClientLib\

set HotreloadableOutputFilename=SynergyClientLib_%time:~0,2%_%time:~3,2%_%time:~6,2%

:: Clear all output files from target folder.
del %TARGET_PATH%SynergyClientLib* /f /q

:: Run build commands for Core Library.
%COMPILER% /I%CORE_INCLUDES_PATH% /Zi /Fo:%CORE_OBJ_PATH% -c %CORE_SOURCES_PATH%SynergyCore.cpp
lib %CORE_OBJ_PATH%

:: Run final build command for Client.
%COMPILER% %CLIENT_SOURCES_PATH%SynergyClientMain.cpp /I%CLIENT_INCLUDES_PATH% /I%CLIENT_INCLUDES_PATH%\Public\ /I%CORE_INCLUDES_PATH% /Fe:%TARGET_PATH%%HotreloadableOutputFilename% %CORE_LIBRARY_PATH% /LDd /Zi