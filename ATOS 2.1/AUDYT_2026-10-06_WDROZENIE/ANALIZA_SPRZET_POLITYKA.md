
> **AKTUALIZACJA 2026-10-11 (przegląd audio / audyt Codexa, pkt 9f).**
> Poniższe stwierdzenie jest **historyczne i nieaktualne**: w jądrze ATOS
> **istnieje** obsługa przerwań okresowych. ISR `kernel_timer_c`
> (`system/kernel_entry.S:330`, wektor 0x45) woła `irq_dispatch(IRQ_SRC_MFP_C)`,
> a warstwa `system/irq.c` ma 4 sloty na źródło. Timer C chodzi z częstotliwością
> zmierzoną w stanie ustalonym na **~50–60 Hz** (58–61 Hz w dwóch oknach
> pomiarowych) i to on napędza interpretację `Dosound`.
> Sekcje o „braku IRQ w jądrze" traktować jako opis stanu sprzed 2026-10-10.

# ANALIZA: polityka dostępu programów TOS do sprzętu i zasobów OS

Raport źródłowy (agent analityczny) + **weryfikacja własna autora** tam, gdzie
teza ma skutki praktyczne. Prefiksy: **[E]** = EmuTOS
(`files-pasted-by-the-user-projekt/work/emutos-reference`), **[A]** = ATOS 2.1.
Odwołania w formie `plik:linia`.

## 0. Ograniczenie metodologiczne

W drzewie **nie ma źródeł jądra MiNT**. `[A] mint/` to desktop ATOS budowany
toolchainem `m68k-atari-mint-gcc` (`[A] mint/Makefile:1-8`,
`[A] mint/desktop_mint.c:1`), nie jądro. Wszystko, co dotyczy MiNT, pochodzi
z dokumentów projektu, nie z kodu — i tak jest oznaczone.

Nie istnieje też żaden komunikat „-8"/„no GEM"; w ATOS `-8` to **kod wyjścia dla
Privilege Violation (wektor 8)** (`[A] system/tos_prg.c:243`), a jedyne
diagnostyki to `TOS FAULT vec=` (`:224`) i `TOS PROGRAM FAULT` (`:3379`).

## 1. Jak program TOS sięga sprzętu — warstwy

Cztery trapy: #1 GEMDOS (`[E] bdos/bdosmain.c:320`), #2 AES/VDI (`:326`),
#13 BIOS (`[E] bios/bios.c:1279-1296`), #14 XBIOS (`[E] bios/xbios.c:1285-1350`).
Wszystko inne to skrót do sprzętu.

* **BIOS** (12 funkcji): `Rwabs`=4 (`[E] bios/bios.c:1072`), `Setexc`=5
  (`:1097` — pisze wprost do tablicy wektorów), `Getbpb`=7, `Mediach`=9,
  `Drvmap`=10, `Kbshift`=11.
* **XBIOS** istotne dla sprzętu: `Floprd/Flopwr/Flopfmt`=8/9/10/19,
  `Mfpint`=13, `Iorec`=14, `Ikbdws`=25, `Jdisint/Jenabint`=26/27,
  `Giaccess/Offgibit/Ongibit`=28-30, `Xbtimer`=31, `Dosound`=32,
  `Kbdvbase`=34, `Vsync`=37, `Supexec`=38, `DMAread/DMAwrite`=2A/2B.
  **`VsetScreen` w EmuTOS nie istnieje** (grep: 0 trafień) — odpowiedniki to
  `Setscreen`/`Setpalette`/`Setcolor` oraz `VsetMode`/`VsetRGB`.
* **`Super()`** (`[E] bdos/rwa.S:280/305/340`): `+1` = zapytanie, `0` = nowy SSP
  z bieżącego stosu, inna wartość = przełączenie stosu i `SR`.
* **`Supexec()`** (`[E] bios/vectors.S:1376-1378`) to dosłownie
  `movea.l 4(a7),a0; jmp (a0)` — źródło mówi wprost, że służy do „hack hardware
  and protected locations” (`[E] bios/xbios.c:771-773`). **Bez żadnej walidacji.**
