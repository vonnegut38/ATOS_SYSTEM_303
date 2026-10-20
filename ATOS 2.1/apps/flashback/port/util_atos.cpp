// debug/warning/error for the ATOS port (replaces util.cpp).
#include <stdarg.h>
#include "util.h"
#include "atos_sys.h"

uint32_t g_debugMask;

void debug(uint32_t cm, const char *msg, ...) {
	if (cm & g_debugMask) {
		char buf[256];
		va_list va;
		va_start(va, msg);
		vsnprintf(buf, sizeof(buf), msg, va);
		va_end(va);
		atos_puts(buf);
		atos_puts("\n");
	}
}

void error(const char *msg, ...) {
	char buf[256];
	va_list va;
	va_start(va, msg);
	vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);
	atos_puts("FLASHBACK ERROR: ");
	atos_puts(buf);
	atos_puts("\n");
	exit(1);
}

void warning(const char *msg, ...) {
	char buf[256];
	va_list va;
	va_start(va, msg);
	vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);
	atos_puts("FLASHBACK WARNING: ");
	atos_puts(buf);
	atos_puts("\n");
}
