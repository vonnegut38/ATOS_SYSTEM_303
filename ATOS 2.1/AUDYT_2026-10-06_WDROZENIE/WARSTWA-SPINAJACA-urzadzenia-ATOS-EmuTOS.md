# WARSTWA SPINAJĄCA: urządzenia ATOS ↔ żądania EmuTOS

Odpowiedź na pytanie: czy ATOS powinien mieć po swojej stronie komponent, który
spina fizyczne urządzenia (RAM, HDD, ET4000, porty, złącza) z żądaniami
EmuTOS-a. **Tak — to jest brakująca połowa wariantu B.** Poniżej: co to jest,
gdzie dokładnie się wpiąć, jak to zrobić i czego NIE robić.

---

## 1. Dlaczego to jest potrzebne — dowód z ostatniego przebiegu

EmuTOS **działa** pod ATOS (własny `_sysbase` = 0x2140, sam wyliczone
`_membot`/`_memtop`, sam przydzielony ekran `_v_bas_ad` = 0xC00000, rosnący
`_hz_200` ≈ 200 Hz, CPU w swoim kodzie pod `PC=0x2d9c`). Ale w logu emulatora
powtarzają się:

```
Bus Error reading at address $ffff8901, PC=$2d9c     <- obszar DMA/ACSI
Bus Error reading at address $ffff9201, PC=$2d9c
Bus Error reading at address $fff0005d, PC=$2d9c
Bus Error reading at address $fffffe00, PC=$2d9c
```

EmuTOS **sonduje sprzęt, którego nasza konfiguracja nie wystawia w oczekiwanym
kształcie** (mamy IDE, ACSI wyłączone). Radzi sobie własnym handlerem, więc
żyje — ale to jest dokładnie ta granica, której brakuje: **ktoś musi odpowiadać
na te żądania po naszej stronie.**

---

## 2. Gdzie dokładnie jest granica (fakty z drzewa EmuTOS)

EmuTOS nie jest monolitą sprzętowym — ma **warstwę dyspozytorów**, a pod nią
transporty. To jest to „wąskie gardło", w które się wpinamy:

| obszar | dyspozytor (tu się wpinamy) | transporty pod spodem |
|---|---|---|
| dysk | `bios/blkdev.c`: `blkdev_init`, `blkdev_getbpb`, `blkdev_rwabs`, `blkdev_mediach` | `bios/acsi.c`, `bios/ide.c`, `bios/floppy.c`, `bios/dma.c` |
| ekran | `bios/screen.c`: `screen_init_mode`, `screen_init_address`, `setscreen`, `physbase`/`logbase` | `bios/videl.c`, shifter, `bios/lineainit.c` |
| wejście | `bios/ikbd.c`: `kbd_init`, `bios/aciavecs.S` (KBDVECS + IOREC) | IKBD/ACIA, `bios/mouse.c` |
| zegar/przerwania | `bios/mfp.c`: `mfp_init`, `bios/clock.c`, `bios/vectors.S` | MFP, VBL/HBL |
| pamięć | `bios/biosmem.c`, `tosvars.ld` (`_membot`/`_memtop`/`_phystop`) | — |
| porty | `bios/chardev.c`, `bios/conout.c`, `bios/sound.c` (PSG), ACIA/MIDI | RS232, drukarka, joystick, PSG |

Wniosek praktyczny: **nie trzeba przepisywać sterowników EmuTOS-a.** Wystarczy
dostarczyć własne implementacje symboli z **dyspozytorów** (albo podmienić
wybrane pliki transportowe), a GEMDOS/AES/VDI EmuTOS-a oraz jego semantyka
urządzeń zostają nietknięte.

---

## 3. Dwa sposoby wpięcia — i dlaczego jeden odpada

### (a) Podmiana przy budowaniu — ZALECANE

