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
    lea child_request(%pc),%a0
    moveq #34,%d0
    trap #0
    tst.l %d0
    bne.s failed
    lea parent_exit(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    moveq #0,%d0
    trap #0
failed:
    lea spawn_failed(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .data
    .align 2
child_request:
    .asciz "\\SYSTEM\\CHILD.APP"
    .space 46
    .space 64
    .long 0
    .long 0

    .section .rodata
parent_exit:  .asciz "ORPHAN: PARENT EXITING BEFORE CHILD\n"
spawn_failed: .asciz "ORPHAN: SPAWN FAILED\n"
