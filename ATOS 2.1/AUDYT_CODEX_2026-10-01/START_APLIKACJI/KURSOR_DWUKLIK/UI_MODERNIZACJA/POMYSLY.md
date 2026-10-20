# ATOS 2.1 — nowoczesny wygląd bez zabijania procesora

Dokument projektowy. **Bez zmian w kodzie** — do decyzji, co wdrażamy.

## Punkt wyjścia (stan faktyczny)

- 800×600×8bpp, ET4000/VOFA, 68030 @ 32 MHz.
- Paleta: **0..19** system + NC (`16..19`), **20..121** ikony (generowane, 102 wpisy),
  **122..255 wolne (134 wpisy)**.
- Okno: obwódka 2 px bevel, pasek tytułu **16 px**, font bitmapowy 8×16 (EmuTOS).
- Pulpit to aplikacja z **trwałą powierzchnią 800×600** w pamięci jądra.
- Kompozytor ma już: listę damage, rozłączne zakresy w warstwie wideo, ROI-limited
  flush ze porównaniem shadow, licznik `wm_damage_pixels`, `kdbg_recompose_cycles`.

## Zasada nadrzędna

W 8 bpp walutą jest **zapis piksela**. Każdy nowy element graficzny musi być:

- **(a) narysowany raz** i potem tylko kopiowany z powierzchni, albo
- **(b) rysowany wyłącznie w obszarze damage**.

Wszystko, co wymaga przemalowania ekranu na klatkę, jest wykluczone. Dzięki trwałej
powierzchni pulpitu **tapeta narysowana raz jest darmowa w czasie pracy** — okno
zasłaniające i odsłaniające fragment nic nie kosztuje, bo kompozytor kopiuje gotowe
piksele z powierzchni.

## 1. Paleta — 134 wolne wpisy to główne narzędzie

Proponowany podział (baza rampy UI **liczona**, nie na sztywno):

| zakres | ile | przeznaczenie |
|---|---:|---|
| 0..19 | 20 | system + NC (zostawić dla zgodności) |
| 20..(20+N-1) | N | ikony (N z `ICON_PAL_COLORS`) |
| reszta | 134 | rampa UI |

Sugestia: **44** wpisy akcentu (pasek tytułu, przyciski, focus, pigułki), **32**
neutralne (obwódki, cienie, tła dialogów), **58** na rampę tapety.

**Pułapka:** liczba kolorów ikon zależy od zestawu ikon. Trzeba (a) liczyć bazę
rampy jako `ICON_PAL_BASE+ICON_PAL_COLORS`, (b) **ograniczyć paletę ikon** (np. do
160 wpisów), żeby UI zawsze miało zagwarantowane ~76.

## 2. Tapeta — największy efekt, zerowy koszt w pracy

Płaski teal to najbardziej „2000" element. Propozycja: **pionowy gradient z
ditheringiem 4×4 Bayer** (58 wpisów + dither = gładko, bez pasów).

Problem: jako lista prostokątów to ~600 pozycji. Rozwiązanie: **nowy rodzaj
elementu batcha `GUI_GRADIENT`** — jedno polecenie, kernel rysuje gradient z
ditheringiem w jednym przebiegu (kolor bazowy + długość rampy w polach `color`/
`background`, jak w istniejącym `gui_draw_t`).

Koszt: jeden przebieg po ekranie **przy starcie i przy zmianie trybu**. Potem zero.
`gui_damage` traktuje go jak zwykły prostokąt.

Warianty tła: gradient pionowy, gradient po przekątnej, winieta w narożnikach
(wszystkie tym samym prymitywem, różny kierunek/unormowanie).

## 3. Ramka okna „flat modern" — ten sam koszt co dziś

| pomysł | koszt | efekt |
|---|---|---|
| koniec beveli 2 px → **1 px jasna obwódka + 1 px cień** | identyczny (rysowane w ROI) | natychmiast „flat" |
| pasek tytułu **2-tonowy gradient** (2 pasy) | +2 FILLe na okno | głębia bez kosztu |
| **twardy cień** 2 px w prawo/dół | ~2×(w+h)×2 px w ROI | okna „odklejają się" od tła |
| **zaokrąglone rogi** (blit świadomy rogów: pomiń N px w 4 rogach) | ~12 px na okno | najmocniejszy sygnał „nowoczesne" |
| przyciski **glify × − □** zamiast ramkowanych pudełek | mniej pikseli niż teraz | czystszy wygląd |
| pasek tytułu **20–22 px** + 1 px cień pod tekstem | +4–6 wierszy w ROI | oddech, czytelność |

