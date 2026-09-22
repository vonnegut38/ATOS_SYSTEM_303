/* Separate DESKTEST.APP: exercise the production mouse handler and real GFX
   IPC in Hatari. No input injection syscall is added to the release kernel. */
    .macro test_mouse x,y,buttons
    move.l #((\x<<16)+\y),%d0
    moveq #\buttons,%d1
    bsr selftest_mouse
    .endm
    .macro check_long label,value
    lea \label(%pc),%a0
    cmpi.l #\value,(%a0)
    bne selftest_failed
    .endm

desktop_mouse_selftest:
    moveq #1,%d7
    test_mouse 120,95,2
    test_mouse 120,95,0
    lea button_active(%pc),%a1
    tst.b (%a1)
    beq selftest_failed
    moveq #2,%d7
    test_mouse 220,95,4
    lea info_active(%pc),%a1
    tst.b (%a1)
    beq selftest_failed
    lea dragging(%pc),%a1
    tst.b (%a1)
    bne selftest_failed
    /* Negative title-bar Y must clamp to 20, not jump to the bottom. */
    moveq #3,%d7
    test_mouse 120,55,2
    test_mouse 100,0,2
    check_long window_xy,((60<<16)+20)
    test_mouse 100,0,4
    lea dragging(%pc),%a1
    tst.b (%a1)
    bne selftest_failed
    /* Move back to the original location through the real copy/damage path. */
    moveq #4,%d7
    test_mouse 100,25,2
    test_mouse 120,55,2
    test_mouse 120,55,0
    check_long window_xy,((80<<16)+50)
    /* Negative extents clamp to the minimum; coalesced release commits. */
    moveq #5,%d7
    test_mouse 314,184,2
    test_mouse 20,20,2
    check_long pending_wh,((220<<16)+130)
    test_mouse 20,20,4
    check_long window_wh,((220<<16)+130)
    lea dragging(%pc),%a1
    tst.b (%a1)
    bne selftest_failed
    /* Last release position must count even without a held-motion packet. */
    moveq #6,%d7
    test_mouse 294,174,2
    test_mouse 350,250,0
    check_long window_wh,((276<<16)+206)
    lea dragging(%pc),%a1
    tst.b (%a1)
    bne selftest_failed
    /* Clamp right and bottom extents to the Mono screen. */
    moveq #7,%d7
    test_mouse 350,250,2
    test_mouse 639,399,0
    check_long window_wh,((560<<16)+350)
    moveq #0,%d0
    rts
selftest_failed:
    move.l %d7,%d0
    rts
selftest_mouse:
    lea event(%pc),%a0
    clr.l (%a0)
    move.l #0x4b4d4f55,4(%a0)
    move.l %d0,8(%a0)
    swap %d1
    lsl.l #8,%d1
    move.l %d1,12(%a0)
    clr.l 16(%a0)
    bsr mouse_event
    rts
    .section .rodata
selftest_pass: .asciz "DESKTEST: MOUSE / DRAG / RESIZE PASS\n"
selftest_fail: .asciz "DESKTEST: FAIL (EXIT CODE = CHECK NUMBER)\n"
    .section .bss
    .align 2
selftest_result: .space 4
