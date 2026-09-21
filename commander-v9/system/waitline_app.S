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
    lea prompt_text(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d2
wait_character:
    moveq #35,%d0
    trap #0
    tst.l %d0
    bmi failed
    cmpi.l #27,%d0
    beq.s cancelled
    cmpi.l #10,%d0
    beq.s complete
    cmpi.l #8,%d0
    beq.s backspace
    cmpi.l #32,%d0
    blo.s wait_character
    cmpi.l #126,%d0
    bhi.s wait_character
    cmpi.l #63,%d2
    bhs.s wait_character
    move.l %d0,%d1
    moveq #8,%d0
    trap #0
    addq.l #1,%d2
    bra.s wait_character
backspace:
    tst.l %d2
    beq wait_character
    moveq #8,%d1
    moveq #8,%d0
    trap #0
    subq.l #1,%d2
    bra wait_character
complete:
    moveq #10,%d1
    moveq #8,%d0
    trap #0
    lea complete_text(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s exit
cancelled:
    lea cancelled_text(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #2,%d1
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
prompt_text:    .asciz "WAITLINE: TYPE TEXT, ENTER FINISHES, ESC CANCELS\n"
complete_text:  .asciz "WAITLINE: LINE COMPLETE\n"
cancelled_text: .asciz "\nWAITLINE: CANCELLED\n"
failed_text:    .asciz "WAITLINE: BLOCKING INPUT FAILED\n"
