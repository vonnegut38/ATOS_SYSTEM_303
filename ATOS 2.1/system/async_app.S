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
    lea request_a(%pc),%a0
    moveq #23,%d0
    trap #0
    tst.l %d0
    bne.s failed
    tst.l 128(%a0)
    beq.s failed
    lea request_b(%pc),%a0
    moveq #23,%d0
    trap #0
    tst.l %d0
    bne.s failed
    tst.l 128(%a0)
    beq.s failed
    lea before(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #10,%d0
    trap #0
    tst.l %d0
    bmi.s failed
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
exit:
    moveq #0,%d0
    trap #0
    illegal
    .section .data
request_a:
    .asciz "\\SYSTEM\\COOPA.APP"
    .space 46
    .space 64
    .long 0
    .long 0
request_b:
    .asciz "\\SYSTEM\\COOPB.APP"
    .space 46
    .space 64
    .long 0
    .long 0
    .section .rodata
before:  .asciz "ASYNC.APP: TWO JOBS QUEUED / YIELD\n"
success: .asciz "ASYNC.APP: PARENT RESUMED / OK\n"
error:   .asciz "ASYNC.APP: COOPERATIVE SPAWN FAILED\n"
