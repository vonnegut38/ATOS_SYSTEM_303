#include "hardware.h"
#include "system.h"

static video_mode_t active_mode;

video_mode_t shifter_detect_mode(void)
{
    /* MFP GPIP bit 7 is the Atari monochrome-monitor detect input.  It is
     * active low.  A color monitor cannot distinguish low from medium, so in
     * that case retain the valid mode already selected in the Shifter. */
    if (!(REG8(MFP_GPIP) & 0x80u))
        return VIDEO_ST_MONO;
    if ((REG8(SHIFTER_RES) & 3u) == VIDEO_ST_MEDIUM)
        return VIDEO_ST_MEDIUM;
    return VIDEO_ST_MEDIUM; /* Dedicated RGB 640x200 test ROM. */
}

const char *shifter_mode_name(video_mode_t mode)
{
    if (mode == VIDEO_ST_MONO) return "ST HIGH MONO 640X400";
    if (mode == VIDEO_ST_MEDIUM) return "ST MEDIUM 640X200";
    return "ST LOW 320X200";
}

uint16_t shifter_text_columns(void)
{
    return active_mode == VIDEO_ST_LOW ? 40u : 80u;
}

uint16_t shifter_line_bytes(void)
{
    return active_mode == VIDEO_ST_MONO ? 80u : 160u;
}

uint16_t shifter_group_bytes(void)
{
    if (active_mode == VIDEO_ST_LOW) return 8u;
    if (active_mode == VIDEO_ST_MEDIUM) return 4u;
    return 2u;
}

void shifter_init(void)
{
    uint32_t i;
    volatile uint32_t *screen = (volatile uint32_t *)SCREEN_BASE;
    active_mode = shifter_detect_mode();
    g_system.video_mode = (uint8_t)active_mode;
    REG8(SHIFTER_BASE_HI) = (uint8_t)(SCREEN_BASE >> 16);
    REG8(SHIFTER_BASE_MID) = (uint8_t)(SCREEN_BASE >> 8);
    REG8(SHIFTER_BASE_LO) = 0;
    REG16(SHIFTER_PALETTE + 0) = 0x0000;
    REG16(SHIFTER_PALETTE + 2) = 0x0777;
    REG8(SHIFTER_RES) = (uint8_t)active_mode;
    for (i = 0; i < SCREEN_BYTES/4u; ++i)
        screen[i] = 0;
}