Zaokrąglone rogi działają tak: kompozytor najpierw rysuje tło i okna pod spodem,
potem blituje to okno. Jeśli blit **pominie** piksele narożne, zostanie to, co pod
spodem — czyli rogi „przezroczyste" bez żadnego kanału alpha. Cała zmiana to kilka
pominiętych pikseli w blitowaniu.

## 4. Górny pasek / dock

- Jeden **akcent** dla aktywnej aplikacji, **„pigułka"** (zaokrąglony prostokąt) pod
  aktywnym kafelkiem.
- Płaska belka bez obwódek; separator 1 px tylko tam, gdzie niesie informację.
- Zegar + data w dwóch liniach, wyrównane do prawej.
- Wskaźnik aktywności (mała pulsująca kropka) **tylko gdy coś się dzieje** — 2 klatki,
  obszar 8×8 px.

Koszt: przerysowanie paska tylko przy zmianie stanu zadań (tak już działa).

## 5. Typografia i odstępy — darmowe, robi połowę roboty

- **Przerysować font 8×16** na czystszy (płaski, bez szeryfów). Koszt runtime: zero.
- **Font 2× (16×32) tylko dla tytułów okien** — rysowany w ROI, ~30 znaków × 32 px.
- Odstępy: padding 8 px w dialogach, wiersze list 20 px zamiast 16.

## 6. Ruch — tylko mały i tylko chwilowy

Miejsce, gdzie najłatwiej zabić procesor. Twarde reguły:

- **Zakazane:** alpha/translucencja (brak kanału), rozmycia, rozmyte cienie,
  animowana tapeta, pełnoekranowe przejścia, gradienty liczone na klatkę.
- **Dozwolone:** menu wjeżdżające 4–6 px w 3 klatkach (obszar ~200×100), otwieranie/
  zamykanie okna przez ramkę XOR + finalny blit (3 klatki), kursor XOR (już jest).
- Każda animacja: **skończona liczba klatek, obszar ograniczony, koniec**.

## 7. Kolejność wdrożenia

**P1** — największy efekt / najmniejsze ryzyko:
1. Paleta + `GUI_GRADIENT` + tapeta.
2. Flat chrome: 1 px obwódka, 2-tonowy tytuł, twardy cień, zaokrąglone rogi.
3. Górny pasek z pigułką akcentu.

**P2:**
4. Dialogi i listy: odstępy, płaskie obramowania, nagłówek akcentem.
5. Font: czystszy 8×16 + wariant 2× dla tytułów.
6. Ikony: pigułka hover zamiast zamiany na `_C`, cień pod etykietą.

**P3 (tylko po pomiarze):**
7. Animacja otwierania/zamykania okna.
8. Wskaźnik aktywności w pasku.

## 8. Kryteria odbioru

Każda zmiana wizualna musi przejść test: **„czy dodała pracę na klatkę?"**

Mierzymy tym, co już mamy, przed i po:
- `wm_damage_pixels` (powierzchnia damage na klatkę),
- `kvideo_dbg_copies` / liczba flushy,
- `kdbg_recompose_cycles`, `kdbg_deco_cycles`,
- licznik FPS w PLASMA (260×180) jako czujnik obciążenia systemu.

Nowy wygląd **nie może** podnieść kosztu przesuwania okna ani kosztu klatki
aplikacji animowanej.

## 9. Czego nie robić

- Alpha/translucencja, „szkło", blur — brak kanału i czytania tła.
- Rozmyte cienie — bez blendowania wyjdzie brudny dither.
- Animowana tapeta i cokolwiek liczonego na klatkę dla całego ekranu.
- Gradienty i wzory generowane per-frame zamiast raz do powierzchni.

## 10. Otwarte pytania do decyzji

1. Kierunek estetyczny: **ciemny neutral + jeden akcent** (nowoczesny, bezpieczny dla
   palety) czy kolorowy „retro-modern"?
2. Dock na górze (jest pasek) czy na dole (bliżej klasycznego taskbaru)?
3. Czy zaokrąglone rogi robimy od razu, czy najpierw tapeta + chrome (mniejsze ryzyko
   przy komponowaniu sąsiadujących okien)?
4. Czy ograniczamy paletę ikon do 160 wpisów, żeby zagwarantować rampy UI?

---

# WDROŻONE: system motywów (OPTIONS -> SETTINGS -> THEMES)

Na życzenie: motyw „STEEL" (ciemny grafit + stalowy pasek górny + akcent) dostępny
z menu, z możliwością powrotu do obecnego wyglądu. Menu na górze zostaje, launcher
na dole i zaokrąglone rogi to osobny krok (patrz wyżej).

## Jak to działa

**Rdzeń** (`system/kernel_theme.c`, `include/kernel_theme.h`): motyw jest właścicielem
CAŁEJ palety 8 bpp opisującej powłokę:

