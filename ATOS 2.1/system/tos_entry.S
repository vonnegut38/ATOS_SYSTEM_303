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
    move.l %usp,%a0             /* save the caller's USP (MOVE USP needs an
                                   address register). A user process such as the
                                   Commander can be the one who asked for the TOS
                                   program, and asf_tos_exit must put it back or
                                   the syscall return faults with
                                   USER_STACK_POINTER_FAULT. */
    move.l %a0,-(%sp)
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l asf_tos_kernel_sp,-(%sp)
    move.l %sp,asf_tos_kernel_sp
    move.l 56(%sp),%a3          /* a3 = basepage (TOS convention) */
    move.l 60(%sp),%a1          /* a1 = entry */
    move.l 64(%sp),%a0          /* a0 = stack_top (= p_hitpa - gouser_stack) */
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
    /* The guest runs in SUPERVISOR mode, but on its OWN stack: the earlier
       supervisor experiment set only the USP, so with S=1 the CPU kept using
       the KERNEL's stack and the program read its basepage from the wrong
       place (A5=0 -> write at $4).  Setting the SSP to the same buffer fixes
       that, and it is what lets ordinary TOS programs execute privileged
       instructions (LDW hit Privilege Violation here).
       Order matters: raise S first, then load the SSP, because the SSP is only
       writable in supervisor mode.  The short window in between uses the
       kernel stack, which is safe for the Timer C interrupt. */
    move.w #0x2500,%sr          /* supervisor mode, IPL 5 */
    move.l %a6,%sp              /* a6 = the program's stack (== USP above) */
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
    move.l (%sp)+,%a0           /* restore the caller's user stack pointer */
    move.l %a0,%usp
    rts
asf_tos_exit_nosp:
    stop #0x2700

    .globl asf_tos_trap1
asf_tos_trap1:
1:  movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea 60(%sp),%a0            /* A0 = SP at the trap = exception frame */
    move.l %usp,%a1            /* user caller: function WORD on USP */
    btst #5,(%a0)              /* saved SR: supervisor caller? */
    beq 1f
    lea 6(%a0),%a1             /* 68000 short exception frame */
    tst.w 0x59e               /* _longframe: 68010+ adds format/vector */
    beq 1f
    addq.l #2,%a1
1:
    moveq #0,%d0
    move.w (%a1),%d0           /* stack-based trap: function WORD at (usp) */
    move.l %a1,-(%sp)          /* arg3: usp (arguments follow the function word) */
    move.l %a0,-(%sp)          /* arg2: frame */
    move.l %d0,-(%sp)          /* arg1: fn */
    jsr asf_tos_gemdos
    lea 12(%sp),%sp
    move.l %d0,(%sp)
    jsr tos_composite_work      /* deferred refresh on each GEMDOS call */
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

    .globl asf_tos_trap13
asf_tos_trap13:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea 60(%sp),%a0            /* A0 = SP at the trap = exception frame */
    move.l %usp,%a1            /* user caller: function WORD on USP */
    btst #5,(%a0)              /* saved SR: supervisor caller? */
    beq 1f
    lea 6(%a0),%a1             /* 68000 short exception frame */
    tst.w 0x59e               /* _longframe: 68010+ adds format/vector */
    beq 1f
    addq.l #2,%a1
1:
    moveq #0,%d0
    move.w (%a1),%d0           /* stack-based trap: function WORD at (usp) */
    move.l %a1,-(%sp)          /* arg3: usp (arguments follow the function word) */
    move.l %a0,-(%sp)          /* arg2: frame */
    move.l %d0,-(%sp)          /* arg1: fn */
    jsr asf_tos_bios
    lea 12(%sp),%sp
    move.l %d0,(%sp)
    jsr tos_composite_work      /* deferred refresh on each BIOS call */
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

    .globl asf_tos_trap14
