@echo off
REM devenv launch debugger
REM devenv /NoSplash /Run msvc/HandmadeHero.sln

REM MSVC DEBUG BUILD
REM MSBuild msvc/HandmadeHero.sln -t:Rebuild -p:Configuration=DebugPlatform -nologo -maxCpuCount -verbosity:quiet
REM MSVC RELEASE BUILD
REM MSBuild msvc/HandmadeHero.sln -t:Rebuild -p:Configuration=Release -nologo -maxCpuCount -verbosity:quiet

REM MSVC DEBUGPLATFORM BUILD
MSBuild msvc/handmade_hero.vcxproj -t:Rebuild -p:Configuration=DebugPlatform -nologo -maxCpuCount -verbosity:quiet
MSBuild msvc/win32_main.vcxproj -p:Configuration=DebugPlatform -nologo -maxCpuCount -verbosity:quiet

REM Cmake build for codeblocks:
REM cmake .. -G "CodeBlocks - NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug/
