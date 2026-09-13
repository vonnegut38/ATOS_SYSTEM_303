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
    lea before_info(%pc),%a0
    moveq #12,%d0
    trap #0
    tst.l %d0
    bne app_failed
    lea iteration(%pc),%a0
    clr.w (%a0)
app_next:
    move.w iteration(%pc),%d2
    btst #0,%d2
    bne.s app_fault_child
    lea normal_request(%pc),%a0
    bra.s app_spawn
app_fault_child:
    lea fault_request(%pc),%a0
app_spawn:
    moveq #34,%d0
    trap #0
    tst.l %d0
    bne app_failed
    move.w iteration(%pc),%d2
    btst #0,%d2
    bne.s app_wait_fault
    move.l normal_pid(%pc),%d1
    lea normal_status(%pc),%a0
    bra.s app_wait
app_wait_fault:
    move.l fault_pid(%pc),%d1
    lea fault_status(%pc),%a0
app_wait:
    moveq #33,%d0
    trap #0
    tst.l %d0
    bne app_failed
    move.w iteration(%pc),%d2
    btst #0,%d2
    bne.s app_check_fault
    lea normal_status(%pc),%a0
    cmpi.l #7,(%a0)
    bne app_failed
    bra.s app_iteration_done
app_check_fault:
    lea fault_status(%pc),%a0
    cmpi.l #-260,(%a0)
    bne app_failed
app_iteration_done:
    lea iteration(%pc),%a0
    addq.w #1,(%a0)
    cmpi.w #8,(%a0)
    blo app_next
    lea after_info(%pc),%a0
    moveq #12,%d0
    trap #0
    tst.l %d0
    bne app_failed
    lea before_info(%pc),%a0
    lea after_info(%pc),%a1
    move.l 16(%a0),%d0
    cmp.l 16(%a1),%d0
    bne app_failed
    move.l 20(%a0),%d0
    cmp.l 20(%a1),%d0
    bne app_failed
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

    .section .data
    .align 2
normal_request:
    .asciz "\\SYSTEM\\CHILD.APP"
    .space 46
    .space 64
normal_pid:    .long 0
normal_status: .long 0
fault_request:
    .asciz "\\SYSTEM\\FLTCHILD.APP"
    .space 43
    .space 64
fault_pid:    .long 0
fault_status: .long 0

    .section .rodata
app_success: .asciz "PROCSTRESS: 8 SPAWN/WAIT/REAP CYCLES + MEMORY PASS\n"
app_error:   .asciz "PROCSTRESS: FAILED\n"

    .section .bss
    .align 2
iteration:   .space 2
before_info: .space 24
after_info:  .space 24