asf_tos_trap14:
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    lea 60(%sp),%a0            /* A0 = SP at the trap = exception frame */
    move.l %usp,%a1            /* user caller: function WORD on USP */
    btst #5,(%a0)              /* saved SR: supervisor caller? */
    beq 1f
    lea 6(%a0),%a1             /* 68000 short exception frame */
    tst.w 0x59e               /* _longframe: 68010+ adds format/vector */
    beq 1f
    addq.l #2,%a1
1:
    moveq #0,%d0
    move.w (%a1),%d0           /* stack-based trap: function WORD at (usp) */
    move.l %a1,-(%sp)          /* arg3: usp (arguments follow the function word) */
    move.l %a0,-(%sp)          /* arg2: frame */
    move.l %d0,-(%sp)          /* arg1: fn */
    jsr asf_tos_xbios
    lea 12(%sp),%sp
    move.l %d0,(%sp)
    jsr tos_composite_work      /* deferred refresh on each XBIOS call */
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
    move.l 20(%a0),%d7          /* addrout */
    move.l 16(%a0),%d6          /* addrin  */
    move.l 4(%a0),%d5           /* global  */
    move.l 12(%a0),%d4          /* intout  */
    move.l 8(%a0),%d3           /* intin   */
    move.l 0(%a0),%d2           /* contrl  */
    move.l %d7,-(%sp)           /* arg9 : addrout */
    move.l %d6,-(%sp)           /* arg8 : addrin */
    move.l %d5,-(%sp)           /* arg7 : global */
    move.l #1,-(%sp)            /* arg6 : is_aes */
    move.l #0,-(%sp)            /* arg5 : ptsout */
    move.l %d4,-(%sp)           /* arg4 : intout */
    move.l #0,-(%sp)            /* arg3 : ptsin */
    move.l %d3,-(%sp)           /* arg2 : intin */
    move.l %d2,-(%sp)           /* arg1 : contrl */
    jsr asf_tos_aesvdi
    lea 36(%sp),%sp
    move.l %d0,(%sp)
    jsr tos_composite_work      /* deferred refresh after each GEM call */
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte
asf_tos_trap2_vdi:
    move.l 0(%a0),%d0           /* contrl */
    move.l 4(%a0),%d1           /* intin  */
    move.l 8(%a0),%d2           /* ptsin  */
    move.l 12(%a0),%d3          /* intout */
    move.l 16(%a0),%d4          /* ptsout */
    move.l #0,-(%sp)            /* arg9 : is_aes = 0 */
    move.l #0,-(%sp)            /* arg8 : addrout */
    move.l #0,-(%sp)            /* arg7 : addrin */
    move.l #0,-(%sp)            /* arg6 : global */
    move.l %d4,-(%sp)           /* arg5 : ptsout */
    move.l %d3,-(%sp)           /* arg4 : intout */
    move.l %d2,-(%sp)           /* arg3 : ptsin */
    move.l %d1,-(%sp)           /* arg2 : intin */
    move.l %d0,-(%sp)           /* arg1 : contrl */
    jsr asf_tos_aesvdi
    lea 36(%sp),%sp
    move.l %d0,(%sp)
    jsr tos_composite_work      /* deferred refresh after each GEM call */
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
    /* A_INIT (0xA000) must also return a0 = Line_A base, a1 = font table and
       a2 = function table; the C handler only returns d0.  The registers were
       saved by the movem above, so patch their slots (d0..d7 = 0..28,
       a0 = 32, a1 = 36, a2 = 40). */
    tst.l kernel_linea_ainit
    beq.s 1f
    clr.l kernel_linea_ainit
    move.l kernel_linea_base,%a0
    move.l %a0,32(%sp)
    move.l kernel_linea_font,%a1
    move.l %a1,36(%sp)
    move.l kernel_linea_tab,%a2
    move.l %a2,40(%sp)
1:  move.l %d0,(%sp)
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rte