Budujemy EmuTOS naszym toolchainem (`make ELF=1` — już to mamy), ale zamiast
`bios/acsi.c`, `bios/ide.c`, `bios/screen.c`, `bios/ikbd.c`, `bios/mfp.c`
linkujemy **nasze implementacje tych samych symboli**:

```
blkdev_rwabs()      -> kernel_block.c   (nasza warstwa blokowa: ACSI/IDE/FDC)
blkdev_getbpb()     -> kernel_fat.c     (nasze BPB i woluminy logiczne)
screen_init_*()     -> video_driver.c   (VOFA/ET4000, tryby, paleta)
kbd_init() / IOREC  -> nasz JEDEN parser IKBD
mfp_init() / clock  -> nasz timer i źródło VBL
biosmem_*           -> kmem (nasz alokator, spójny z TPA sesji)
```

Zalety: brak narzutu, brak pułapek, pełna prędkość, EmuTOS zachowuje swoje
semantyki (GEMDOS, VDI, AES, obsługa błędów), a kompetencja „kto rządzi
sprzętem" zostaje u nas. To jest dosłownie „program spinający po naszej
stronie", tylko zrealizowany jako warstwa usług, nie jako proces.

### (b) Przechwytywanie w runtime (wektory, MMU, pułapki na MMIO) — ODRZUCONE

Zgodnie z audytem użytkownika: koszt na 68030 (pętle odpytywania, dźwięk),
konieczność dekodowania rozmiaru/kierunku dostępu i wznawiania instrukcji,
aliasy adresów, `cache-inhibit`, brak przezroczystego obejścia przez TT0/TT1,
a MMU CPU nie jest IOMMU dla ST DMA. **Nie na ścieżce każdej operacji.**

---

## 4. Tabela własności (zasada: jeden właściciel na urządzenie)

| zasób | właściciel | co dostaje EmuTOS | czego NIE dostaje |
|---|---|---|---|
| RAM | jądro ATOS | `_phystop`/`_membot`/`_memtop`/`_themd` z naszego alokatora, TPA sesji | swobodnej zmiany globalnej mapy |
| HDD | nasz sterownik + FAT | `Rwabs`/`Getbpb`/`Mediach` na **woluminach logicznych** | rejestrów DMA/ACSI |
| ekran | nasz `video_driver_t` + kompozytor | powierzchnia sesji, tryby, paleta (`_v_bas_ad` = powierzchnia sesji) | rejestrów shiftera/VOFA |
| klawiatura/mysz | **jeden** parser IKBD | wspólne IOREC i KBDVECS | drugiego parsera (inaczej dwa walczą o ACIA) |
| zegar/VBL | jądro (MFP) | `_hz_200`, `_vblsem`/`_nvbls`/`_vblqueue`/`_vbclock`, zdarzenie VBL | rejestrów MFP, zmiany trybu EOI |
| wektory | jądro | rejestracje sesji z **generacją** | trwałych wpisów po wyjściu programu |
| PSG | nasz sterownik | usługi dźwięku | wspólnego selektora rejestru (nasz FDC używa go do wyboru stacji) |
| porty (RS232, drukarka, joystick, MIDI) | nasze sterowniki | usługi | surowych rejestrów ACIA/MFP |

Zasada nadrzędna z audytu: **dysk nie może mieć dwóch właścicieli FAT.** Dwa
niezależne systemy plików nie mogą swobodnie zarządzać tym samym zamontowanym
woluminem — dlatego:

* **profil C** (dziś): EmuTOS dostaje **prywatny obraz dysku** i włada nim sam,
* **profil B** (cel): właścicielem woluminu jest **nasza** warstwa, a GEMDOS
  EmuTOS-a rozmawia z nią przez `blkdev_*`.

---

## 5. Profil C nie potrzebuje tej warstwy — i to jest jego wartość

Żeby mieć **działający punkt odniesienia teraz**, nie musimy integrować niczego:
wystarczy dać EmuTOS-owi maszynę, którą rozumie — czyli **dysk, który sam
obsłuży** (ACSI/IDE podłączony w Hatari, własny obraz) oraz klawiaturę/zegar,
które już emuluje. Wtedy jego sondowania `$ffff8901` **przestaną być błędami**,
bo sprzęt będzie istniał, i zobaczymy jego pulpit.

