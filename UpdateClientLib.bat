@echo off
set ClientLibPath=..\Synergy\Build\x64\Debug\

xcopy %ClientLibPath%SynergyClientLib.dll %__CD__%Build\ /E /Y
xcopy %ClientLibPath%\ClientIncludes\ %__CD__%Build\ClientIncludes\ /E /Y