/* Fault stubs installed only while a TOS program runs: instead of halting
 * the kernel they end the program and return control to the shell. */
    .extern kernel_fault_vector,kernel_fault_pc,kernel_fault_sr,kernel_fault_frame,kernel_fault_regs,kernel_fault_usp
    .extern tos_composite_work
    /* Capture only the common six-byte SR/PC prefix; do not overread a
       short hardware frame. Register capture below preserves the caller. */
    .macro tos_fault_stub name, vec
    .globl \name
\name:
    /* A TOS program that installs its own exception vector expects to receive
       the exception ITSELF: on real TOS the vector points straight at the
       program's handler, so nothing sits in between and the CPU frame is
       exactly what the handler's rte needs.  Our stub runs first, but at this
       point the frame is still untouched on the stack, so when the vector slot
       no longer holds one of our stubs we hand the exception over with a plain
       jump.  Only d0/a0 are clobbered by this check (documented); the frame,
       the rest of the register set and the stack position are as the CPU left
       them. */
    movem.l %d0-%d1/%a0-%a1,-(%sp)
    move.l #(\vec)*4,%a1
    move.l (%a1),%a0
    move.l %a0,%d1
    tst.l %d1
    beq 9f
    lea tos_fault_stub_table,%a1
    moveq #7,%d0
8:  cmp.l (%a1)+,%a0
    beq 9f
    dbra %d0,8b
    move.l %a0,%d0
    and.l #1,%d0
    bne 9f                        /* odd address: not a usable handler */
    cmp.l #0x100,%a0
    bls 9f                        /* inside the vector page: refuse */
    /* Hand the exception to the program's handler without disturbing a single
       register: the target goes on the stack BELOW the saved registers, they
       are restored from offset 4, and rts consumes the target.  SP then points
       exactly at the CPU's exception frame again - which is what the handler's
       rte expects. */
    move.l %a0,-(%sp)
    movem.l 4(%sp),%d0-%d1/%a0-%a1
    rts
9:  movem.l (%sp)+,%d0-%d1/%a0-%a1
    move.l #\vec,kernel_fault_vector
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
    .section .rodata
    .align 2
tos_fault_stub_table:
    .long asf_tos_fault2,asf_tos_fault3,asf_tos_fault4,asf_tos_fault5
    .long asf_tos_fault6,asf_tos_fault7,asf_tos_fault8,asf_tos_fault11
    .text
    /* Line-F (vector 11) = coprocessor/FPU instruction. A machine without an
       FPU takes this exception; TOS and MagiC simply skip the instruction so
       the program learns "no FPU" and carries on. Killing the program here
       (like the other stubs) would stop programs at their FPU probe.
       On 68000/68010+ the Line-F frame is SR(2) + PC(4), so SP+2 is the PC. */
    .globl asf_tos_fault11
asf_tos_fault11:
    /* Same courtesy for the FPU/Line-F vector: a program with its own FPU
       emulator handler must see the exception before we silently skip the
       instruction. */
    movem.l %d0-%d1/%a0-%a1,-(%sp)
    move.l #(11)*4,%a1
    move.l (%a1),%a0
    move.l %a0,%d1
    tst.l %d1
    beq 9f
    lea tos_fault_stub_table,%a1
    moveq #7,%d0
8:  cmp.l (%a1)+,%a0
    beq 9f
    dbra %d0,8b
    move.l %a0,%d0
    and.l #1,%d0
    bne 9f
    cmp.l #0x100,%a0
    bls 9f
    movem.l (%sp)+,%d0-%d1/%a0-%a1
    move.l %a0,-(%sp)                /* target below the saved registers */
    movem.l 4(%sp),%d0-%d1/%a0-%a1
    rts                              /* the program's FPU handler, frame intact */
9:  movem.l (%sp)+,%d0-%d1/%a0-%a1    /* symmetric with the save above: 16 bytes */
    addq.l #2,2(%sp)
    rte

