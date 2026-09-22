#ifndef ASF_LANG_H
#define ASF_LANG_H
#include <stdint.h>
/* ATOS language core (v10).  Simple, freestanding, host-testable.
 * One X-macro keeps enum, keys and the built-in English defaults in sync.
 * Language files are plain text (docs/LANGUAGE.md):
 *     @code=PL
 *     BIOS_TITLE=ATARI BIOS PL
 * Missing keys fall back to the built-in English default. */
#define LANG_MAX_LEN 48
#define LANG_TABLE(X) \
    X(LANG_ID_BIOS_TITLE,  "BIOS_TITLE",  "ATARI BIOS 0.03") \
    X(LANG_ID_MACHINE,     "MACHINE",     "MACHINE") \
    X(LANG_ID_CPU,         "CPU",         "CPU") \
    X(LANG_ID_CLOCK,       "CLOCK",       "CLOCK") \
    X(LANG_ID_CACHE,       "CACHE",       "CACHE") \
    X(LANG_ID_SHIFTER,     "SHIFTER",     "SHIFTER") \
    X(LANG_ID_RAM,         "RAM",         "RAM") \
    X(LANG_ID_ALTRAM,      "ALT_RAM",     "ALT-RAM") \
    X(LANG_ID_BLITTER,     "BLITTER",     "BLITTER") \
    X(LANG_ID_IDE,         "IDE",         "IDE") \
    X(LANG_ID_ACSI,        "ACSI",        "ACSI") \
    X(LANG_ID_STORAGE,     "STORAGE",     "STORAGE") \
    X(LANG_ID_BOOT_DEVICE, "BOOT_DEVICE", "BOOT DEVICE") \
    X(LANG_ID_UNKNOWN,     "UNKNOWN",     "UNKNOWN") \
    X(LANG_ID_NOT_PRESENT, "NOT_PRESENT", "NOT PRESENT") \
    X(LANG_ID_PRESENT,     "PRESENT",     "PRESENT") \
    X(LANG_ID_NONE,        "NONE",        "NONE")
typedef enum {
#define LANG_ENUM(id,key,en) id,
    LANG_TABLE(LANG_ENUM)
#undef LANG_ENUM
    LANG_ID_COUNT
} lang_id_t;
void lang_init(void);                              /* load built-in English */
int  lang_set(lang_id_t id,const char *text);      /* override one string */
const char *lang_str(lang_id_t id);                /* never NULL */
const char *lang_key(lang_id_t id);
int  lang_id_for_key(const char *key);             /* -1 when unknown */
int  lang_parse(const char *data,uint32_t length,int *applied);
void lang_set_code(const char *code);
const char *lang_code(void);
int  lang_is_overridden(lang_id_t id);
#endif
