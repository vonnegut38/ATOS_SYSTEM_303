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
    lea arguments(%pc),%a0
    moveq #64,%d1
    moveq #7,%d0
    trap #0
    tst.l %d0
    beq show_current
    bmi failed
    lea arguments(%pc),%a0
    moveq #35,%d0
    trap #0
    tst.l %d0
    bne failed
    lea label(%pc),%a0
    moveq #1,%d0
    trap #0
    lea arguments(%pc),%a0
    moveq #1,%d0
    trap #0
    lea newline(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra exit
show_current:
    lea arguments(%pc),%a0
    moveq #46,%d0
    trap #0
    tst.l %d0
    bne failed
    lea label(%pc),%a0
    moveq #1,%d0
    trap #0
    lea arguments(%pc),%a0
    moveq #1,%d0
    trap #0
    lea newline(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra exit
failed:
    lea usage(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
label: .asciz "PATH="
newline: .asciz "\n"
usage: .asciz "USAGE: PATH.APP DIRECTORY\n"
    .section .bss
    .align 2
arguments: .space 64
