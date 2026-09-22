    .section .app_header,"a"
    .ascii "ASYSAPP0"
    .word 1,32
#ifndef APP_STACK
#define APP_STACK 8192
#endif
    .long __app_file_end,__app_memory_end,app_entry,APP_STACK,0
    .section .text.startup,"ax"
    .globl app_entry
app_entry:
    bsr app_main
    move.l %d0,%d1
    moveq #0,%d0
    trap #0
    illegal
    .section .text,"ax"
    .globl gui_call
/* C ABI: call(number, d1, d2, a0, a1). Preserve all C callee-saved regs. */
gui_call:
    move.l %d2,-(%sp)
    move.l 8(%sp),%d0
    move.l 12(%sp),%d1
    move.l 16(%sp),%d2
    move.l 20(%sp),%a0
    move.l 24(%sp),%a1
    trap #0
    move.l (%sp)+,%d2
    rts
