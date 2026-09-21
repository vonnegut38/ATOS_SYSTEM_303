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
    lea normal_request(%pc),%a0
    moveq #23,%d0
    trap #0
    tst.l %d0
    bne failed
    lea normal_id(%pc),%a1
    move.l 128(%a0),(%a1)
    lea fault_request(%pc),%a0
    moveq #23,%d0
    trap #0
    tst.l %d0
    bne failed
    lea fault_id(%pc),%a1
    move.l 128(%a0),(%a1)

    lea query_result(%pc),%a0
    move.l normal_id(%pc),%d1
    moveq #0,%d2
    moveq #24,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #1,(%a0)
    bne failed
    lea queued_ok(%pc),%a0
    moveq #1,%d0
    trap #0

    moveq #10,%d0
    trap #0
    lea query_result(%pc),%a0
    move.l normal_id(%pc),%d1
    moveq #1,%d2
    moveq #24,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #3,(%a0)
    bne failed
    cmpi.l #7,4(%a0)
    bne failed
    lea normal_ok(%pc),%a0
    moveq #1,%d0
    trap #0

    moveq #10,%d0
    trap #0
    lea query_result(%pc),%a0
    move.l fault_id(%pc),%d1
    moveq #1,%d2
    moveq #24,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #3,(%a0)
    bne failed
    cmpi.l #-260,4(%a0)
    bne failed
    lea fault_ok(%pc),%a0
    moveq #1,%d0
    trap #0

    lea query_result(%pc),%a0
    move.l normal_id(%pc),%d1
    moveq #0,%d2
    moveq #24,%d0
    trap #0
    cmpi.l #-817,%d0
    bne failed
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
normal_request:
    .asciz "\\SYSTEM\\CHILD.APP"
    .space 46
    .space 64
    .long 0
    .long 0
fault_request:
    .asciz "\\SYSTEM\\FLTCHILD.APP"
    .space 43
    .space 64
    .long 0
    .long 0
normal_id: .long 0
fault_id: .long 0
query_result: .space 8
    .section .rodata
queued_ok: .asciz "WAITTEST.APP: NONBLOCKING QUERY = QUEUED\n"
normal_ok: .asciz "WAITTEST.APP: CHILD EXIT 7 / REAPED\n"
fault_ok:  .asciz "WAITTEST.APP: CHILD FAULT -260 / REAPED\n"
success:   .asciz "WAITTEST.APP: WAIT/QUERY/REAP OK\n"
error:     .asciz "WAITTEST.APP: FAILED\n"
