OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(vfs_module_entry)
SECTIONS {
    . = 0;
    .module_header : { KEEP(*(.module_header)) }
    .text : { *(.text.startup) *(.text*) *(.rodata*) }
    .data : { *(.data*) }
    __image_file_end = .;
    .bss (NOLOAD) : { *(.bss*) *(COMMON) }
    __image_memory_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
