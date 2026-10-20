    .section .module_header,"a"
    .ascii  "ASYSMOD0"
    .word   3
    .word   32
    .long   __image_file_end
    .long   0
    .long   acsi_module_entry
    .long   __image_memory_end
    .long   0

    .section .text.startup,"ax"
    .globl acsi_module_entry
acsi_module_entry:
    /* ABI v3 registers the module's own low-level ACSI DMA read service. */
    move.l 4(%sp),%a0
    cmpi.l #0x4b4d4150,(%a0)
    bne.s 1f
    cmpi.w #3,4(%a0)
    bne.s 1f
    cmpi.w #24,6(%a0)
    blo.s 1f
    /* Optional tail: preserve v3 compatibility with smaller kernel APIs. */
    cmpi.w #176,6(%a0)
    blo.s 2f
    lea acsi_input_capture(%pc),%a1
    move.l 172(%a0),(%a1)
    lea acsi_module_api(%pc),%a1
    move.l %a0,(%a1)
2:  pea acsi_read_blocks(%pc)
    move.l 20(%a0),%a1
    jsr (%a1)
    addq.l #4,%sp
    rts
1:  moveq #0,%d0
    rts

acsi_read_blocks:
    /* C ABI: int read(uint8_t id, uint32_t lba, uint16_t count,
     *                 void *st_ram_buffer).
     * READ(6) is used for 21-bit LBAs and ICD extended READ(10) above that.
     * DMA buffers must be even and reside in 24-bit ST-RAM. */
    movem.l %d2-%d5/%a2,-(%sp)
    suba.w #12,%sp
    move.l 36(%sp),%d2
    move.l 40(%sp),%d3
    move.l 44(%sp),%d4
    move.l 48(%sp),%a2
    cmpi.l #8,%d2
    bhs acsi_bad_argument
    tst.l %d4
    beq acsi_bad_argument
    cmpi.l #255,%d4
    bhi acsi_bad_argument
    move.l %d3,%d0
    add.l %d4,%d0
    bcs acsi_bad_argument
    subq.l #1,%d0
    move.l %a2,%d0
    btst #0,%d0
    bne acsi_bad_argument
    cmpi.l #0x01000000,%d0
    bhs acsi_bad_argument
    move.l %d4,%d1
    lsl.l #8,%d1
    lsl.l #1,%d1
    add.l %d0,%d1
    bcs acsi_bad_argument
    cmpi.l #0x01000000,%d1
    bhi acsi_bad_argument

    /* Build a position-independent command buffer on the stack. */
    move.l %d2,%d0
    lsl.b #5,%d0
    cmpi.l #0x001fffff,%d3
    bhi acsi_build_read10
    ori.b #0x08,%d0
    move.b %d0,0(%sp)
    move.l %d3,%d0
    swap %d0
    andi.b #0x1f,%d0
    move.b %d0,1(%sp)
    move.l %d3,%d0
    lsr.l #8,%d0
    move.b %d0,2(%sp)
    move.b %d3,3(%sp)
    move.b %d4,4(%sp)
    clr.b 5(%sp)
    move.b #6,11(%sp)
    bra.s acsi_command_ready

acsi_build_read10:
    ori.b #0x1f,%d0             /* ICD extended-command prefix + target. */
    move.b %d0,0(%sp)
    move.b #0x28,1(%sp)         /* SCSI READ(10). */
    clr.b 2(%sp)
    move.l %d3,%d0
    swap %d0
    lsr.l #8,%d0
    move.b %d0,3(%sp)
    move.l %d3,%d0
    swap %d0
    move.b %d0,4(%sp)
    move.l %d3,%d0
    lsr.l #8,%d0
    move.b %d0,5(%sp)
    move.b %d3,6(%sp)
    clr.b 7(%sp)
    clr.b 8(%sp)                /* count is deliberately limited to 255. */
    move.b %d4,9(%sp)
    clr.b 10(%sp)
    move.b #11,11(%sp)

acsi_command_ready:

    moveq #2,%d5                  /* First attempt plus two retries. */
acsi_retry:

    /* Program the byte-wide DMA address registers. */
    move.l %a2,%d0              /* Retry must restore the real DMA address. */
    move.l %d0,%d1
    swap %d1
    move.b %d1,0xffff8609
    move.l %d0,%d1
    lsr.l #8,%d1
    move.b %d1,0xffff860b
    move.b %d0,0xffff860d

    /* Reset/select the DMA sector-count register, with settling delays. */
    move.w #0x0190,0xffff8606
    moveq #95,%d0
