OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_reset)

MEMORY {
    ROM (rx)  : ORIGIN = 0x00e00000, LENGTH = 256K
    /* Keep the reset vectors/system area untouched during early bootstrap. */
    RAM (rwx) : ORIGIN = 0x00000400, LENGTH = 0x003ffc00
}

SECTIONS {
    .header ORIGIN(ROM) : { KEEP(*(.header)) } > ROM
    .text : { *(.text.startup) *(.text*) *(.rodata*) } > ROM
    .data : { *(.data*) } > RAM AT> ROM
    .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON); __bss_end = .; } > RAM
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
    ASSERT(LOADADDR(.data) + SIZEOF(.data) <= ORIGIN(ROM)+LENGTH(ROM)-1,
           "Firmware exceeds 256 KiB ROM")
}
