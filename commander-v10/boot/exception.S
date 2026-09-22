    .section .text,"ax"
    .globl probe_read8,probe_read16,probe_read32,probe_write8,probe_rw32
    .globl exception_install
    .globl cpu_probe_vbr,cpu_probe_extb
    .globl cpu_probe_tc,cpu_probe_urp,cpu_probe_pcr
    .globl cpu_probe_030_cacr
    .globl cpu_probe_fpu
    .extern bus_recovery_pc

exception_install:
    lea     default_exception,%a0
    lea     8,%a1
    moveq   #62,%d0
1:  move.l  %a0,(%a1)+
    dbra    %d0,1b
    move.l  #bus_error,8
    move.l  #bus_error,12
    rts

/* Bus/address error handlers rewrite the saved PC and report failure. */
bus_error:
    tst.l   bus_probe_armed
    beq.s   bus_fatal
    move.l  bus_probe_sp,%sp
    move.l  bus_recovery_pc,%a0
    moveq   #0,%d0
    jmp     (%a0)
/* Phase 5: a real bus/address fault (outside a hardware probe) reports
 * instead of silently recovering or looping. */
bus_fatal:
    move.l  %sp,exception_frame_ptr
    jsr     bios_exception_report
1:  bra.s   1b
default_exception:
    /* Phase 5 (v10): diagnostics instead of an infinite loop. */
    move.l  %sp,exception_frame_ptr
    jsr     bios_exception_report
1:  bra.s   1b

/* int probe_read8(uint32_t address, uint8_t *value) */
probe_read8:
    move.l  4(%sp),%a0
    move.l  8(%sp),%a1
    move.l  8,%d1
    move.l  #bus_error,8
    move.l  #2f,bus_recovery_pc
    move.l  %sp,bus_probe_sp
    move.l  #1,bus_probe_armed
    moveq   #1,%d0
    move.b  (%a0),(%a1)
2:  clr.l   bus_probe_armed
    move.l  %d1,8
    rts

/* Safe read-only probes. Word/long addresses must be even. */
probe_read16:
    move.l  4(%sp),%a0
    move.l  8(%sp),%a1
    move.l  8,%d1
    move.l  #bus_error,8
    move.l  #6f,bus_recovery_pc
    move.l  %sp,bus_probe_sp
    move.l  #1,bus_probe_armed
    moveq   #1,%d0
    move.w  (%a0),(%a1)
6:  clr.l   bus_probe_armed
    move.l  %d1,8
    rts

probe_read32:
    move.l  4(%sp),%a0
    move.l  8(%sp),%a1
    move.l  8,%d1
    move.l  #bus_error,8
    move.l  #7f,bus_recovery_pc
    move.l  %sp,bus_probe_sp
    move.l  #1,bus_probe_armed
    moveq   #1,%d0
    move.l  (%a0),(%a1)
7:  clr.l   bus_probe_armed
    move.l  %d1,8
    rts

/* Bus-error-protected byte write (Phase 2, v10): used by the STE shifter
 * write/readback test.  Returns 1 on success, 0 if the write faulted.
 * Uses only scratch registers (d0/d1/a0); d2-d7/a2-a6 stay untouched. */
probe_write8:
    move.l  4(%sp),%a0
    move.l  8(%sp),%d0
    move.l  8,%d1
    move.l  #bus_error,8
    move.l  #2f,bus_recovery_pc
    move.l  %sp,bus_probe_sp
    move.l  #1,bus_probe_armed
    move.b  %d0,(%a0)
    clr.l   bus_probe_armed
    move.l  %d1,8
    moveq   #1,%d0
    rts
2:  clr.l   bus_probe_armed
    move.l  %d1,8
    rts

/* Non-destructive longword RAM probe. Address must be aligned. */
probe_rw32:
    move.l  4(%sp),%a0
    move.l  8,%a1
    move.l  #bus_error,8
    move.l  #3f,bus_recovery_pc
    move.l  %sp,bus_probe_sp
    move.l  #1,bus_probe_armed
    moveq   #1,%d0
    move.l  (%a0),%d1
    move.l  #0x55aa33cc,(%a0)
    cmpi.l  #0x55aa33cc,(%a0)
    bne.s   2f
    move.l  %d1,(%a0)
    bra.s   3f
2:  moveq   #0,%d0
    move.l  %d1,(%a0)
3:  clr.l   bus_probe_armed
    move.l  %a1,8
    rts

/* Feature probes redirect illegal-instruction vector 4 to a local recovery
 * point. Raw opcodes keep this file buildable for the baseline 68000. */
cpu_probe_vbr:
    move.l  0x10,%d1
    move.l  #illegal_probe,0x10
    move.l  #4f,bus_recovery_pc
    moveq   #1,%d0
    .word   0x4e7a,0x0801         /* movec vbr,d0: 68010+ */
4:  move.l  %d1,0x10
    rts
cpu_probe_extb:
    move.l  0x10,%d1
    move.l  #illegal_probe,0x10
    move.l  #5f,bus_recovery_pc
    moveq   #1,%d0
    .word   0x49c0                /* extb.l d0: 68020+ */
5:  move.l  %d1,0x10
    rts

/* Read-only MOVEC probes, ordered by cpu_detect_type from newest to oldest.
 * TC exists on 68030+, URP on 68040+, PCR on 68060. */
cpu_probe_tc:
    move.l  0x10,%d1
    move.l  #illegal_probe,0x10
    move.l  #6f,bus_recovery_pc
    moveq   #1,%d0
    .word   0x4e7a,0x0003         /* movec tc,d0 */
6:  move.l  %d1,0x10
    rts
cpu_probe_urp:
    move.l  0x10,%d1
    move.l  #illegal_probe,0x10
    move.l  #7f,bus_recovery_pc
    moveq   #1,%d0
    .word   0x4e7a,0x0806         /* movec urp,d0 */
7:  move.l  %d1,0x10
    rts
cpu_probe_pcr:
    move.l  0x10,%d1
    move.l  #illegal_probe,0x10
    move.l  #8f,bus_recovery_pc
    moveq   #1,%d0
    .word   0x4e7a,0x0808         /* movec pcr,d0 */
8:  move.l  %d1,0x10
    rts

/* Distinguish a full/EC 68030 from a 68020 when TC is unavailable.  CACR bit
 * 8 controls the 030 data cache and is not implemented by the 020. */
cpu_probe_030_cacr:
    .word   0x4e7a,0x1002         /* movec cacr,d1 */
    move.l  %d1,%d0
    ori.l   #0x00000100,%d0
    .word   0x4e7b,0x0002         /* movec d0,cacr */
    .word   0x4e7a,0x0002         /* movec cacr,d0 */
    .word   0x4e7b,0x1002         /* movec d1,cacr: restore */
    andi.l  #0x00000100,%d0
    sne     %d0
    andi.l  #1,%d0
    rts

/* Side-effect-free boot-time FPU presence probe. */
cpu_probe_fpu:
    move.l  0x2c,%d1
    move.l  #illegal_probe,0x2c
    move.l  #9f,bus_recovery_pc
    moveq   #1,%d0
    .word   0xf280,0x0000         /* fnop */
9:  move.l  %d1,0x2c
    rts
illegal_probe:
    move.l  bus_recovery_pc,2(%sp)
    moveq   #0,%d0
    rte

    .section .bss,"aw",@nobits
    .align 2
bus_recovery_pc: .space 4
bus_probe_sp: .space 4
bus_probe_armed: .space 4
