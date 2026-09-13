/* SMARTDRV.PRG: enables or disables the ASF disk cache.
 *   smartdrv.prg        -> -on  (enable)
 *   smartdrv.prg -on    -> enable
 *   smartdrv.prg -off   -> disable and release the cache memory
 * A0 on entry is the basepage; p_cmdlin (length + string) lives at 0x80. */
    .section .tosprog,"a"
    .word 0x601a
    .long text_end-text_start
    .long 0
    .long 0
    .long 0
    .long 0
    .word 0
    .long 0
text_start:
    movea.l %a0,%a3              /* A3 = basepage */
    lea 0x81(%a3),%a1            /* command line string */
    moveq #0,%d0                 /* default: on */
    bsr find_token               /* looks for "off" */
    tst.l %d0
    bne.s do_off

    move.w #1,-(%sp)             /* XBIOS 0xFE, cmd 1 = cache on */
    move.w #0x00fe,%d0
    trap #14
    addq.l #2,%sp
    bra.s done
do_off:
    move.w #6,-(%sp)             /* XBIOS 0xFE, cmd 6 = off + release */
    move.w #0x00fe,%d0
    trap #14
    addq.l #2,%sp
done:
    move.w #0,%d0
    trap #1

/* Returns D0=1 when the string at A1 contains 'o','f','f'. */
find_token:
    tst.b (%a1)
    beq.s not_found
    cmpi.b #111,(%a1)            /* 'o' */
    bne.s next_char
    cmpi.b #102,1(%a1)           /* 'f' */
    bne.s next_char
    cmpi.b #102,2(%a1)           /* 'f' */
    bne.s next_char
    moveq #1,%d0
    rts
next_char:
    addq.l #1,%a1
    bra.s find_token
not_found:
    moveq #0,%d0
    rts
text_end:
