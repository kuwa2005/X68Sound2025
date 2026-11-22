#!/bin/bash
# =============================================================================
# mxwav build script (same as Makefile, for environments without make)
#   ./build.sh            Linux native            -> build/linux/mxwav
#   ./build.sh windows    Windows x86 (MinGW)     -> build/win32/mxwav.exe
#   ./build.sh windows64  Windows x64 (MinGW)     -> build/win64/mxwav.exe
#   ./build.sh windows-dll    Windows x86 DLL版   -> build/win32-dll/{mxwav.exe,X68Sound.dll}
#   ./build.sh windows64-dll  Windows x64 DLL版   -> build/win64-dll/{mxwav64.exe,X68Sound.dll}
#   ./build.sh linux-dll      Linux DLL版(テスト用) -> build/linux-dll/{mxwav_dll,libX68Sound.so}
#   ./build.sh all        all of the above
# Visual Studio: build_vc2022.bat
# =============================================================================
set -e
cd "$(dirname "$0")"

ROOT=..
X68SRC=$ROOT/X68Sound/X68Sound.cpp
SRC="src/mxwav.cpp src/lowmem.cpp src/mxdrv/mxdrv.cpp src/mxdrv/sound_iocs.cpp"
COMMON="-std=c++14 -O2 -fno-operator-names -w"

build_linux() {
	mkdir -p build/linux
	# -no-pie: MXDRVg static work areas must be in the low 4GB (see src/lowmem.h)
	${CXX:-g++} $COMMON -I$ROOT/compat/linux -fno-pie -c $X68SRC -o build/linux/X68Sound.o
	# fully static: the single "mxwav" file runs on any x86_64 Linux (no shared libraries)
	${CXX:-g++} $COMMON -I$ROOT/compat/linux -fno-pie $SRC build/linux/X68Sound.o \
		-o build/linux/mxwav -static -no-pie -pthread
	strip build/linux/mxwav 2>/dev/null || true
	echo "built: build/linux/mxwav"
}

build_win32() {
	mkdir -p build/win32
	i686-w64-mingw32-g++ $COMMON -static $SRC $X68SRC -o build/win32/mxwav.exe -lwinmm
	echo "built: build/win32/mxwav.exe"
}

build_win64() {
	mkdir -p build/win64
	# MXDRVg handles 32-bit addresses: load the image low and disable ASLR;
	# heap buffers come from LowAlloc() (VirtualAlloc below 2GB)
	x86_64-w64-mingw32-g++ $COMMON -static $SRC $X68SRC -o build/win64/mxwav.exe -lwinmm \
		-Wl,--image-base,0x10000000 -Wl,--disable-dynamicbase -Wl,--disable-high-entropy-va
	echo "built: build/win64/mxwav.exe"
}

# ---- DLL versions: mxwav loads X68Sound2025's X68Sound.dll at run time ------
DLLSRC="src/mxwav.cpp src/lowmem.cpp src/mxdrv/mxdrv.cpp src/mxdrv/sound_iocs.cpp src/x68sound_dll.cpp"

build_win32_dll() {
	mkdir -p build/win32-dll
	i686-w64-mingw32-g++ $COMMON -shared -static $X68SRC $ROOT/X68Sound/x68sound.def \
		-o build/win32-dll/X68Sound.dll -lwinmm -luser32
	i686-w64-mingw32-g++ $COMMON -static $DLLSRC -o build/win32-dll/mxwav.exe
	echo "built: build/win32-dll/mxwav.exe + X68Sound.dll"
}

build_win64_dll() {
	mkdir -p build/win64-dll
	x86_64-w64-mingw32-g++ $COMMON -shared -static $X68SRC $ROOT/X68Sound/x68sound.def \
		-o build/win64-dll/X68Sound.dll -lwinmm -luser32
	x86_64-w64-mingw32-g++ $COMMON -static $DLLSRC -o build/win64-dll/mxwav64.exe \
		-Wl,--image-base,0x10000000 -Wl,--disable-dynamicbase -Wl,--disable-high-entropy-va
	echo "built: build/win64-dll/mxwav64.exe + X68Sound.dll"
}

build_linux_dll() {   # for testing the DLL code path on Linux
	mkdir -p build/linux-dll
	${CXX:-g++} $COMMON -I$ROOT/compat/linux -fPIC -shared $X68SRC -o build/linux-dll/libX68Sound.so -lpthread
	${CXX:-g++} $COMMON -I$ROOT/compat/linux -fno-pie $DLLSRC -o build/linux-dll/mxwav_dll -no-pie -ldl -lpthread
	echo "built: build/linux-dll/mxwav_dll + libX68Sound.so"
}

case "${1:-linux}" in
	linux) build_linux ;;
	windows) build_win32 ;;
	windows64) build_win64 ;;
	windows-dll) build_win32_dll ;;
	windows64-dll) build_win64_dll ;;
	linux-dll) build_linux_dll ;;
	all) build_linux; build_win32; build_win64; build_win32_dll; build_win64_dll ;;
	*) echo "usage: $0 [linux|windows|windows64|windows-dll|windows64-dll|linux-dll|all]"; exit 1 ;;
esac
