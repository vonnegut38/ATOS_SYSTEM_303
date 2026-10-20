/* FLASHBK.APP image layout (ATOS ASYSAPP0, see linker/app-sys.ld).
   Linked at 0 and relocated by crt0.S from the .app_reloc table. */
OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(app_entry)
SECTIONS {
    . = 0;
    .app_header : { KEEP(*(.app_header)) }
    .text : {
        KEEP(*(.text.startup))
        /* keep relocated code out of the start-up code's cache lines */
        . = ALIGN(16);
        . = . + 256;
        *(.text .text.*)
    }
    __app_executable_end = .;
    .rodata : {
        *(.rodata .rodata.*)
        *(.data.rel.ro .data.rel.ro.*)
    }
    .data : {
        __app_writable_start = .;
        . = ALIGN(4);
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
        __ctors_start = .;
        KEEP(*(SORT(.ctors.*)))
        KEEP(*(.ctors))
        __ctors_end = .;
        *(.data .data.*)
        *(.data.rel .data.rel.*)
        *(.got .got.*)
    }
    .app_reloc : {
        . = ALIGN(4);
        __app_reloc_start = .;
        KEEP(*(.app_reloc))
    }
    .app_policy : {
        LONG(0x41505732)
        LONG(__app_executable_end)
        LONG(__app_writable_start)
    }
    __app_file_end = .;
    .bss (NOLOAD) : {
        . = ALIGN(4);
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
    }
    __app_memory_end = .;
    /DISCARD/ : {
        *(.comment) *(.note*) *(.eh_frame*) *(.gcc_except_table*)
        *(.fini_array*) *(.dtors*) *(.jcr)
    }
}
