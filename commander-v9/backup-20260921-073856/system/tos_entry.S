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
    move.l 52(%sp),%a0
    move.l 56(%sp),%a1
    move.l 60(%sp),%sp
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
    move.l %usp,%a1            /* A1 = user stack at the trap (user-mode args) */
    move.l %a1,-(%sp)          /* arg3: usp */
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
    lea 60(%sp),%a0
    move.l %usp,%a1
    move.l %a1,-(%sp)
    move.l %a0,-(%sp)
    move.l %d0,-(%sp)
    jsr asf_tos_bios
    lea 12(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

    .globl asf_tos_trap14
asf_tos_trap14:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea 60(%sp),%a0
    move.l %usp,%a1
    move.l %a1,-(%sp)
    move.l %a0,-(%sp)
    move.l %d0,-(%sp)
    jsr asf_tos_xbios
    lea 12(%sp),%sp
    move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

    .globl asf_tos_trap2
asf_tos_trap2:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    move.l %d4,-(%sp)          /* ptsout */
    move.l %d3,-(%sp)          /* intout */
    move.l %d2,-(%sp)          /* ptsin  */
    move.l %d1,-(%sp)          /* intin  */
    move.l %d0,-(%sp)          /* contrl */
    jsr asf_tos_aesvdi
    lea 20(%sp),%sp
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
    .macro tos_fault_stub name, vec
    .globl \name
\name:
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
