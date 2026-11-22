// =============================================================================
// mxwav (DLL version): forwards the X68Sound_* API to an external
// X68Sound.dll (X68Sound2025) instead of linking X68Sound statically.
//   Windows : X68Sound.dll in the same folder as the exe (normal DLL search)
//   Linux   : libX68Sound.so next to the executable (used for testing)
// The DLL is loaded on the first call, i.e. after mxwav has prepared the
// environment (-A etc.), so the DLL's X68SOUND_* settings include them.
// X68Sound_InitConfig / X68Sound_GetSamprate are optional (X68Sound2025 only),
// so older X68Sound.dll builds also work (without X68SOUND_OUTPUT_RATE support).
// =============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#include "windows.h"	// compat/linux
#endif

#include "mxdrv/x68sound.h"

namespace {

#ifdef _WIN32
HMODULE g_dll = NULL;
const char *DLL_NAME = "X68Sound.dll";
void *RawSym(const char *name) { return (void *)GetProcAddress(g_dll, name); }
#else
void *g_dll = NULL;
const char *DLL_NAME = "libX68Sound.so";
void *RawSym(const char *name) { return dlsym(g_dll, name); }
#endif

void Load() {
	if (g_dll) return;
#ifdef _WIN32
	g_dll = LoadLibraryA(DLL_NAME);
#else
	char exe[4096];
	ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if (n > 0) {
		exe[n] = 0;
		std::string dir(exe);
		dir = dir.substr(0, dir.rfind('/') + 1);
		g_dll = dlopen((dir + DLL_NAME).c_str(), RTLD_NOW | RTLD_LOCAL);
	}
	if (!g_dll) g_dll = dlopen(DLL_NAME, RTLD_NOW | RTLD_LOCAL);
#endif
	if (!g_dll) {
		fprintf(stderr, "%sの読み込みに失敗しました。mxwavと同じフォルダに置いてください。\n", DLL_NAME);
		exit(EXIT_FAILURE);
	}
}

void *Sym(const char *name, bool required = true) {
	Load();
	void *p = RawSym(name);
	if (!p && required) {
		fprintf(stderr, "%sに関数%sがありません（対応していないDLLです）。\n", DLL_NAME, name);
		exit(EXIT_FAILURE);
	}
	return p;
}

}  // namespace

// Each wrapper resolves its function once and forwards the call.
#define X68_FWD(ret, name, params, args)                             \
	extern "C" ret name params {                                     \
		typedef ret (*fn_t) params;                                  \
		static fn_t fn = (fn_t)Sym(#name);                           \
		return fn args;                                              \
	}

X68_FWD(int, X68Sound_Start, (int samprate, int opmflag, int adpcmflag, int betw, int pcmbuf, int late, double rev),
        (samprate, opmflag, adpcmflag, betw, pcmbuf, late, rev))
X68_FWD(int, X68Sound_StartPcm, (int samprate, int opmflag, int adpcmflag, int pcmbuf), (samprate, opmflag, adpcmflag, pcmbuf))
X68_FWD(int, X68Sound_GetPcm, (void *buf, int len), (buf, len))
X68_FWD(void, X68Sound_Free, (), ())
X68_FWD(unsigned char, X68Sound_OpmPeek, (), ())
X68_FWD(void, X68Sound_OpmReg, (unsigned char no), (no))
X68_FWD(void, X68Sound_OpmPoke, (unsigned char data), (data))
X68_FWD(void, X68Sound_OpmInt, (void (CALLBACK *proc)()), (proc))
X68_FWD(int, X68Sound_OpmWait, (int wait), (wait))
X68_FWD(void, X68Sound_AdpcmPoke, (unsigned char data), (data))
X68_FWD(unsigned char, X68Sound_PpiPeek, (), ())
X68_FWD(void, X68Sound_PpiPoke, (unsigned char data), (data))
X68_FWD(void, X68Sound_PpiCtrl, (unsigned char data), (data))
X68_FWD(unsigned char, X68Sound_DmaPeek, (unsigned char adrs), (adrs))
X68_FWD(void, X68Sound_DmaPoke, (unsigned char adrs, unsigned char data), (adrs, data))
X68_FWD(void, X68Sound_DmaInt, (void (CALLBACK *proc)()), (proc))
X68_FWD(void, X68Sound_DmaErrInt, (void (CALLBACK *proc)()), (proc))
X68_FWD(int, X68Sound_Pcm8_Out, (int ch, void *adrs, int mode, int len), (ch, adrs, mode, len))
X68_FWD(int, X68Sound_Pcm8_Abort, (), ())
X68_FWD(int, X68Sound_TotalVolume, (int v), (v))

// Optional (X68Sound2025 only)
extern "C" void X68Sound_InitConfig() {
	typedef void (*fn_t)();
	fn_t fn = (fn_t)Sym("X68Sound_InitConfig", false);   // also loads the DLL
	if (fn) fn();                                         // re-read X68SOUND_* (DllMain already did)
}
extern "C" int X68Sound_GetSamprate() {
	typedef int (*fn_t)();
	static fn_t fn = (fn_t)Sym("X68Sound_GetSamprate", false);
	return fn ? fn() : 0;    // 0 -> mxwav uses the -S rate
}
