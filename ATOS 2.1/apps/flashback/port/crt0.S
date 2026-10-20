/* FLASHBK.APP start-up: ATOS application header + self-relocation.
 *
 * ATOS loads a .APP image at an arbitrary address and performs no relocation
 * (system apps are -mpcrel C).  A C++ program cannot be position independent
 * that way: vtables, member-function tables and the engine's string/pointer
 * tables hold absolute addresses.  The image is therefore linked at 0 with
 * --emit-relocs and tools/mkreloc.py stores the offset of every absolute 32-bit
 * word in the .app_reloc table:   .long count, .long offset[count]
 * Here, before any C/C++ code runs, each word gets the load base added.
 *
 * Instruction cache: the 68030 I-cache only holds lines that were executed.
 * Nothing but this loop has run yet, the code after it is reached with a
 * PC-relative bsr.l, and the linker script leaves a 256-byte gap behind this
 * section, so no patched word can sit in a stale cache line.  (The 68030
 * data cache is write-through.)  Requires 68020+ (bsr.l) - the TF536.
 */
#ifndef APP_STACK
#define APP_STACK 32768
#endif
    .section .app_header,"a"
    .globl app_header
app_header:
    .ascii "ASYSAPP0"
    .word 1,32
    .long __app_file_end,__app_memory_end,app_entry,APP_STACK,0

    .section .text.startup,"ax"
    .globl app_entry
app_entry:
    lea app_header(%pc),%a0         /* runtime base: the image is linked at 0 */
    move.l %a0,%d1
    move.l #__app_reloc_start,%d0   /* read before the loop patches this word */
    lea 0(%a0,%d0.l),%a1
    move.l (%a1)+,%d0               /* entry count */
    beq.s 2f
1:  move.l (%a1)+,%d2
    add.l %d1,0(%a0,%d2.l)
    subq.l #1,%d0
    bne.s 1b
2:  bsr.l atos_crt_start            /* C: constructors, arguments, main, exit */
    move.l %d0,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .text.atos_call,"ax"
/* int32_t atos_call(number, d1, d2, a0, a1) - preserves the C callee-saved
   registers (d2 is the only one the trap ABI uses). */
    .globl atos_call
atos_call:
    move.l %d2,-(%sp)
    move.l 8(%sp),%d0
    move.l 12(%sp),%d1
    move.l 16(%sp),%d2
    move.l 20(%sp),%a0
    move.l 24(%sp),%a1
    trap #0
    move.l (%sp)+,%d2
    rts
