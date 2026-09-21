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
    lea isolation_flag(%pc),%a0
    move.l #0x11223344,(%a0)
    lea child_entry(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne clone_failed
    lea parent_1(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #10,%d0
    trap #0
    lea parent_run(%pc),%a0
    moveq #1,%d0
    trap #0
    move.l #100000,%d3
wait_loop:
    lea child_result(%pc),%a0
    move.l child_pid(%pc),%d1
    moveq #0,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #5,(%a0)
    beq.s woke
    moveq #10,%d0
    trap #0
    subq.l #1,%d3
    bne.s wait_loop
    bra.s failed
woke:
    lea child_result(%pc),%a0
    tst.l 4(%a0)
    bne failed
    lea isolation_flag(%pc),%a0
    cmpi.l #0x11223344,(%a0)
    bne failed
    lea child_result(%pc),%a0
    move.l child_pid(%pc),%d1
    moveq #1,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
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
    bra.s exit
clone_failed:
    move.l %d0,%d1
    neg.l %d1
exit:
    moveq #0,%d0
    trap #0
    illegal

child_entry:
    lea child_sleep(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #2,%d1
    moveq #11,%d0
    trap #0
    tst.l %d0
    bmi.s child_failed
    lea child_woke(%pc),%a0
    moveq #1,%d0
    trap #0
    lea isolation_flag(%pc),%a0
    move.l #0x55667788,(%a0)
    moveq #0,%d1
    moveq #0,%d0
    trap #0
child_failed:
    moveq #2,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .bss
    .align 2
child_pid: .space 4
child_result: .space 8
isolation_flag: .space 4
    .section .rodata
parent_1:   .asciz "SLPCTX: PROCESS A YIELDS\n"
child_sleep:.asciz "SLPCTX: PROCESS B SLEEPING\n"
parent_run: .asciz "SLPCTX: PROCESS A RUNNING\n"
child_woke: .asciz "SLPCTX: PROCESS B WOKE\n"
success:    .asciz "SLPCTX: SLEEP / PRIVATE IMAGE / REAP OK\n"
error:      .asciz "SLPCTX: FAILED\n"
