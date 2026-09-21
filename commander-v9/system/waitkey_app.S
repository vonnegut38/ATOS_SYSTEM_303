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
    lea waiting_text(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #35,%d0
    trap #0
    tst.l %d0
    bmi.s failed
    move.l %d0,%d2
    lea received_text(%pc),%a0
    moveq #1,%d0
    trap #0
    move.l %d2,%d1
    moveq #8,%d0
    trap #0
    moveq #10,%d1
    moveq #8,%d0
    trap #0
    moveq #0,%d1
    bra.s exit
failed:
    lea failed_text(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
waiting_text:  .asciz "WAITKEY: INPUT REQUESTED - USE JOBS, THEN FG PID\n"
received_text: .asciz "WAITKEY: RECEIVED: "
failed_text:   .asciz "WAITKEY: BLOCKING INPUT FAILED\n"
