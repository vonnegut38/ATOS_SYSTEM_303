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
    bsr find_gui
    tst.l %d0
    bne failed
    moveq #18,%d0
    trap #0
    lea parent_pid(%pc),%a0
    move.l %d0,(%a0)
    moveq #1,%d2
    bsr gui_command
    tst.l %d0
    bne failed
    cmp.l parent_pid(%pc),%d5
    bne failed
    cmpi.l #1,%d6
    bne failed
    lea child_entry(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne parent_unregister_failed
    lea ipc_message(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne parent_unregister_failed
    cmpi.l #0x47554931,4(%a0)
    bne parent_unregister_failed
    moveq #4,%d2
    bsr gui_command
    tst.l %d0
    bne parent_unregister_failed
    cmp.l child_pid(%pc),%d5
    bne parent_unregister_failed
    cmpi.l #2,%d6
    bne parent_unregister_failed
    lea continue_message(%pc),%a0
    move.l child_pid(%pc),%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne parent_unregister_failed
    /* Child deliberately faults without unregistering. Wait for and reap it;
       the kernel must notify SYSTEM.GUI and make the parent foreground. */
reap_fault_wait:
    lea child_result(%pc),%a0
    move.l child_pid(%pc),%d1
    moveq #0,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne parent_unregister_failed
    cmpi.l #5,(%a0)
    beq reap_fault
    moveq #10,%d0
    trap #0
    bra reap_fault_wait
reap_fault:
    cmpi.l #-260,4(%a0)
    bne parent_unregister_failed
    moveq #1,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne parent_unregister_failed
    moveq #4,%d2
    bsr gui_command
    tst.l %d0
    bne parent_unregister_failed
    cmp.l parent_pid(%pc),%d5
    bne parent_unregister_failed
    cmpi.l #1,%d6
    bne parent_unregister_failed
    moveq #2,%d2
    bsr gui_command
    tst.l %d0
    bne failed
    tst.l %d5
    bne failed
    tst.l %d6
    bne failed
    lea success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra exit
parent_unregister_failed:
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

child_entry:
    bsr find_gui
    tst.l %d0
    bne child_failed
    moveq #1,%d2
    bsr gui_command
    tst.l %d0
    bne child_failed
    moveq #3,%d2
    bsr gui_command
    tst.l %d0
    bne child_failed
    moveq #18,%d0
    trap #0
    cmp.l %d0,%d5
    bne child_failed
    lea ready_message(%pc),%a0
    move.l parent_pid(%pc),%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne child_failed
    lea ipc_message(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne child_failed
    cmpi.l #0x47554943,4(%a0)
    bne child_failed
    illegal
child_failed:
    moveq #2,%d1
    moveq #0,%d0
    trap #0
    illegal

find_gui:
    lea service_name(%pc),%a0
    moveq #40,%d0
    trap #0
    tst.l %d0
    bmi 1f
    move.l %d0,%d4
    moveq #0,%d0
1:  rts

/* D2 command; returns status D0, foreground D5, count D6. */
gui_command:
    lea command(%pc),%a0
    move.l %d2,(%a0)
    move.l %d4,%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne 2f
    lea gui_reply(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne 2f
    lea gui_reply(%pc),%a0
    cmp.l (%a0),%d4
    bne 2f
    ori.l #0x80000000,%d2
    cmp.l 4(%a0),%d2
    bne 2f
    move.l 12(%a0),%d5
    move.l 16(%a0),%d6
    move.l 8(%a0),%d0
    rts
2:  moveq #-1,%d0
    rts

    .section .data
    .align 2
command: .long 0,0,0,0
ready_message: .long 0x47554931,0,0,0
continue_message: .long 0x47554943,0,0,0
    .section .rodata
service_name: .asciz "SYSTEM.GUI"
success: .asciz "GUITEST.APP: FOREGROUND HANDOFF / FALLBACK OK\n"
error: .asciz "GUITEST.APP: GUI SERVICE FAILED\n"
    .section .bss
    .align 2
parent_pid: .space 4
child_pid: .space 4
child_result: .space 8
gui_reply: .space 20
ipc_message: .space 20