* ATOS uruchamia gościa **w supervisorze na własnym stosie**
  (`[A] system/tos_entry.S:51-53`), a `Supexec` tylko podnosi SR do `0x2500`
  (`[A] system/tos_entry.S:357-360`). Skutek policzalny: **IPL 5 maskuje VBL
  (poziom 4) i HBL (2)** przez cały bieg programu; EmuTOS formułuje to samo
  ograniczenie w `[E] bios/mfp.c:211`.

## 2. Weryfikacja własna (to, co sprawdziłem w kodzie sam)

| teza | dowód |
|---|---|
| `Setexc` **nie istnieje** → `return 0` | brak `case 4u/5u` w dyspozytorze BIOS; `[A] system/tos_prg.c:1509` kończy `return 0` |
| `Rwabs` zwraca **0 = sukces** | tamże |
| strona Line-A leży w **BSS jądra** | `static uint8_t tos_linea_page[0x800];` `[A] system/tos_prg.c:794` |
| zapisywanych/przywracanych jest **13 wektorów** | `static const uint16_t vidx[13]={2,3,4,5,6,7,8,10,11,33,34,45,46};` `[A] system/tos_prg.c:3280`, przywracanie `:3358` |
| **brak VBL/HBL** w tej liście → wektor 0x70 nie jest ani instalowany, ani przywracany | tamże + `[A] include/irq.h:8-9` („not yet wired to the actual hardware vectors”) |
| **auto-EOI `0x40`** zamiast TOS-owego software EOI `0x48` | `move.b #0x40,0xfffffa17` `[A] system/kernel_entry.S:81` vs `[E] bios/mfp.c:108` |
| **KBDVECS ma 5 pól** zamiast 9 | `[A] system/tos_prg.c:378-384` vs `[E] include/biosdefs.h:105-116` (9 wskaźników: `midivec, vkbderr, vmiderr, statvec, mousevec, clockvec, joyvec, midisys, ikbdsys`) |
| **brak zamka DMA** | jeden bufor i jeden `block_device`, `[A] system/kernel_block.c:24, 88-97`; ryzyko zapisane w `[A] docs/FLOPPY-BOOT-PLAN.md:124` |
| persona TOS **nie waliduje wskaźników** gościa | `rd32()/rd16()` bez odpowiednika `user_write_range` z `[A] system/kernel_exception.c:1536-1560` |

## 3. Co się psuje, gdy program omija OS (z dowodami)

1. **„Cicha zgoda” zamiast odmowy** — nieobsłużone XBIOS/BIOS zwracają **0**
   (`[A] system/tos_prg.c:1509`, `:1632`), a GEMDOS zwraca `-32` (`:1486`).
   Skutki: `Floprd`→0 = „sukces” z niezainicjowanym buforem; `Rwabs`→0 = „sukces”
   bez transferu; **`Setexc`→0 = „poprzedni wektor = NULL”** i program wpisuje
   NULL do tablicy wektorów. To ten sam wzorzec, który w projekcie objawił się
   jako pisanie pod adres 0 (`[A] docs/PLAN-GEM-KOMPATYBILNOSC.md:172-174`).
2. **Przechwycenie wektorów**: gość jest w supervisorze z VBR=0
   (`[A] system/kernel_entry.S:44-46`), więc zapis wektora VBL **się uda** — ale
   IPL 5 go zamaskuje (cicha porażka), a po zakończeniu ATOS przywraca tylko 13
   wektorów, więc **wektor zostaje z adresem w zwolnionej TPA**
   (`tos_tpa_next=tpa_mark`, `[A] system/tos_prg.c:3364`).
3. **Maskowanie SR/IPL** i długie sekcje krytyczne: pełna konwersja planarna
   w przerwaniu zamrażała maszynę — przeniesiona do pętli głównej
   (`[A] system/tos_prg.c:2701-2710`); wzorzec dla polityki: w ISR tylko
   opróżnianie pierścienia i flaga.
4. **DMA**: jedno `Supexec` w trakcie operacji dyskowej rozjeżdża transfer.
5. **ACIA/IKBD**: program piszący do `0xfffffc00/02` zabiera bajty parserowi
   INPUT.SYS (`[A] system/kernel_module.c:92-100`).
6. **MFP**: program może wyłączyć Timer C (`IMRA/IMRB`, `0xfffffa13/15`) →
   ginie `kernel_timer_ticks`, `_hz_200`, preempcja i pomiar SysInfo.
