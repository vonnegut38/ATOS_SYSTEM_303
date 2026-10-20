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
    .long   0                     /* 0x14: ROM checksum, patched post-link (0 = unpatched) */
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
    /* POST on the MIDI ACIA (0xfffffc06). Its 31250 baud clock is fixed in
       hardware, so no MFP or baud programming is needed and a PC/MIDI adapter
       can watch bring-up even when there is no working video. */
    move.b  #3,0xfffffc06
    move.b  #0x16,0xfffffc06
    move.b  #0x41,0xfffffc06      /* 'A' BIOS entry */
    /* 0x0420 survives a reset because RAM is not cleared. A stale magic means
       the previous boot did not finish cleanly: report it, then always clear
       the cell so the next boot starts from a known state. */
    cmpi.l  #0x752019f3,0x0420
    bne.s   0f
    move.b  #0x57,0xfffffc06      /* 'W' warm reset */
    bra.s   1f
0:  move.b  #0x43,0xfffffc06      /* 'C' cold boot */
1:  clr.l   0x0420
    /* Bring the keyboard ACIA up next: a key held at power-on
     * (the Alt video failsafe) must survive until the ROM polls for it. Same
     * sequence as ikbd_init(); two byte stores, no stack, no calls. */
    move.b  #3,0xfffffc00
    move.b  #0x96,0xfffffc00
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
    /* Copy .data from ROM to RAM. The startup used to clear BSS only, so any
     * initialised static silently read as zero (the first ROM-side console
     * screen pointer did, and looped on a bus error at address 0). */
    lea     __data_load_start,%a0
    lea     __data_start,%a1
    lea     __data_end,%a2
6:  cmpa.l  %a2,%a1
    bhs.s   7f
    move.b  (%a0)+,(%a1)+
    bra.s   6b
7:
    move.l  %d5,early_ram_bytes
    move.b  #0x52,0xfffffc06      /* 'R' RAM ceiling detected */
    move.l  %d5,%d0
    lsr.l   #8,%d0                /* 68000 allows immediate shifts of 1..8 only */
    lsr.l   #8,%d0
    lsr.l   #4,%d0                /* total >>20 = detected RAM in MiB */
    move.b  %d0,0xfffffc06
    jsr     exception_install
    move.b  #0x46,0xfffffc06      /* 'F' entering firmware_main */
    jsr     firmware_main
3:  stop    #0x2700
    bra.s   3b
