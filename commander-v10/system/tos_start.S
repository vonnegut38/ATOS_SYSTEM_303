/* Freestanding C entry for a TOS .PRG (A2).  The tool prepends the 0x601A
 * header, so this only has to call the C body and Pterm. */
    .section .text.startup,"ax"
    .globl _tos_start
_tos_start:
    bsr     _tos_main
    move.w  #0,-(%sp)
    move.w  #0x4c,%d0
    trap    #1                  /* Pterm(0) */
    rts
