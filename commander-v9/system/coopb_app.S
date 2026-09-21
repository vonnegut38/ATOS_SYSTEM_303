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
    lea first(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #10,%d0
    trap #0
    tst.l %d0
    bmi.s failed
    lea second(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #12,%d1
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
    .section .rodata
first:  .asciz "COOPB.APP: STEP 1\n"
second: .asciz "COOPB.APP: STEP 2\n"
error:  .asciz "COOPB.APP: YIELD FAILED\n"
