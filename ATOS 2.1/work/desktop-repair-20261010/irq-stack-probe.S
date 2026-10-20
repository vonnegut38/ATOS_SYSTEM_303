/* A self-extractor-style trampoline below SSP. IRQs must not write software
 * register saves there; only the CPU's eight-byte exception frame fits. */
    .text
    .globl _start
_start:
    move.l %sp,%a1
    suba.l #24,%a1
    move.l %a1,%a3
    lea trampoline(%pc),%a0
    moveq #5,%d0
copy:
    move.w (%a0)+,(%a1)+
    dbra %d0,copy
    lea done(%pc),%a2
    jmp (%a3)
trampoline:
    move.l #800000,%d0
1:  subq.l #1,%d0
    bne.s 1b
    jmp (%a2)
done:
    clr.w -(%sp)
    trap #1
