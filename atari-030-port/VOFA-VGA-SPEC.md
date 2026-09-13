# VOFA ET4000 — specyfikacja docelowa (VGA 640×480 mono od startu)

Ustalone z użytkownikiem. Konfiguracja docelowa: **Mega ST bez blittera + TF536
(68030, 64 MB ALT-RAM, IDE) + VOFA ET4000 (zwykły, bez blittera) + monitor VGA**.

## Zachowanie docelowe

1. Po włączeniu BIOS **od razu wstaje w trybie VGA 640×480 mono** — nie w Shifterze.
2. Na górze ekranu **małe logo karty ET4000**, widoczne około **1 sekundy**.
3. Po tym opóźnieniu startuje normalny BIOS, a **POST i shell wyświetlają się już
   na karcie w 640×480**.
4. Użytkownik nie powinien nigdzie widzieć obrazu Shiftera.

## Strona systemu (nasz ASF BIOS)

- `hw/nova.c` — detekcja VOFA **już działa**: `VOFA_IO 0x00d00000`,
  `VOFA_MEM 0x00c00000`, sonda `+0x3C3` przechodzi bez bus errora.
  Wariant `NOVA_ADAPTER_VOFA` jest obecny, POST wypisuje linię `GRAPHICS`.
- Do napisania:
  - **init VGA w BIOS-ie**: ustawić tryb 640×480 mono (sekwencer, CRTC, kontroler
    graficzny, atrybuty, DAC), przenieść `video_active` na `VIDEO_ACTIVE_NOVA`,
  - **logo karty** na górze ekranu + opóźnienie ~1 s,
  - **konsola BIOS-u i POST** rysowane do VRAM karty (okno `0x00C00000`),
    a nie do pamięci Shiftera,
  - **backend ET4000 w `kgfx`** obok Shiftera — w kodzie są już miejsca
    `VIDEO_PREF_NOVA` / `VIDEO_ACTIVE_NOVA`.
- **Zaleta, której nie mieliśmy z EmuTOS**: tryb karty wybieramy sami. Dzięki temu
  geometria karty i emulatora mogą być zgodne co do piksela.

## Strona emulatora (Hatari)

Stan obecny:
- `src/vofa.c` — rejestry VGA, 1 MB VRAM, okno `0x00C00000` (całe VRAM),
  okno rejestrów `0x00D00000` (8-bit), `VOFA_GetMode()`, most liniowy
  `VOFA_NextLine()` wołany z `Video_CopyScreenLineMono()`.
- Most jest **wyłączony** (`vofa_drive = 0`), bo ekran ST ma 400 linii, a karta 480.

Do zrobienia:
- **Wyświetlić 640×480, a nie 640×400.** Ekran ST High ma 400 linii i nie pomieści
  480, a wymuszanie ST High zmienia zachowanie bootującego systemu (sprawdzone
  empirycznie na EmuTOS). Właściwa droga: potraktować kartę jako **własne źródło
  obrazu o geometrii 640×480** — Hatari ma już maszynerię renderowania ekranu
  o dowolnym rozmiarze w obsłudze rozdzielczości rozszerzonych VDI
  (`bUseExtVdiResolutions`, `nVdiWidth`/`nVdiHeight`/`nVdiColors`); trzeba z niej
  skorzystać, podstawiając bufor karty zamiast pamięci ST.
- Po tym `VOFA_NextLine()` może kopiować **1:1** (80 B/linia, krok 1) — bez
  skalowania i bez zgadywania geometrii.

## Fakty ustalone eksperymentalnie (nie zgadywać ponownie)

| Fakt | Wartość / wniosek |
|---|---|
| Tryb programowany przez EmuTOS | 640×480, 1 bpp, `crtc13=0x28` → 80 B/linia |
| Układ VRAM | **liniowy, 80 B/linia — BEZ chain2** (mój wcześniejszy wniosek o chain2 był błędny) |
| Zawartość VRAM po starcie EmuTOS | **pełny pulpit** (36336/38400 B niezerowych) — czyli EmuTOS **przełącza się na kartę** |
| Adresy | VRAM `0x00C00000` (1 MB), rejestry `0x00D00000`, 8-bit; forma 24-bitowa, bo ST dekoduje A0–A23 |
| Wymuszenie ST High | **psuje** zachowanie bootującego OS (zaczyna rysować na Shifterze) |
| `0x3DA` | musi być synchronizowany z klatką (VSYNC), nie z liczbą odczytów — zrobione |
| Wypisy diagnostyczne na stderr | przy pętli na `0x3DA` (420 tys. odczytów) **zalewają terminal** i wyglądają jak zawieszenie |

## Pułapki narzędziowe

- `--avirecord` w tej wersji **wymaga wartości** (`on`).
- **Jedna instancja** Hatari na tym samym obrazie dysku — druga padnie na blokadzie
  pliku i wyjdzie z kodem 1.
- Etykiety w GUI Hatari **nie mogą mieć zduplikowanych skrótów** (`_X`) — to błąd
  krytyczny, nie ostrzeżenie.
- Budowa: cmake+ninja z `tools/py` (pip, bez admina), `-DCMAKE_OSX_ARCHITECTURES=arm64`,
  `-DENABLE_OSX_BUNDLE=0`. Uruchamianie: `./hatari-tf536`.
- Build Hatari ze źródeł: `hatari-src`, przeładowanie po zmianach:
  `ninja -C build`.

## Korekta kierunku: najpierw 640×480 × 256 kolorów (8 bpp)

Użytkownik zauważył (słusznie), że system startuje w trybie **kolorowym** Shiftera,
a nie w VGA — bo **sterownik VGA jeszcze nie istnieje** i nic nie przełącza wyjścia
na kartę. Karta jest wykrywana, ale obraz nadal rysuje Shifter.

Propozycja użytkownika: **640×480 w 256 kolorach zamiast mono** jako pierwszy etap.
Przyjęte — i jest to istotnie łatwiejsze:

- **1 bajt = 1 piksel** (chunky), bez rozpakowywania bitów, bez przeplotu planów,
  bez przenoszenia pojedynczych bitów — czyli znika cała klasa błędów, na których
  się wykładałem przy 1 bpp,
- Hatari **już obsługuje** głębię 256 kolorów w ścieżce rozdzielczości rozszerzonych
  VDI (`nVdiColors = GEMCOLOR_256`), więc jest gotowa maszyneria do wykorzystania,
- łatwiej zweryfikować wzrokowo: wystarczy wypełnić VRAM wzorcem kolorów i sprawdzić,
  czy widać go na ekranie.

Kolejność bez zmian, tylko inny tryb docelowy pierwszego etapu:

1. **Emulator**: ekran 640×480 z VRAM karty (geometria nadal musi być własna, bo ST High
   ma 400 linii) — ale tym razem 8 bpp, czyli kopiowanie bajtów 1:1.
2. **Nasz BIOS**: tryb ET4000 640×480×8bpp, zapis bajtów do VRAM przez `0x00C00000`,
   potem logo, POST i shell na karcie.

Tryb mono 640×480 zostaje jako późniejszy wariant — po ustabilizowaniu 256 kolorów.
