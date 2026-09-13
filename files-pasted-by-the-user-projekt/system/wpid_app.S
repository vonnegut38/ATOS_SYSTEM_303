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
    lea normal_child(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne failed
    lea waiting_normal(%pc),%a0
    moveq #1,%d0
    trap #0
    move.l child_pid(%pc),%d1
    lea child_status(%pc),%a0
    moveq #33,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #9,(%a0)
    bne failed
    lea normal_ok(%pc),%a0
    moveq #1,%d0
    trap #0

    lea fault_child(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne failed
    lea waiting_fault(%pc),%a0
    moveq #1,%d0
    trap #0
    move.l child_pid(%pc),%d1
    lea child_status(%pc),%a0
    moveq #33,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #-260,(%a0)
    bne failed
    lea success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s exit
failed:
    lea error(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

normal_child:
    lea child_normal(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #9,%d1
    moveq #0,%d0
    trap #0
fault_child:
    lea child_fault(%pc),%a0
    moveq #1,%d0
    trap #0
    illegal

    .section .bss
    .align 2
child_pid: .space 4
child_status: .space 4
    .section .rodata
waiting_normal:.asciz "WPID: PARENT WAITING FOR EXIT\n"
child_normal:  .asciz "WPID: CHILD EXITS 9\n"
normal_ok:     .asciz "WPID: PARENT WOKE WITH 9\n"
waiting_fault: .asciz "WPID: PARENT WAITING FOR FAULT\n"
child_fault:   .asciz "WPID: CHILD TRIGGERS ILLEGAL\n"
success:       .asciz "WPID: PARENT WOKE WITH -260 / OK\n"
error:         .asciz "WPID: BLOCKING WAITPID FAILED\n"
