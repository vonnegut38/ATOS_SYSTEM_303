    .section .module_header,"a"
    .ascii "ASYSMOD0"
    .word 3
    .word 32
    .long __image_file_end
    .long 0
    .long input_module_entry
    .long __image_memory_end
    .long 0

    .section .text.startup,"ax"
    .globl input_module_entry
input_module_entry:
    move.l 4(%sp),%a0
    cmpi.l #0x4b4d4150,(%a0)
    bne input_fail
    cmpi.w #3,4(%a0)
    bne input_fail
    cmpi.w #172,6(%a0)
    blo input_fail
    lea input_api(%pc),%a1
    move.l %a0,(%a1)
    move.l #1,-(%sp)
    move.l #0x45564e54,-(%sp)
    move.l 28(%a0),%a1
    jsr (%a1)
    addq.l #8,%sp
    tst.l %d0
    beq input_fail
    move.l %d0,%a0
    cmpi.l #0x45564e54,(%a0)
    bne input_fail
    cmpi.w #1,4(%a0)
    blo input_fail
    cmpi.w #20,6(%a0)
    blo input_fail
    lea input_event(%pc),%a1
    move.l %a0,(%a1)

    move.l #1,-(%sp)
    move.l #0x54494d52,-(%sp)
    lea input_api(%pc),%a0
    move.l (%a0),%a0
    move.l 28(%a0),%a1
    jsr (%a1)
    addq.l #8,%sp
    tst.l %d0
    beq input_fail
    move.l %d0,%a0
    cmpi.l #0x54494d52,(%a0)
    bne input_fail
    cmpi.w #1,4(%a0)
    blo input_fail
    cmpi.w #16,6(%a0)
    blo input_fail
    lea input_timer(%pc),%a1
    move.l %a0,(%a1)

    /* Drop bounded firmware/IKBD residue before the shell becomes visible.
       Preserve partial mouse-packet framing when coordinate bytes have not
       arrived yet; normal polling will discard the remaining bytes. */
    moveq #31,%d0
    lea input_state(%pc),%a0
input_startup_drain:
    move.b 0xfffffc00,%d1
    btst #0,%d1
    beq.s input_startup_clean
    move.b 0xfffffc02,%d1
    tst.b (%a0)
    beq.s input_startup_packet
    subq.b #1,(%a0)
    bra.s input_startup_next
input_startup_packet:
    cmpi.b #0xf8,%d1
    blo.s input_startup_next
    cmpi.b #0xfb,%d1
    bhi.s input_startup_next
    move.b #2,(%a0)
input_startup_next:
    dbra %d0,input_startup_drain
