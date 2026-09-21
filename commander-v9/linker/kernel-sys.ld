OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(kernel_entry)
SECTIONS {
    . = 0x00080000;
    __image_start = .;
    .kernel_header : { KEEP(*(.kernel_header)) }
    .text : { *(.text.startup) *(.text*) *(.rodata*) }
    .data : { *(.data*) }
    __image_file_end = .;
    .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON); __bss_end = .; }
    __image_memory_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
