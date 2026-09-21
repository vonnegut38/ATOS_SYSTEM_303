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
    lea app_text(%pc),%a0
    moveq #1,%d0
    trap #0
    illegal
    illegal

    .section .rodata
app_text: .asciz "FLTCHILD.APP: TRIGGERING ILLEGAL INSTRUCTION\n"
