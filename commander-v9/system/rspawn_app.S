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
    bne failed
    move.l child_pid(%pc),%d1
    lea child_status(%pc),%a0
    moveq #33,%d0
    trap #0
    tst.l %d0
    bne failed
    move.l child_status(%pc),%d2
    cmpi.l #7,%d2
    bne failed
    lea child_ok(%pc),%a0
    moveq #1,%d0
    trap #0

    lea fault_request(%pc),%a0
    moveq #34,%d0
    trap #0
    tst.l %d0
    bne failed
    move.l fault_pid(%pc),%d1
    lea fault_status(%pc),%a0
    moveq #33,%d0
    trap #0
    tst.l %d0
    bne failed
    move.l fault_status(%pc),%d2
    cmpi.l #-260,%d2
    bne failed
    lea fault_ok(%pc),%a0
    moveq #1,%d0
    trap #0
    lea success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s exit
failed:
    lea error(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .data
    .align 2
child_request:
    .asciz "\\SYSTEM\\CHILD.APP"
    .space 46
    .space 64
child_pid:    .long 0
child_status: .long 0
fault_request:
    .asciz "\\SYSTEM\\FLTCHILD.APP"
    .space 43
    .space 64
fault_pid:    .long 0
fault_status: .long 0

    .section .rodata
child_ok: .asciz "RSPAWN: SEPARATE CHILD EXIT 7 OK\n"
fault_ok: .asciz "RSPAWN: SEPARATE CHILD FAULT -260 OK\n"
success:  .asciz "RSPAWN: ASYNC IMAGE / WAITPID OK\n"
error:    .asciz "RSPAWN: ASYNC IMAGE TEST FAILED\n"
