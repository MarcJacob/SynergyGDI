@ECHO OFF

:: Launches the app while first running a setup progran which sets up environment variables to speed up the hotreload script.

set SETUP_PROGRAM="C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"

cd ..\..\Build\
call %SETUP_PROGRAM% && Synergy.exe