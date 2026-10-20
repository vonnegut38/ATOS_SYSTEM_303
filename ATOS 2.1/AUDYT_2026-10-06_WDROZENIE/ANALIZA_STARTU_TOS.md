# ANALIZA: co musi być gotowe, zanim uruchomimy program TOS

Dokument analityczny (nie kod). Odpowiada na pytanie: **co trzeba załadować
i ustawić przed startem programu TOS, żeby dało się go w ogóle odpalić**, oraz
**czy i na jakich zasadach dawać programowi dostęp do sprzętu**.

Podstawa dowodowa: deasemblacja `KOBOLD_3.PRG` (m68k-elf-objdump), źródła naszego
jądra (`system/tos_prg.c`, `system/tos_entry.S`), porównanie z EmuTOS i MiNT.

---

## 1. Przypadek, który to ujawnił: KOBOLD 3.51

Objaw: program startuje, pokazuje biały pulpit i **sam się wyłącza**.
Pomiar: 122 wywołania GEM, potem wyjście; wcześniej dzielenie przez zero
(rejestry: `d6=0`, `d7=0`, `d4=8` — metryki tekstu).

### 1.1 Czego KOBOLD żąda od systemu (z deasemblacji)

| API | funkcje, których używa | czy mamy |
|---|---|---|
| GEMDOS (`trap #1`) | 47 wywołań: pliki (`Fopen`/`Fread`/`Fwrite`/`Fclose`…), `Cconws`(9), `Dsetdrv`(14), **`Super`(32)** | pliki i `Super` ✓ |
| BIOS (`trap #13`) | `Bconstat`(1), `Bconin`(2), `Bconout`(3), **`Rwabs`(9)**, **`Setexc`**(wektory), `Drvmap`(10), `Kbshift`(11), `Getbpb`(7) | **`Rwabs` ✗, `Setexc` ✗** |
| XBIOS (`trap #14`) | `Flopfmt`(9), `Midiws`(10), `Offgibit`(16), `Xbtimer`(18), +32 | **✗ (zwracamy 0)** |
| AES/VDI (`trap #2`) | `rsrc_obfix`(114) w pętli po obiektach, okna, zdarzenia, VDI | częściowo ✓ |

### 1.2 Sprzęt, po który KOBOLD sięga **bezpośrednio**

| adres | wystąpień | co to jest |
|---|---|---|
| `0xFFFF8800` / `0xFFFF8802` | 36 / 18 | PSG / Microwire — **układ dźwięku** |
| `0xFFFF8606` | 19 | DMA: tryb/status (ACSI/FDC) |
| `0xFFFF8604` | 6 | DMA: dane |
| `0xFFFF8609/B/D/E/F` | po 2–4 | rejestry FDC/ACSI |
| `0xFFFFFA01` | 18 | MFP: USART (MIDI/serial) |
| `0xFFFFFA23` | 2 | MFP: rejestr przerwań/timera |

Wniosek: KOBOLD **nie jest** programem, który wszystko robi przez system.
Ma własne procedury dyskowe (DMA + ACSI/FDC), własną obsługę dźwięku (PSG)
i własne timery/serial (MFP). Instaluje też **własne wektory** (wrapper wołający
BIOS-owe `Setexc` z numerem wektora i handlerem).

---

## 2. Konflikt: ten sam sprzęt należy już do naszego jądra

To jest odpowiedź na „czy i na jakich zasadach dać dostęp do sprzętu" — bo
problem nie polega na *braku* funkcji, ale na **kolizji właścicieli**:

| sprzęt | KOBOLD | nasze jądro | skutek |
|---|---|---|---|
| DMA `FF8606` | 19× | **23×** | ✗ kolizja transferów dyskowych |
| DMA `FF8604` | 6× | **13×** | ✗ kolizja danych |
| FDC `8609/B/D` | ✓ | ✓ | ✗ kolizja |
| MFP `FFFA01` | 18× | 3× | ✗ kolizja (serial/timer) |
| MFP `FFFA23` | 2× | 6× | ✗ kolizja (przerwania/timer) |
| PSG `FF8800/8802` | 36×/18× | **0** | ✓ brak kolizji — można pozwolić |

Nasze jądro używa tego samego kontrolera DMA/ACSI do własnego dysku i tego
samego MFP do zegara (Timer C / odświeżanie). Program, który w tym samym czasie
programuje DMA, **rozjedzie** transfer jądra — i odwrotnie. Na prawdziwym
sprzęcie to się nie „uda", to się psuje.

---

## 3. Co dokładnie jest u nas niegotowe przed startem programu

1. **`Rwabs` (BIOS 9) nie istnieje.** Program dyskowy nie ma jak zrobić surowego
   I/O przez system — a to jedyna droga zgodna z zasadami.
