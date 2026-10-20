// Freestanding C++ runtime pieces for the ATOS port (-fno-exceptions -fno-rtti
// -fno-threadsafe-statics): allocation operators and the ABI hooks g++ emits.
#include <stddef.h>
#include "stdlib.h"
#include "atos_sys.h"

void *operator new(size_t n) {
	void *p = malloc(n);
	if (!p) {
		atos_puts("FLASHBACK: out of memory (operator new)\n");
		exit(4);
	}
	return p;
}
void *operator new[](size_t n) { return operator new(n); }
void operator delete(void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }

extern "C" {
void *__dso_handle = 0;
// Static destructors never run: the process image is discarded on exit.
int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
int atexit(void (*)(void)) { return 0; }
void __cxa_pure_virtual() {
	atos_puts("FLASHBACK: pure virtual call\n");
	exit(5);
}
void __cxa_deleted_virtual() { __cxa_pure_virtual(); }
}
