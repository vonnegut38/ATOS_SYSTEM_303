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
    lea started(%pc),%a0
    moveq #1,%d0
    trap #0

    /* Deliberately never calls the kernel again. */
1:
    bra.s 1b

    .section .rodata
started: .asciz "HANG: CPU LOOP ACTIVE - USE KILL PID\n"
