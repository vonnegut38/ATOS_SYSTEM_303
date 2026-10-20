    .section .module_header,"a"
    .ascii "ASYSMOD0"
    .word 3
    .word 32
    .long __image_file_end
    .long 0
    .long vofa_module_entry
    .long __image_memory_end
    .long 0

    .section .text.startup,"ax"
    .globl vofa_module_entry
vofa_module_entry:
    /* Tail-call the C part with the module API pointer still in 4(%sp). */
    lea vofa_module_main(%pc),%a0
    jmp (%a0)