input_startup_clean:

    lea input_service(%pc),%a1
    move.l #0x494e5054,(%a1)
    move.w #1,4(%a1)
    move.w #20,6(%a1)
    lea input_poll(%pc),%a0
    move.l %a0,8(%a1)
    lea input_classify(%pc),%a0
    move.l %a0,12(%a1)
    lea input_dropped(%pc),%a0
    move.l %a0,16(%a1)
    move.l %a1,-(%sp)
    move.l #1,-(%sp)
    move.l #0x494e5054,-(%sp)
    lea input_api(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%a1
    jsr (%a1)
    lea 12(%sp),%sp
    rts
input_fail:
    moveq #0,%d0
    rts

input_classify:
    move.l 4(%sp),%d0
    andi.l #0xff,%d0
    cmpi.l #0xf0,%d0
    bhs.s input_filtered
    btst #7,%d0
    bne.s input_key_up
    moveq #1,%d0
    rts
input_key_up:
    moveq #2,%d0
    rts
input_filtered:
    moveq #0,%d0
    rts

input_poll:
    lea input_api(%pc),%a0
    move.l (%a0),%a0
    move.l 152(%a0),%a1
    jsr (%a1)
    lea input_raw_dropped_seen(%pc),%a0
    cmp.l (%a0),%d0
    beq.s input_raw_state_ready
    move.l %d0,(%a0)
    bsr input_clear_pressed
input_raw_state_ready:
    lea input_api(%pc),%a0
    move.l (%a0),%a0
    move.l 148(%a0),%a0
    jsr (%a0)
    tst.l %d0
    bpl.s input_data_ready
    /* Boot-safe fallback for machines where the ACIA/MFP interrupt line is
       unavailable or externally rerouted. Supervisor polling cannot race the
       level-6 handler because kernel work retains the higher interrupt mask. */
    moveq #0,%d0
    move.b 0xfffffc00,%d0
    btst #0,%d0
    beq.s input_no_data
    moveq #0,%d1
    move.b 0xfffffc02,%d1
    bra.s input_have_status_data
input_no_data:
    /* Keep framing across normal serial gaps, but release a genuinely stale
       partial packet after two 50 Hz ticks so it cannot eat a later key. */
    lea input_state(%pc),%a0
    tst.b (%a0)
    bne.s input_timeout_check
    tst.b 1(%a0)
    beq input_filtered
input_timeout_check:
    lea input_timer(%pc),%a1
    move.l (%a1),%a1
    move.l 12(%a1),%a1
    jsr (%a1)
    lea input_state(%pc),%a0
    sub.l 8(%a0),%d0
    cmpi.l #2,%d0
    blo input_filtered
    clr.b (%a0)
    clr.b 1(%a0)
    bra input_filtered
input_data_ready:
    move.l %d0,%d1
    andi.l #0xff,%d1
    lsr.l #8,%d0
    andi.l #0xff,%d0
input_have_status_data:
    lea input_state(%pc),%a0
    clr.b 2(%a0)
    btst #5,%d0
    beq.s input_no_overrun
    /* A lost byte destroys packet boundaries. Quarantine the complete burst
       until two quiet 50 Hz ticks instead of guessing how many bytes remain. */
    clr.b (%a0)
    move.b #1,1(%a0)
    addq.l #1,4(%a0)
    bsr input_clear_pressed
    lea input_timer(%pc),%a1
    move.l (%a1),%a1
    move.l 12(%a1),%a1
    jsr (%a1)
    lea input_state(%pc),%a0
    move.l %d0,8(%a0)
input_no_overrun:
    tst.b (%a0)
    beq.s input_packet_start
    cmpi.b #2,(%a0)
    bne.s input_mouse_complete
    move.b %d1,13(%a0)          /* signed relative X byte */
    move.b #1,(%a0)
    bra input_filtered
input_mouse_complete:
    /* Publish one complete relative mouse packet on event task 1.  Keeping
       this queue separate from task 0 guarantees that the shell can never
       interpret a coordinate or button byte as a scan code. */
    clr.b (%a0)
    moveq #0,%d0
    move.b 12(%a0),%d0
    lsl.l #8,%d0
    move.b 13(%a0),%d0
    lsl.l #8,%d0
    move.b %d1,%d0
    move.l %d0,-(%sp)
    move.l #3,-(%sp)
    move.l #1,-(%sp)
    lea input_event(%pc),%a1
    move.l (%a1),%a1
    move.l 12(%a1),%a1
    jsr (%a1)
    lea 12(%sp),%sp
    tst.l %d0
    bne input_sent
    lea input_state(%pc),%a0
    addq.l #1,4(%a0)
    moveq #0,%d0
    rts
input_packet_start:
    tst.b 1(%a0)
    beq.s input_packet_header
    /* Still inside an overrun burst: drop this byte and restart the quiet
       interval. Empty polling clears the flag after two timer ticks. */
    lea input_timer(%pc),%a1
    move.l (%a1),%a1
    move.l 12(%a1),%a1
    jsr (%a1)
    lea input_state(%pc),%a0
    move.l %d0,8(%a0)
    bra input_filtered
input_packet_header:
    cmpi.b #0xf8,%d1
    blo.s input_keyboard
    cmpi.b #0xfb,%d1
    bhi.s input_keyboard
    move.b #2,(%a0)
    move.b %d1,12(%a0)
    lea input_timer(%pc),%a1
    move.l (%a1),%a1
    move.l 12(%a1),%a1
    jsr (%a1)
    lea input_state(%pc),%a0
    move.l %d0,8(%a0)
    bra input_filtered
input_keyboard:
    move.l %d1,-(%sp)
    bsr input_classify
    addq.l #4,%sp
    tst.l %d0
    beq input_filtered
    /* Accept at most one make code until the matching break code arrives.
       This prevents IKBD/host key-repeat bursts from duplicating commands.
       Releases update the bitmap even when the shell does not consume them. */
    move.l %d2,-(%sp)
    move.l %d1,%d2
    andi.l #0x7f,%d2
    lea input_pressed(%pc),%a1
    move.l %d2,-(%sp)
    lsr.l #3,%d2
    adda.l %d2,%a1
    move.l (%sp)+,%d2
    andi.l #7,%d2
    cmpi.l #2,%d0
    beq.s input_release_state
    btst %d2,(%a1)
    bne.s input_duplicate_down
    bset %d2,(%a1)
    move.l (%sp)+,%d2
    bra.s input_state_ready
input_release_state:
    bclr %d2,(%a1)
    move.l (%sp)+,%d2
    bra.s input_state_ready
input_duplicate_down:
    /* Repeat make code: deliver it so a held key keeps moving the selection. */
    move.l (%sp)+,%d2
    bra input_state_ready
input_state_ready:
    cmpi.l #2,%d0
    bne.s input_deliver
    /* Preserve every break event for GUI clients. The terminal translator
       consumes these events but returns no character, so shell behaviour is
       unchanged while GUI sessions receive complete make/break pairs. */
    moveq #2,%d0
input_deliver:
    movem.l %d0-%d1,-(%sp)
    move.l %d1,-(%sp)
    move.l %d0,-(%sp)
    lea input_api(%pc),%a0
    move.l (%a0),%a0
    move.l 168(%a0),%a0
    jsr (%a0)
    addq.l #8,%sp
    tst.l %d0
    bne input_system_key
    movem.l (%sp)+,%d0-%d1
    move.l %d1,-(%sp)
    move.l %d0,-(%sp)
    move.l 12(%sp),-(%sp)
    lea input_event(%pc),%a1
    move.l (%a1),%a1
    move.l 12(%a1),%a1
    jsr (%a1)
    lea 12(%sp),%sp
    tst.l %d0
    bne.s input_sent
    lea input_state(%pc),%a0
    addq.l #1,4(%a0)
    moveq #0,%d0
input_sent:
    rts
input_system_key:
    movem.l (%sp)+,%d0-%d1
    bra input_filtered

input_dropped:
    lea input_state(%pc),%a0
    move.l 4(%a0),%d0
    move.l %d0,-(%sp)
    lea input_api(%pc),%a0
    move.l (%a0),%a0
    move.l 152(%a0),%a0
    jsr (%a0)
    add.l (%sp)+,%d0
    rts

input_clear_pressed:
    movem.l %d0/%a0,-(%sp)
    lea input_pressed(%pc),%a0
    moveq #15,%d0
1:  clr.b (%a0)+
    dbra %d0,1b
    movem.l (%sp)+,%d0/%a0
    rts

    .section .bss
    .align 2
input_api:
    .space 4
input_event:
    .space 4
input_timer:
    .space 4
/* remaining mouse bytes, overrun quarantine, reserved, padding, dropped count,
   50 Hz tick when partial framing began, packet header and relative X */
input_state:
    .space 16
/* One bit per IKBD scan code. A make code is delivered only once per break. */
input_pressed:
    .space 16
input_raw_dropped_seen:
    .space 4
input_service:
    .space 20
