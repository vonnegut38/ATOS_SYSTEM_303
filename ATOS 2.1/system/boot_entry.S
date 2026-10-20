    .section .boot_header,"a"
    .ascii  "ASYSBOOT"
    .word   1
    .word   32
    .long   __image_file_end-0x00008000
    .long   0x00008000
    .long   boot_entry-0x00008000
    .long   __image_memory_end-0x00008000
    .long   0                       /* patched byte-sum */

    .section .text.startup,"ax"
    .globl boot_entry
boot_entry:
    lea __bss_start,%a0
    lea __bss_end,%a1
1:  cmpa.l %a1,%a0
    bhs.s 2f
    clr.b (%a0)+
    bra.s 1b
2:
    jmp boot_main
