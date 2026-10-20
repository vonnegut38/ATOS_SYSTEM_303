    .text
    .globl kvideo_read8,kvideo_write8,kvideo_shifter_base
/* Boot-only 68000 probes. Discard the fault frame using the saved supervisor
 * SP, restore vector and SR on both paths. Interrupts cannot nest a probe. */
kvideo_read8:
    move.l 4(%sp),%a0
    move.l 8(%sp),%a1
    movem.l %d2/%a2,-(%sp)
    move.w %sr,%d2
    ori.w #0x0700,%sr
    move.l 8,%a2
    move.l %sp,video_probe_sp
    move.l #video_bus_fault,8
    moveq #1,%d0
    move.b (%a0),(%a1)
    bra.s video_probe_done
kvideo_write8:
    move.l 4(%sp),%a0
    move.l 8(%sp),%d1
    movem.l %d2/%a2,-(%sp)
    move.w %sr,%d2
    ori.w #0x0700,%sr
    move.l 8,%a2
    move.l %sp,video_probe_sp
    move.l #video_bus_fault,8
    moveq #1,%d0
    move.b %d1,(%a0)
    bra.s video_probe_done
video_bus_fault:
    move.l video_probe_sp,%sp
    moveq #0,%d0
video_probe_done:
    move.l %a2,8
    move.w %d2,%sr
    movem.l (%sp)+,%d2/%a2
    rts
kvideo_shifter_base:
    move.l 4(%sp),%d0
    move.w %sr,%d1
    ori.w #0x0700,%sr
    lsr.l #8,%d0
    move.b %d0,0xffff8203
    lsr.l #8,%d0
    move.b %d0,0xffff8201
    move.w %d1,%sr
    rts
    .bss
    .align 2
video_probe_sp: .space 4
