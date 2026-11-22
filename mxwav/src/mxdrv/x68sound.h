// =============================================================================
// X68Sound interface for MXDRVg (mxwav port)
// =============================================================================
// Original MXDRVg loaded X68Sound.dll at run time (x68sound_load.cpp) through
// function pointers. This port links X68Sound2025 statically, so the real
// function declarations from X68Sound2025 are used and X68Sound_Load() is a
// no-op.
// =============================================================================
#ifndef MXDRV_PORT_X68SOUND_H
#define MXDRV_PORT_X68SOUND_H

#include <stdint.h>
#include "../../../X68Sound/x68sound.h"

static inline int X68Sound_Load() { return 0; }

// DMAに16bit値をビッグエンディアン(68オーダー)で書き込む
#define	X68Sound_DmaPokeW(adrs,data)	(X68Sound_DmaPoke((adrs),(unsigned char)((unsigned short)(data)>>8)), \
										 X68Sound_DmaPoke((adrs)+1, (unsigned char)(unsigned short)(data)))
// DMAに32bit値をビッグエンディアン(68オーダー)で書き込む
// (アドレスを渡す場合、64bit環境では下位4GB内のメモリである必要がある: lowmem.h)
#define	X68Sound_DmaPokeL(adrs,data)	(X68Sound_DmaPoke((adrs),(unsigned char)((uint32_t)(uintptr_t)(data)>>24)), \
										 X68Sound_DmaPoke((adrs)+1, (unsigned char)((uint32_t)(uintptr_t)(data)>>16)), \
										 X68Sound_DmaPoke((adrs)+2, (unsigned char)((uint32_t)(uintptr_t)(data)>>8)), \
										 X68Sound_DmaPoke((adrs)+3, (unsigned char)((uint32_t)(uintptr_t)(data))))
#define X68Sound_y(no,data)	(X68Sound_OpmReg(no),X68Sound_OpmPoke(data))

#endif
