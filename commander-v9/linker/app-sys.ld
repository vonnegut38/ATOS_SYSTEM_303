OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(app_entry)
SECTIONS {
    . = 0;
    .app_header : { KEEP(*(.app_header)) }
    .text : { *(.text.startup) *(.text*) }
    __app_executable_end = .;
    .rodata : { *(.rodata*) }
    .data : { __app_writable_start = .; *(.data*) }
    .app_policy : {
        LONG(0x41505732)
        LONG(__app_executable_end)
        LONG(__app_writable_start)
    }
    __app_file_end = .;
    .bss (NOLOAD) : { *(.bss*) *(COMMON) }
    __app_memory_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
