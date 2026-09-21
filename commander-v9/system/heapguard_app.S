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
    moveq #0,%d2
    moveq #32,%d1
    moveq #16,%d0
    trap #0
    tst.l %d0
    bmi.s app_failed
    move.l %d0,%a2
    move.b #0x55,32(%a2)
    /* Any following kernel entry must isolate this process immediately. */
    moveq #10,%d0
    trap #0
app_failed:
    lea app_error(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
app_exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
app_error:   .asciz "HGUARD: HEAP GUARD TEST FAILED\n"
