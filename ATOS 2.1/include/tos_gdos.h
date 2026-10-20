#ifndef TOS_GDOS_H
#define TOS_GDOS_H
#include <stdint.h>

/* Minimal GDOS layer: ASSIGN.SYS, the bitmap .FNT header and the font list the
   VDI reports through vst_load_fonts (119) / vqt_name (130) / vqt_fontinfo
   (131).  A full GDOS (outline fonts, metafiles, printer drivers) is not
   implemented; this is the part an application probes before it draws. */

#define TOS_GDOS_MAX_FONTS 64
#define TOS_GDOS_NAME_LEN  32

typedef struct {
    char     name[TOS_GDOS_NAME_LEN + 1];  /* font name ("Dutch") */
    char     file[16];                     /* source file, 8.3 */
    uint16_t point;                        /* point size from the header */
    uint16_t dev;                          /* ASSIGN.SYS device it belongs to */
    uint16_t font_id;                      /* Fonthead.font_id */
    uint16_t first_ade,last_ade;           /* character range */
    uint16_t top,ascent,descent,bottom;    /* vertical metrics */
    uint16_t max_char_width,max_cell_width;
    uint16_t form_width,form_height;       /* raster geometry */
    uint16_t flags;                        /* F_STDFORM etc. */
    uint8_t  big_endian;                   /* byte order the header validated in */
    uint8_t  bitmap;                       /* 1 = bitmap .FNT, 0 = outline/unknown */
    uint8_t  loaded;                       /* 1 only after a validated header read */
} tos_gdos_font_t;

typedef struct {
    tos_gdos_font_t font[TOS_GDOS_MAX_FONTS];
    uint16_t count;
    uint16_t screen_dev;                   /* device id of the screen entry */
    uint16_t printer_dev;
    char     path[64];                     /* "path = ..." from ASSIGN.SYS */
    uint8_t  parsed;
    uint8_t  state;                        /* 0 = not tried, 1 = ready, 2 = failed */
    uint16_t loaded_count;                 /* fonts whose header actually loaded */
} tos_gdos_t;

/* Pure parsers - no I/O, so they are unit-tested on the host with the real
   files from the LDW Power disk. */
int tos_gdos_assign_parse(const char *text, unsigned len, tos_gdos_t *out);
int tos_gdos_fnt_probe(const uint8_t *data, unsigned len,
                       char *name, unsigned name_len, uint16_t *point, uint8_t *bitmap);
/* Full Fonthead probe (EmuTOS include/fonthdr.h).  Byte order is decided by
   validating the fields, not assumed: these printer fonts are little-endian
   while standard GDOS screen fonts are Motorola/big-endian. */
int tos_gdos_fnt_header(const uint8_t *data, unsigned len, tos_gdos_font_t *out);
/* Fonts for one ASSIGN.SYS device (1..4 screen, 21..30 printer, 31+ metafile).
   The screen normally has NO extra fonts - only the built-in one. */
uint16_t tos_gdos_device_count(uint16_t dev);
uint16_t tos_gdos_loaded_count(uint16_t dev);

/* Boot-time loading: read ASSIGN.SYS from the boot volume and note the fonts it
   lists.  Safe to call more than once; does nothing when already loaded. */
void tos_gdos_boot_load(void);

uint16_t tos_gdos_font_count(void);
const tos_gdos_font_t *tos_gdos_font(unsigned index);
/* Font selected by vst_font (a GDOS font id).  0 = the system font. */
void tos_gdos_select(uint16_t id);
uint16_t tos_gdos_selected(void);

#endif