| wpisy | rola |
|---|---|
| 0 | ink — tekst aplikacji, glify |
| 1 | paper — korpusy okien, etykiety ikon |
| 2 | tło biurka (kompozytor maluje je przez `GUI_BG`) |
| 16..19 | chrom okien: rama, tytuł nieaktywny, tytuł aktywny, akcent |
| `ICON_PAL_BASE+ICON_PAL_COLORS` + 0..15 | **rampa UI** (stal, akcent, neutralne) |

Rampa leży **za wygenerowaną paletą ikon**, więc dołożenie ikon nigdy jej nie
nadpisze. Motywów pilnuje jądro, bo chrom okien i tło biurka rysuje jądro — pulpit
tylko wybiera indeks.

**Nowy opcode GFX 37** (`xy` = motyw, `color` = podoperacja):
`0` zapytanie `(count<<16)|current`, `1` zastosuj, `2` flagi, `3` baza rampy,
`4` kolory podglądu.

**Motywy:** `CLASSIC` (dotychczasowy wygląd, 1:1) i `STEEL` (grafitowe biurko,
stalowy pasek, akcentowy pasek tytułu; korpusy okien zostają jasne, więc **każda
istniejąca aplikacja pozostaje czytelna bez zmian**).

## Interfejs

`OPTIONS -> THEME` otwiera SETTINGS na zakładce **THEMES**; `OPTIONS -> SETTINGS`
na zakładce **RESOLUTION**. Zakładki przełącza się strzałkami LEWO/PRAWO, wiersze
GÓRA/DÓŁ, zatwierdza ENTER lub klik. **Wybranie motywu stosuje go od razu** — cały
ekran jest podglądem, a wybranie `CLASSIC` to powrót do poprzedniego wyglądu.
`SAVE DESKTOP CONFIG` zapisuje motyw do `\SYSTEM\DESKTOP.CFG` (`THEME=n`);
sprawdzone: po pełnym restarcie motyw wraca.

## Dwa błędy naprawione po drodze

1. **Tablice wskaźników do łańcuchów w aplikacji są puste.** Aplikacja jest
   ładowana pod adresem innym niż linkowany i adresuje się PC-relative, więc
   `static const char *tab[]={"..."}` przechowuje **bezwzględne** adresy, które
   po załadowaniu wskazują w nic. Dlatego lista rozdzielczości **nigdy** nie
   pokazywała nazw trybów. Naprawione: funkcje ze `switch` zwracające literały
   (kompilują się do PC-relative). **Zasada na przyszłość: w aplikacjach nie
   używać statycznych tablic wskaźników do łańcuchów.**
2. **Opcode 37 nie zwracał wyniku** — brak `return 0;` powodował, że końcowe
   `user_syscall_result=0` w łańcuchu dyspozytora zerowało odpowiedź.

## Limit, o którym warto pamiętać

Cały obraz aplikacji (text+data+bss) musi zmieścić się w jednym oknie ±32 KB
(`-mpcrel`). Pulpit był tuż pod granicą i dodanie motywów przełamało linkowanie
(`relocation truncated to fit: R_68K_PC16`). Naprawione przez przeniesienie
tablicy rysowania `list[40]` (1920 B) na stertę jądra (`gui_call(16,…)`), bez
zmiany ani jednego użycia. **Nowe funkcje w pulpicie trzeba liczyć w tym budżecie.**

## Stan

Bramka **43 testy zielone**, boot czysty, brak kodu TEMP. `DESKTOP.APP` 26 522 B,
`KERNEL.SYS` 233 338 B. Zrzuty: `t0_classic`, `t2_steel`, `t3_back` (powrót),
`p_reboot` (trwałość), `s_win`, `s_menu`.

## Poprawka: tekst na pasku bez własnego tła

Zgłoszone: litery na górnym pasku miały białe prostokąty. Przyczyna: renderer
tekstu **zawsze** malował tło glifów (`kgfx_text(...,opaque=1)`), a `add(GUI_TEXT,…)`
domyślnie ustawia `background=1` (paper). Na płaskim, białym pasku CLASSIC to było
niewidoczne; na stalowym gradiencie stało się białym prostokątem.

Naprawa — renderer już umiał rysować same glify (`chunky_glyph` z `opaque=0`),
brakowało tylko drogi, żeby to zamówić:

- **kontrakt**: `gui_draw_t.reserved` bit 0 dla `GUI_TEXT` = **bez tła** (tylko
  piksele glifów); `GUI_TEXT` przekazuje `opaque = !(reserved&1)`;
- **pulpit**: `FILE` i `OPTIONS` na pasku zawsze bez tła; pasek statusu bez tła
  tylko w motywie ciemnym (`theme_ink()!=0`), więc CLASSIC zachowuje dotychczasową
  papierową plakietkę;
