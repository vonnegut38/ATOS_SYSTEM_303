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
    lea app_prompt(%pc),%a0
    moveq #1,%d0
    trap #0
app_wait:
    moveq #9,%d0
    trap #0
    tst.l %d0
    bpl.s app_got_key
    moveq #10,%d0
    trap #0
    bra.s app_wait
app_got_key:
    beq.s app_wait
    move.l %d0,%d1
    moveq #8,%d0
    trap #0
    moveq #10,%d1
    moveq #8,%d0
    trap #0
    lea app_ok(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
app_prompt: .asciz "IOTEST.APP: PRESS A KEY: "
app_ok: .asciz "IOTEST.APP: CHARACTER I/O OK\n"
