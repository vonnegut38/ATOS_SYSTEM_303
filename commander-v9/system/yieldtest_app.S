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
    moveq #10,%d0
    trap #0
    tst.l %d0
    bmi.s app_failed
    move.l %d0,%d4
    move.l #200000,%d3
app_wait_tick:
    moveq #10,%d0
    trap #0
    tst.l %d0
    bmi.s app_failed
    cmp.l %d4,%d0
    bne.s app_ok
    subq.l #1,%d3
    bne.s app_wait_tick
app_failed:
    lea app_error(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
    bra.s app_exit
app_ok:
    lea app_success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
app_exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
app_success: .asciz "YIELDTEST.APP: TIMER / SCHEDULER OK\n"
app_error: .asciz "YIELDTEST.APP: YIELD FAILED\n"
