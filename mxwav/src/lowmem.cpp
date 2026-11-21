// Low (32-bit addressable) memory allocation - see lowmem.h
#include "lowmem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if UINTPTR_MAX <= 0xFFFFFFFFu
// ---- 32-bit build: every address is 32-bit -----------------------------------
void *LowAlloc(size_t size) { return calloc(1, size ? size : 1); }
void LowFree(void *p) { free(p); }

#elif defined(_WIN32)
// ---- Windows x64 --------------------------------------------------------------
#include <windows.h>
void *LowAlloc(size_t size) {
	SYSTEM_INFO si; GetSystemInfo(&si);
	size_t gran = si.dwAllocationGranularity;
	for (uintptr_t a = 0x10000000u; a + size < 0x80000000u; a += gran * 16) {
		void *p = VirtualAlloc((void *)a, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (p) return p;
	}
	return NULL;
}
void LowFree(void *p) { if (p) VirtualFree(p, 0, MEM_RELEASE); }

#else
// ---- Linux / Unix 64-bit --------------------------------------------------------
#include <sys/mman.h>
// Header in front of each block to remember the mapping size
struct LowHdr { size_t size; size_t pad; };
void *LowAlloc(size_t size) {
	size_t total = size + sizeof(LowHdr);
#ifdef MAP_32BIT
	void *p = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
#else
	void *p = MAP_FAILED;
	for (uintptr_t a = 0x10000000u; p == MAP_FAILED && a < 0xF0000000u; a += 0x1000000u) {
		p = mmap((void *)a, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (p != MAP_FAILED && ((uintptr_t)p + total) > 0xFFFFFFFFu) { munmap(p, total); p = MAP_FAILED; }
	}
#endif
	if (p == MAP_FAILED) return NULL;
	LowHdr *h = (LowHdr *)p;
	h->size = total;
	return h + 1;   // mmap memory is already zero-filled
}
void LowFree(void *p) {
	if (!p) return;
	LowHdr *h = (LowHdr *)p - 1;
	munmap(h, h->size);
}
#endif

void LowMem_PointerOverflow(const void *p) {
	fprintf(stderr, "internal error: address %p is not 32-bit addressable (MXDRVg requires low memory)\n", p);
	abort();
}
