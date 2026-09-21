/*
 * Atari ROMs do not start with the 68000 SSP/PC vector pair.  The MMU/GLUE
 * reset logic enters the ROM's TOS header, whose first instruction branches
 * to the conventional entry point at offset 0x30.  Hatari validates and boots
 * ROM images using this layout too.
 */
    .section .header,"ax"
    .globl _reset
_rom_header:
    bra.s   _reset                /* 0x00: normally 0x602e */
#ifdef ROM_ST
    .word   0x0104                /* 0x02: ST/STE/Mega ST compatible header */
#else
    .word   0x0206                /* 0x02: TOS-compatible version for MSTE */
#endif
    .long   _reset                /* 0x04: reset handler */
    .long   _rom_header           /* 0x08: OS base (_sysbase) */
    .long   __bss_end             /* 0x0c: first byte beyond resident RAM */
    .long   _reset                /* 0x10: reserved */
    .long   0                     /* 0x14: GEM MUPB pointer (none) */
    .long   0x09032026            /* 0x18: build date, informational */
    .word   0                     /* 0x1c: PAL/country configuration */
    .word   0x5d23                /* 0x1e: DOS date 2026-09-03 */
    .long   0                     /* 0x20: GEMDOS root (not provided) */
    .long   0                     /* 0x24: keyboard shift state pointer */
    .long   0                     /* 0x28: current process pointer */
    .ascii  "ASFW"                /* 0x2c: private firmware signature */
    .org    0x30

_reset:
    move.w  #0x2700,%sr           /* supervisor mode, interrupts masked */
    /* The detector's short-lived calls stay in the always-present low bank;
     * its final return uses A6 because applying the final map can invalidate
     * ordinary return addresses elsewhere. */
    lea     0x00008000,%sp
    lea     1f,%a6
    jmp     ram_detect_early
1:
    /* Some emulators and floating buses can make an absent bank resemble a
     * mirrored one. Install the probe trampoline while the temporary low
     * stack is valid, then validate the detected ceiling in 512 KiB steps. */
    jsr     exception_install
4:  tst.l   %d5
    beq.s   5f
    move.l  %d5,%d0
    subq.l  #4,%d0
    move.l  %d0,-(%sp)
    jsr     probe_rw32
    addq.l  #4,%sp
    tst.l   %d0
    bne.s   5f
    subi.l  #0x80000,%d5
    bra.s   4b
5:
    move.l  %d5,%d0
    subi.l  #0x1000,%d0           /* stack below the detected RAM ceiling */
    move.l  %d0,%sp
    lea     __bss_start,%a0
    lea     __bss_end,%a1
1:  cmpa.l  %a1,%a0
    bhs.s   2f
    clr.b   (%a0)+
    bra.s   1b
2:
    move.l  %d5,early_ram_bytes
    jsr     exception_install
    jsr     firmware_main
3:  stop    #0x2700
    bra.s   3b
