# =============================================================================
# mxwav (MXDRVg + X68Sound2025) build
#   make                 Linux native (64-bit OK)        -> build/linux/mxwav
#   make windows         Windows x86 via MinGW cross     -> build/win32/mxwav.exe
#   make windows64       Windows x64 via MinGW cross     -> build/win64/mxwav.exe
#   make test            Linux build + tests
# For Visual Studio see build_vc2022.bat
# =============================================================================

ROOT    := ..
X68DIR  := $(ROOT)/X68Sound
SRC     := src/mxwav.cpp src/lowmem.cpp src/mxdrv/mxdrv.cpp src/mxdrv/sound_iocs.cpp
X68SRC  := $(X68DIR)/X68Sound.cpp

COMMON  := -std=c++14 -O2 -fno-operator-names -Wall -Wno-unused -Wno-unused-parameter \
           -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare \
           -Wno-misleading-indentation -Wno-parentheses -Wno-unknown-pragmas \
           -Wno-conversion-null -Wno-unused-label -Wno-unused-function \
           -Wno-write-strings -Wno-format -Wno-narrowing -Wno-dangling-else \
           -Wno-strict-aliasing -Wno-class-memaccess -Wno-array-bounds

# ---- Linux -------------------------------------------------------------------
CXX      ?= g++
LIN_OUT  := build/linux
# -no-pie: static data (MXDRVg work areas) must be in the low 4GB (see lowmem.h)
LIN_FLAGS := $(COMMON) -I$(ROOT)/compat/linux -fno-pie
LIN_LDFLAGS := -no-pie -lpthread

$(LIN_OUT)/mxwav: $(SRC) $(X68SRC) $(wildcard src/*.h src/mxdrv/*.h $(X68DIR)/*.h)
	@mkdir -p $(LIN_OUT)
	$(CXX) $(LIN_FLAGS) -c $(X68SRC) -o $(LIN_OUT)/X68Sound.o
	$(CXX) $(LIN_FLAGS) $(SRC) $(LIN_OUT)/X68Sound.o -o $@ $(LIN_LDFLAGS)

# ---- Windows (MinGW cross compile) -------------------------------------------
WIN_FLAGS := $(COMMON) -static -Wno-attributes
WIN_LIBS  := -lwinmm

build/win32/mxwav.exe: $(SRC) $(X68SRC)
	@mkdir -p build/win32
	i686-w64-mingw32-g++ $(WIN_FLAGS) $(SRC) $(X68SRC) -o $@ $(WIN_LIBS)

# x64: MXDRVg handles 32-bit addresses -> low image base, no ASLR (heap: LowAlloc)
build/win64/mxwav.exe: $(SRC) $(X68SRC)
	@mkdir -p build/win64
	x86_64-w64-mingw32-g++ $(WIN_FLAGS) $(SRC) $(X68SRC) -o $@ $(WIN_LIBS) -Wl,--image-base,0x10000000 -Wl,--disable-dynamicbase -Wl,--disable-high-entropy-va

.PHONY: all windows windows64 test clean
all: $(LIN_OUT)/mxwav
windows: build/win32/mxwav.exe
windows64: build/win64/mxwav.exe
test: $(LIN_OUT)/mxwav
	./tests/run_mxwav_tests.sh
clean:
	rm -rf build
