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
    lea started(%pc),%a0
    moveq #1,%d0
    trap #0
    lea child_entry(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne failed
    move.l #20000000,%d7
1:
    subq.l #1,%d7
    bne.s 1b
    lea child_result(%pc),%a0
    move.l child_pid(%pc),%d1
    moveq #1,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #5,(%a0)
    bne failed
    cmpi.l #7,4(%a0)
    bne failed
    lea finished(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    moveq #0,%d0
    trap #0
    illegal

failed:
    lea failure(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
    moveq #0,%d0
    trap #0
    illegal

child_entry:
    lea child_ran(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #7,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
started:  .asciz "HOGTEST: CPU LOOP STARTED / NO YIELD\n"
child_ran:.asciz "HOGTEST: PREEMPTIVE CHILD RAN\n"
finished: .asciz "HOGTEST: PREEMPT / CONTEXT / REAP PASS\n"
failure:  .asciz "HOGTEST: PREEMPTION FAILED\n"

    .section .bss
    .align 2
child_pid:    .space 4
child_result: .space 8
