#ifndef ASF_ICONS_BLOB_H
#define ASF_ICONS_BLOB_H
#include <stdint.h>
/* Format pliku \SYSTEM\ICONS.DAT: ikony pulpitu jako dane w TT-RAM zamiast
   254 KB .rodata w obrazie jadra (patrz RAPORT-EKRAN-20261015.txt, sekcja 17).
   Wszystkie pola 32-bit sa w kolejnosci BIG-ENDIAN (natywnej dla 68000), a
   offsety sa wzgledne wobec poczatku bloba - plik mozna wiec polozyc pod
   dowolnym adresem bez relokacji.

   WERSJA 5 (od 19.10.2026 - wybor rozmiaru ikon 32/48/64):
     naglowek (20 B), paleta (ICON_PAL_COLORS x uint16), tabela sekcji
     icons_section_t[sizes], potem STREAMY sekcji (LZSS).
     Kazda sekcja to SAMODZIELNY blob v1 spakowany LZSS: naglowek v1, wpisy
     [count], wlasna kopia palety, maski i piksele.  Dzieki temu jadro rozwija
     tylko wybrany rozmiar (RAM = jedna sekcja, nie trzy) i czyta go tym samym
     kodem co pliki v1.  Paleta jest ta sama we wszystkich sekcjach (jeden
     zestaw wpisow RAMDAC), wiec zmiana rozmiaru nie przeprogramowuje kolorow.

   UKLAD v1/v2/v3/v4 (starsze pliki, nadal czytane):
     naglowek, wpisy[count], paleta, potem kolejno dla kazdej ikony: maska
     (ICON_MASK_BYTES), piksele (ICON_PIXEL_BYTES).  W v2/v3/v4 cala reszta
     pliku po wpisach jest jednym spakowanym strumieniem (RLE/PackBits/LZSS),
     ktory po rozwinieciu daje uklad v1. */
#define ICONS_BLOB_MAGIC 0x49434F4EUL      /* 'ICON' */
#define ICONS_BLOB_VERSION 1u               /* uklad pojedynczego rozmiaru */
#define ICONS_BLOB_VERSION_SIZES 5u         /* uklad z sekcjami rozmiarow */
#define ICONS_BLOB_MAX_COUNT 256u
#define ICONS_SECTION_STRUCT 28u            /* sizeof(icons_section_t) */
typedef struct {
    uint32_t magic;         /* ICONS_BLOB_MAGIC */
    uint32_t version;       /* ICONS_BLOB_VERSION (1..4) lub 5 */
    uint32_t count;         /* liczba ikon (v1..v4) lub liczba sekcji (v5) */
    uint32_t palette_off;   /* offset tablicy uint16[ICON_PAL_COLORS] */
    uint32_t entries_off;   /* v1..v4: wpisy[count]; v5: sekcje[sizes] */
} icons_blob_header_t;
typedef struct {
    uint32_t mask_off;      /* offset maski (mask_bytes z sekcji) */
    uint32_t pixels_off;    /* offset pikseli (pixel_bytes z sekcji) */
} icons_blob_entry_t;
/* v5: jedna sekcja = jeden rozmiar ikony. */
typedef struct {
    uint32_t px;            /* 32, 48, 64 ... */
    uint32_t count;         /* liczba ikon w sekcji */
    uint32_t mask_bytes;    /* px*px/8 (wiersze po px bitow) */
    uint32_t pixel_bytes;   /* px*px (1 bajt = indeks palety) */
    uint32_t raw_size;      /* rozmiar rozwietego bloba v1 */
    uint32_t blob_off;      /* offset strumienia LZSS w pliku */
    uint32_t blob_size;     /* dlugosc strumienia LZSS */
} icons_section_t;
#endif
