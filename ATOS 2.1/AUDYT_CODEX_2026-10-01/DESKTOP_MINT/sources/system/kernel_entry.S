    .section .kernel_header,"a"
    .ascii  "ASYSKERN"
    .word   1
    .word   32
    .long   __image_file_end-0x00020000
    .long   0x00020000
    .long   kernel_entry-0x00020000
    .long   __image_memory_end-0x00020000
    .long   0
    .section .text.startup,"ax"
    .globl kernel_entry
kernel_entry:
    lea __bss_start,%a0
    lea __bss_end,%a1
0:  cmpa.l %a1,%a0
    bhs.s 1f
    clr.b (%a0)+
    bra.s 0b
1:
    jmp kernel_main

    .globl kernel_image_cache_sync
kernel_image_cache_sync:
    move.l 4(%sp),%d0
    cmpi.l #2,%d0
    blo.s 3f
    cmpi.l #3,%d0
    bhi.s 2f
    moveq #9,%d0
    .word 0x4e7b,0x0002          /* 020/030: invalidate + enable I-cache; D-cache off */
    bra.s 3f
2:  .word 0xf4f8                 /* cpusha bc: 68040/68060 */
3:  rts

    .globl kernel_exception_install
kernel_exception_install:
    move.l 4(%sp),%d0
    move.b %d0,kernel_cpu_type
    beq.s 1f
    moveq #0,%d0
    .word 0x4e7b,0x0801           /* movec d0,vbr: 68010+ */
1:
    move.l #kernel_bus_error,8
    move.l #kernel_address_error,12
    move.l #kernel_illegal,16
    move.l #kernel_divzero,20
    move.l #kernel_chk,24
    move.l #kernel_trapv,28
    move.l #kernel_privilege,32
    move.l #kernel_line_a,40
    move.l #kernel_line_f,44
    move.l #kernel_trap0,128
    move.l #kernel_trap1,132
    move.l #kernel_trap2,136
    move.l #kernel_trap3,140
    move.l #kernel_trap4,144
    move.l #kernel_trap5,148
    move.l #kernel_trap6,152
    move.l #kernel_trap7,156
    move.l #kernel_trap8,160
    move.l #kernel_trap9,164
    move.l #kernel_trap10,168
    move.l #kernel_trap11,172
    move.l #kernel_trap12,176
    move.l #kernel_trap13,180
    move.l #kernel_trap14,184
    move.l #kernel_trap15,188
    rts

    .globl kernel_preemption_enable
    .extern kernel_ikbd_irq_reset,kernel_ikbd_irq_capture
kernel_preemption_enable:
    /* Timer C is the only enabled MFP source.  Auto-EOI avoids a stale
       in-service bit if a user process is returned to the kernel scheduler. */
    clr.b 0xfffffa07
    clr.b 0xfffffa09
    clr.b 0xfffffa13
    clr.b 0xfffffa15
    /* This firmware cannot assume that TOS initialized MFP VR.  Reserve the
       conventional 0x40-0x4f MFP range explicitly; Timer C is vector 0x45. */
    move.b #0x40,0xfffffa17
    move.l #kernel_timer_c,0x00000114
    move.l #kernel_ikbd_acia,0x00000118
    bclr #4,0xfffffa05            /* GPIP4 is the shared ACIA IRQ input */
    bclr #4,0xfffffa03            /* active on the falling edge */
    move.b #3,0xfffffc04          /* reset shared MIDI ACIA */
    move.b #0x15,0xfffffc04       /* /16, 8N1, RxIRQ off until MIDI.SYS */
    clr.b 0xfffffa0d
    clr.b 0xfffffa11
    jsr kernel_ikbd_irq_reset
    move.b #0x60,0xfffffa09
    move.b #0x60,0xfffffa15
    moveq #1,%d0
    rts
    .globl kernel_ikbd_poll_capture
kernel_ikbd_poll_capture:
    /* A disk syscall may inherit IPL5. Serialize status/data/ring writes
       against the level-6 ACIA IRQ, then restore the original mask. */
    move.w %sr,-(%sp)
    ori.w #0x0700,%sr
    jsr kernel_ikbd_irq_capture
    move.w (%sp)+,%sr
    rts
    .globl kernel_irq_mask
kernel_irq_mask:
    /* Mask every interrupt and return the previous SR so a caller can poll a
       device without the level-6 ACIA handler stealing the reply bytes. */
    move.w %sr,%d0
    ori.w #0x0700,%sr
    rts
    .globl kernel_irq_restore
