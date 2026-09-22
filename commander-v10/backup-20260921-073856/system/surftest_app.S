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
    moveq #40,%d0
    trap #0
    tst.l %d0
    bmi failed
    move.l %d0,%d4
    moveq #18,%d0
    trap #0
    lea parent_pid(%pc),%a0
    move.l %d0,(%a0)
    lea create_message(%pc),%a0
    bsr send_gfx
    tst.l %d0
    ble failed
    lea surface_handle(%pc),%a0
    move.l %d0,(%a0)
    lea child_entry(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne cleanup_failed
    lea child_reply(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne cleanup_failed
    lea child_reply(%pc),%a0
    move.l child_pid(%pc),%d2
    cmp.l (%a0),%d2
    bne cleanup_failed
    cmpi.l #0x53555246,4(%a0)
    bne cleanup_failed
    lea query_message(%pc),%a0
    move.l surface_handle(%pc),4(%a0)
    bsr send_gfx
    cmpi.l #0x00400020,%d0
    bne cleanup_failed
    lea destroy_message(%pc),%a0
    move.l surface_handle(%pc),4(%a0)
    bsr send_gfx
    tst.l %d0
    bne failed
    lea query_message(%pc),%a0
    move.l surface_handle(%pc),4(%a0)
    bsr send_gfx
    cmpi.l #-0x3f1,%d0
    bne failed
reap_wait:
    lea child_result(%pc),%a0
    move.l child_pid(%pc),%d1
    moveq #0,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne failed
    cmpi.l #5,(%a0)
    beq reap
    moveq #10,%d0
    trap #0
    bra reap_wait
reap:
    tst.l 4(%a0)
    bne failed
    moveq #1,%d2
    moveq #32,%d0
    trap #0
    tst.l %d0
    bne failed
    lea success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra exit
cleanup_failed:
    lea destroy_message(%pc),%a0
    move.l surface_handle(%pc),4(%a0)
    bsr send_gfx
failed:
    lea error(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

child_entry:
    lea service_name(%pc),%a0
    moveq #40,%d0
    trap #0
    tst.l %d0
    bmi child_failed
    move.l %d0,%d4
    lea query_message(%pc),%a0
    move.l surface_handle(%pc),4(%a0)
    bsr send_gfx
    cmpi.l #-0x3f1,%d0
    bne child_failed
    lea child_ok(%pc),%a0
    move.l parent_pid(%pc),%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne child_failed
    moveq #0,%d1
    moveq #0,%d0
    trap #0
child_failed:
    moveq #2,%d1
    moveq #0,%d0
    trap #0
    illegal

/* A0 = four-long command, D4 = broker PID; D0 = broker status/value. */
send_gfx:
    move.l %a0,%a2
    move.l %d4,%d1
    moveq #36,%d0
    trap #0
    tst.l %d0
    bne send_failed
    lea gfx_reply(%pc),%a0
    moveq #38,%d0
    trap #0
    tst.l %d0
    bne send_failed
    lea gfx_reply(%pc),%a0
    cmp.l (%a0),%d4
    bne send_failed
    move.l (%a2),%d2
    ori.l #0x80000000,%d2
    cmp.l 4(%a0),%d2
    bne send_failed
    move.l 8(%a0),%d0
    rts
send_failed:
    move.l #-1,%d0
    rts

    .section .data
    .align 2
create_message:  .long 9,0x00400020,0,0
query_message:   .long 11,0,0,0
destroy_message: .long 10,0,0,0
child_ok:        .long 0x53555246,0,0,0
    .section .rodata
service_name: .asciz "SYSTEM.GFX"
success: .asciz "SURFTEST.APP: PID OWNERSHIP / STALE HANDLE OK\n"
error: .asciz "SURFTEST.APP: SURFACE ISOLATION FAILED\n"
    .section .bss
    .align 2
surface_handle: .space 4
parent_pid: .space 4
child_pid: .space 4
child_result: .space 8
gfx_reply: .space 20
child_reply: .space 20
