/* Minimalne funkcje pamieciowe dla JADRA.
 *
 * Jadro jest budowane z -ffreestanding -nostdlib i nie linkuje libc, a jednak
 * GCC potrafi wygenerowac wywolanie memcpy/memset z wlasnego kodu: od GCC 12
 * wlaczona jest optymalizacja -ftree-loop-distribute-patterns (petla kopiujaca
 * -> memcpy), a duze przypisania struktur tez koncza sie wywolaniem memcpy.
 * Na toolchainie Homebrew (m68k-elf) te wywolania nie powstawaly, ale na
 * m68k-linux-gnu-gcc 13 (CI na Ubuntu) link jadra padal na
 * "undefined reference to `memcpy`" w kgfx_self_test.
 *
 * Dlatego jadro ma wlasne, male implementacje. Sa linked tylko wtedy, gdy
 * ktos ich naprawde uzyje (-Wl,--gc-sections i tak usunie nieuzywane).
 * #pragma blokuje petle->memcpy w TYCH funkcjach, zeby nie bylo rekurencji.
 */
#pragma GCC optimize ("no-tree-loop-distribute-patterns")

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *to = (unsigned char *)destination;
    const unsigned char *from = (const unsigned char *)source;
    while (count--) *to++ = *from++;
    return destination;
}

void *memmove(void *destination, const void *source, size_t count)
{
    unsigned char *to = (unsigned char *)destination;
    const unsigned char *from = (const unsigned char *)source;
    if (to == from || count == 0u) return destination;
    if (to < from) {
        while (count--) *to++ = *from++;
    } else {
        to += count;
        from += count;
        while (count--) *--to = *--from;
    }
    return destination;
}

void *memset(void *destination, int value, size_t count)
{
    unsigned char *to = (unsigned char *)destination;
    while (count--) *to++ = (unsigned char)value;
    return destination;
}

int memcmp(const void *left, const void *right, size_t count)
{
    const unsigned char *a = (const unsigned char *)left;
    const unsigned char *b = (const unsigned char *)right;
    while (count--) {
        if (*a != *b) return (int)*a - (int)*b;
        ++a;
        ++b;
    }
    return 0;
}
