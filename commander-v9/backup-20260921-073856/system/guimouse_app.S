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
    moveq #0,%d5                  /* movement, button down, button up */
event_loop:
    lea event(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne session_failed
    lea event(%pc),%a0
    tst.l (%a0)                  /* input must be kernel-authenticated */
    bne event_loop
    cmpi.l #0x4b4d4f55,4(%a0)   /* KMOU */
    bne event_loop
    /* A host may coalesce or burst motion faster than this deliberately
       bounded mailbox can be scheduled. Loss is reported in value[3], but
       it is not corruption: the absolute position and latest buttons remain
       authoritative, so validate the state transition rather than latency. */
    move.l 12(%a0),%d0
    move.l %d0,%d1
    andi.l #0x0000ffff,%d1
    beq 1f
    bset #0,%d5
1:  swap %d0
    lsr.w #8,%d0
    andi.l #3,%d0
    beq 2f
    bset #1,%d5
    bra 3f
2:  btst #1,%d5
    beq 3f
    bset #2,%d5
3:  cmpi.l #7,%d5
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
waiting: .asciz "GUIMOUSE.APP: DRAG WITH A MOUSE BUTTON\n"
success: .asciz "GUIMOUSE.APP: MOVE / BUTTON DOWN / UP OK\n"
error: .asciz "GUIMOUSE.APP: MOUSE STREAM FAILED\n"
    .section .bss
    .align 2
reply: .space 20
event: .space 20
