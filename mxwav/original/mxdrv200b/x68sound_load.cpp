#undef _WIN32_IE
#define _WIN32_IE 0x370
#include	<windows.h>

#include	"x68sound.h"

  
int (*X68Sound_Start)(int, int, int, int, int, int, double);
int (*X68Sound_Samprate)(int);
void (*X68Sound_Reset)();
void (*X68Sound_Free)();
void (*X68Sound_BetwInt)(void (CALLBACK *)());

int (*X68Sound_StartPcm)(int, int, int, int);
int (*X68Sound_GetPcm)(void *, int);

unsigned char (*X68Sound_OpmPeek)();
void (*X68Sound_OpmReg)(unsigned char);
void (*X68Sound_OpmPoke)(unsigned char);
void (*X68Sound_OpmInt)(void (CALLBACK *)());
int (*X68Sound_OpmWait)(int);
int (*X68Sound_OpmClock)(int);

unsigned char (*X68Sound_AdpcmPeek)();
void (*X68Sound_AdpcmPoke)(unsigned char);
unsigned char (*X68Sound_PpiPeek)();
void (*X68Sound_PpiPoke)(unsigned char);
void (*X68Sound_PpiCtrl)(unsigned char);
unsigned char (*X68Sound_DmaPeek)(unsigned char);
void (*X68Sound_DmaPoke)(unsigned char, unsigned char);
void (*X68Sound_DmaInt)(void (CALLBACK *)());
void (*X68Sound_DmaErrInt)(void (CALLBACK *)());
void (*X68Sound_MemReadFunc)(int (CALLBACK *)(unsigned char *));

void (*X68Sound_WaveFunc)(int (CALLBACK *)());

int (*X68Sound_Pcm8_Out)(int, void *, int, int);
int (*X68Sound_Pcm8_Aot)(int, void *, int, int);
int (*X68Sound_Pcm8_Lot)(int, void *, int);
int (*X68Sound_Pcm8_SetMode)(int, int);
int (*X68Sound_Pcm8_GetRest)(int);
int (*X68Sound_Pcm8_GetMode)(int);
int (*X68Sound_Pcm8_Abort)();

int (*X68Sound_TotalVolume)(int v);


int (*X68Sound_ErrorCode)();
int (*X68Sound_DebugValue)();