2. **`Setexc` (BIOS 4/5) nie istnieje.** Program pyta o stary wektor, dostaje
   **0**, zapamiętuje 0 i — wołając go w łańcuchu — skacze pod adres 0.
   (Ten sam wzorzec, który wcześniej zatrzymał LDW: brak domyślnego `mousevec`.)
3. **Nieznane funkcje BIOS/XBIOS zwracają 0** bez rozróżnienia „nie ma" od
   „0 to poprawny wynik". Program nie ma jak odróżnić awarii od braku wsparcia.
4. **Brak `Getmpb` (BIOS 0), `Floprd`/`Flopwr`/`Flopfmt`, `Xbtimer`, `Giaccess`,
   `Midiws`** — czyli nie ma warstwy, przez którą program dyskowy/dźwiękowy
   mógłby działać bez sięgania do rejestrów.
5. **Struktury systemowe**: KBDVECS (5 z 9 wektorów), IOREC (brak pól
   low/high water), wspólna kolejka klawiatury — patrz audyt wejść, punkty 1–2.
6. **Brak polityki sprzętowej**: nie ma mechanizmu, który by powiedział
   „DMA i MFP są moje, PSG możesz używać, a próbę dotknięcia mojego sprzętu
   zgłaszam błędem".

---

## 4. Wzorzec z EmuTOS i MiNT: właściciel sprzętu jest jeden

Na TOS/MiNT programy **nie** dotykają DMA ani MFP bezpośrednio — robią to przez:

* GEMDOS (`Fopen`/`Fread`/`Fwrite`, `Dfree`, `Dcreate`),
* BIOS `Rwabs` (surowe sektory), `Getbpb`, `Drvmap`, `Setexc`,
* XBIOS `Floprd`/`Flopwr`/`Flopfmt`, `Giaccess`, `Xbtimer`, `Midiws`,
* `Super`/`Supexec` dla krótkich, świadomych sekcji uprzywilejowanych,
* łańcuch wektorów: program **zapisuje stary wektor przez `Setexc`** i go woła.

MiNT dokłada do tego ochronę pamięci i to, że o sprzęcie decyduje jeden sterownik.

---

## 5. Rekomendowana polityka dla ATOS (do decyzji)

| klasa zasobu | kto jest właścicielem | jak udostępnić programowi TOS | czego odmawiać i jak to zgłosić |
|---|---|---|---|
| dysk (DMA/ACSI/FDC) | **jądro** | `Rwabs`, `Floprd`/`Flopwr`, `Getbpb`, `Drvmap` — przez naszą warstwę blokową | bezpośrednich zapisów do `FF86xx` nie wykonywać: przechwycić i zwrócić błąd (docelowo MMU 68030 → fault → emulacja) |
| klawiatura/mysz (IKBD, MFP) | **jądro** | wspólna kolejka IOREC + KBDVECS z prawdziwymi wektorami | podmiany wektorów IKBD bez łańcucha |
| zegar/timery (MFP) | **jądro** | `Xbtimer`, `Vsync`, `_hz_200`, `Supexec` | zapisów do rejestrów MFP timera |
| dźwięk (PSG) | **nikt** (jądro nie używa) | pozwolić na bezpośrednie `FF8800/8802` | nic — brak kolizji |
| obraz (Shifter/ET4000) | **jądro** | XBIOS `Physbase`/`Logbase`/`Setscreen`, VDI | bezpośredniego programowania CRTC |
| wektory wyjątków | **program, ale przez nas** | `Setexc` z prawdziwym, działającym domyślnym handlerem | instalacji bez łańcucha (musimy umieć go zawołać) |
| DMA/serial (MIDI) | **jądro** | `Midiws` | bezpośredniego `FFFA01` |

Zasada nadrzędna: **program ma dostać funkcję systemową zamiast rejestru**.
Dopiero gdy funkcji nie ma, można rozważać przechwycenie sprzętu — i wtedy
wynikiem musi być czytelny błąd, nie ciche uszkodzenie transferu.

---

## 6. Kolejność prac wynikająca z tej analizy

1. **`Setexc` (BIOS) z prawdziwym domyślnym handlerem** — najtańsze, a usuwa
   klasę awarii „JSR pod 0" (ta sama, która zatrzymała LDW).
2. **`Rwabs` + `Getbpb`/`Drvmap`** na naszej warstwie blokowej — jedyna zgodna
   z zasadami droga do dysku dla KOBOLD-a i narzędzi dyskowych.
3. **Nieznane funkcje BIOS/XBIOS: rozróżnić „brak wsparcia" od zera** i to
   raportować (żeby program mógł się wycofać, a nie dzielić przez zero).