- **WM/zegar**: `clock_set_transparent()` + `kgfx_theme_ink_on_bar()` — gdy pasek
  jest gradientem, zegar rysuje same cyfry. (Zegar jest w tej kompilacji wyłączony
  przez `WM_CLOCK_APPLET 0`, więc zmiana jest nieaktywna, ale gotowa.)

Efekt: `final_bar_c.png` (STEEL) i `b_classic_bar.png` (CLASSIC — bez zmian).
Bramka nadal **43 testy zielone**.

---

# Ikony 48x48 z zestawu ICONS 8 (ciemny motyw)

## Zmienione

- `tools/mk_icons_color.py`: `SIZE 32 → 48`, `PAL_COLORS 236 → 216`.
- `include/gui_draw.h`: `GUI_ICON_COLOR_SIZE 32 → 48` (reszta geometrii jest pochodna:
  `CELL_W`, `CELL_H`, `CELL_ERASE_H`, `MA_ROW_H`, granice damage, ścieżki rysowania
  ikon w oknach — wszystko liczy się z tej jednej stałej).
- `system/ICONS/`: 96 plików z `ICONS 8/Icons_16` (48 nazw × stan normalny + `_C`).
- `system/icons_color.h`: wygenerowany na nowo — `ICON_SIZE 48`,
  `ICON_PAL_COLORS 216`, `ICON_MASK_BYTES 288`, `ICON_PIXEL_BYTES 2304`,
  `ICON_COLOR_COUNT 96`.
- Commander dostał ikonę `nc` (w `ICONS 7` jej nie było, więc był folder).

## Dwa ograniczenia, które trzeba było policzyć

**1. Paleta.** `ICONS 8` ma **39 773 różnych kolorów** (ikony cieniowane gradientami),
więc bezstratna paleta jak przy `ICONS 7` (102 kolory) nie wchodzi w grę — potrzebna
jest kwantyzacja. Generator robi to od początku (`MAXCOVERAGE`), ale musiałem
zmniejszyć paletę ikon z 236 do **216**, bo motywy trzymają **16 wpisów rampy UI
za paletą ikon** (`kgfx_theme_ramp_base()`):

```
20 (system) + 216 (ikony) + 16 (rampa) = 252 <= 256   ✔
```

Przy 236 wpisach rampa zaczynałaby się na 256 — poza RAMDAC.

**2. Rozmiar jądra.** 96 ikon × (288 B maski + 2304 B pikseli) = **243 KB danych**,
wpiekanych w jądro. `KERNEL.SYS`: **234 784 → 387 184 B**. To bezpieczne, bo:
- jądro ładuje się od `0x00020000`, kończy ok. `0x7E830` — poniżej `0x80000`,
- ekran jest na `0x00010000` (32 000 B), więc **nie ma kolizji**,
- oryginalny ATOS miał właśnie 48×48 (docstring generatora to potwierdza), więc to
  powrót do sprawdzonej konfiguracji, nie nowy eksperyment.

`DESKTOP.APP` został 26 606 B — **ikony są w jądrze, nie w aplikacji**, więc limit
`-mpcrel` pulpitu nie ucierpiał.

## Weryfikacja

- Bramka **43 testy zielone**.
- Biurko rysuje 48×48 1:1, bez skalowania; kwantyzacja do 216 kolorów bez widocznych
  pasów (gradienty kosza, folderu i PLASMY gładkie).
- **Przełączanie motywów nadal działa po przesunięciu palety**: CLASSIC = teal
  `(0,138,138)`, STEEL = grafit `(16,32,32)` — sprawdzone na żywo, bo paleta ikon
  urosła o 114 wpisów i to była główna obawa regresji.
- Okno plików i ikony pulpitu rysują się poprawnie.

## Siatka ikon przesunięta w prawo

Zgłoszone: pierwsza kolumna wygląda na obciętą (np. SYSINFO). Przyczyna jest
w sposobie rysowania etykiety, nie w ikonie:

```c
/* kgfx_icon_color(): etykieta startuje 8 px NA LEWO od ikony */
tx = x - 8 + k*8;
```

Czyli przy `GRID_X = 16` tekst zaczynał się już na **x = 8** — 8 px od krawędzi
ekranu. Sama ikona nie była obcinana: sprawdziłem bounding boxy źródłowych PNG
(i tak `informacje`, `komputer`, `terminal`, `folder`, `obraz` mają po **2 px**
marginesu w swoim 48×48, `kosz` 6/7, `konfiguracja` 4/5).

Zmiana: **`GRID_X 16 → 28`**, więc etykieta startuje na `x = 20`. Kolumn nadal
mieści się 7 (`(800-28)/100 = 7`), bo limit liczy się z `GRID_X`.
Komentarz w kodzie wyjaśnia, że to margines TEKSTU, nie ikony.
