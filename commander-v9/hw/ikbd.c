#include "hardware.h"
#include "system.h"
static uint8_t mouse_state, mouse_buttons, mouse_pending;
static int8_t mouse_x, mouse_y;

void ikbd_init(void)
{
    REG8(IKBD_ACIA_CTRL) = 3;     /* master reset */
    REG8(IKBD_ACIA_CTRL) = 0x96;  /* /64 clock, 8N1, receiver IRQ enabled */
}

int ikbd_scancode(void)
{
    uint8_t data;
    if (!(REG8(IKBD_ACIA_CTRL) & 1u))
        return -1;
    data=REG8(IKBD_ACIA_DATA);
    if (mouse_state==1) { mouse_x=(int8_t)data; mouse_state=2; return -1; }
    if (mouse_state==2) { mouse_y=(int8_t)data; mouse_state=0; mouse_pending=1; return -1; }
    if (data>=0xf8u && data<=0xfbu) {
        mouse_buttons=(uint8_t)(data&3u); mouse_state=1; return -1;
    }
    /* IKBD status/reset packets are protocol bytes, not keyboard scancodes. */
    if (data>=0xf0u) return -1;
    if (data&0x80u) return 0x100 | (int)(data&0x7fu); /* explicit release */
    return data;
}

int ikbd_mouse_event(int8_t *dy, uint8_t *buttons)
{
    (void)mouse_x;
    if (!mouse_pending) return 0;
    *dy=mouse_y; *buttons=mouse_buttons; mouse_pending=0; return 1;
}