int X68Sound_Load() {
	static HINSTANCE hDLL;

	if ( hDLL ) {
		FreeLibrary( hDLL );
	}
	if ((hDLL=LoadLibrary("X68Sound.dll")) == NULL) {
		return X68SNDERR_DLL;
	}

	X68Sound_Start = (int (__cdecl *)(int, int, int, int, int, int, double))GetProcAddress(hDLL, "X68Sound_Start");
	X68Sound_Samprate = (int (__cdecl *)(int))GetProcAddress(hDLL, "X68Sound_Samprate");
	X68Sound_Reset = (void (__cdecl *)())GetProcAddress(hDLL, "X68Sound_Reset");
	X68Sound_Free = (void (__cdecl *)())GetProcAddress(hDLL, "X68Sound_Free");
	X68Sound_BetwInt = (void (__cdecl *)(void (CALLBACK *)()))GetProcAddress(hDLL, "X68Sound_BetwInt");

	X68Sound_StartPcm = (int (__cdecl *)(int, int, int, int))GetProcAddress(hDLL, "X68Sound_StartPcm");
	X68Sound_GetPcm = (int (__cdecl *)(void *, int))GetProcAddress(hDLL, "X68Sound_GetPcm");

	X68Sound_OpmPeek = (unsigned char (__cdecl *)())GetProcAddress(hDLL, "X68Sound_OpmPeek");
	X68Sound_OpmReg = (void (__cdecl *)(unsigned char))GetProcAddress(hDLL, "X68Sound_OpmReg");
	X68Sound_OpmPoke = (void (__cdecl *)(unsigned char))GetProcAddress(hDLL, "X68Sound_OpmPoke");
	X68Sound_OpmInt = (void (__cdecl *)(void (CALLBACK *)()))GetProcAddress(hDLL, "X68Sound_OpmInt");
	X68Sound_OpmWait = (int (__cdecl *)(int))GetProcAddress(hDLL, "X68Sound_OpmWait");
	X68Sound_OpmClock = (int (__cdecl *)(int))GetProcAddress(hDLL, "X68Sound_OpmClock");

	X68Sound_AdpcmPeek = (unsigned char (__cdecl *)())GetProcAddress(hDLL, "X68Sound_AdpcmPeek");
	X68Sound_AdpcmPoke = (void (__cdecl *)(unsigned char))GetProcAddress(hDLL, "X68Sound_AdpcmPoke");
	X68Sound_PpiPeek = (unsigned char (__cdecl *)())GetProcAddress(hDLL, "X68Sound_PpiPeek");
	X68Sound_PpiPoke = (void (__cdecl *)(unsigned char))GetProcAddress(hDLL, "X68Sound_PpiPoke");
	X68Sound_PpiCtrl = (void (__cdecl *)(unsigned char))GetProcAddress(hDLL, "X68Sound_PpiCtrl");
	X68Sound_DmaPeek = (unsigned char (__cdecl *)(unsigned char))GetProcAddress(hDLL, "X68Sound_DmaPeek");
	X68Sound_DmaPoke = (void (__cdecl *)(unsigned char, unsigned char))GetProcAddress(hDLL, "X68Sound_DmaPoke");
	X68Sound_DmaInt = (void (__cdecl *)(void (CALLBACK *)()))GetProcAddress(hDLL, "X68Sound_DmaInt");
	X68Sound_DmaErrInt = (void (__cdecl *)(void (CALLBACK *)()))GetProcAddress(hDLL, "X68Sound_DmaErrInt");
	X68Sound_MemReadFunc = (void (__cdecl *)(int (CALLBACK *)(unsigned char *)))GetProcAddress(hDLL, "X68Sound_MemReadFunc");

	X68Sound_WaveFunc = (void (__cdecl *)(int (CALLBACK *)()))GetProcAddress(hDLL, "X68Sound_WaveFunc");

	X68Sound_Pcm8_Out = (int (__cdecl *)(int, void *, int, int))GetProcAddress(hDLL, "X68Sound_Pcm8_Out");
	X68Sound_Pcm8_Aot = (int (__cdecl *)(int, void *, int, int))GetProcAddress(hDLL, "X68Sound_Pcm8_Aot");
	X68Sound_Pcm8_Lot = (int (__cdecl *)(int, void *, int))GetProcAddress(hDLL, "X68Sound_Pcm8_Lot");
	X68Sound_Pcm8_SetMode = (int (__cdecl *)(int, int))GetProcAddress(hDLL, "X68Sound_Pcm8_SetMode");
	X68Sound_Pcm8_GetRest = (int (__cdecl *)(int))GetProcAddress(hDLL, "X68Sound_Pcm8_GetRest");
	X68Sound_Pcm8_GetMode = (int (__cdecl *)(int))GetProcAddress(hDLL, "X68Sound_Pcm8_GetMode");
	X68Sound_Pcm8_Abort = (int (__cdecl *)())GetProcAddress(hDLL, "X68Sound_Pcm8_Abort");
	
	X68Sound_TotalVolume = (int (__cdecl *)(int))GetProcAddress(hDLL, "X68Sound_TotalVolume");

	X68Sound_ErrorCode = (int (__cdecl *)())GetProcAddress(hDLL, "X68Sound_ErrorCode");
	X68Sound_DebugValue = (int (__cdecl *)())GetProcAddress(hDLL, "X68Sound_DebugValue");
	
	if (
		   X68Sound_Start == NULL
		|| X68Sound_Samprate == NULL
		|| X68Sound_Reset == NULL
		|| X68Sound_Free == NULL
		|| X68Sound_BetwInt == NULL

		|| X68Sound_StartPcm == NULL
		|| X68Sound_GetPcm == NULL

		|| X68Sound_OpmPeek == NULL
		|| X68Sound_OpmReg == NULL
		|| X68Sound_OpmPoke == NULL
		|| X68Sound_OpmInt == NULL
		|| X68Sound_OpmWait == NULL
		|| X68Sound_OpmClock == NULL

		|| X68Sound_AdpcmPeek == NULL
		|| X68Sound_AdpcmPoke == NULL
		|| X68Sound_PpiPeek == NULL
		|| X68Sound_PpiPoke == NULL
		|| X68Sound_PpiCtrl == NULL
		|| X68Sound_DmaPeek == NULL
		|| X68Sound_DmaPoke == NULL
		|| X68Sound_DmaInt == NULL
		|| X68Sound_DmaErrInt == NULL
		|| X68Sound_MemReadFunc == NULL

		|| X68Sound_Pcm8_Out == NULL
		|| X68Sound_Pcm8_Aot == NULL
		|| X68Sound_Pcm8_Lot == NULL
		|| X68Sound_Pcm8_SetMode == NULL
		|| X68Sound_Pcm8_GetRest == NULL
		|| X68Sound_Pcm8_GetMode == NULL
		|| X68Sound_Pcm8_Abort == NULL

		|| X68Sound_TotalVolume == NULL

		|| X68Sound_ErrorCode == NULL
		|| X68Sound_DebugValue == NULL
		) {
		FreeLibrary(hDLL);
		return X68SNDERR_FUNC;
	}
	return 0;
}
