/* TOS personality (T1): VBR switch, app entry/exit and trap stubs. */
    .section .text,"ax"

    .globl asf_tos_set_vbr
asf_tos_set_vbr:
    move.l 4(%sp),%d0
    .word 0x4e7b,0x0801          /* movec d0,vbr (68010+) */
    rts

    .globl asf_tos_enter
asf_tos_enter:
    /* void asf_tos_enter(void *basepage, void *entry, void *stack_top)
       The TOS program is untrusted: protect the kernel's callee-saved
       registers (D2-D7/A2-A6) and chain the previous kernel SP so nested
       Pexec runs unwind correctly. */
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l asf_tos_kernel_sp,-(%sp)
    move.l %sp,asf_tos_kernel_sp
    move.l 52(%sp),%a3          /* a3 = basepage (TOS convention) */
    move.l 56(%sp),%a1          /* a1 = entry */
    move.l 60(%sp),%a0          /* a0 = stack_top (= p_hitpa - gouser_stack) */
    move.l %a0,%usp             /* user stack */
    move.l %a0,%a6              /* a6 = usp, like EmuTOS gouser */
    move.l 16(%a3),%a5          /* a5 = p_dbase (TOS) */
    move.l 24(%a3),%a4          /* a4 = p_bbase (TOS) */
    moveq #0,%d1
    moveq #0,%d2
    moveq #0,%d3
    moveq #0,%d4
    moveq #0,%d5
    moveq #0,%d6
    moveq #0,%d7
    move.l #0,%a0               /* a0 = 0: a normal (not accessory) program */
    move.l #0,%a2
    move.w #0x0700,%sr          /* supervisor -> user mode, IPL 7 (interrupts masked;
                                   the kernel polls and does not own the TOS VBL vectors) */
    jmp (%a1)

    .globl asf_tos_exit
asf_tos_exit:
    /* Guard: if the saved kernel context is zero (e.g. a fault inside a TOS
       program that probes hardware before any kernel context exists), jumping
       to it would set SP=0 and cause an access at address 0, i.e. a second
       fault and a CPU halt. Never do that: stop cleanly instead. */
    move.l asf_tos_kernel_sp,%d0
    beq.s asf_tos_exit_nosp
    move.l %d0,%sp
    move.l (%sp)+,asf_tos_kernel_sp
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts
asf_tos_exit_nosp:
    stop #0x2700

    .globl asf_tos_trap1
