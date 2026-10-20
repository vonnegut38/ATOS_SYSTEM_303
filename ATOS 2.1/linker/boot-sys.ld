OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(boot_entry)

SECTIONS {
    . = 0x00008000;
    __image_start = .;
    .boot_header : { KEEP(*(.boot_header)) }
    .text : { *(.text.startup) *(.text*) *(.rodata*) }
    .data : { *(.data*) }
    __image_file_end = .;
    .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON); __bss_end = .; }
    __image_memory_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
