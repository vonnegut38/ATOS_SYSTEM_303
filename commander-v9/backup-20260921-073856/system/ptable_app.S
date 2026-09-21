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
    moveq #0,%d4
    moveq #0,%d3
app_slot_loop:
    lea app_info(%pc),%a0
    move.l %d4,%d1
    moveq #20,%d0
    trap #0
    tst.l %d0
    bne.s app_failed
    lea app_info(%pc),%a0
    tst.l 12(%a0)
    beq.s app_not_current
    cmpi.l #2,4(%a0)
    bne.s app_failed
    addq.l #1,%d3
    bra.s app_next_slot
app_not_current:
    cmpi.l #5,4(%a0)
    bne.s app_next_slot
    addq.l #1,%d3
app_next_slot:
    addq.l #1,%d4
    cmpi.l #4,%d4
    blo.s app_slot_loop
    cmpi.l #2,%d3
    blo.s app_failed
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
app_success: .asciz "PTABLE.APP: RUNNING / TERMINATED OK\n"
app_error: .asciz "PTABLE.APP: PROCESS TABLE FAILED\n"

    .section .bss
    .align 2
app_info: .space 16
