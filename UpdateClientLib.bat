@echo off

:: Note: We assume that the Synergy project is located in a folder that is a "sibling" of the SynergyGDI root folder. If it isn't, adapt this variable.
set SynergyProjectPath=..\Synergy\
set ClientLibPath=%SynergyProjectPath%Build\SynergyClientLib\
set ClientIncludesPath=%SynergyProjectPath%SynergyClientLib\Sources\Public\

xcopy %ClientLibPath%SynergyClientLib.dll %__CD__%\Build\ /E /Y
rmdir /Q /S %__CD__%Dependencies\ClientIncludes\
xcopy %ClientIncludesPath% %__CD__%Dependencies\ClientIncludes\ /E /Y