/* ATEST.APP — test-only native audio owner with a SCHEDULED STOP in the future.
 *
 * Why it exists: the audit (R13 point D) wants the native STOP verified while a
 * TOS session runs.  Arkanoid's queue drains within a frame, so it cannot be
 * caught by sampling.  This app claims the chip, keeps channel A audible
 * (volume 12) and re-arms a STOP ~1.5 s ahead every ~3 s, so a TOS session that
 * lands in between has a native event that is genuinely pending and must still
 * fire on time - with the owner unchanged and the TOS refused.
 */
#include "app_abi.h"
#include "gui_app.h"

static void wait_ticks(uint32_t t)
{
    uint32_t t0=(uint32_t)gui_call(67,0,0,0,0);
    while(((uint32_t)gui_call(67,0,0,0,0)-t0)<t)(void)gui_call(11,1,0,0,0);
}

int app_main(void)
{
    for(;;){
        atos_audio_event_t ev;
        /* audible channel A: period 0x100, volume 12 (claims for our pid) */
        (void)gui_call(ATOS_CALL_AUDIO_TONE,(0x100u<<16)|(0x0Cu<<8)|0u,0,0,0);
        /* STOP due ~1.5 s from now */
        ev.wait=24576u;ev.aux=0u;ev.op=ATOS_AUDIO_OP_STOP;ev.a=7u;ev.b=0u;
        ev.pad[0]=ev.pad[1]=ev.pad[2]=0u;
        (void)gui_call(ATOS_CALL_AUDIO_QUEUE,1u,0,&ev,0);
        wait_ticks(12288u*5u/2u);
    }
    return 0;
}
