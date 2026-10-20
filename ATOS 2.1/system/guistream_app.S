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
    moveq #0,%d5
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
    tst.l 16(%a0)
    bne session_failed
    move.l 8(%a0),%d0
    andi.l #0xff00007f,%d0
    cmpi.l #0x01000025,%d0
    bne check_up
    cmpi.l #'k',12(%a0)
    bne session_failed
    moveq #1,%d5
    bra event_loop
check_up:
    cmpi.l #0x02000025,%d0
    bne event_loop
    tst.l %d5
    beq session_failed
    tst.l 12(%a0)
    bne session_failed
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
waiting: .asciz "GUISTREAM.APP: KEY SESSION - PRESS K\n"
success: .asciz "GUISTREAM.APP: K DOWN/UP STREAM OK\n"
error: .asciz "GUISTREAM.APP: KEY SESSION FAILED\n"
    .section .bss
    .align 2
reply: .space 20
event: .space 20