2:  nop
    dbra %d0,2b
    move.w #0x0090,0xffff8606
    moveq #95,%d0
3:  nop
    dbra %d0,3b
    move.w %d4,0xffff8604
    move.w #0x0088,0xffff8606

    /* Send all command bytes except the final byte with NOT_NEWCDB set. */
    lea 0(%sp),%a1
    moveq #0,%d1
    move.b 11(%sp),%d1
    subq.w #2,%d1
acsi_command_loop:
    moveq #0,%d0
    move.b (%a1)+,%d0
    bsr acsi_send_byte
    tst.l %d0
    beq acsi_timeout
    dbra %d1,acsi_command_loop
    moveq #0,%d0
    move.b (%a1),%d0
    move.w %d0,0xffff8604
    nop
    clr.w 0xffff8606
    move.l #400000,%d0
    bsr acsi_wait_irq
    tst.l %d0
    beq acsi_timeout

    move.w #0x008a,0xffff8606
    move.w 0xffff8604,%d0
    andi.l #0x000000ff,%d0
    move.w #0x0080,0xffff8606
    tst.b %d0
    bne acsi_retry_failed
    moveq #1,%d0
    bra acsi_done

acsi_timeout:
    move.w #0x0080,0xffff8606
acsi_retry_failed:
    dbra %d5,acsi_retry
acsi_bad_argument:
acsi_failed:
    moveq #0,%d0
acsi_done:
    adda.w #12,%sp
    movem.l (%sp)+,%d2-%d5/%a2
    rts

/* Send one command byte in D0 and wait for the active-low ACSI interrupt. */
acsi_send_byte:
    move.w %d0,0xffff8604
    nop
    move.w #0x008a,0xffff8606
    move.l #50000,%d0
acsi_wait_irq:
    move.l #4096,%d1          /* poll the kernel timer every ~2 ms; otherwise the
                                poll-based 50 Hz tick misses wraps during a long
                                DMA wait and the wall clock stalls */
4:  /* DMA wait may run with IRQs masked. Atomically drain ACIA
       into the same raw ring as the IRQ; INPUT.SYS alone parses packets. */
    btst #0,0xfffffc00
    beq.s 6f
    movem.l %d0-%d1/%a0-%a1,-(%sp)
    lea acsi_input_capture(%pc),%a0
    move.l (%a0),%d0
    beq.s 7f
    move.l %d0,%a0
    jsr (%a0)
7:  movem.l (%sp)+,%d0-%d1/%a0-%a1
6:  btst #5,0xfffffa01
    beq.s 5f
    subq.l #1,%d0
    beq.s 3f
    subq.l #1,%d1
    bne.s 4b
    bsr acsi_poll_timer
    move.l #4096,%d1
    bra.s 4b
3:  rts                        /* timeout: d0 == 0 */
5:  moveq #1,%d0
    rts

/* Poll the kernel timer service from the DMA wait so its tick keeps counting.
   The service is found lazily (TIMER.SYS loads after ACSI.SYS). */
acsi_poll_timer:
    movem.l %d0-%d2/%a0-%a1,-(%sp)
    lea acsi_timer_poll(%pc),%a0
    move.l (%a0),%a1
    bne.s 2f
    lea acsi_module_api(%pc),%a0
    move.l (%a0),%d0
    beq.s 3f
    move.l %d0,%a0
    move.l 28(%a0),%a1        /* kernel_module_api_t.find_service */
    pea 1
    pea 0x54494d52            /* KERNEL_SERVICE_TIMER, version 1 */
    jsr (%a1)
    addq.l #8,%sp
    move.l %d0,%a1
    beq.s 3f
    move.l 8(%a1),%a1         /* kernel_timer_service_t.poll */
    lea acsi_timer_poll(%pc),%a0
    move.l %a1,(%a0)
2:  move.l %a1,%a0
    jsr (%a0)
3:  movem.l (%sp)+,%d0-%d2/%a0-%a1
    rts

    .section .bss
    .align 2
acsi_input_capture:
    .space 4
acsi_module_api:
    .space 4
acsi_timer_poll:
    .space 4
