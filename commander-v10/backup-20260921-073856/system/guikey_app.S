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
    moveq #5,%d2
    bsr gui_command
    tst.l %d0
    bne unregister_failed
    lea key_message(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne unregister_failed
    lea key_message(%pc),%a0
    tst.l (%a0)
    bne unregister_failed
    cmpi.l #0x4b4b4559,4(%a0)
    bne unregister_failed
    cmpi.l #0x01000025,8(%a0)
    bne unregister_failed
    cmpi.l #'k',12(%a0)
    bne unregister_failed
2:
    lea success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #2,%d2
    bsr gui_command
    tst.l %d0
    bne failed
    moveq #0,%d1
    bra exit
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
waiting: .asciz "GUIKEY.APP: FOREGROUND KEY REQUEST - PRESS K\n"
success: .asciz "GUIKEY.APP: FOREGROUND KEY DELIVERED OK\n"
error: .asciz "GUIKEY.APP: KEY ROUTING FAILED\n"
    .section .bss
    .align 2
reply: .space 20
key_message: .space 20
