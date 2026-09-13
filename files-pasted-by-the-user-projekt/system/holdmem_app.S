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
    moveq #0,%d2
    move.l #1048576,%d1
    moveq #16,%d0
    trap #0
    tst.l %d0
    bmi reserve_ok
    lea held(%pc),%a0
    moveq #1,%d0
    trap #0
1:  bra.s 1b

reserve_ok:
    lea protected(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
held:      .asciz "HOLDMEM: 1 MB PRIVATE HEAP ACTIVE\n"
protected: .asciz "HOLDMEM: KERNEL MEMORY RESERVE PROTECTED\n"
