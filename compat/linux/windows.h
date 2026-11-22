// =============================================================================
// Win32 API compatibility layer for building X68Sound / MXDRVg / mxwav on Linux
// =============================================================================
// Scope: OFFLINE rendering only (X68Sound_StartPcm / X68Sound_GetPcm).
//   - Real-time playback (waveOut, multimedia timer, threads) is NOT provided:
//     those functions fail cleanly, so X68Sound_Start() returns an error.
//   - Environment variables are mapped to getenv(), so X68SOUND_* settings work.
// Used by: mxwav (Linux build) and tests/ (integration / unit tests).
// =============================================================================
#ifndef X68_COMPAT_LINUX_WINDOWS_H
#define X68_COMPAT_LINUX_WINDOWS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>

#define WINAPI
#define CALLBACK
#define __stdcall
#define __cdecl
#define __declspec(x)

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef int64_t LONGLONG;
typedef unsigned int UINT;
typedef uintptr_t DWORD_PTR;
typedef uintptr_t UINT_PTR;
typedef void *LPVOID;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *HINSTANCE;
typedef void *HWAVEOUT;
typedef char TCHAR;
typedef char *LPSTR;
typedef char *LPTSTR;
typedef const char *LPCSTR;
typedef const char *LPCTSTR;
typedef DWORD *LPDWORD;
typedef long HRESULT;
typedef UINT MMRESULT;
typedef struct { unsigned char b[16]; } GUID, IID;
typedef const IID &REFIID;
typedef void *FARPROC;

#define TRUE 1
#define FALSE 0
#define MAX_PATH 4096
#define S_OK 0
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)

#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define DLL_THREAD_ATTACH 2
#define DLL_THREAD_DETACH 3

#define WM_USER 0x0400
#define WOM_DONE 0x3BD
#define WAVE_MAPPER ((UINT)-1)
#define WAVE_FORMAT_PCM 1
#define CALLBACK_FUNCTION 0x00030000
#define MMSYSERR_NOERROR 0
#define TIME_PERIODIC 1
#define TIME_CALLBACK_FUNCTION 0
#define THREAD_PRIORITY_HIGHEST 2
#define GMEM_MOVEABLE 2
#define GMEM_SHARE 0x2000
#define INFINITE 0xFFFFFFFF
#define PM_REMOVE 1

typedef struct { WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec; WORD nBlockAlign, wBitsPerSample, cbSize; } WAVEFORMATEX;
typedef struct wavehdr_tag { LPSTR lpData; DWORD dwBufferLength, dwBytesRecorded; DWORD_PTR dwUser; DWORD dwFlags, dwLoops; struct wavehdr_tag *lpNext; DWORD_PTR reserved; } WAVEHDR, *LPWAVEHDR;
typedef struct { void *hwnd; UINT message; uintptr_t wParam; intptr_t lParam; DWORD time; } MSG;
typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);
typedef void (CALLBACK *LPTIMECALLBACK)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

#ifdef __cplusplus
struct IUnknown {
	virtual HRESULT QueryInterface(REFIID, void **) = 0;
	virtual unsigned long AddRef() = 0;
	virtual unsigned long Release() = 0;
	virtual ~IUnknown() {}
};
#endif

// ---- Environment -------------------------------------------------------------
static inline DWORD GetEnvironmentVariableA(const char *name, char *buf, DWORD size) {
	const char *v = getenv(name);
	if (!v) return 0;
	DWORD len = (DWORD)strlen(v);
	if (len + 1 > size) return len + 1;
	memcpy(buf, v, len + 1);
	return len;
}
static inline DWORD GetModuleFileNameA(HMODULE, char *buf, DWORD size) {
	ssize_t n = readlink("/proc/self/exe", buf, size ? size - 1 : 0);
	if (n < 0) { if (size) buf[0] = 0; return 0; }
	buf[n] = 0;
	return (DWORD)n;
}
static inline void OutputDebugStringA(const char *) {}
static inline void Sleep(DWORD ms) { usleep((useconds_t)ms * 1000); }

