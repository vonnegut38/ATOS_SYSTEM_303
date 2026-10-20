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
    lea app_info(%pc),%a0
    moveq #12,%d0
    trap #0
    tst.l %d0
    bne.s app_failed
    lea app_info(%pc),%a0
    cmpi.l #1,(%a0)
    bne.s app_failed
    cmpi.l #12,4(%a0)
    blo.s app_failed
    tst.l 16(%a0)
    beq.s app_failed
    lea app_success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s app_exit
app_failed:
    lea app_error(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
app_exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
app_success: .asciz "SYSINFO.APP: ABI / CPU / MEMORY OK\n"
app_error: .asciz "SYSINFO.APP: SYSTEM INFO FAILED\n"

    .section .bss
    .align 2
app_info: .space 24