/* XBIOS Supexec(function): call a user callback in supervisor mode.  The TOS
   ABI leaves the callback argument in d3, so asf_tos_supexec takes the user's
   saved d3 and loads it before the call.  A few GEM applications (SysInfo is
   one) read system/AES state through Supexec instead of a plain load. */
    .globl asf_tos_supexec
asf_tos_supexec:
    move.l 4(%sp),%a0           /* guest function */
    move.l 8(%sp),%d0           /* guest d3 */
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.w %sr,-(%sp)
    move.l %d0,%d3
    /* Permit MFP level 6 (Timer C/input), keep unowned VBL level 4 masked.
       Restore both the caller's mask and every C callee-saved register. */
    move.w #0x2500,%sr
    jsr (%a0)
    move.w (%sp)+,%sr
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

/* Call a TOS vector (mousevec) in supervisor mode with the IKBD packet in d0.
   The target address goes through a slot so no guest register is disturbed;
   serialised by tos_input_busy, so a single slot is enough. */
    .globl asf_tos_call_mousevec
asf_tos_call_mousevec:
    /* void asf_tos_call_mousevec(void *fn, uint8_t *packet)
       TOS calls mousevec with A0 pointing at the three packet bytes
       (header = 0xF8|buttons, then signed dx and dy), and the same pointer is
       pushed so a C-compiled handler sees it as its first argument.  Passing
       the packed number instead made LDW dereference garbage and never return,
       which showed up as tos_input_busy stuck at 1. */
    move.l 4(%sp),kernel_mousevec_target
    move.l 8(%sp),kernel_mousevec_packet
    movem.l %d0-%d7/%a0-%a6,-(%sp)
    move.l kernel_mousevec_target,%a1
    move.l kernel_mousevec_packet,%a0
    move.l %a0,-(%sp)                /* C handlers take it as the argument */
    jsr (%a1)
    addq.l #4,%sp
    movem.l (%sp)+,%d0-%d7/%a0-%a6
    rts

/* A valid default mousevec: TOS always has one, and a program that saves the
   old vector and chains to it must not jump to address 0. */
    .globl asf_tos_default_mousevec
asf_tos_default_mousevec:
    rts

/* Reported by Setexc as the "previous handler" for vectors the kernel does not
   own, so a program that chains to it never jumps to address 0. */
    .globl asf_tos_just_rte
asf_tos_just_rte:
    rte

    .section .bss
    .align 2
    .globl kernel_mousevec_target
kernel_mousevec_target:
    .space 4
    .align 2
    .globl kernel_mousevec_packet
kernel_mousevec_packet:
    .space 4
    .align 2
    .globl kdbg_super_n
kdbg_super_n:
    .space 4
    .align 2
    .globl kdbg_super_sup_n
kdbg_super_sup_n:
    .space 4
    .align 2
    .globl asf_tos_kernel_sp
asf_tos_kernel_sp:
    .space 4

/* Swap supervisor stacks at an explicit cooperative boundary. Interrupts are
 * masked by C. USP and callee-saved registers belong to each domain. */
    .section .text,"ax"
    .globl tos_async_switch
tos_async_switch:
    movem.l %d2-%d7/%a2-%a6,-(%sp)
    move.l %usp,%a0
    move.l %a0,-(%sp)
    move.l 52(%sp),%a0
    move.l 56(%sp),%a1
    move.l %sp,(%a0)
    move.l %a1,%sp
    move.l (%sp)+,%a0
    move.l %a0,%usp
    movem.l (%sp)+,%d2-%d7/%a2-%a6
    rts

    .globl tos_async_read_cacr
tos_async_read_cacr:
    .word 0x4e7a,0x0002
    rts
    .globl tos_async_write_cacr
tos_async_write_cacr:
    move.l 4(%sp),%d0
    .word 0x4e7b,0x0002
    rts

    .globl tos_async_read_vbr
tos_async_read_vbr:
    .word 0x4e7a,0x0801
    rts
