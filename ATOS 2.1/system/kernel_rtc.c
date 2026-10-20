/* Kernel RTC read for the desktop clock (ATOS 2.0).
 *
 * ST/STe/Mega talk to the keyboard-processor clock: send $1C and read the six
 * BCD bytes back (seconds, minutes, hours, day, month, year).  Mega ST/Mega STe
 * additionally expose an RP5C15 at $fffc21, used as a second chance.  When no
 * clock answers the caller keeps the 12:00 AM software default, so the applet
 * runs on every machine.
 *
 * The IKBD read masks interrupts and polls the ACIA directly: the level-6 ACIA
 * handler would otherwise steal the reply bytes into the raw input ring, where
 * they could not be told apart from mouse packets.
 */
#include <stdint.h>
#include "hardware.h"
#include "rtc.h"
#include "system.h"   /* MACHINE_* classification */

extern unsigned kernel_irq_mask(void);
extern void kernel_irq_restore(unsigned sr);
static uint8_t bcd2bin(uint8_t v){return (uint8_t)((v>>4)*10u+(v&15u));}
static int rtc_valid(const rtc_datetime_t *d)
{
    return d->month>=1&&d->month<=12&&d->day>=1&&d->day<=31&&
           d->hour<24&&d->minute<60&&d->second<60;
}

/* --- ST/STe keyboard-processor clock ($1C) ------------------------------- */
static int read_ikbd(rtc_datetime_t *d)
{
    uint8_t raw[6];unsigned i;uint32_t guard;unsigned sr;int ok=1;
    sr=kernel_irq_mask();
    for(i=0;i<16u&&(REG8(IKBD_ACIA_CTRL)&1u);++i)(void)REG8(IKBD_ACIA_DATA);
    for(guard=0;guard<20000u&&!(REG8(IKBD_ACIA_CTRL)&2u);++guard){}
    if(!(REG8(IKBD_ACIA_CTRL)&2u))ok=0;
    else{
        REG8(IKBD_ACIA_DATA)=0x1cu;
        for(i=0;i<6u&&ok;++i){
            /* The IKBD answers at 7812.5 baud: ~1.3 ms per byte, so a ~30 ms
               guard per byte is generous and still bounded. */
            for(guard=0;guard<100000u&&!(REG8(IKBD_ACIA_CTRL)&1u);++guard){}
            if(!(REG8(IKBD_ACIA_CTRL)&1u))ok=0;else raw[i]=REG8(IKBD_ACIA_DATA);
        }
    }
    kernel_irq_restore(sr);
    if(!ok)return 0;
    d->second=bcd2bin(raw[0]);d->minute=bcd2bin(raw[1]);d->hour=bcd2bin(raw[2]);
    d->day=bcd2bin(raw[3]);d->month=bcd2bin(raw[4]);
    {uint16_t y=bcd2bin(raw[5]);d->year=(uint16_t)(y<70u?2000u+y:1900u+y);}
    return rtc_valid(d);
}

/* --- Mega ST / Mega STe RP5C15 ($fffc21) --------------------------------- */
static uint8_t mega_reg(uint8_t index)
{return (uint8_t)(REG8(MEGA_RTC_BASE+1u+(uint32_t)index*2u)&15u);}
static int read_mega(rtc_datetime_t *d)
{
    uint8_t mode;uint32_t guard;
    for(guard=0;guard<64u;++guard){}
    mode=REG8(MEGA_RTC_BASE+0x1bu);
    REG8(MEGA_RTC_BASE+0x1bu)=(uint8_t)(mode&~1u);
    d->second=(uint8_t)(mega_reg(0)+10u*mega_reg(1));
    d->minute=(uint8_t)(mega_reg(2)+10u*mega_reg(3));
    d->hour=(uint8_t)(mega_reg(4)+10u*mega_reg(5));
    d->day=(uint8_t)(mega_reg(7)+10u*mega_reg(8));
    d->month=(uint8_t)(mega_reg(9)+10u*mega_reg(10));
    d->year=(uint16_t)(1980u+mega_reg(11)+10u*mega_reg(12));
    return rtc_valid(d);
}

/* --- Mega STE / TT / Falcon: battery-backed NVRAM (MC146818) ------------
   The NVRAM page is unmapped on plain ST/Mega ST, so this is only attempted on
   machines that actually have the chip. */
extern uint8_t kernel_machine_type(void);
static int has_nvram(void)
{
    uint8_t m=kernel_machine_type();
    return m==MACHINE_MEGA_STE||m==MACHINE_TT||m==MACHINE_FALCON;
}
static uint8_t nv_get(uint8_t index){REG8(NVRAM_SELECT)=index;return REG8(NVRAM_DATA);}
static int read_nvram(rtc_datetime_t *d)
{
    uint8_t b,binary,y;
    b=nv_get(11);
    binary=(uint8_t)(b&4u);
    REG8(NVRAM_SELECT)=11;REG8(NVRAM_DATA)=(uint8_t)(b|0x80u);   /* stop updates */
    d->second=nv_get(0);d->minute=nv_get(2);d->hour=(uint8_t)(nv_get(4)&0x3fu);
    d->day=nv_get(7);d->month=nv_get(8);y=nv_get(9);
    REG8(NVRAM_SELECT)=11;REG8(NVRAM_DATA)=b;                     /* restore */
    if(!binary){
        d->second=bcd2bin(d->second);d->minute=bcd2bin(d->minute);
        d->hour=bcd2bin(d->hour);d->day=bcd2bin(d->day);
        d->month=bcd2bin(d->month);y=bcd2bin(y);
    }
    d->year=(uint16_t)(y<70u?2000u+y:1900u+y);
    return rtc_valid(d);
}

int kernel_rtc_read(rtc_datetime_t *d)
{
    if(!d)return 0;
    if(read_ikbd(d))return 1;      /* ST/STe: keyboard-processor clock   */
    if(read_mega(d))return 1;      /* Mega ST/Mega STE: RP5C15           */
    if(has_nvram()&&read_nvram(d))return 1;   /* Mega STE/TT/Falcon: NVRAM */
    return 0;
}