7. **Wideo/paleta**: bezpośredni zapis do `0xffff8240/0xffff8260` jest możliwy
   i psuje paletę pulpitu oraz tryb skanowania.
8. **Dźwięk**: jedyny realny kolizyjny punkt to PSG rejestr 14 — FDC używa go do
   wyboru stacji/strony (`[A] hw/fdc.c:33-41`).

## 4. Dysk: czego potrzebuje menedżer plików (np. KOBOLD)

Warstwa 1 (każdy menedżer): GEMDOS `Fopen/Fread/Fwrite/Dfree/Fsfirst/…`.
EmuTOS realizuje je przez `Rwabs` (`[E] bdos/fs.h:60-75` → `blkdev_rwabs`
`[E] bios/blkdev.c:406` → `disk_rw` → `acsi_rw`/`ide_rw` `[E] bios/disk.c:1177-1236`).
Warstwa 2 (narzędzia): `Rwabs`, `Floprd/Flopwr`, XHDI `XHReadWrite`.
Warstwa 3 (partycjonery): bezpośrednie DMA/ACSI — tak robi sam ATOS
(`[A] system/kernel_block.c:143-215`, `[A] hw/acsi.c:100-135`).

Czego brakuje u nas: **`Rwabs`** (BIOS 4), **`Floprd/Flopwr`**, **`Mediach`**,
oraz **zamka DMA**. EmuTOS trzyma `flock` + `next_acsi_time`
(`[E] bios/acsi.c:361-379`: `acsi_begin` ustawia `flock=-1`, `acsi_end` przywraca
DMA flopom i `flock=0`; dokumentacja w `[E] bios/floppy.c:220-227`).
XHDI-owe `XHReserve/XHLock/XHStop` są w EmuTOS **niezaimplementowane**
(`[E] bios/xhdi.c:156-178`) — gotowego wzorca nie ma, ATOS musi wprowadzić własny.

## 5. Rekomendowana polityka (tabela decyzyjna)

Zasada nadrzędna: **program ma dostać funkcję systemową zamiast rejestru**.

| Klasa | Kto dziś włada w ATOS | Jak wystawić | Czego odmawiać i jak zgłosić |
|---|---|---|---|
| Dysk (DMA/ACSI/IDE) | `kblock_*`, jeden bufor DMA, cache | BIOS `Rwabs`, `Getbpb`, `Mediach`, `Drvmap`; XBIOS `Floprd/Flopwr`; XHDI | surowych zapisów do BPB/sektora 0 i DMA w trakcie transferu jądra → `EACCDN`/`EBUSY` + log |
| Klawiatura | INPUT.SYS + pierścień ACIA | `Bconstat/Bconin/Bconout`, `Kbshift`, `Kbdvbase`, `Iorec`, **pełne KBDVECS (9 pól)** | surowego ACIA; `Ikbdws` jako jedyne wyjście do IKBD |
| Mysz | `kernel_raw_mouse_event` → `mousevec` | jak dziś (`[A] system/tos_entry.S:365-385`), clamp ±127 | drugiego czytnika IKBD |
| Timery (MFP/VBL/HBL) | Timer C 50 Hz, `_hz_200`; **VBL/HBL brak** | `Vsync`, `Xbtimer`, `Mfpint`, `Jdisint/Jenabint`; kolejka VBL `_vblqueue`/`_nvbls` | timerów A/B/D; **VBL zaimplementować** (przejść kolejkę w Timer C) albo jawnie zgłosić odmowę |
| Wideo | kompozytor + `graphics_device_t` | `Physbase/Logbase` (prywatna powierzchnia), `Setscreen`, `Setpalette/Setcolor`, Line-A | programowania CRTC; **przenieść stronę Line-A do TPA** |
| Dźwięk (PSG) | **nikt** (tylko FDC używa rejestru 14) | `Dosound`, `Giaccess` (gdy powstaną) | jawnie `EINVFN`, bo 0 udaje sukces; chronić rejestr 14 przy transferze FDD |
| DMA (szyna) | jeden bufor bez zamka | tylko przez `Rwabs`/XHDI | zawsze gdy `dma_lock` zajęty → `EBUSY`; nigdy nie pozwalać gościowi ustawić `DMA_ADDR_*` |
| MFP | Timer C + auto-EOI `0x40` | `Mfpint`/`Jdisint`/`Jenabint` jako rejestr życzeń | przejęcia Timer C/ACIA; **ujednolicić EOI do `0x48`** dla persony |
| ACIA/MIDI | ACIA przechwytuje kernel | `Ikbdws`, `Midiws`, `Iorec(2)` jako emulacja bez przerwania | bezpośredniego ACIA; nie włączać RxIRQ MIDI bez modułu |
| Blitter | brak sterownika w personie | XBIOS 40 `Blitmode` (informacja), Line-A 7/8, VDI `vro_cpyfm` | programowania blittera (własność kompozytora) |
| Pamięć/wektory | `.APP` waliduje, **persona TOS nie** | `Malloc/Mfree/Mshrink`, `Super`, `Supexec` | wskaźników poza TPA (`EIMBA`/`ERANGE` + log); **rozszerzyć save/restore wektorów z 13 na 2..47 i 0x60-0x7F**; **zaimplementować `Setexc`** |

