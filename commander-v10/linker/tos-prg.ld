OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_tos_start)

SECTIONS {
    /* The loader copies the text to basepage+0x100, so link the text there
       and its absolute longwords need nothing more than +load_address. */
    . = 0x100;
    .text : { *(.text.startup) *(.text*) *(.rodata*) }
    .data : { *(.data*) }
    .bss (NOLOAD) : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) }
}
