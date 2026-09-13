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
    lea normal_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne failed
    moveq #10,%d0
    trap #0
    lea child_result(%pc),%a0
    move.l normal_pid(%pc),%d1
    moveq #1,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #5,(%a0)
    bne failed
    cmpi.l #7,4(%a0)
    bne failed
    lea normal_ok(%pc),%a0
    moveq #1,%d0
    trap #0

    lea fault_child(%pc),%a1
    move.l %a1,%d1
    lea fault_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne failed
    moveq #10,%d0
    trap #0
    lea child_result(%pc),%a0
    move.l fault_pid(%pc),%d1
    moveq #1,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #5,(%a0)
    bne failed
    cmpi.l #-260,4(%a0)
    bne failed
    lea fault_ok(%pc),%a0
    moveq #1,%d0
    trap #0

    lea child_result(%pc),%a0
    move.l normal_pid(%pc),%d1
    moveq #0,%d2
    moveq #32,%d0
    trap #0
    cmpi.l #-881,%d0
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
    moveq #7,%d1
    moveq #0,%d0
    trap #0
    illegal
fault_child:
    illegal

    .section .bss
    .align 2
normal_pid: .space 4
fault_pid: .space 4
child_result: .space 8
    .section .rodata
normal_ok: .asciz "PIDWAIT: CHILD EXIT 7 / REAP OK\n"
fault_ok:  .asciz "PIDWAIT: CHILD FAULT -260 / REAP OK\n"
success:   .asciz "PIDWAIT: OWNERSHIP / STALE PID OK\n"
error:     .asciz "PIDWAIT: FAILED\n"
