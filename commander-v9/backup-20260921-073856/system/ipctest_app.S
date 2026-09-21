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
    moveq #18,%d0
    trap #0
    lea parent_pid(%pc),%a0
    move.l %d0,(%a0)
    lea child_entry(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne failed
    /* Let the child enter blocking receive before the request is sent. */
    moveq #10,%d0
    trap #0
    lea request(%pc),%a0
    move.l child_pid(%pc),%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne failed
    lea received(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne failed
parent_received:
    lea received(%pc),%a0
    move.l child_pid(%pc),%d2
    cmp.l (%a0),%d2
    bne failed
    cmpi.l #0x49504332,4(%a0)
    bne failed
    cmpi.l #0x55667788,8(%a0)
    bne failed
    cmpi.l #0xa5a55a5a,12(%a0)
    bne failed
    cmpi.l #2,16(%a0)
    bne failed
parent_reap_wait:
    lea child_result(%pc),%a0
    move.l child_pid(%pc),%d1
    moveq #0,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #5,(%a0)
    beq parent_reap
    moveq #10,%d0
    trap #0
    bra parent_reap_wait
parent_reap:
    tst.l 4(%a0)
    bne failed
    moveq #1,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne failed
    lea success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra exit
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
    lea received(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne child_failed
    lea received(%pc),%a0
    move.l parent_pid(%pc),%d2
    cmp.l (%a0),%d2
    bne child_failed
    cmpi.l #0x49504331,4(%a0)
    bne child_failed
    cmpi.l #0x11223344,8(%a0)
    bne child_failed
    cmpi.l #0x5a5aa5a5,12(%a0)
    bne child_failed
    cmpi.l #1,16(%a0)
    bne child_failed
    lea reply(%pc),%a0
    move.l parent_pid(%pc),%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne child_failed
    moveq #0,%d1
    moveq #0,%d0
    trap #0
child_failed:
    moveq #2,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .data
    .align 2
request: .long 0x49504331,0x11223344,0x5a5aa5a5,1
reply:   .long 0x49504332,0x55667788,0xa5a55a5a,2
    .section .bss
    .align 2
parent_pid:   .space 4
child_pid:    .space 4
child_result: .space 8
received:     .space 20
    .section .rodata
success: .asciz "IPCTEST.APP: BLOCKING PROCESS MAILBOX OK\n"
error:   .asciz "IPCTEST.APP: IPC FAILED\n"
