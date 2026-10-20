/* WD1772 access through the ST DMA chip.  Protocol taken from the EmuTOS
   reference (bios/fdc.h, bios/dma.h, bios/floppy.c): registers are reached by
   writing the DMA control register (0xffff8606) then the data register
   (0xffff8604); the drive/side select is on PSG port A (register 14). */
#include "hardware.h"
#include "system.h"
#include "fdc.h"
#include "psg.h"

/* Readable from the debugger/tests after a probe. */
volatile uint32_t fdc_dbg_status,fdc_dbg_sig;

#define FDC_CS   0x0080u
#define FDC_TR   0x0082u
#define FDC_SR   0x0084u
#define FDC_DR   0x0086u
#define FDC_RESTORE 0x00u
#define FDC_SEEK    0x10u
#define FDC_READ    0x80u
#define FDC_IRUPT   0xd0u
#define FDC_RATE3   0x03u
#define FDC_BUSY    0x01u
#define FDC_CRCERR  0x08u
#define FDC_RNF     0x10u
#define FDC_MOTORON 0x80u

static void fdc_delay(void){volatile unsigned i;for(i=0u;i<40u;++i)__asm__ volatile("nop");}
static void fdc_set(uint16_t reg,uint16_t value)
{REG16(DMA_CONTROL)=reg;fdc_delay();REG16(DMA_DATA)=value;}
static uint16_t fdc_get(uint16_t reg)
{REG16(DMA_CONTROL)=reg;fdc_delay();return REG16(DMA_DATA);}
static void fdc_addr(uint32_t address)
{REG8(DMA_ADDR_LOW)=(uint8_t)address;REG8(DMA_ADDR_MID)=(uint8_t)(address>>8);REG8(DMA_ADDR_HIGH)=(uint8_t)(address>>16);}
/* Drive A/B and side select live on PSG port A (register 14), low three bits.
   The access goes through hw/psg.c so that it cannot interleave with an audio
   register write: select + read-back + write are one atomic unit. */
static void fdc_select(uint8_t drive,uint8_t side)
{
    uint8_t n=0x07u;
    if(drive==0u)n=(uint8_t)(n&~2u);else n=(uint8_t)(n&~4u);
    if(side)n=(uint8_t)(n&~1u);
    psg_port_a_modify(0x07u,n);
}
/* Wait for the current command to finish.  Polling the FDC BUSY bit is more
   reliable than the MFP GPIP4 line, which can already read low for a stale
   interrupt and make the wait return before any data has been transferred. */
/* Poll the status register directly.  The old loop called fdc_get() (control
   write + 40-nop delay) 2,000,000 times, so an absent drive cost seconds of
   boot on a 68000.  A 4-nop gap is enough between register accesses. */
static uint16_t fdc_status(void)
{
    REG16(DMA_CONTROL)=FDC_CS;
    {volatile unsigned i;for(i=0u;i<4u;++i)__asm__ volatile("nop");}
    return REG16(DMA_DATA);
}
static int fdc_wait(void)
{
    uint32_t t;fdc_delay();fdc_delay();
    for(t=0u;t<200000u;++t)if(!(fdc_status()&FDC_BUSY))return 1;
    return 0;
}
static uint8_t fdc_current_track=0xffu;   /* 0xff = unknown, force a seek */

/* Plan item 4: Mega STE / TT / Falcon add the FDC clock/density register at
   0xffff860e (bit0 = clock, bit1 = density); a plain ST/STE has no such
   register, so it must not be touched.  Force 8 MHz + DD for the 720 KiB boot
   floppy, independently of the state left by a previous boot. */
static void fdc_clock_init(void)
{
    static uint8_t done;
    if(done)return;
    done=1u;
    if(g_system.machine_type==MACHINE_MEGA_STE||
       g_system.machine_type==MACHINE_TT||
       g_system.machine_type==MACHINE_FALCON)
        REG8(0xffff860eu)=0u;
}
int fdc_read_sector(uint8_t drive,uint8_t track,uint8_t side,uint8_t sector,void *buffer)
{
    uint16_t status;
    fdc_clock_init();
    (void)fdc_get(FDC_CS);   /* clear any stale FDC interrupt before the command */
    fdc_select(drive,side);
    if(track!=fdc_current_track){
        fdc_set(FDC_DR,track);
        fdc_set(FDC_CS,(uint16_t)(FDC_SEEK|FDC_RATE3));
        if(!fdc_wait()){fdc_set(FDC_CS,FDC_IRUPT);fdc_delay();return 0;}
        (void)fdc_get(FDC_CS);
        fdc_current_track=track;
    }
    fdc_set(FDC_SR,sector);
    fdc_addr((uint32_t)(uintptr_t)buffer);
    REG16(DMA_CONTROL)=0x0190u;fdc_delay();   /* clear DMA buffers ... */
    REG16(DMA_CONTROL)=0x0090u;fdc_delay();
    REG16(DMA_DATA)=1u;                       /* ... then one sector */
    fdc_set(FDC_CS,FDC_READ);
    if(!fdc_wait()){fdc_set(FDC_CS,FDC_IRUPT);fdc_delay();return 0;}
    status=fdc_get(FDC_CS);
    if(status&(FDC_RNF|FDC_CRCERR))return 0;
    return 1;
}
int fdc_read_boot_sector(uint8_t drive,void *buffer)
{
    uint8_t attempt;int ok=0;volatile uint32_t spin;
    for(attempt=0u;attempt<4u&&!ok;++attempt){
        if(fdc_read_sector(drive,0u,0u,1u,buffer))ok=1;
        else for(spin=0u;spin<5000u;++spin)__asm__ volatile("nop");    /* spin-up gap */
    }
    fdc_dbg_status=ok?1u:0u;
    if(ok){const uint8_t *b=(const uint8_t *)buffer;
        fdc_dbg_sig=((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];}
    return ok;
}