## 6. Zasady przekrojowe

1. **Żadne nieobsłużone wywołanie nie może zwracać 0** — ma zwracać `EINVFN`
   (`-32`), jak GEMDOS, i podnosić licznik (`tos_bios_opcount`/`tos_xbios_opcount`
   już istnieją, `[A] system/tos_prg.c:1491, 1512`).
2. **Jeden właściciel na zasób, nazwany w kodzie** (ATOS już tak robi dla IKBD
   i DMA w warstwie blokowej).
3. **Most, nie dziura** — `Supexec` zostaje dla zgodności, ale wszystko, co da
   się wyemulować (VBL, Iorec, KBDVECS, `Rwabs`, `Floprd`), ma być emulowane.
4. **Odmowa musi być widoczna**: kod TOS (`EINVFN`, `EACCDN`, `EIMBA`, `EDRVNR`)
   + jedna linia `TOS REFUSED <warstwa> fn=<n> reason=<...>` (wzorzec
   `TOS FAULT vec=`) + opcode rozszerzenia XBIOS `0xFE` zwracający ostatni powód.
5. **Wektory per uruchomienie i w pełnym zakresie.**
6. **Nie obiecywać ochrony, której nie ma** — bez MMU granica user/supervisor dla
   gościa TOS nie istnieje (`[A] docs/memory-and-multitasking.md:6-8`); politykę
   formułować jako „współdzielenie z przywracaniem stanu”, nie „ochronę”.

## 7. Kolejność wg ryzyka

1. `Setexc` + pełny save/restore wektorów (dziś program może wpisać NULL).
2. `Rwabs` + `Floprd/Flopwr` z jawnymi kodami błędów zamiast 0.
3. Zamek DMA (`flock`) wokół `kblock_*`/FDC/ACSI/IDE.
4. KBDVECS do 9 pól + przeniesienie strony Line-A z BSS jądra do TPA.
5. VBL: kolejka `_vblqueue` w istniejącym Timer C albo jawna odmowa.
6. Walidacja wskaźników gościa względem TPA.
7. `vr=0x48` (software EOI) dla persony.

---

## ERRATA (po ocenie użytkownika)

* **PSG**: wykreślam wiersz „dźwięk: pozwolić na bezpośredni dostęp". PSG ma
  wspólny selektor rejestru i jest używany przez nasz sterownik FDC (rejestr 14),
  więc podlega **serializacji całych transakcji**, a dźwięk trzeba oddzielić od
  portów sprzętowych.
* **MFP EOI**: nie zmieniać globalnie na software-EOI; życzenia programów
  przechowywać w stanie sesji.
* **Numery funkcji**: patrz errata w `ANALIZA_STARTU_TOS.md` (Rwabs=4, Setexc=5,
  Mediach=9; Floprd=8, Flopwr=9, Flopfmt=10, Midiws=12, Offgibit=29, Xbtimer=31).
* **Zakres wektorów**: MFP `0x40..0x4F` (Timer C `0x45`, ACIA `0x46`) i
  systemowe `0x100+` (`etv_*`) też wymagają modelu; nie mieszać numeru wektora
  z jego adresem w bajtach.
* **Surowe zapisy**: blokada DMA nie wystarcza — potrzebne **wyłączne przejęcie
  woluminu**, zatrzymanie operacji plikowych, rozliczenie uchwytów, unieważnienie
  cache i ponowna walidacja po oddaniu.
