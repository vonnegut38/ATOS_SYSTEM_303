    .section .module_header,"a"
    .ascii "ASYSMOD0"
    .word 3
    .word 32
    .long __image_file_end
    .long 0
    .long event_module_entry
    .long __image_memory_end
    .long 0

    .section .text.startup,"ax"
    .globl event_module_entry
event_module_entry:
    move.l 4(%sp),%a0
    cmpi.l #0x4b4d4150,(%a0)
    bne event_fail
    cmpi.w #3,4(%a0)
    bne event_fail
    cmpi.w #40,6(%a0)
    blo event_fail
    lea event_api(%pc),%a1
    move.l %a0,(%a1)
    move.l #4,-(%sp)
    move.l #0x50524f43,-(%sp)
    move.l 28(%a0),%a1
    jsr (%a1)
    addq.l #8,%sp
    tst.l %d0
    beq event_fail
    move.l %d0,%a0
    cmpi.l #0x50524f43,(%a0)
    bne event_fail
    cmpi.w #4,4(%a0)
    blo event_fail
    cmpi.w #48,6(%a0)
    blo event_fail
    lea event_proc(%pc),%a1
    move.l %a0,(%a1)

    lea event_service(%pc),%a1
    move.l #0x45564e54,(%a1)
    move.w #1,4(%a1)
    move.w #20,6(%a1)
    lea event_receive(%pc),%a0
    move.l %a0,8(%a1)
    lea event_send(%pc),%a0
    move.l %a0,12(%a1)
    lea event_pending(%pc),%a0
    move.l %a0,16(%a1)
    move.l %a1,-(%sp)
    move.l #1,-(%sp)
    move.l #0x45564e54,-(%sp)
    lea event_api(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%a1
    jsr (%a1)
    lea 12(%sp),%sp
    rts
event_fail:
    moveq #0,%d0
    rts

event_mailbox:
    cmpi.l #2,%d0
    bhs.s event_bad_task
    mulu.w #260,%d0
    lea event_mailboxes(%pc),%a0
    adda.l %d0,%a0
    moveq #1,%d0
    rts
event_bad_task:
    moveq #0,%d0
    rts

event_pending:
    move.l 4(%sp),%d0
    bsr event_mailbox
    tst.l %d0
    beq.s event_return
    moveq #0,%d0
    move.b 2(%a0),%d0
event_return:
    rts

event_send:
    move.l 4(%sp),%d0
    bsr event_mailbox
    tst.l %d0
    beq.s event_return
    cmpi.b #32,2(%a0)
    bhs.s event_bad_task
    moveq #0,%d1
    move.b 1(%a0),%d1
    lsl.w #2,%d1
    move.l 8(%sp),4(%a0,%d1.w)
    lea 132(%a0),%a1
    move.l 12(%sp),0(%a1,%d1.w)
    moveq #0,%d1
    move.b 1(%a0),%d1
    addq.b #1,%d1
    andi.b #31,%d1
    move.b %d1,1(%a0)
    addq.b #1,2(%a0)
    move.l 4(%sp),-(%sp)
    lea event_proc(%pc),%a1
    move.l (%a1),%a1
    move.l 40(%a1),%a1
    jsr (%a1)
    addq.l #4,%sp
    tst.l %d0
    bne.s event_return
    move.l 4(%sp),%d0
    bsr event_mailbox
    subq.b #1,2(%a0)
    rts

event_receive:
    move.l 4(%sp),%d0
    bsr event_mailbox
    tst.l %d0
    beq.s event_return
    tst.b 2(%a0)
    beq.s event_wait
    moveq #0,%d1
    move.b (%a0),%d1
    lsl.w #2,%d1
    move.l 8(%sp),%a1
    cmpa.l #0,%a1
    beq event_bad_task
    move.l 12(%sp),%d0
    beq event_bad_task
    move.l 4(%a0,%d1.w),(%a1)
    move.l %d0,%a1
    adda.w #132,%a0
    move.l 0(%a0,%d1.w),(%a1)
    suba.w #132,%a0
    moveq #0,%d1
    move.b (%a0),%d1
    addq.b #1,%d1
    andi.b #31,%d1
    move.b %d1,(%a0)
    subq.b #1,2(%a0)
    moveq #1,%d0
    rts
event_wait:
    move.l 4(%sp),-(%sp)
    lea event_proc(%pc),%a1
    move.l (%a1),%a1
    move.l 36(%a1),%a1
    jsr (%a1)
    addq.l #4,%sp
    moveq #0,%d0
    rts

    .section .bss
    .align 2
event_api:
    .space 4
event_proc:
    .space 4
event_mailboxes:
    .space 520
event_service:
    .space 20
