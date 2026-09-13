    .section .module_header,"a"
    .ascii "ASYSMOD0"
    .word 3
    .word 32
    .long __image_file_end
    .long 0
    .long timer_module_entry
    .long __image_memory_end
    .long 0

    .section .text.startup,"ax"
    .globl timer_module_entry
timer_module_entry:
    move.l 4(%sp),%a0
    cmpi.l #0x4b4d4150,(%a0)
    bne timer_fail
    cmpi.w #3,4(%a0)
    bne timer_fail
    cmpi.w #40,6(%a0)
    blo timer_fail
    lea timer_api(%pc),%a1
    move.l %a0,(%a1)

    /* Timer C: stop it, load 246, then start with the /200 prescaler.
     * The low Timer-D control nibble is preserved. No interrupt is enabled. */
    moveq #0,%d0
    move.b 0xfffffa1d,%d0
    lea timer_state(%pc),%a1
    move.b %d0,1(%a1)
    move.b 0xfffffa23,2(%a1)
    andi.b #0x0f,%d0
    move.b %d0,0xfffffa1d
    move.b #246,0xfffffa23
    ori.b #0x70,%d0
    move.b %d0,0xfffffa1d
    moveq #0,%d0
    move.b 0xfffffa23,%d0
    lea timer_state(%pc),%a1
    move.b %d0,(%a1)

    lea timer_service(%pc),%a1
    move.l #0x54494d52,(%a1)
    move.w #1,4(%a1)
    move.w #32,6(%a1)
    lea timer_poll(%pc),%a0
    move.l %a0,8(%a1)
    lea timer_ticks(%pc),%a0
    move.l %a0,12(%a1)
    lea timer_switch_pending(%pc),%a0
    move.l %a0,16(%a1)
    lea timer_acknowledge(%pc),%a0
    move.l %a0,20(%a1)
    move.l #50,24(%a1)
    move.l #3,28(%a1)
    move.l %a1,-(%sp)
    move.l #1,-(%sp)
    move.l #0x54494d52,-(%sp)
    lea timer_api(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%a1
    jsr (%a1)
    lea 12(%sp),%sp
    tst.l %d0
    beq timer_restore
    rts
timer_restore:
    lea timer_state(%pc),%a0
    moveq #0,%d0
    move.b 1(%a0),%d0
    move.b %d0,%d1
    andi.b #0x0f,%d1
    move.b %d1,0xfffffa1d
    move.b 2(%a0),0xfffffa23
    move.b %d0,0xfffffa1d
    moveq #0,%d0
    rts
timer_fail:
    moveq #0,%d0
    rts

timer_poll:
    moveq #0,%d0
    move.b 0xfffffa23,%d0
    lea timer_state(%pc),%a0
    moveq #0,%d1
    move.b (%a0),%d1
    move.b %d0,(%a0)
    cmp.b %d1,%d0
    bls.s timer_no_wrap
    addq.l #1,4(%a0)
    move.b #1,8(%a0)
timer_no_wrap:
    move.l 4(%a0),%d0
    rts

timer_ticks:
    lea timer_state(%pc),%a0
    move.l 4(%a0),%d0
    rts

timer_switch_pending:
    lea timer_state(%pc),%a0
    moveq #0,%d0
    move.b 8(%a0),%d0
    rts

timer_acknowledge:
    lea timer_state(%pc),%a0
    clr.b 8(%a0)
    rts

    .section .bss
    .align 2
/* last counter, saved TCDCR/TCDR, padding, ticks.l, pending byte */
timer_api:
    .space 4
timer_state:
    .space 12
timer_service:
    .space 32