4. **Wspólna kolejka IOREC + pełny KBDVECS** (audyt wejść, punkty 1–2).
5. **Polityka sprzętowa**: najpierw jawny rejestr „kto używa czego", potem
   przechwytywanie zapisów do `FF86xx`/`FFFAxx` (docelowo przez MMU 68030).
6. Dopiero potem dalsze funkcje AES/VDI (okna, formularze, edycja) — bo one
   i tak wymagają spójnego wejścia i właściciela sprzętu.

---

## 7. Czego ta analiza nie rozstrzyga

* Nie mam jeszcze pełnej listy zmiennych systemowych, które EmuTOS ustawia przed
  startem programu (w toku: inwentaryzacja źródeł EmuTOS).
* Nie wiem, które z bezpośrednich dostępów KOBOLD-a są w jego ścieżce startowej,
  a które w funkcjach opcjonalnych (dźwięk/menu) — to wymaga śladu wykonania.
* Przechwytywanie sprzętu przez MMU 68030 wymaga sprawdzenia, jak Hatari
  emuluje MMU w tej konfiguracji (TF536) — do zmierzenia.

---

## 8. Uzupełnienie: czy przechwycenie sprzętu jest wykonalne (zmierzone)

* **Hatari w tej konfiguracji ma emulację MMU**: opcja `--mmu <bool>`
  („Use MMU emulation") istnieje w binarce TF536. Nasze przebiegi testowe
  używały `--mmu off`, więc mechanizm jest dostępny, ale wyłączony.
* **Nasze jądro nie programuje MMU**: jedyna operacja uprzywilejowana związana
  z mapowaniem to `movec d0,vbr` (VBR, `system/tos_entry.S:7`,
  `system/kernel_entry.S:41`). Nie ma ustawień `%tc`, `%srp`, `%crp`.

Wniosek: droga „przechwyć zapis do `FF86xx`/`FFFAxx` i obsłuż go po swojemu"
jest **wykonalna na 68030 + tym Hatari**, ale wymaga wejścia w MMU, którego
jądro dziś nie używa. To osobny, świadomy etap — nie efekt uboczny.

## 9. Stan analiz towarzyszących

Trzy równoległe analizy źródłowe są w toku i zostaną dopisane do tego dokumentu:

1. inwentaryzacja tego, co EmuTOS ustawia przed startem programu (zmienne
   systemowe, wektory, KBDVECS/IOREC, basepage, środowisko, AES/VDI, ekran),
2. analiza zasad dostępu do sprzętu w EmuTOS i MiNT (Supexec/Super, łańcuch
   wektorów, własność IKBD, własność dysku, ochrona pamięci),
3. pełna lista luk naszego launchera (co z listy kontrolnej już jest, co jest
   częściowe, co brakuje — z odwołaniami do linii).

Ten dokument zawiera część, którą mam już **udowodnioną pomiarem**: profil
wymagań KOBOLD-a, tabelę kolizji sprzętowych z naszym jądrem oraz luki
w BIOS/XBIOS.

---

## 10. Weryfikacja własna: które zmienne systemowe EmuTOS my ustawiamy

Porównałem listę zmiennych z raportu EmuTOS (sekcja 1.2 tamtego dokumentu) z
naszym kodem (`grep` po `system/`, `include/`). Wynik:

**Ustawiamy (lub odwołujemy się):**
`_hz_200` 0x4BA, `_drvbits` 0x4C2, `_bootdev` 0x446, `_phystop` 0x42E,
`_membot` 0x432, `_memtop` 0x436, `_v_bas_ad` 0x44E, `_p_cookies` 0x5A0,
`_longframe` 0x59E, `_sysbase` 0x4F2, `_dskbufp` 0x4C6, `_conterm` 0x484.

**BRAK — i to są realne luki:**

| adres | zmienna | konsekwencja braku |
|---|---|---|
| 0x452 | `_vblsem` | VBL nie jest obsługiwany — program, który czeka na `_vblsem`, nie ruszy |
| 0x454 | `_nvbls` | nie ma tablicy VBL |
| 0x456 | `_vblqueue` | **instalacja procedury VBL przez kolejkę TOS nie działa** (program myśli, że ją zainstalował) |
| 0x462 | `_vbclock` | brak licznika wykonanych VBL |
| 0x466 | `_frclock` | brak licznika odebranych VBL (`Vsync`-owe pętle programów) |
| 0x408 | `_etv_term` | program, który instaluje handler zakończenia GEM, **nie zostanie o nim powiadomiony** |
| 0x404 | `_etv_critic` | j.w. dla błędu krytycznego |
| 0x4A6 | `_nflops` | programy czytające liczbę stacji widzą 0 |
| 0x4B2 | `_bufl` | listy buforów GEMDOS (nasz GEMDOS używa własnej ścieżki) |

To domyka obraz z raportu o sprzęcie: **brak podsystemu VBL to nie jeden brak,
a pięć zmiennych i cała kolejka**, której programy GEM faktycznie używają
(kursor myszy w VDI wisi na slocie 0 `_vbl_list`, `vdi/vdi_mouse.c:602`).

---

## 11. ERRATA — korekty po ocenie użytkownika (`OCENA_I_PLAN.txt`)

Moje wcześniejsze wnioski były w kilku miejscach **błędne**. Poprawki:

1. **PSG NIE jest zasobem bezkonfliktowym.** Napisałem „jądro nie używa PSG, można
   pozwolić na bezpośredni dostęp" — to nieprawda: `hw/fdc.c:33-41` używa PSG
   (rejestr 14) do wyboru napędu i strony dyskietki. Co ważniejsze, PSG ma
   **wspólny selektor rejestru** (numer do `FF8800`, dane do `FF8802`), więc
   przerwanie lub inny klient między tymi dwoma zapisami skieruje dane do
   innego rejestru. Trzeba serializować **całą transakcję selektor+dane**
   i rozdzielić dźwięk od portów sprzętowych (rejestr 7 to kierunki portów).
   Ochrona samego rejestru 14 nie wystarcza.

2. **Pomyliłem numery funkcji.** Poprawnie:
   BIOS: `Rwabs=4`, `Setexc=5`, `Mediach=9` (nie „Rwabs=9").
   XBIOS: `Floprd=8`, `Flopwr=9`, `Flopfmt=10`, `Midiws=12`, `Offgibit=29`,
   `Xbtimer=31`; `16=Keytbl`, `18=Protobt`; `Blitmode=64` (0x40).
   Napisałem m.in. „Rwabs(9)", „Flopfmt(9)", „Midiws(10)", „Xbtimer(18)" —
   wszystkie te numery były błędne.

3. **`FFFA01` to MFP GPIP**, nie rejestr USART/MIDI. `FFFA23` = Timer C data.
   `FF8609/B/D` to bajty adresu DMA. `FF8800/8802` to PSG (nie Microwire).
   MIDI ACIA jest pod `FFFFFC04/06`.

4. **Obecność adresu w binarce niczego nie dowodzi** — ani tego, że kod się
   wykona przy starcie, ani że to zapis, ani że dotyczy to dźwięku. Dostępy do
   PSG w KOBOLD-zie mogą należeć do jego kodu **stacji dyskietek**. Sam to
   napisałem jako „profil wymagań", a było to nadinterpretacją. Ślad wykonania
   (poniżej) pokazuje, że program zatrzymuje **luka VDI**, a nie sprzęt.

5. **Nie stosować zbiorczego „−32 z każdego nieobsłużonego API".** Kontrakty się
   różnią: część funkcji zwraca **adres** (program potraktuje −32 jako wskaźnik),
   część status, część nic. Potrzebna tabela kontraktów per funkcja.

6. **Nie zmieniać globalnie trybu EOI MFP** (auto-EOI `0x40`) tylko dla
   zgodności z gościem — to dotyczy wszystkich źródeł przerwań i schedulera.
   Życzenia `Mfpint/Jdisint/Jenabint` trzymać w stanie sesji.

7. **Kolejka VBL w Timer C to przybliżenie**, nie synchronizacja obrazu:
   50 Hz tick ≠ VBL właściwego trybu; `_hz_200` rośnie skokami po 4. Nie
   obiecywać HBL ani efektów rasterowych.

8. **„Bez MMU nie ma granicy user/supervisor" było zbyt szerokie.**
   Sprzętowe poziomy uprawnień i wyjątek instrukcji uprzywilejowanej istnieją
   bez stronicowania; brakuje pełnej **izolacji pamięci i urządzeń**.

9. **Sam supervisor gościa wyklucza ochronę polityką** (`tos_entry.S:51`,
   SR=0x2500): gość może zmienić IPL, VBR, konfigurację MMU i rejestry MMIO.
   Tabela uprawnień ani mutex tego nie zatrzymają. Izolacja wymaga trzymania
   gościa w **user mode** z programową obsługą oczekiwanego stanu supervisor
   (albo szerszej techniki) — to **osobny projekt**, nie jedna poprawka.

10. **`--mmu` w Hatari dowodzi istnienia opcji, nie działającej sesji**;
    w `src/options.c` wybór maszyny ST/STE zeruje `bMMU`. Nasz `boot/mmu030.S`
    to eksperyment bootowania, nie menedżer pamięci procesu.
