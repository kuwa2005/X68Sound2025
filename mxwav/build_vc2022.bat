@echo off
rem ===========================================================================
rem mxwav build with Visual Studio 2022
rem   build_vc2022.bat        -> build\vc_x86\mxwav.exe  (32-bit, recommended)
rem   build_vc2022.bat x64    -> build\vc_x64\mxwav.exe  (64-bit)
rem Sources are UTF-8 (/utf-8).
rem ===========================================================================
setlocal
set ARCH=%1
if "%ARCH%"=="" set ARCH=x86

if "%ARCH%"=="x64" (
	call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
	set OUT=build\vc_x64
	rem MXDRVg handles 32-bit addresses: keep the whole process below 2GB
	set LINKOPT=/LARGEADDRESSAWARE:NO /DYNAMICBASE:NO /BASE:0x10000000
) else (
	call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64_x86
	set OUT=build\vc_x86
	set LINKOPT=
)
if not exist %OUT% mkdir %OUT%

cl.exe /nologo /MT /EHsc /O2 /utf-8 /W1 /Fo%OUT%\ /Fe%OUT%\mxwav.exe ^
	src\mxwav.cpp src\lowmem.cpp src\mxdrv\mxdrv.cpp src\mxdrv\sound_iocs.cpp ..\X68Sound\X68Sound.cpp ^
	/link %LINKOPT% winmm.lib user32.lib
if %errorlevel% neq 0 goto :fail

echo built: %OUT%\mxwav.exe
exit /b 0

:fail
echo BUILD FAILED
pause
exit /b 1
