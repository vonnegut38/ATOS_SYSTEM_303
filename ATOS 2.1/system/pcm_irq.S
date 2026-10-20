/* Constant, short Timer A handler. No C dispatcher or task switch per sample.
 * Always use a private IRQ stack: TOS packers may have a tiny supervisor stack.
 * All MFP sources run at IPL6 and cannot nest; no change to VBL/Timer C/ACIA. */
    .text
    .globl kpcm_irq
kpcm_irq:
    move.l %sp,pcm_saved_sp
    lea pcm_stack_end,%sp
    movem.l %d0-%d1/%a0,-(%sp)
    move.l kpcm_clip,%d0
    beq.s 3f
    move.l %d0,%a0
    move.l kpcm_clip_pos,%d0
    adda.l %d0,%a0
    moveq #0,%d1
    move.b (%a0),%d1
    lsl.w #2,%d1
    lea kpcm_lut,%a0
    move.l (%a0,%d1.w),%d1
    addq.l #1,%d0
    cmp.l kpcm_clip_size,%d0
    blo.s 4f
    moveq #0,%d0
4:  move.l %d0,kpcm_clip_pos
    addq.l #1,kpcm_played
    move.l %d1,psg_pcm_sample
    bra.s 2f
3:  move.l kpcm_read,%d0
    cmp.l kpcm_write,%d0
    beq.s 1f
    move.l %d0,%d1
    andi.l #8191,%d1
    lsl.l #2,%d1
    lea kpcm_ring,%a0
    adda.l %d1,%a0
    move.l (%a0),%d1
    addq.l #1,%d0
    move.l %d0,kpcm_read
    addq.l #1,kpcm_played
    move.l %d1,psg_pcm_sample
    bra.s 2f
1:  addq.l #1,kpcm_underruns
    move.l psg_pcm_sample,%d1  /* hold last sample on underrun, avoid DC jump */
2:  lea 0xffff8800,%a0
    move.b #10,(%a0)
    nop
    nop
    move.b %d1,2(%a0)
    lsr.l #8,%d1
    move.b #9,(%a0)
    nop
    nop
    move.b %d1,2(%a0)
    lsr.l #8,%d1
    move.b #8,(%a0)
    nop
    nop
    move.b %d1,2(%a0)
    move.b #0xdf,0xfffffa0f /* clear only Timer A in-service (software EOI too) */
    movem.l (%sp)+,%d0-%d1/%a0
    move.l pcm_saved_sp,%sp
    rte
    .bss
    .balign 4
pcm_saved_sp: .space 4
pcm_stack: .space 256
pcm_stack_end:
