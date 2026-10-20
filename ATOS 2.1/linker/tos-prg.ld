OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_tos_start)

SECTIONS {
    /* Standard GEMDOS PRG: the relocator adds the TEXT base (basepage+0x100)
       to every absolute longword, so the text must be linked from address 0.
       Its absolute longwords are then plain offsets from the start of text. */
    . = 0;
    .text : { *(.text.startup) *(.text*) *(.rodata*) }
    .data : { *(.data*) }
    .bss (NOLOAD) : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) }
}
