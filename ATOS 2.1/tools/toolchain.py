"""Rozwiazywanie nazw narzedzi binutils dla roznych toolchainow.

Makefile przekazuje CROSS (np. "m68k-elf-" albo "m68k-linux-gnu-"), ale skrypty
bywaja uruchamiane takze recznie.  Kolejnosc: CROSS z srodowiska, potem
m68k-elf- (Homebrew), potem m68k-linux-gnu- (Debian/Ubuntu, uzywane w CI).
Dzieki temu ten sam skrypt dziala lokalnie i w GitHub Actions bez edycji.
"""
import os
import shutil


def tool(name):
    """Zwraca nazwe narzedzia (np. 'objdump') z dostepnym prefiksem."""
    prefix = os.environ.get("CROSS", "")
    for candidate in (prefix, "m68k-elf-", "m68k-linux-gnu-"):
        if candidate and shutil.which(candidate + name):
            return candidate + name
    return name
