@echo off
REM devenv launch debugger
REM devenv /NoSplash /Run msvc/HandmadeHero.sln

REM MSVC DEBUG BUILD
MSBuild msvc/HandmadeHero.sln -t:Rebuild -p:Configuration=Debug -nologo -maxCpuCount -verbosity:quiet

REM MSVC RELEASE BUILD
REM MSBuild msvc/HandmadeHero.sln -p:Configuration=Release -nologo -maxCpuCount -verbosity:quiet