kernel_irq_restore:
    move.w 6(%sp),%sr
    rts
kernel_ikbd_acia:
    movem.l %d0-%d1/%a0-%a1,-(%sp)
    bclr #6,0xfffffa11
    jsr kernel_ikbd_irq_capture
    /* v10: central IRQ layer (IRQ_SRC_ACIA = 6). */
    move.l  #6,-(%sp)
    jsr     irq_dispatch
    addq.l  #4,%sp
    movem.l (%sp)+,%d0-%d1/%a0-%a1
    rte

    .extern kernel_fault_vector,kernel_fault_pc,kernel_fault_sr,kernel_fault_frame,kernel_fault_usp
    .extern kernel_fault_w4,kernel_fault_w6,kernel_fault_w8,kernel_fault_w10,kernel_fault_w12
    /* Capture only the six-byte SR/PC prefix, never bytes beyond a short
       frame. Remaining snapshot bytes are explicitly absent (zero). */
    .macro kernel_capture_fault vector
    move.l #\vector,kernel_fault_vector
    move.l (%sp),kernel_fault_frame
    clr.l kernel_fault_frame+4
    move.w 4(%sp),kernel_fault_frame+4
    clr.l kernel_fault_frame+8
    clr.l kernel_fault_frame+12
    clr.l kernel_fault_frame+16
    clr.l kernel_fault_frame+20
    move.l 2(%sp),kernel_fault_pc
    clr.l kernel_fault_sr
    move.w (%sp),kernel_fault_sr+2
    /* Registers that usually hold the bad pointer behind a bogus jump. */
    move.l %a0,kernel_fault_a0
    move.l %d0,kernel_fault_d0
    /* User stack pointer at the fault: the only reliable clue for a group-1
       frame, which carries neither the PC nor the USP.  %a0 is already saved. */
    move.l %usp,%a0
    move.l %a0,kernel_fault_usp
    .endm

    /* The 68000 bus error (2) and address error (3) use the six-word group-1
       frame instead: mode@0, fault address@2, opcode@6, SR@8, with no stacked
       PC.  The mode word at +0 is not the SR, and its undocumented opcode bits
       can imitate the supervisor bit, so a 68000 must read SR from +8. */
    .macro kernel_capture_group1 vector
    move.l #\vector,kernel_fault_vector
    move.l (%sp),kernel_fault_frame
    clr.l kernel_fault_frame+4
    move.w 4(%sp),kernel_fault_frame+4
    clr.l kernel_fault_frame+8
    clr.l kernel_fault_frame+12
    clr.l kernel_fault_frame+16
    clr.l kernel_fault_frame+20
    tst.b kernel_cpu_type
    bne.s .Lcapstd\@
    clr.l kernel_fault_sr
    move.w 8(%sp),kernel_fault_sr+2
    move.l 2(%sp),kernel_fault_pc
    bra.s .Lcapdone\@
.Lcapstd\@:
    move.l 2(%sp),kernel_fault_pc
    clr.l kernel_fault_sr
    move.w (%sp),kernel_fault_sr+2
.Lcapdone\@:
    /* Registers that usually hold the bad pointer behind a bogus jump. */
    move.l %a0,kernel_fault_a0
    move.l %d0,kernel_fault_d0
    /* Raw group-1 frame words.  For the 68000 that is
       SSW@0, address@2, IR@6, SR@8, PC@10 - the only place the faulting
       instruction can be read from, since a group-1 frame has no useful
       stacked PC field.  Harmless on 68020+ (pure diagnostics). */
    move.w 4(%sp),kernel_fault_w4
    move.w 6(%sp),kernel_fault_w6
    move.w 8(%sp),kernel_fault_w8
    move.w 10(%sp),kernel_fault_w10
    move.w 12(%sp),kernel_fault_w12
    /* User stack pointer at the fault (group-1 frames have no stacked PC). */
    move.l %usp,%a0
    move.l %a0,kernel_fault_usp
    .endm

kernel_bus_error:
    kernel_capture_group1 2
    moveq #2,%d0
    bsr kernel_try_user_fault
    move.l #2,-(%sp)
    bra kernel_fault
kernel_address_error:
    kernel_capture_group1 3
    moveq #3,%d0
    bsr kernel_try_user_fault
    move.l #3,-(%sp)
    bra kernel_fault
kernel_illegal:
    kernel_capture_fault 4
    moveq #4,%d0
    bsr kernel_try_user_fault
    move.l #4,-(%sp)
    bra kernel_fault
