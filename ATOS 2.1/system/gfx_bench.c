/* GFXBENCH.APP - Phase 1 graphics performance probe.
 *
 * Drives the public SYSTEM.GFX request path (IPC -> broker -> kgfx_* compositor
 * primitives).  Each primitive runs for a fixed budget of 50 Hz timer ticks
 * (0.4 s of guest wall time) and reports how many complete requests fit, so the
 * numbers include the broker dispatch cost a desktop window manager will pay.
 *
 * Timing uses the ABI 1.1 hi-res clock (syscall 67: free-running MFP Timer C,
 * 12288 counts/s) sampled once per request.  The old 50 Hz ticks() counter was
 * both a rescheduling yield and too coarse to attribute work inside a frame.
 */
#include "gui_app.h"

#define TARGET_COUNTS 6144u     /* 0.5 s per pass at the 12288 Hz Timer C rate */
#define PASSES 3u               /* keep the pass with the most iterations */

static gui_message_t reply;
static int32_t broker;
static char digits[12];

static void print(const char *s){gui_call(1,0,0,(void *)s,0);}
static void print_dec(uint32_t n){number(digits,n);print(digits);}
/* Syscall 67 (ABI 1.1) returns a monotonic MFP Timer C count: 12288 counts
   per second, ~81 us resolution.  Unlike ticks() (kernel_user_yield, ~2 ms) it
   does not reschedule, so it can be sampled once per request. */
static uint32_t clock_hires(void){return hires();}

/* One GFX request: send the 4-long command, then wait for the broker reply.
   Returns the status/payload word (reply.v[1]) or -1 on protocol failure. */
static int32_t req(uint32_t op,uint32_t xy,uint32_t wh,uint32_t color)
{
    uint32_t v[4];
    v[0]=op;v[1]=xy;v[2]=wh;v[3]=color;
    if(gui_call(36,(uint32_t)broker,0,v,0))return -1;
    if(gui_call(38,0,0,&reply,0))return -1;
    if(reply.sender!=(uint32_t)broker||reply.v[0]!=(op|0x80000000u))return -1;
    return (int32_t)reply.v[1];
}

static void emit(const char *name,uint32_t iterations,uint32_t elapsed)
{
    print(name);print(" ITER=");print_dec(iterations);
    print(" CNT=");print_dec(elapsed);print("\n");
}

/* Fixed-budget timed loop: one request per iteration, polling every iteration
   so the polled tick counter cannot lose a 20 ms wrap. */
static void run(const char *name,uint32_t op,uint32_t xy,uint32_t wh,uint32_t color)
{
    uint32_t best_n=0,best_t=0,p;
    for(p=0;p<PASSES;p++){
        uint32_t t0=clock_hires(),now=t0,n=0;
        while((now-t0)<TARGET_COUNTS){req(op,xy,wh,color);n++;now=clock_hires();}
        if(n>best_n){best_n=n;best_t=now-t0;}   /* least interference */
    }
    emit(name,best_n,best_t);
}

/* Baseline: the syscall timer poll on its own, no graphics work. */
static void run_poll(void)
{
    uint32_t best_n=0,best_t=0,p;
    for(p=0;p<PASSES;p++){
        uint32_t t0=clock_hires(),now=t0,n=0;
        while((now-t0)<TARGET_COUNTS){now=clock_hires();n++;}
        if(n>best_n){best_n=n;best_t=now-t0;}
    }
    emit("POLL    ",best_n,best_t);
}

int app_main(void)
{
    uint32_t iv;
    broker=lookup("SYSTEM.GFX");
    if(broker<0){print("GFXBENCH: NO SYSTEM.GFX\n");return 1;}
    if(req(1u,0,0,0)<0){print("GFXBENCH: GFX PATH FAILED\n");return 2;}

    iv=reply.v[1];
    print("GFXBENCH W=");print_dec(iv>>16);
    print(" H=");print_dec(iv&0xffffu);
    print(" PLANES=");print_dec(reply.v[2]>>16);
    print(" MODE=");print_dec(reply.v[2]&0xffffu);
    print(" LINE=");print_dec(reply.v[3]&0xffffu);
    print(" CAPS=");print_dec(reply.v[3]>>16);
    print("\n");

    run_poll();
    run("CALL    ",5u,0x00000000u,0,1u);                  /* pixel(0,0): dispatch overhead */
    run("PIXEL   ",5u,0x012c00c8u,0,1u);                  /* pixel(300,200) */
    run("HLINE   ",7u,0x000000c8u,0x027f00c8u,1u);        /* line(0,200)-(639,200) */
    run("VLINE   ",7u,0x01400000u,0x0140018fu,1u);        /* line(320,0)-(320,399) */
    run("FILL32  ",3u,0x00640064u,0x00200020u,1u);        /* fill(100,100,32,32) */
    run("FILL128 ",3u,0x00400040u,0x00800080u,1u);        /* fill(64,64,128,128) */
    run("FRAME   ",4u,0x00320032u,0x00c80064u,1u);        /* frame(50,50,200,100) */
    run("TEXT1   ",14u,0x00080008u,0x00000041u,0x00010001u); /* glyph A at (8,8) */
    run("COPYMONO",12u,0x00000000u,0x01400040u,0x01400000u); /* copy 320x64 -> +320 (reverse) */
    run("COPYSHFT",12u,0x00000064u,0x012c0040u,0x000700c8u); /* copy 300x64 -> +7,+200 (shifted) */
    run("CMONO1  ",12u,0x00000000u,0x01400001u,0x01400000u); /* aligned 320x1 (reverse) */
    run("CMONO16 ",12u,0x00000000u,0x01400010u,0x01400000u); /* aligned 320x16 (reverse) */
    run("CMONOB  ",12u,0x01400000u,0x01400040u,0x00000000u); /* aligned 320x64 forward */
    run("CSHFT1  ",12u,0x00000064u,0x012c0001u,0x000700c8u); /* shifted 300x1 */
    run("FILLHALF",3u,0x00000000u,0x028000c8u,1u);        /* fill(0,0,640,200) */
    run("CLEAR   ",2u,0,0,0u);                            /* clear screen */

    print("GFXBENCH DONE\n");
    return 0;
}
