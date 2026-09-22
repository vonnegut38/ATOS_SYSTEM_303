    .section .module_header,"a"
    .ascii  "ASYSMOD0"
    .word   3
    .word   32
    .long   __image_file_end
    .long   0
    .long   core_module_entry
    .long   __image_memory_end
    .long   0

    .section .text.startup,"ax"
    .globl core_module_entry
core_module_entry:
    move.l 4(%sp),%a0
    cmpi.l #0x4b4d4150,(%a0)
    bne.s 1f
    cmpi.w #3,4(%a0)
    bne.s 1f
    cmpi.w #32,6(%a0)
    blo.s 1f
    pea core_service(%pc)
    move.l #1,-(%sp)
    move.l #0x434f5245,-(%sp)
    move.l 24(%a0),%a1
    jsr (%a1)
    lea 12(%sp),%sp
    rts
1:  moveq #0,%d0
    rts

    .align 2
core_service:
    .long 0x434f5245
    .word 1
    .word 8
