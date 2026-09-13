        .text
        .globl ram_image_start
        .globl system_image_cache_sync
ram_image_start:
        move.l  4(%sp),%a0
        move.l  8(%sp),%d0
        move.l  12(%sp),%d1
        move.l  16(%sp),%d2
        move.w  #0x2700,%sr
        clr.l   0x06fc.w
        clr.l   0x0426.w
        move.l  #0x752019f3,0x0420.w
        move.b  %d1,0x0424.w
        move.l  %d0,0x042e.w
        move.l  #0x237698aa,0x043a.w
        move.l  #0x5555aaaa,0x051a.w
        clr.l   0x05a4.w
        move.l  #0x1357bd13,0x05a8.w
        cmpi.l  #2,%d2
        beq.s   1f
        cmpi.l  #3,%d2
        bne.s   2f
1:      moveq   #8,%d0
        .word   0x4e7b,0x0002
2:
        move.l  4(%a0),%a1
        jmp     (%a1)

/* Make code written by ACSI DMA visible before jumping into it. */
system_image_cache_sync:
        move.l  4(%sp),%d0
        cmpi.l  #2,%d0
        blo.s   3f
        cmpi.l  #3,%d0
        bhi.s   2f
        moveq   #8,%d0
        .word   0x4e7b,0x0002     /* movec d0,cacr: clear 020/030 I-cache */
        bra.s   3f
2:      .word   0xf4f8            /* cpusha bc: 68040/68060 */
3:      rts
