// =============================================================================
// Low (32-bit addressable) memory allocation for the MXDRVg port
// =============================================================================
// MXDRVg is a machine translation of 68000 assembly: it keeps addresses in
// 32-bit data registers (D0-D7) and hands them to the emulated DMA controller
// as 32-bit values. On 64-bit builds every buffer whose address can end up in
// a data register / DMA register must therefore live below 4GB.
//
//  - 32-bit builds        : plain malloc (all addresses are 32-bit anyway)
//  - Linux x86_64         : mmap(MAP_32BIT) (below 2GB). Static data is low
//                           because the program is linked with -no-pie.
//  - Windows x64          : VirtualAlloc with a top-down search below 2GB
//                           (link with /LARGEADDRESSAWARE:NO as well)
//
// LowAlloc() returns zero-filled memory. PtrToU32() aborts if a pointer does
// not fit in 32 bits, so a violation is detected instead of corrupting audio.
// =============================================================================
#ifndef MXWAV_LOWMEM_H
#define MXWAV_LOWMEM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *LowAlloc(size_t size);
void LowFree(void *p);

#ifdef __cplusplus
}
#endif

void LowMem_PointerOverflow(const void *p);

static inline uint32_t PtrToU32(const volatile void *p) {
	uintptr_t v = (uintptr_t)p;
	if (sizeof(uintptr_t) > 4 && (v >> 31 >> 1) != 0) LowMem_PointerOverflow((const void *)p);
	return (uint32_t)v;
}

#endif