// ---- Interlocked -------------------------------------------------------------
static inline long _InterlockedCompareExchange(volatile long *d, long x, long c) { return __sync_val_compare_and_swap(d, c, x); }
static inline long InterlockedCompareExchange(volatile long *d, long x, long c) { return __sync_val_compare_and_swap(d, c, x); }
static inline long InterlockedIncrement(volatile long *d) { return __sync_add_and_fetch(d, 1); }
static inline long InterlockedDecrement(volatile long *d) { return __sync_sub_and_fetch(d, 1); }
static inline long _InterlockedIncrement(volatile long *d) { return __sync_add_and_fetch(d, 1); }
static inline long _InterlockedDecrement(volatile long *d) { return __sync_sub_and_fetch(d, 1); }

// ---- Critical section (recursive mutex, like Win32) ---------------------------
typedef pthread_mutex_t CRITICAL_SECTION;
static inline void InitializeCriticalSection(CRITICAL_SECTION *cs) {
	pthread_mutexattr_t a; pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(cs, &a); pthread_mutexattr_destroy(&a);
}
static inline void DeleteCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_destroy(cs); }
static inline void EnterCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_lock(cs); }
static inline void LeaveCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_unlock(cs); }

// ---- Threads / messages / timers: not supported (offline rendering only) -----
static inline HANDLE CreateThread(void *, size_t, LPTHREAD_START_ROUTINE, LPVOID, DWORD, DWORD *) { return NULL; }
static inline BOOL SetThreadPriority(HANDLE, int) { return TRUE; }
static inline DWORD WaitForSingleObject(HANDLE, DWORD) { return 0; }
static inline BOOL CloseHandle(HANDLE) { return TRUE; }
static inline BOOL PostThreadMessage(DWORD, UINT, uintptr_t, intptr_t) { return TRUE; }
static inline BOOL GetMessage(MSG *, void *, UINT, UINT) { return FALSE; }
static inline MMRESULT timeBeginPeriod(UINT) { return 0; }
static inline MMRESULT timeEndPeriod(UINT) { return 0; }
static inline MMRESULT timeSetEvent(UINT, UINT, LPTIMECALLBACK, DWORD_PTR, UINT) { return 0; }
static inline MMRESULT timeKillEvent(UINT) { return 0; }
static inline DWORD timeGetTime(void) { return 0; }

// waveOut: always fails -> X68Sound_Start() reports X68SNDERR_PCMOUT
static inline MMRESULT waveOutOpen(HWAVEOUT *, UINT, WAVEFORMATEX *, DWORD_PTR, DWORD_PTR, DWORD) { return 1; }
static inline MMRESULT waveOutClose(HWAVEOUT) { return 0; }
static inline MMRESULT waveOutReset(HWAVEOUT) { return 0; }
static inline MMRESULT waveOutPrepareHeader(HWAVEOUT, LPWAVEHDR, UINT) { return 0; }
static inline MMRESULT waveOutUnprepareHeader(HWAVEOUT, LPWAVEHDR, UINT) { return 0; }
static inline MMRESULT waveOutWrite(HWAVEOUT, LPWAVEHDR, UINT) { return 0; }

// ---- Memory ------------------------------------------------------------------
#define GlobalAllocPtr(flags, size) calloc(1, (size))
#define GlobalFreePtr(p) (free((void *)(p)), 1)
#define ZeroMemory(p, n) memset((void *)(p), 0, (n))
#define CopyMemory(d, s, n) memcpy((void *)(d), (const void *)(s), (n))

// ---- DLL loading: not used (everything is linked statically) -----------------
static inline HMODULE LoadLibrary(const char *) { return NULL; }
static inline HMODULE LoadLibraryA(const char *) { return NULL; }
static inline FARPROC GetProcAddress(HMODULE, const char *) { return NULL; }
static inline BOOL FreeLibrary(HMODULE) { return TRUE; }

// ---- Strings -----------------------------------------------------------------
#define lstrcpy(d, s) strcpy((d), (s))
#define lstrcat(d, s) strcat((d), (s))
#define lstrlen(s) ((int)strlen(s))
#define wsprintf sprintf

// ---- Compiler intrinsics -----------------------------------------------------
static inline unsigned short _byteswap_ushort(unsigned short v) { return (unsigned short)((v >> 8) | (v << 8)); }
static inline unsigned long _byteswap_ulong(unsigned long v) { return (unsigned long)__builtin_bswap32((uint32_t)v); }

#endif