kernel_divzero:
    kernel_capture_fault 5
    moveq #5,%d0
    bsr kernel_try_user_fault
    move.l #5,-(%sp)
    bra kernel_fault
kernel_chk:
    kernel_capture_fault 6
    moveq #6,%d0
    bsr kernel_try_user_fault
    move.l #6,-(%sp)
    bra kernel_fault
kernel_trapv:
    kernel_capture_fault 7
    moveq #7,%d0
    bsr kernel_try_user_fault
    move.l #7,-(%sp)
    bra kernel_fault
kernel_privilege:
    kernel_capture_fault 8
    moveq #8,%d0
    bsr kernel_try_user_fault
    move.l #8,-(%sp)
    bra kernel_fault
kernel_line_a:
    kernel_capture_fault 10
    moveq #10,%d0
    bsr kernel_try_user_fault
    move.l #10,-(%sp)
    bra kernel_fault
kernel_line_f:
    kernel_capture_fault 11
    moveq #11,%d0
    bsr kernel_try_user_fault
    move.l #11,-(%sp)
kernel_fault:
    jsr kernel_exception_dispatch
    addq.l #4,%sp
2:
    stop #0x2700
    bra.s 2b

    .macro kernel_unhandled_trap name, vector
\name:
    kernel_capture_fault \vector
    moveq #\vector,%d0
    bsr kernel_try_user_fault
    move.l #\vector,-(%sp)
    bra kernel_fault
    .endm

    kernel_unhandled_trap kernel_trap1,33
    kernel_unhandled_trap kernel_trap2,34
    kernel_unhandled_trap kernel_trap3,35
    kernel_unhandled_trap kernel_trap4,36
    kernel_unhandled_trap kernel_trap5,37
    kernel_unhandled_trap kernel_trap6,38
    kernel_unhandled_trap kernel_trap7,39
    kernel_unhandled_trap kernel_trap8,40
    kernel_unhandled_trap kernel_trap9,41
    kernel_unhandled_trap kernel_trap10,42
    kernel_unhandled_trap kernel_trap11,43
    kernel_unhandled_trap kernel_trap12,44
    kernel_unhandled_trap kernel_trap13,45
    kernel_unhandled_trap kernel_trap14,46
    kernel_unhandled_trap kernel_trap15,47

kernel_try_user_fault:
    /* BSR adds four bytes before the hardware SR. A supervisor fault must
       never be mistaken for a user fault just because a process is active.
       On the 68000 the bus/address-error frame has no SR at +0 (SR is at
       frame+8); every other frame stacks SR first.  %d0 holds the vector. */
    cmpi.w #2,%d0
    beq.s .Ltry_group1
    cmpi.w #3,%d0
    beq.s .Ltry_group1
    btst #5,4(%sp)
    bra.s .Ltry_decide
.Ltry_group1:
    tst.b kernel_cpu_type
    bne.s .Ltry_group1_std
    btst #5,12(%sp)
    bra.s .Ltry_decide
.Ltry_group1_std:
    btst #5,4(%sp)
.Ltry_decide:
    bne.s 1f
    move.l %d0,-(%sp)
    jsr kernel_user_fault
    addq.l #4,%sp
    tst.l %d0
    beq.s 1f
    move.l %d0,-(%sp)
    jsr kernel_user_finish_fault
    move.l (%sp)+,%d0
    jmp kernel_user_recover
1:
    rts

kernel_timer_c:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    addq.l #1,kernel_timer_ticks
    /* v10: central IRQ layer (IRQ_SRC_MFP_C = 4). */
    move.l  #4,-(%sp)
    jsr     irq_dispatch
    addq.l  #4,%sp
#if CONFIG_TOS
    /* B4: the native timer must not depend on the TOS persona when no TOS
       program is running; only then are the TOS ticks reachable. */
    tst.b kernel_tos_runtime_enabled
    beq.s 8f
    jsr tos_composite_tick
    /* G1.3b: preemptive TOS task switch.  Returns 0 while a single task
       exists, so this is a no-op until the scheduler is enabled. */
    move.l %sp,-(%sp)
    jsr tos_task_tick
    addq.l #4,%sp
    tst.l %d0
    beq.s 8f
    move.l %d0,%sp
