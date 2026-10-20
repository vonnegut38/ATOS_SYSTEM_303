OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(app_entry)
SECTIONS {
    . = 0;
    .app_header : { KEEP(*(.app_header)) }
    .text : { *(.text.startup) *(.text*) }
    __app_executable_end = .;
    .rodata : { *(.rodata*) }
    /* Zmienne okna plikow (fb_*) MUSZA lezec blisko kodu, ktory ich uzywa:
       limit PC-wzgledny na 68000 to +/-32 KB, a .bss jest na samym koncu
       (za polityka), co dawalo ~32 800 B od .text.fb_page_calc i link konczyl
       sie "relocation truncated to fit".  Sa bezwartosciowe na starcie, wiec
       leza w obszarze zapisywalnym (data), ale PRZED polityka - blizej kodu
       o rozmiar .bss.  Nadal sa zerowane: leza w sekcji plikowej zainicjowanej
       zerami, wiec wczytywacz widzi je jako zapisywalne i wyzerowane. */
    .data : {
        __app_writable_start = .;
        *(.data*)
        /* WZORZEC, nie lista nazw: wszystkie zmienne okna plikow (fb_*).
           Poprzednia wersja wymieniala szesc nazw i przeoczyla fb_sel, fb_path,
           fb_title, fb_click_tick i fb_open - przy pierwszym wiekszym dodatku
           do kodu fb_sel znow przekroczyl zasieg.  Wzorzec zamyka te klase
           bledow takze na przyszlosc. */
        *(.bss.fb_*)

    }
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
