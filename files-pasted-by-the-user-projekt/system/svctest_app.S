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
    lea temporary_name(%pc),%a0
    moveq #39,%d0
    trap #0
    tst.l %d0
    bne failed
    lea temporary_name(%pc),%a0
    moveq #41,%d0
    trap #0
    tst.l %d0
    bne failed
    lea temporary_name(%pc),%a0
    moveq #40,%d0
    trap #0
    cmpi.l #-0x3d4,%d0
    bne failed
    lea child_entry(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne failed
    /* Child registers the service and wakes us with a ready message. */
    lea received(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne failed
    lea received(%pc),%a0
    move.l child_pid(%pc),%d2
    cmp.l (%a0),%d2
    bne failed
    cmpi.l #0x53564331,4(%a0)
    bne failed
    /* Lookup is case-insensitive and must return the live owner's PID. */
    lea lookup_name(%pc),%a0
    moveq #40,%d0
    trap #0
    cmp.l child_pid(%pc),%d0
    bne failed
    /* A second owner may not claim the same name. */
    lea service_name(%pc),%a0
    moveq #39,%d0
    trap #0
    cmpi.l #-0x3d2,%d0
    bne failed
    lea stop_message(%pc),%a0
    move.l child_pid(%pc),%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
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
    /* Process exit must automatically remove its service. */
    lea service_name(%pc),%a0
    moveq #40,%d0
    trap #0
    cmpi.l #-0x3d4,%d0
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
    lea service_name(%pc),%a0
    moveq #39,%d0
    trap #0
    tst.l %d0
    bne child_failed
    lea ready_message(%pc),%a0
    move.l parent_pid(%pc),%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne child_failed
    lea received(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne child_failed
    cmpi.l #0x53564332,4(%a0)
    bne child_failed
    moveq #0,%d1
    moveq #0,%d0
    trap #0
child_failed:
    moveq #2,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
service_name: .asciz "ECHO.TEST"
lookup_name:  .asciz "echo.test"
temporary_name: .asciz "PARENT.TMP"
    .align 2
ready_message: .long 0x53564331,1,2,3
stop_message:  .long 0x53564332,4,5,6
success: .asciz "SVCTEST.APP: NAMED SERVICE DISCOVERY / CLEANUP OK\n"
error:   .asciz "SVCTEST.APP: SERVICE REGISTRY FAILED\n"
    .section .bss
    .align 2
parent_pid:   .space 4
child_pid:    .space 4
child_result: .space 8
received:     .space 20