asf_tos_trap1:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea 60(%sp),%a0            /* A0 = SP at the trap = exception frame */
    move.l %usp,%a1            /* A1 = user stack at the trap */
    moveq #0,%d0
    move.w (%a1),%d0           /* stack-based trap: function WORD at (usp) */
    move.l %a1,-(%sp)          /* arg3: usp (arguments follow the function word) */
    move.l %a0,-(%sp)          /* arg2: frame */
    move.l %d0,-(%sp)          /* arg1: fn */
    jsr asf_tos_gemdos
    lea 12(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

    .globl asf_tos_trap13
asf_tos_trap13:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea 60(%sp),%a0            /* A0 = SP at the trap = exception frame */
    move.l %usp,%a1            /* A1 = user stack at the trap */
    moveq #0,%d0
    move.w (%a1),%d0           /* stack-based trap: function WORD at (usp) */
    move.l %a1,-(%sp)          /* arg3: usp (arguments follow the function word) */
    move.l %a0,-(%sp)          /* arg2: frame */
    move.l %d0,-(%sp)          /* arg1: fn */
    jsr asf_tos_bios
    lea 12(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

    .globl asf_tos_trap14
asf_tos_trap14:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea 60(%sp),%a0            /* A0 = SP at the trap = exception frame */
    move.l %usp,%a1            /* A1 = user stack at the trap */
    moveq #0,%d0
    move.w (%a1),%d0           /* stack-based trap: function WORD at (usp) */
    move.l %a1,-(%sp)          /* arg3: usp (arguments follow the function word) */
    move.l %a0,-(%sp)          /* arg2: frame */
    move.l %d0,-(%sp)          /* arg1: fn */
    jsr asf_tos_xbios
    lea 12(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

    .globl asf_tos_trap2
asf_tos_trap2:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    /* GEM AES/VDI call convention (verified against MagiC mt_aes/MAGIXLIB):
         d0 = 0xC8 (AES) or 0x73 (VDI); d1 = parameter block pointer.
       AESPB = {contrl,global,intin,intout,addrin,addrout}
       VDIPB = {contrl,intin,ptsin,intout,ptsout}
       The old stub wrongly read d0..d4 as the five arrays, so every GEM call
       saw garbage (contrl pointed at a small integer) and nothing drew. */
    move.l %d1,%a0              /* parameter block */
    cmpi.w #0xC8,%d0
    beq.s asf_tos_trap2_aes
    cmpi.w #0x73,%d0
    beq.s asf_tos_trap2_vdi
    clr.l %d0                   /* other trap #2 services: report success */
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte
asf_tos_trap2_aes:
    move.l 0(%a0),%d0           /* contrl */
    move.l 8(%a0),%d1           /* intin  */
    move.l 12(%a0),%d3          /* intout */
    move.l #0,%d2
    move.l #0,%d4
    move.l #1,-(%sp)            /* arg6: is_aes */
    move.l %d4,-(%sp)
    move.l %d3,-(%sp)
    move.l %d2,-(%sp)
    move.l %d1,-(%sp)
    move.l %d0,-(%sp)
    jsr asf_tos_aesvdi
    lea 24(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte
asf_tos_trap2_vdi:
    move.l 0(%a0),%d0           /* contrl */
    move.l 4(%a0),%d1           /* intin  */
    move.l 8(%a0),%d2           /* ptsin  */
    move.l 12(%a0),%d3          /* intout */
    move.l 16(%a0),%d4          /* ptsout */
    move.l #0,-(%sp)            /* arg6: is_aes = 0 */
    move.l %d4,-(%sp)
    move.l %d3,-(%sp)
    move.l %d2,-(%sp)
    move.l %d1,-(%sp)
    move.l %d0,-(%sp)
    jsr asf_tos_aesvdi
    lea 24(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

    .globl asf_tos_linea
asf_tos_linea:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea 60(%sp),%a2            /* A2 = exception frame */
    move.l 2(%a2),%a1          /* A1 = stacked PC (points at the Line-A opcode) */
    moveq #0,%d1
    move.w (%a1),%d1           /* opcode 0xA00n */
    andi.w #0x0fff,%d1
    addq.l #2,%a1
    move.l %a1,2(%a2)          /* return after the opcode */
    move.l %a0,-(%sp)          /* arg2: Line-A parameter block */
    move.l %d1,-(%sp)          /* arg1: function number */
    jsr asf_tos_linea_op
    lea 8(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

/* Fault stubs installed only while a TOS program runs: instead of halting
 * the kernel they end the program and return control to the shell. */
    .extern kernel_fault_vector,kernel_fault_pc,kernel_fault_sr,kernel_fault_frame,kernel_fault_regs,kernel_fault_usp
    /* Task 2/3: a TOS fault is also captured with move.l only, so the frame is
       readable from normal context (kernel_fault_report) or the debugger. */
    .macro tos_fault_stub name, vec
    .globl \name
\name:
    move.l #\vec,kernel_fault_vector
    move.l (%sp),kernel_fault_frame
    move.l 4(%sp),kernel_fault_frame+4
    move.l 8(%sp),kernel_fault_frame+8
    move.l 12(%sp),kernel_fault_frame+12
    move.l 16(%sp),kernel_fault_frame+16
    move.l 20(%sp),kernel_fault_frame+20
    move.l 2(%sp),kernel_fault_pc
    move.l (%sp),kernel_fault_sr
    movem.l %d0-%d7/%a0-%a6,kernel_fault_regs
    move.l %usp,%a0            /* user stack at the fault (call chain) */
    move.l %a0,kernel_fault_usp
    move.l #\vec,-(%sp)
    jsr asf_tos_fault
    addq.l #4,%sp
    rte
    .endm

    tos_fault_stub asf_tos_fault2,2
    tos_fault_stub asf_tos_fault3,3
    tos_fault_stub asf_tos_fault4,4
    tos_fault_stub asf_tos_fault5,5
    tos_fault_stub asf_tos_fault6,6
    tos_fault_stub asf_tos_fault7,7
    tos_fault_stub asf_tos_fault8,8
    /* Line-F (vector 11) = coprocessor/FPU instruction. A machine without an
       FPU takes this exception; TOS and MagiC simply skip the instruction so
       the program learns "no FPU" and carries on. Killing the program here
       (like the other stubs) would stop programs at their FPU probe.
       On 68000/68010+ the Line-F frame is SR(2) + PC(4), so SP+2 is the PC. */
    .globl asf_tos_fault11
asf_tos_fault11:
    addq.l #2,2(%sp)
    rte

    .section .bss
    .align 2
asf_tos_kernel_sp:
    .space 4