To jest tania, rozstrzygająca próba dla etapu 3 planu („zaliczyć C użytkowo:
pulpit, klawiatura, mysz, zegar, odczyt pliku") — **bez** pracy nad adapterami,
które są potrzebne dopiero dla B.

---

## 6. Kolejność prac (zgodna z przyjętym planem)

1. **C z własnymi urządzeniami** — prywatny obraz dysku + sprzęt, który EmuTOS
   rozumie → pulpit, wejście, zegar, odczyt pliku, powtórzone uruchomienia LDW,
   SysInfo, KOBOLD.
2. **Warstwa spinająca, minimalny zestaw** — pięć dyspozytorów: pamięć, ekran,
   dysk, wejście, zegar. Każdy z właścicielem, kolejką i regułą zwolnienia.
3. **Dopiero potem** rozszerzanie współdzielenia (surowe I/O z przejęciem
   woluminu, PSG jako transakcje, porty).
4. **Izolacja 68030 osobno** — nie jest warunkiem żadnego z powyższych.

---

## 7. Jedno zdanie podsumowania

EmuTOS dostarcza **kontrakty i logikę TOS** (GEMDOS, AES, VDI, semantyka
urządzeń), a ATOS dostarcza **urządzenia, pamięć i czas** przez wąską warstwę
spinającą podpiętą w dyspozytory `blkdev_*`, `screen_*`, `ikbd_*`, `mfp_*`,
`biosmem_*`. Bez tej warstwy EmuTOS działa „na ślepo" — jak teraz, gdy sonduje
nieistniejący sprzęt. Z nią — działa na naszym sprzęcie, a my zachowujemy
własność zasobów.

---

# DOWÓD Z PRZEBIEGU: z „maszyną, którą rozumie" EmuTOS idzie znacznie dalej

Żeby sprawdzić tezę z §5 (profil C nie potrzebuje adapterów — potrzebuje
sprzętu, który rozumie), dałem EmuTOS-owi **prywatny dysk ACSI** (osobna kopia
obrazu, nie współdzielona z ATOS) i powtórzyłem start:

```
--acsi 0=work/emutos-acsi.img      (prywatny dysk EmuTOS)
--ide-master work/tostestA.img      (nasz dysk ATOS z obrazem EmuTOS)
```

Wynik:

```
_hz_200  = 3769          <- zegar EmuTOS-a nadal chodzi
_sysbase = 0x00002140    <- jego wlasny naglowek OS
zrzut: ATOS-owa belka statusu + TLO EMUTOS-a + KURSOR MYSZY na srodku ekranu
```

Zrzut: `zrzuty/47-EmuTOS-kursor-i-tlo-pod-ATOS.png`.

To znaczy, że EmuTOS **przejął ekran i doszedł do inicjalizacji VDI/AES**
(rysuje własne tło i własny kursor), a nie tylko „żyje w tle". Wcześniej, bez
sprzętu dyskowego, kończyło się na białym ekranie i sondowaniu `$ffff8901`.

**Wniosek dla architektury:** to, co zrobiliśmy ręcznie (podłożyć sprzęt, który
EmuTOS rozumie), jest dokładnie tym, co warstwa spinająca ma robić
**systematycznie i po naszej stronie** — z tą różnicą, że wtedy podkładamy
**nasze** urządzenia (blok, FAT, VOFA/ET4000, IKBD, zegar), a nie emulowane
odpowiedniki, i zachowujemy własność woluminu oraz wektorów.

---

# CZY EMUTOS MOŻE DZIAŁAĆ W OKNIE NA NASZYM PULPICIE (i czy okno może być zmieniane)?

## Krótka odpowiedź

**Tak — ale nie przez „przełączenie".** Trzeba EmuTOS-owi **odebrać ekran
fizyczny i dać powierzchnię sesji**, a wejście podać przez adapter. Mechanizm
okna, kompozytor i powierzchnie **już w ATOS istnieją i są sprawdzone** — na
personie TOS, która właśnie tak działa.

## Co już mamy (dowód z kodu)

Persona TOS uruchamia programy w oknie o wymiarach wewnętrznych **640×400**:

```
system/tos_prg.c:2702   mode 2 (640x400x2) -> client 640x400, 1 plane
                        (fixed window, no scaling)
system/tos_prg.c:2703   #define TOS_WIN_H 200
system/tos_prg.c:2704   #define TOS_TITLE 18
system/tos_prg.c:2828   ... tos_client_surface ...   <- prywatna powierzchnia klienta
```

Do tego `system/kernel_wm.c`, `system/wm_core.c`, `system/wm_blit.c`,
`system/kernel_surface.c`, `system/desktop_wm.c` — menedżer okien, kompozytor
i powierzchnie z regionami. To nie jest plan: to działający kod, w którym
uruchamialiśmy LDW i KOBOLD-a.

## Dlaczego dziś (profil C) jest pełny ekran

EmuTOS w trybie C **przejmuje maszynę**: programuje shifter, ustawia
`_v_bas_ad` na **fizyczny** ekran, bierze wektory przerwań MFP/VBL i rysuje
własny kursor przez swoje VDI. Nie wie o naszym menedżerze okien — i nie ma
jak się dowiedzieć, dopóki nie podstawimy mu innych funkcji ekranu.

## Co konkretnie trzeba dodać (jeden punkt krytyczny + cztery pozostałe)

1. **EKran — punkt krytyczny.** Adapter `screen_init_mode`,
   `screen_init_address`, `setscreen`, `physbase`/`logbase` oraz zmienna
   `_v_bas_ad` muszą wskazywać **powierzchnię sesji** (640×400 mono = 32 KB),
   a nie shifter. Wtedy EmuTOS rysuje do pamięci, a **nasz kompozytor**
   pokazuje tę pamięć w oknie. Bez tego punktu reszta nie ma sensu.
2. **Wejście.** Klawiatura → IOREC/KBDVECS **sesji**; mysz → pakiet do
   `mousevec` z **przesunięciem okna** i obcięciem do 640×400; fokus z WM.
3. **Zegar/VBL.** Nasz timer → `_hz_200` i kolejka VBL. Jądro zostaje
   właścicielem MFP — nie oddajemy mu przerwań (zgodnie z audytem).
4. **Dysk.** `blkdev_*` → nasza warstwa blokowa/FAT; najpierw wolumin prywatny.
5. **Prezentacja.** Kompozytor przenosi **brudne regiony** — już to robi.

## Czy okno może być zmieniane (resizable)?

**Tak, ale to viewport na stałej rozdzielczości — nie zmiana rozdzielczości.**
Trzy warianty, uczciwie:

| wariant | co robimy | konsekwencja |
|---|---|---|
| skala | 1×, 1.5×, 2×, „fit" — kompozytor skaluje powierzchnię | działa ze wszystkimi aplikacjami TOS |
| przewijanie | okno mniejsze niż 640×400, suwaki | aplikacja nadal „widzi" 640×400 |
| zmiana trybu | `Setscreen`/nowy tryb w locie | **nie zadziała dobrze** — aplikacje TOS i VDI zakładają stały ekran, a zmiana trybu resetuje środowisko |

Czyli: okno można swobodnie zmieniać **jako widok**, a wewnętrzna rozdzielczość
pozostaje stała (np. 640×400 mono). To jest ta sama granica, którą mają
emulatory GEM-u — i warto ją powiedzieć wprost, żeby nie obiecywać rzeczy,
której TOS nie potrafi.

## Zastrzeżenie, którego nie wolno pominąć

Dopóki gość działa w **rzeczywistym trybie supervisor**, może tę powierzchnię
**ominąć** i pisać wprost do shiftera, DMA czy MFP. Okno jest więc
**kooperatywne, nie wymuszone**. Wymuszenie wymaga izolacji 68030 — to osobny
projekt, świadomie odłożony (zgodnie z Waszą decyzją).

## Wniosek praktyczny

* **C (pełny ekran)** zostaje jako tryb referencyjny i dla programów
  przejmujących sprzęt — on ma pełny ekran z definicji i to jest jego zaleta.
* **B (okno)** budujemy przez warstwę spinającą, zaczynając od **jednego**
  punktu: ekran sesji. Reszta infrastruktury (WM, kompozytor, powierzchnie,
  regiony, routing fokusu) **już istnieje** i jest sprawdzona na personie.
* Pierwszy mierzalny krok B: po podmianie funkcji ekranu zrzucić **pamięć
  powierzchni sesji** i sprawdzić, czy EmuTOS narysował w niej swoje tło
  i kursor. To da się zmierzyć bez patrzenia na ekran.

---

# POMIAR: EmuTOS już rysuje w VRAM karty ET4000 — i dlatego obraz jest „ditherem"

## Co zmierzyłem

```
_v_bas_ad = 0x00C00000     <- VRAM karty ET4000 (nie ST-RAM!)
_phystop  = 0x00400000     <- 4 MB: ST-RAM konczy sie duzo nizej
_membot   = 0x0005005C
probka 4 KB pod _v_bas_ad: 1 unikalny bajt (jednolite tlo)
ta sama probka po 3 s     : 4 unikalne bajty -> COS SIE TAM RYSUJE
```

Potwierdzenie, że `0xC00000` to okno karty, jest w naszym własnym kodzie:
`system/kernel_main.c:473` — *„framebuffer=0x10000 (ST-RAM) while the card
scanned 0xC00000"*.

**Czyli: EmuTOS ustawił sobie ekran na VRAM karty ET4000 i tam rysuje.**
To jest dokładnie powierzchnia, którą włada nasz kompozytor
(`kgfx_framebuffer()` w `include/kernel_graphics.h`) — integracja ekranu dla
wariantu B jest **bliżej, niż zakładałem**: nie trzeba tworzyć powierzchni,
trzeba tylko uzgodnić format i obszar.

## Dlaczego obraz wygląda jak szum/dither (wyjaśnienie)

Nasz kompozytor rysuje do karty w formacie **chunky** (bajt na piksel,
`kgfx_framebuffer()` zwraca `pitch` i `bpp`), a EmuTOS w trybie mono rysuje
**planar 1 bpp** (80 bajtów na linię dla 640 px). Te dwa formaty są
niekompatybilne: to, co EmuTOS zapisze jako bity planu, kompozytor czyta jako
kolejne piksele chunky — i stąd jednolity „dither" zamiast pulpitu, przy
poprawnie narysowanym (bo rysowanym przez nas) kursorze... a właściwie: kursor
też jest w tym samym formacie, więc to, co widać jako „kursor", jest zbitą
plamą bitów, a nie kursorem VDI.

## Co z tego wynika dla wariantu B (korekta planu ekranu)

1. **Nie** wskazujemy EmuTOS-owi `_v_bas_ad` na VRAM karty (to jest właśnie
   źródło niezgodności formatu).
2. Dajemy mu **własną powierzchnię mono** w formacie, którego oczekuje:
   640×400×1 bpp, 80 bajtów/linia, w ST-RAM lub TT-RAM (32 000 B).
   To jest „ekran sesji" z §EKran — konkretnie: mono, nie chunky.
3. Nasz kompozytor **konwertuje** mono → chunky przy prezentacji w oknie
   (mamy do tego materiał: `system/emutos_vdi_mono.c`, blity w `wm_blit.c`).
4. Okno pozostaje viewportem: skala/przewijanie, stała rozdzielczość
   wewnętrzna 640×400 — tak jak w poprzedniej sekcji.
5. Pomiar kontrolny tego kroku jest **liczbowy, bez patrzenia na ekran**:
   zrzucić 32 000 B powierzchni sesji i sprawdzić, czy EmuTOS narysował tam
   strukturę (pulpir: menu, ikony) zamiast jednolitego wypełnienia.

## Wniosek

EmuTOS już traktuje VRAM karty jako swój ekran — czyli **pierwszy krok B
(„ekran sesji") nie polega na tworzeniu powierzchni, tylko na tym, żeby dać
EmuTOS-owi powierzchnię w JEGO formacie i przekonwertować ją u siebie**.
To zmiana mniejsza, niż wynikało z wcześniejszego planu, i mierzalna od razu.

---

# KOREKTA (ważna, na korzyść): to NIE jest niezgodność formatu

Poprzednia sekcja tłumaczyła „dither" niezgodnością formatu (mono planar
vs chunky). **To było błędne.** Źródła EmuTOS mówią inaczej:

```
bios/nova.c:252   EmuTOS wykrywa karte: sonduje 0xD00000+VIDSUB,
                  0xC00000, 0xC80000  (Hatari emuluje te rejestry)
bios/nova.c:261   novamembase = (UBYTE *)0x00C00000UL;
bios/nova.c:405   VGAREG_W(M64_CRTC_GEN_CNTL) = 0x0002;   /* 8 bpp */
bios/nova.c:733   v_bas_ad = (UBYTE *)novamembase;
bios/nova.c:295   ... czyta nawet identyfikator ukladu karty ('6','2')
```

Czyli EmuTOS **ma własny sterownik Nova/ET4000**, wykrył naszą kartę i ustawił
ekran **na jej VRAM w trybie 8 bpp chunky** — czyli **w tym samym formacie,
którego używa nasz kompozytor** (`kgfx_framebuffer()` zwraca `pitch` i `bpp`).
Żadnej konwersji nie trzeba.

Co w takim razie widzieliśmy na zrzucie? **Prawdziwy pulpit EmuTOS-a**:
jego charakterystyczne szare, ditherowane tło oraz **kursor myszy** VDI —
narysowane poprawnie, w naszym formacie, w VRAM karty. Brakuje na razie
menu/ikon, bo AES prawdopodobnie jeszcze kończy inicjalizację (albo czeka na
zasoby z `desk`).

## Co to znaczy dla wariantu B — plan ekranu się upraszcza

1. **Nie trzeba tworzyć powierzchni sesji ani konwertować formatu.** EmuTOS
   już rysuje w VRAM karty ET4000 w 8 bpp chunky — tam, gdzie rysuje nasz
   pulpit.
2. **Prezentacja w oknie** to blit z VRAM karty do okna (z przesunięciem
   i ewentualną skalą) — dokładnie ta operacja, którą kompozytor już wykonuje.
3. **Okno = viewport** na obrazie karty: skala 1×/2×/„fit" albo przewijanie;
   rozdzielczość wewnętrzna stała (ustalana przez tryb karty). Bez zmian
   względem wcześniejszej sekcji.
4. **Uwaga o własności:** skoro EmuTOS sam programuje rejestry karty
   (`M64_CRTC_GEN_CNTL` itd.), to w trybie B trzeba mu **odebrać** te rejestry
   i dać powierzchnię — inaczej dwa systemy będą przestawiać tryb karty.
   Najprościej: w warstwie spinającej podmienić `nova.c` na nasz adapter,
   który wystawia EmuTOS-owi ten sam interfejs (adres VRAM + tryb 8 bpp),
   ale tryb ustawia **raz**, po stronie ATOS.

## Wniosek

Ekran dla wariantu B jest **znacznie bliżej**, niż zakładałem: EmuTOS obsługuje
naszą kartę natywnie i w naszym formacie. Do zrobienia zostaje własność
rejestrów karty i prezentacja w oknie — a nie pisanie sterownika ekranu.
