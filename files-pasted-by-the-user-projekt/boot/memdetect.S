/*
 * Early ST/STE MMU bank detection.  This is an original implementation of
 * the wraparound method documented in EmuTOS doc/memdetect.txt.  It uses no
 * persistent stack state across the final MEMCONF change. Its local calls use
 * the low 32 KiB, and entry/return uses A6.
 *
 * Output: D5 = contiguous ST-RAM size, D6.b = final MEMCONF value.
 */
    .section .text.startup,"ax"
    .globl ram_detect_early
ram_detect_early:
    move.b  #0x0a,0xffff8001     /* expose two provisional 2 MiB banks */

    /* Seed both banks with a long pattern. Missing RAM is very unlikely to
     * reproduce all 252 bytes, while smaller banks mirror it at known offsets. */
    lea     8,%a0
    lea     0x200008,%a1
    moveq   #0,%d0
1:  move.w  %d0,(%a0)+
    move.w  %d0,(%a1)+
    addi.w  #0xfa54,%d0
    cmpa.l  #0x200,%a0
    bne.s   1b

    move.l  #0x200000,%d1       /* start with bank 1 */
    moveq   #0,%d5              /* accumulated byte count */
    moveq   #0,%d6              /* final MEMCONF */

bank_start:
    lsr.w   #2,%d6              /* move previous bank code into low field */
    move.l  #0x20000,%d4        /* assume 128 KiB */

    move.l  #0x40008,%a0        /* STe 128 KiB mirror */
    bsr     pattern_at
    beq.s   bank_done
    move.l  #0x208,%a0          /* ST 128 KiB mirror */
    bsr     pattern_at
    beq.s   bank_done

    move.l  #0x80000,%d4        /* assume 512 KiB */
    move.l  #0x80008,%a0        /* STe 512 KiB mirror */
    bsr     pattern_at
    beq.s   bank_512
    move.l  #0x408,%a0          /* ST 512 KiB mirror */
    bsr     pattern_at
    beq.s   bank_512

    moveq   #0,%d4              /* no mirror: either 2 MiB or absent */
    move.l  #8,%a0
    bsr     pattern_at
    bne.s   bank_done           /* no base pattern => bank absent */
    move.l  #0x200000,%d4
    addq.w  #4,%d6              /* code 2 */
bank_512:
    addq.w  #4,%d6              /* code 1 (or second half of code 2) */
bank_done:
    add.l   %d4,%d5
    subi.l  #0x200000,%d1
    beq.s   bank_start          /* then test bank 0 */

    move.b  %d6,0xffff8001
    jmp     (%a6)

/* Compare seeded pattern at bank base D1 + candidate offset A0.
 * Returns Z=1 only when the whole sequence matches. Uses A1,D0. */
pattern_at:
    adda.l  %d1,%a0
    lea     0x1f8(%a0),%a1
    moveq   #0,%d0
2:  cmp.w   (%a0)+,%d0
    bne.s   3f
    addi.w  #0xfa54,%d0
    cmpa.l  %a0,%a1
    bne.s   2b
3:  rts
