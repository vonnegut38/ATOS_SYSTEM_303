# T2 — desktop TOS w oknie (jak odpalić)

Nakładka T2 pozwala uruchamiać programy TOS tak, jakby działały na własnym
ekranie: każdy program dostaje **prywatny bufor 32 KB** (Physbase), a kompozytor
jądra wkleja go do okna na prawdziwym ekranie ET4000 (8 bpp) z belką tytułu,
tłem pulpitu i paskiem statusu.

Tryb okna jest **opt-in** — domyślnie działa zwykła konsola (T1).

## Wymagania

- BIOS + jądro z tej gałęzi wgrane na obraz: `BOOT.SYS`, `KERNEL.SYS`
  (handoff **v5** — jeśli odtwarzasz obraz od zera, wgraj oba).
- Programy na partycji BGM: `DESKTOP.PRG`, `T2TEST.PRG`.

## Uruchomienie

W shellu ASF (po POST): 

```
C:\> run desktop.prg
DESKTOP: window mode enabled - run any .PRG to see it in a window
C:\> run t2test.prg
```

Po `desktop.prg` **każdy** kolejny `run <program.prg>` startuje w oknie:
pełnoekranowe okno aplikacji z belką (nazwa pliku), tłem pulpitu i paskiem
`ASF DESKTOP`. Program rysuje do swojego prywatnego ekranu (Physbase), a
kompozytor odświeża okno ~10 Hz tylko gdy zawartość się zmieni.

Powrót do konsoli: `run winon.prg` zostało zastąpione — użyj `XBIOS 0x00FE`
cmd **8** (albo reset Ctrl+Alt+Del). Program `T2TEST.PRG` czeka na klawisz,
więc okno zostaje na ekranie.

## Sterowanie z programów (XBIOS 0x00FE = "ASF control")

| cmd | znaczenie |
|----:|-----------|
| 0 | cache OFF |
| 1 | cache ON |
| 2 | ticks (50 Hz) |
| 3 | cache hits |
| 4 | cache misses |
| 5 | benchmark DISKPD |
| 6 | cache OFF + zwolnienie pamięci |
| 7 | tryb okna **ON** |
| 8 | tryb okna **OFF** |

## Co jest zaimplementowane

- prywatny ekran 32 KB per program, `Physbase`/`Logbase` → ten bufor,
- kompozytor w Timer C (50 Hz, dzielnik → ~10 Hz) + rolling checksum → blit tylko przy zmianie,
- `video8_blit_mono`: LUT 256×8 + zapisy 32-bitowe (1bpp TOS → 8bpp ET4000),
- **skalowanie 2:1** w jądrze (640×400 → bufor 320×200, 40 B/linia) — cały ekran aplikacji mieści się w oknie,
- pulpit: czarne tło z ditheringiem 50%, ikony `FLOPPY A:`, `HARDDISK C:`, `TRASH`,
  okno z 1-px ramką, belką tytułu z **nazwą uruchomionego .PRG** i gadżetem zamknięcia,
  pasek statusu `ASF DESKTOP | RUN <APP.PRG> | CTRL+ALT+DEL = RESET`.

## Uwagi z testów na emulatorze (Hatari)

- Desktop jest stabilny: dwa zrzuty 5 s apart są identyczne (brak migotania).
- Wyświetlanie idzie przez mono-bridge VOFA, więc 8-bpp pulpit jest progowany do
  1 bpp — wybieraj kolory o skrajnej jasności (0 = czarny, 15 = biały).
- Widoczny obszar to 640×400 powiększony o linię statusu Atari (okno Hatari ma 640×436),
  dlatego pasek statusu pulpitu jest na y=424, a nie 456.

## Czego jeszcze nie ma (kolejne kroki T2/T3)

- wiele okien/programów jednocześnie (osobny stan per proces, trapy z trybu użytkownika),
- przesuwanie i aktywacja okna (Z-order, focus),
- skalowanie/klatkowanie okna w trybach color/medium,
- VDI/AES z EmuTOS (T3) — rysowanie GEM w oknie.
