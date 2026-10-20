OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_reset)

MEMORY {
    ROM (rx)  : ORIGIN = 0x00e00000, LENGTH = 256K
    /* Keep the reset vectors/system area untouched during early bootstrap. */
    RAM (rwx) : ORIGIN = 0x00000400, LENGTH = 0x003ffc00
}

SECTIONS {
    __rom_start = ORIGIN(ROM);
    __rom_end = ORIGIN(ROM) + LENGTH(ROM);
    .header ORIGIN(ROM) : { KEEP(*(.header)) } > ROM
    .text : { *(.text.startup) *(.text*) *(.rodata*) } > ROM
    .data : { __data_start = .; *(.data*) ; __data_end = .; } > RAM AT> ROM
    __data_load_start = LOADADDR(.data);
    .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON); __bss_end = .; } > RAM
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
    ASSERT(LOADADDR(.data) + SIZEOF(.data) <= ORIGIN(ROM)+LENGTH(ROM)-1,
           "Firmware exceeds 256 KiB ROM")
}
