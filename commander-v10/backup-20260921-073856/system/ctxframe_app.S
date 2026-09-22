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
    moveq #26,%d0
    trap #0
after_capture:
    tst.l %d0
    bne.s failed
    lea context(%pc),%a0
    moveq #27,%d0
    trap #0
    tst.l %d0
    bne.s failed
    lea context(%pc),%a0
    move.l (%a0),%d1
    beq.s failed
    btst #0,%d1
    bne.s failed
    lea after_capture(%pc),%a1
    cmp.l 4(%a0),%a1
    bne.s failed
    move.l 8(%a0),%d1
    andi.l #0x00002000,%d1
    bne.s failed
    move.l 8(%a0),%d1
    andi.l #0x00000700,%d1
    cmpi.l #0x00000700,%d1
    bne.s failed
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
    .section .bss
    .align 2
context: .space 12
    .section .rodata
success: .asciz "CTXFRAME.APP: USP / PC / SR CAPTURE OK\n"
error:   .asciz "CTXFRAME.APP: CONTEXT FRAME FAILED\n"