8:
#endif
    bclr #5,0xfffffa11
    moveq #0,%d0
    move.w 60(%sp),%d0
    andi.w #0x2000,%d0
    bne.s kernel_timer_return

    /* An IRQ is another untrusted user-to-kernel boundary. */
    moveq #0,%d0
    move.w 60(%sp),%d0
    move.l 62(%sp),%d1
    move.l %usp,%a0
    move.l %d0,-(%sp)
    move.l %d1,-(%sp)
    move.l %a0,-(%sp)
    jsr kernel_user_validate_entry
    lea 12(%sp),%sp
    tst.l %d0
    bne.s kernel_timer_ended

    lea 60(%sp),%a0
    lea 62(%sp),%a1
    move.l %a0,-(%sp)
    move.l %a1,-(%sp)
    move.l %usp,%a0
    move.l %a0,-(%sp)
    lea 12(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_preempt_context
    lea 16(%sp),%sp
    bra.s kernel_timer_switched

kernel_timer_ended:
    lea 60(%sp),%a0
    lea 62(%sp),%a1
    move.l %a0,-(%sp)
    move.l %a1,-(%sp)
    lea 8(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_switch_after_exit
    lea 12(%sp),%sp

kernel_timer_switched:
    tst.l %d0
    beq.s kernel_timer_return
    cmpi.l #1,%d0
    beq kernel_user_pause_recover
    cmpi.l #2,%d0
    beq kernel_user_output_recover
    move.l %d0,%a0
    move.l %a0,%usp
kernel_timer_return:
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

kernel_trap0:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    /* One validation gate for every user-to-kernel transition.  Validate the
       live USP and exception-frame PC/SR before interpreting the syscall. */
    moveq #0,%d0
    move.w 60(%sp),%d0
    move.l 62(%sp),%d1
    move.l %usp,%a0
    move.l %d0,-(%sp)
    move.l %d1,-(%sp)
    move.l %a0,-(%sp)
    jsr kernel_user_validate_entry
    lea 12(%sp),%sp
    tst.l %d0
    bne kernel_trap0_process_ended
    cmpi.l #10,(%sp)
    beq kernel_switch_user_context
    cmpi.l #11,(%sp)
    beq kernel_sleep_user_context
    cmpi.l #33,(%sp)
    beq kernel_waitpid_user_context
    cmpi.l #35,(%sp)
    beq kernel_wait_input_context
    cmpi.l #38,(%sp)
    beq kernel_wait_message_context
    cmpi.l #26,(%sp)
    beq kernel_capture_user_context
    cmpi.l #28,(%sp)
    beq kernel_capture_user_full_context
    cmpi.l #30,(%sp)
    beq kernel_restore_user_full_context
kernel_trap0_regular:
    move.l 36(%sp),-(%sp)
    move.l 36(%sp),-(%sp)
    move.l 16(%sp),-(%sp)
    move.l 16(%sp),-(%sp)
    move.l 16(%sp),-(%sp)
    jsr kernel_user_syscall
    lea 20(%sp),%sp
    tst.l %d0
    bne.s 2f
    jsr kernel_user_syscall_result
    move.l %d0,(%sp)
1:  movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte
2:
kernel_trap0_process_ended:
    lea 60(%sp),%a0
    lea 62(%sp),%a1
    move.l %a0,-(%sp)
    move.l %a1,-(%sp)
    lea 8(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_switch_after_exit
    lea 12(%sp),%sp
    tst.l %d0
    beq.s 4f
    move.l %d0,%a0
    move.l %a0,%usp
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte
4:  jsr kernel_user_syscall_result
    bra kernel_user_recover

kernel_switch_user_context:
    lea 60(%sp),%a0
    lea 62(%sp),%a1
    move.l %a0,-(%sp)
    move.l %a1,-(%sp)
    move.l %usp,%a0
    move.l %a0,-(%sp)
    lea 12(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_switch_context
    lea 16(%sp),%sp
    tst.l %d0
    beq kernel_trap0_regular
    cmpi.l #1,%d0
    beq kernel_user_pause_recover
    cmpi.l #2,%d0
    beq kernel_user_output_recover
    move.l %d0,%a0
    move.l %a0,%usp
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

kernel_user_output_recover:
    movea.l #kernel_dispatch_completed,%a0
    move.w #1,(%a0)
    movea.l #kernel_dispatch_output,%a0
    move.w #1,(%a0)
    bra kernel_user_pause_recover

kernel_sleep_user_context:
    move.l 4(%sp),-(%sp)
    lea 64(%sp),%a0
    lea 66(%sp),%a1
    move.l %a0,-(%sp)
    move.l %a1,-(%sp)
    move.l %usp,%a0
    move.l %a0,-(%sp)
    lea 16(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_sleep_context
    lea 20(%sp),%sp
    tst.l %d0
    beq kernel_trap0_regular
    cmpi.l #-1,%d0
    beq kernel_user_pause_recover
    move.l %d0,%a0
    move.l %a0,%usp
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

kernel_waitpid_user_context:
    move.l 32(%sp),%a2
    move.l 4(%sp),%d2
    lea 60(%sp),%a0
    lea 62(%sp),%a1
    move.l %usp,%a3
    move.l %a2,-(%sp)
    move.l %d2,-(%sp)
    move.l %a0,-(%sp)
    move.l %a1,-(%sp)
    move.l %a3,-(%sp)
    lea 20(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_waitpid_context
    lea 24(%sp),%sp
    tst.l %d0
    beq kernel_trap0_regular
    move.l %d0,%a0
    move.l %a0,%usp
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

kernel_wait_input_context:
    lea 60(%sp),%a0
    lea 62(%sp),%a1
    move.l %a0,-(%sp)
    move.l %a1,-(%sp)
    move.l %usp,%a0
    move.l %a0,-(%sp)
    lea 12(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_wait_input_context
    lea 16(%sp),%sp
    tst.l %d0
    beq kernel_trap0_regular
    cmpi.l #1,%d0
    beq kernel_user_pause_recover
    cmpi.l #2,%d0
    beq kernel_user_output_recover
    move.l %d0,%a0
    move.l %a0,%usp
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

kernel_wait_message_context:
    move.l 32(%sp),%a2
    lea 60(%sp),%a0
    lea 62(%sp),%a1
    move.l %a2,-(%sp)
    move.l %a0,-(%sp)
    move.l %a1,-(%sp)
    move.l %usp,%a0
    move.l %a0,-(%sp)
    lea 16(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_wait_message_context
    lea 20(%sp),%sp
    tst.l %d0
    beq kernel_trap0_regular
    cmpi.l #1,%d0
    beq kernel_user_pause_recover
    cmpi.l #2,%d0
    beq kernel_user_output_recover
    move.l %d0,%a0
    move.l %a0,%usp
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

kernel_capture_user_context:
    moveq #0,%d0
    move.w 60(%sp),%d0
    move.l 62(%sp),%d1
    move.l %usp,%a0
    move.l %d0,-(%sp)
    move.l %d1,-(%sp)
    move.l %a0,-(%sp)
    jsr kernel_user_capture_context
    lea 12(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

kernel_capture_user_full_context:
    moveq #0,%d0
    move.w 60(%sp),%d0
    move.l 62(%sp),%d1
    move.l %usp,%a0
    move.l %d0,-(%sp)
    move.l %d1,-(%sp)
    move.l %a0,-(%sp)
    lea 12(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_capture_full_context
    lea 16(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

kernel_restore_user_full_context:
    move.l %sp,-(%sp)
    jsr kernel_user_restore_full_context
    addq.l #4,%sp
    tst.l %d0
    beq.s 3f
    move.l %d0,%a0
    move.l %a0,%usp
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte
3:  move.l #-0x353,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

    .globl kernel_run_user
kernel_run_user:
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l 48(%sp),%d2
    move.l 52(%sp),%d3
    move.l 56(%sp),%d4
    move.l 60(%sp),%d5
    move.l 64(%sp),%d6
    lea kernel_user_saved_depth,%a0
    moveq #0,%d0
    move.w (%a0),%d0
    cmpi.w #4,%d0
    bhs kernel_user_depth_failed
    addq.w #1,(%a0)
    lsl.w #2,%d0
    lea kernel_user_saved_sp,%a0
    move.l %sp,0(%a0,%d0.w)
    move.l %d3,-(%sp)
    move.l %d6,-(%sp)
    move.l %d5,-(%sp)
    move.l %d4,-(%sp)
    jsr kernel_user_begin
    lea 16(%sp),%sp
    tst.l %d0
    beq.s kernel_user_slot_failed
    move.l %d3,%a0
    move.l %a0,%usp
    clr.w -(%sp)                 /* 68010+ format/vector word (format 0) */
    move.l %d2,-(%sp)
    move.w #0x0500,-(%sp)        /* user mode; only level 6 Timer-C may preempt */
    rte

kernel_user_recover:
    lea kernel_user_saved_depth,%a0
    subq.w #1,(%a0)
    moveq #0,%d1
    move.w (%a0),%d1
    lea kernel_dispatch_active,%a1
    tst.w (%a1)
    beq.s 1f
    lea kernel_dispatch_depth,%a1
    cmp.w (%a1),%d1
    bne.s 1f
    lea kernel_dispatch_active,%a1
    clr.w (%a1)
    lea kernel_dispatch_completed,%a1
    move.w #1,(%a1)
1:
    lsl.w #2,%d1
    lea kernel_user_saved_sp,%a0
    move.l 0(%a0,%d1.w),%sp
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

kernel_user_pause_recover:
    lea kernel_user_saved_depth,%a0
    subq.w #1,(%a0)
    moveq #0,%d1
    move.w (%a0),%d1
    lsl.w #2,%d1
    lea kernel_user_saved_sp,%a0
    move.l 0(%a0,%d1.w),%sp
    moveq #0,%d0
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    /* This path returns to the supervisor with RTS instead of RTE, so the
       Timer-C exception's SR (IPL6, IRQ6 masked) would leak into the main
       loop. Restore the kernel's baseline supervisor SR (S=1, IPL5): the
       same level used when a native process is entered below (0x0500 user). */
    move.w #0x2500,%sr
    rts

kernel_user_depth_failed:
    moveq #-1,%d0
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

kernel_user_slot_failed:
    lea kernel_user_saved_depth,%a0
    subq.w #1,(%a0)
    moveq #-2,%d0
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

    .globl kernel_dispatch_ready_user
kernel_dispatch_ready_user:
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    lea kernel_user_saved_depth,%a0
    moveq #0,%d0
    move.w (%a0),%d0
    cmpi.w #4,%d0
    bhs.s kernel_dispatch_none
    lea kernel_dispatch_depth,%a1
    move.w %d0,(%a1)
    lea kernel_dispatch_active,%a1
    move.w #1,(%a1)
    addq.w #1,(%a0)
    lsl.w #2,%d0
    lea kernel_user_saved_sp,%a0
    move.l %sp,0(%a0,%d0.w)
    lea -68(%sp),%sp
    clr.w 66(%sp)                /* fabricated 68010+ format-0 frame */
    lea 62(%sp),%a0
    lea 60(%sp),%a1
    move.l %a1,-(%sp)
    move.l %a0,-(%sp)
    lea 8(%sp),%a0
    move.l %a0,-(%sp)
    jsr kernel_user_take_ready
    lea 12(%sp),%sp
    tst.l %d0
    beq.s kernel_dispatch_cancel
    move.l %d0,%a0
    move.l %a0,%usp
    /* A Timer-C request accumulated while the supervisor was drawing is
       not a quantum consumed by this user. Discard only that pending bit
       atomically before RTE; otherwise a slow GUI polling pass can preempt
       the resumed process before its first instruction, forever. Timer
       timekeeping polls the hardware counter separately; leave IKBD pending. */
    ori.w #0x0700,%sr
    move.b #0xdf,0xfffffa0d       /* MFP IPRB: clear Timer C only (write-zero) */
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte
kernel_dispatch_cancel:
    lea 68(%sp),%sp
    lea kernel_user_saved_depth,%a0
    subq.w #1,(%a0)
    lea kernel_dispatch_active,%a0
    clr.w (%a0)
kernel_dispatch_none:
    moveq #0,%d0
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

    .globl kernel_dispatch_completed_user
kernel_dispatch_completed_user:
    lea kernel_dispatch_completed,%a0
    moveq #0,%d0
    move.w (%a0),%d0
    clr.w (%a0)
    rts

    .globl kernel_dispatch_output_user
kernel_dispatch_output_user:
    lea kernel_dispatch_output,%a0
    moveq #0,%d0
    move.w (%a0),%d0
    clr.w (%a0)
    rts

    .globl kernel_dispatch_active_user
kernel_dispatch_active_user:
    lea kernel_dispatch_active,%a0
    moveq #0,%d0
    move.w (%a0),%d0
    rts

    .section .bss
    .align 2
    .globl kernel_timer_ticks
kernel_timer_ticks:
    .space 4
kernel_user_saved_sp:
    .space 16
kernel_user_saved_depth:
    .space 2
kernel_dispatch_depth:
    .space 2
kernel_dispatch_active:
    .space 2
kernel_dispatch_completed:
    .space 2
kernel_dispatch_output:
    .space 2

    /* Boot-time CPU family (0 = 68000), used to pick the correct hardware
       exception frame for the group-1 vectors. */
    .globl kernel_cpu_type
kernel_cpu_type:
    .space 1
    .align 2
