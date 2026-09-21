    .section .module_header,"a"
    .ascii "ASYSMOD0"
    .word 3
    .word 32
    .long __image_file_end
    .long 0
    .long proc_module_entry
    .long __image_memory_end
    .long 0

    .section .text.startup,"ax"
    .globl proc_module_entry
proc_module_entry:
    move.l 4(%sp),%a0
    cmpi.l #0x4b4d4150,(%a0)
    bne proc_fail
    cmpi.w #3,4(%a0)
    bne proc_fail
    cmpi.w #40,6(%a0)
    blo proc_fail
    lea proc_api(%pc),%a1
    move.l %a0,(%a1)
    bsr proc_alloc_stack
    tst.l %d0
    beq proc_fail
    lea proc_tasks(%pc),%a0
    move.l %d0,(%a0)
    lea proc_worker_0(%pc),%a1
    bsr proc_prepare_stack
    lea proc_tasks(%pc),%a0
    move.l %d0,4(%a0)
    move.l %a1,8(%a0)
    bsr proc_alloc_stack
    tst.l %d0
    beq proc_free_task_0
    lea proc_tasks+32(%pc),%a0
    move.l %d0,(%a0)
    lea proc_worker_1(%pc),%a1
    bsr proc_prepare_stack
    lea proc_tasks+32(%pc),%a0
    move.l %d0,4(%a0)
    move.l %a1,8(%a0)
    lea proc_scheduler(%pc),%a0
    move.w #2,6(%a0)

    lea proc_service(%pc),%a1
    move.l #0x50524f43,(%a1)
    move.w #5,4(%a1)
    move.w #52,6(%a1)
    lea proc_task_count(%pc),%a0
    move.l %a0,8(%a1)
    lea proc_cooperate(%pc),%a0
    move.l %a0,12(%a1)
    lea proc_dispatch_count(%pc),%a0
    move.l %a0,16(%a1)
    move.l #15,20(%a1)
    lea proc_sleep_ticks(%pc),%a0
    move.l %a0,24(%a1)
    lea proc_timer_tick(%pc),%a0
    move.l %a0,28(%a1)
    lea proc_sleep_remaining(%pc),%a0
    move.l %a0,32(%a1)
    lea proc_wait_event(%pc),%a0
    move.l %a0,36(%a1)
    lea proc_wake_task(%pc),%a0
    move.l %a0,40(%a1)
    lea proc_task_state(%pc),%a0
    move.l %a0,44(%a1)
    lea proc_context_self_test(%pc),%a0
    move.l %a0,48(%a1)
    move.l %a1,-(%sp)
    move.l #5,-(%sp)
    move.l #0x50524f43,-(%sp)
    lea proc_api(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%a1
    jsr (%a1)
    lea 12(%sp),%sp
    tst.l %d0
    beq proc_free_both
    moveq #1,%d0
    rts

proc_alloc_stack:
    move.l #0,-(%sp)
    move.l #11,-(%sp)
    move.l #0x0103,-(%sp)
    move.l #16,-(%sp)
    move.l #1024,-(%sp)
    lea proc_api(%pc),%a0
    move.l (%a0),%a0
    move.l 8(%a0),%a0
    jsr (%a0)
    lea 20(%sp),%sp
    rts

proc_prepare_stack:
    move.l %d0,%a0
    move.l #0x5354414b,(%a0)
    move.l #0x4b415453,1020(%a0)
    adda.l #1016,%a0
    move.l %a1,-(%a0)
    moveq #14,%d1
proc_clear_context:
    clr.l -(%a0)
    dbra %d1,proc_clear_context
    move.l %a0,%d0
    rts

proc_free_both:
    lea proc_tasks+32(%pc),%a0
    move.l (%a0),%d0
    bsr proc_free_stack
proc_free_task_0:
    lea proc_tasks(%pc),%a0
    move.l (%a0),%d0
    bsr proc_free_stack
proc_fail:
    moveq #0,%d0
    rts

proc_free_stack:
    move.l #0x0103,-(%sp)
    move.l %d0,-(%sp)
    lea proc_api(%pc),%a0
    move.l (%a0),%a0
    move.l 12(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    rts

proc_task_count:
    lea proc_scheduler(%pc),%a0
    moveq #0,%d0
    move.w 6(%a0),%d0
    rts

proc_dispatch_count:
    move.l 4(%sp),%d0
    cmpi.l #2,%d0
    bhs.s proc_bad_task
    mulu.w #32,%d0
    lea proc_tasks(%pc),%a0
    adda.l %d0,%a0
    move.l 12(%a0),%d0
    rts
proc_bad_task:
    moveq #0,%d0
    rts

proc_sleep_ticks:
    move.l 4(%sp),%d0
    cmpi.l #2,%d0
    bhs.s proc_bad_task
    mulu.w #32,%d0
    lea proc_tasks(%pc),%a0
    adda.l %d0,%a0
    move.l 8(%sp),24(%a0)
    tst.l 24(%a0)
    bne.s proc_set_sleeping
    clr.l 28(%a0)
    bra.s 1f
proc_set_sleeping:
    move.l #1,28(%a0)
1:
    moveq #1,%d0
    rts

proc_timer_tick:
    lea proc_tasks(%pc),%a0
    tst.l 24(%a0)
    beq.s 1f
    subq.l #1,24(%a0)
    bne.s 1f
    clr.l 28(%a0)
1:  tst.l 56(%a0)
    beq.s 2f
    subq.l #1,56(%a0)
    bne.s 2f
    clr.l 60(%a0)
2:  rts

proc_sleep_remaining:
    move.l 4(%sp),%d0
    cmpi.l #2,%d0
    bhs.s proc_bad_task
    mulu.w #32,%d0
    lea proc_tasks(%pc),%a0
    adda.l %d0,%a0
    move.l 24(%a0),%d0
    rts

proc_wait_event:
    move.l 4(%sp),%d0
    cmpi.l #2,%d0
    bhs proc_bad_task
    mulu.w #32,%d0
    lea proc_tasks(%pc),%a0
    adda.l %d0,%a0
    clr.l 24(%a0)
    move.l #2,28(%a0)
    moveq #1,%d0
    rts

proc_wake_task:
    move.l 4(%sp),%d0
    cmpi.l #2,%d0
    bhs proc_bad_task
    mulu.w #32,%d0
    lea proc_tasks(%pc),%a0
    adda.l %d0,%a0
    clr.l 24(%a0)
    clr.l 28(%a0)
    moveq #1,%d0
    rts

proc_task_state:
    move.l 4(%sp),%d0
    cmpi.l #2,%d0
    bhs proc_bad_task
    mulu.w #32,%d0
    lea proc_tasks(%pc),%a0
    adda.l %d0,%a0
    move.l 28(%a0),%d0
    rts

proc_cooperate:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea proc_tasks(%pc),%a0
    move.l (%a0),%a1
    cmpi.l #0x5354414b,(%a1)
    bne proc_context_bad_stack
    cmpi.l #0x4b415453,1020(%a1)
    bne proc_context_bad_stack
    move.l 32(%a0),%a1
    cmpi.l #0x5354414b,(%a1)
    bne proc_context_bad_stack
    cmpi.l #0x4b415453,1020(%a1)
    bne proc_context_bad_stack
    lea proc_scheduler(%pc),%a1
    move.w (%a1),%d0
    beq.s proc_host_to_0
    cmpi.w #1,%d0
    beq.s proc_0_to_1
proc_1_to_host:
    move.l %sp,36(%a0)
    move.l 2(%a1),%sp
    clr.w (%a1)
    bra.s proc_restore
proc_host_to_0:
    move.l %sp,2(%a1)
    tst.l 28(%a0)
    bne.s proc_host_try_1
    move.l 4(%a0),%sp
    move.w #1,(%a1)
    bra.s proc_restore
proc_host_try_1:
    tst.l 60(%a0)
    bne.s proc_restore
    move.l 36(%a0),%sp
    move.w #2,(%a1)
    bra.s proc_restore
proc_0_to_1:
    move.l %sp,4(%a0)
    tst.l 60(%a0)
    bne.s proc_0_to_host
    move.l 36(%a0),%sp
    move.w #2,(%a1)
    bra.s proc_restore
proc_0_to_host:
    move.l 2(%a1),%sp
    clr.w (%a1)
proc_restore:
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    moveq #1,%d0
    rts
proc_context_bad_stack:
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    moveq #0,%d0
    rts

proc_context_self_test:
    movem.l %d1-%d7/%a0-%a6,-(%sp)
    move.l #0x11111111,%d1
    move.l #0x22222222,%d2
    move.l #0x33333333,%d3
    move.l #0x44444444,%d4
    move.l #0x55555555,%d5
    move.l #0x66666666,%d6
    move.l #0x77777777,%d7
    move.l #0xa0a0a0a0,%a0
    move.l #0xa1a1a1a1,%a1
    move.l #0xa2a2a2a2,%a2
    move.l #0xa3a3a3a3,%a3
    move.l #0xa4a4a4a4,%a4
    move.l #0xa5a5a5a5,%a5
    move.l #0xa6a6a6a6,%a6
    bsr proc_cooperate
    cmpi.l #0x11111111,%d1
    bne.s proc_context_test_failed
    cmpi.l #0x22222222,%d2
    bne.s proc_context_test_failed
    cmpi.l #0x33333333,%d3
    bne.s proc_context_test_failed
    cmpi.l #0x44444444,%d4
    bne.s proc_context_test_failed
    cmpi.l #0x55555555,%d5
    bne.s proc_context_test_failed
    cmpi.l #0x66666666,%d6
    bne.s proc_context_test_failed
    cmpi.l #0x77777777,%d7
    bne.s proc_context_test_failed
    cmpa.l #0xa0a0a0a0,%a0
    bne.s proc_context_test_failed
    cmpa.l #0xa1a1a1a1,%a1
    bne.s proc_context_test_failed
    cmpa.l #0xa2a2a2a2,%a2
    bne.s proc_context_test_failed
    cmpa.l #0xa3a3a3a3,%a3
    bne.s proc_context_test_failed
    cmpa.l #0xa4a4a4a4,%a4
    bne.s proc_context_test_failed
    cmpa.l #0xa5a5a5a5,%a5
    bne.s proc_context_test_failed
    cmpa.l #0xa6a6a6a6,%a6
    bne.s proc_context_test_failed
    lea proc_tasks(%pc),%a0
    subq.l #1,12(%a0)
    subq.l #1,44(%a0)
    movem.l (%sp)+,%d1-%d7/%a0-%a6
    moveq #1,%d0
    rts
proc_context_test_failed:
    lea proc_tasks(%pc),%a0
    subq.l #1,12(%a0)
    subq.l #1,44(%a0)
    movem.l (%sp)+,%d1-%d7/%a0-%a6
    moveq #0,%d0
    rts

proc_worker_0:
proc_worker_0_loop:
    lea proc_tasks(%pc),%a0
    addq.l #1,12(%a0)
    bsr proc_cooperate
    bra.s proc_worker_0_loop

proc_worker_1:
proc_worker_1_loop:
    lea proc_tasks+32(%pc),%a0
    addq.l #1,12(%a0)
    bsr proc_cooperate
    bra.s proc_worker_1_loop

    .section .bss
    .align 2
proc_api:
    .space 4
proc_tasks:
    .space 64
proc_scheduler:
    .space 8
proc_service:
    .space 52
