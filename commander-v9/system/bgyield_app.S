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
    lea begin_text(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #10,%d0
    trap #0
    tst.l %d0
    bmi.s failed
    move.l %d0,%d2
wait_loop:
    moveq #10,%d0
    trap #0
    tst.l %d0
    bmi.s failed
    move.l %d0,%d1
    sub.l %d2,%d1
    cmpi.l #500,%d1
    blo.s wait_loop
    lea end_text(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s exit
failed:
    lea fail_text(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
begin_text: .asciz "BGYIELD: RUNNING, SHELL SHOULD REMAIN ACTIVE\n"
end_text:   .asciz "BGYIELD: FINISHED AFTER 500 TICKS\n"
fail_text:  .asciz "BGYIELD: YIELD FAILED\n"
