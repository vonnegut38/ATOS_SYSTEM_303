    .section .app_header,"a"
    .ascii "ASYSAPP0"
    .word 1
    .word 32
    .long __app_file_end
    .long __app_memory_end
    .long app_entry
    .long 2048
    .long 0
    .section .text.startup,"ax"
    .globl app_entry
app_entry:
    lea service_name(%pc),%a0
    moveq #40,%d0
    trap #0
    tst.l %d0
    bmi failed
    move.l %d0,%d4
    moveq #1,%d2
    bsr gui_command
    tst.l %d0
    bne failed
    lea waiting(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #6,%d2
    bsr gui_command
    tst.l %d0
    bne unregister_failed
    moveq #0,%d5                  /* bits: shift-down, K-down, K-up, shift-up */
event_loop:
    lea event(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne session_failed
    lea event(%pc),%a0
    tst.l (%a0)
    bne event_loop
    cmpi.l #0x4b4b4559,4(%a0)
    bne event_loop
    tst.l 16(%a0)                /* no drops permitted in this test */
    bne session_failed
    move.l 8(%a0),%d0
    move.l %d0,%d1
    andi.l #0xff00007f,%d1
    cmpi.l #0x0100002a,%d1
    beq left_shift_down
    cmpi.l #0x01000036,%d1
    beq right_shift_down
    cmpi.l #0x01000025,%d1
    beq k_down
    cmpi.l #0x02000025,%d1
    beq k_up
    cmpi.l #0x0200002a,%d1
    beq shift_up
    cmpi.l #0x02000036,%d1
    beq shift_up
    bra event_loop
right_shift_down:
    btst #17,%d0
    beq session_failed
    move.l #0x00020000,%d6
    bra shift_down_ok
left_shift_down:
    btst #16,%d0
    beq session_failed
    move.l #0x00010000,%d6
shift_down_ok:
    bset #0,%d5
    bra event_loop
k_down:
    btst #0,%d5
    beq session_failed
    move.l %d0,%d1
    andi.l #0x00030000,%d1
    beq session_failed
    cmpi.l #'K',12(%a0)
    bne session_failed
    bset #1,%d5
    bra event_loop
k_up:
    btst #1,%d5
    beq session_failed
    tst.l 12(%a0)
    bne session_failed
    bset #2,%d5
    bra maybe_complete
shift_up:
    btst #0,%d5
    beq session_failed
    move.l %d0,%d1
    andi.l #0x00030000,%d1
    bne event_loop
    tst.l 12(%a0)
    bne session_failed
    bset #3,%d5
maybe_complete:
    cmpi.l #15,%d5
    bne event_loop
    moveq #7,%d2
    bsr gui_command
    tst.l %d0
    bne session_failed
    moveq #2,%d2
    bsr gui_command
    tst.l %d0
    bne failed
    lea success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra exit
session_failed:
    moveq #7,%d2
    bsr gui_command
unregister_failed:
    moveq #2,%d2
    bsr gui_command
failed:
    lea error(%pc),%a0
    moveq #1,%d0
    trap #0
    lea hex(%pc),%a0
    moveq #0,%d1
    move.b 0(%a0,%d5.w),%d1
    moveq #8,%d0
    trap #0
    moveq #'\n',%d1
    moveq #8,%d0
    trap #0
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

gui_command:
    lea command(%pc),%a0
    move.l %d2,(%a0)
    move.l %d4,%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne 1f
    lea reply(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne 1f
    lea reply(%pc),%a0
    cmp.l (%a0),%d4
    bne 1f
    ori.l #0x80000000,%d2
    cmp.l 4(%a0),%d2
    bne 1f
    move.l 8(%a0),%d0
    rts
1:  moveq #-1,%d0
    rts

    .section .data
    .align 2
command: .long 0,0,0,0
    .section .rodata
service_name: .asciz "SYSTEM.GUI"
waiting: .asciz "GUIMOD.APP: PRESS SHIFT+K\n"
success: .asciz "GUIMOD.APP: SHIFT+K MAKE/BREAK OK\n"
error: .asciz "GUIMOD.APP: MODIFIER STREAM FAILED, STATE "
hex: .ascii "0123456789ABCDEF"
    .section .bss
    .align 2
reply: .space 20
event: .space 20
