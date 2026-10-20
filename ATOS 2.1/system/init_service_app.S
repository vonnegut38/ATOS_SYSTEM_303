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
    lea service_name(%pc),%a0
    moveq #39,%d0
    trap #0
    tst.l %d0
    bne service_failed
service_loop:
    lea message(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne service_failed
    /* Reserved control message: 'STOP' requests an orderly shutdown. */
    cmpi.l #0x53544f50,4(%a0)
    bne service_loop
    moveq #0,%d1
    moveq #0,%d0
    trap #0
service_failed:
    moveq #1,%d1
    moveq #0,%d0
    trap #0
    illegal
    .section .rodata
service_name: .asciz "SYSTEM.INIT"
    .section .bss
    .align 2
message: .space 20
