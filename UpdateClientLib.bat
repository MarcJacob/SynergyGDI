@echo off
set ClientLibPath=..\Synergy\Build\x64\Debug\

xcopy %ClientLibPath%SynergyClientLib.dll %__CD__%\Build\ /E /Y
rmdir /Q /S %__CD__%Dependencies\ClientIncludes\
xcopy %ClientLibPath%\ClientIncludes\ %__CD__%Dependencies\ClientIncludes\ /E /Y