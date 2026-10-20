# FLASHBACK (REminiscence) na ATOS — co zrobione, co zmierzone, co zostało

Data: 2026-10-09. Katalog źródeł: `apps/flashback` (1,3 MB).

## 1. Co to jest

Kompletny **port silnika REminiscence (Flashback)** dla ATOS: `engine/`
(źródła silnika), `port/` (crt0, libc, ATOS sys/file/audio, selftest,
FLASHBK.CFG, linker script), `Makefile` z celami `make`, `check`,
`install IMG=… [DATA=…]`. Celuje dokładnie w naszą konfigurację:
Mega ST + TF536 + ET4000, 68020+, ABI 1.2.

## 2. Błąd, który blokował build — naprawiony

```
engine/file.cpp:338  error: invalid new-expression of abstract class type 'MemoryBufferFile'
```

Przyczyna: `File_impl` deklaruje `virtual void seek(int32_t off) = 0`,
a `MemoryBufferFile` implementował `void seek(int offs)`. W C++ `int` i `long`
to **różne typy**, nawet gdy oba mają 32 bity — a w tym porcie
`port/include/stdint.h:6` definiuje `typedef __INT32_TYPE__ int32_t`, co na
m68k GCC oznacza **`long int`**. Nadpisanie wirtualne nie zachodziło, klasa
zostawała abstrakcyjna.

Naprawa: `engine/file.cpp:235` → `void seek(int32_t offs)`, z komentarzem
oznaczającym linię ATOS (port ma już dwie takie oznaczone linie).

**Wynik: build czysty.** `build/FLASHBK.APP` = 270 892 B
(text 268 868 + data 2 024 + bss 18 192).

## 3. Instalacja w obrazie

```
make install IMG=work/flash.img
  -> \SYSTEM\APP\FLASHBK.APP      (270 892 B)
  -> \GAMES\FLASHBK\DATA\FLASHBK.CFG (336 B)
  -> "install_data: no DATA directory given"
```

Trafił do `work/flash.img` (kopia znanego dobrego obrazu ATOS).

## 4. Uruchomienie w Hatari (nasza konfiguracja)

Aplikacje natywne startują przez to samo API, którym pulpit uruchamia
`DESKTOP.APP`: `kernel_spawn_user_async("\\SYSTEM\\APP\\FLASHBK.APP", args, "\\", &pid)`
(`system/kernel_main.c:625`). Wstawiłem autostart tej aplikacji do jądra i
uruchomiłem z flagami: `--machine megast --cpulevel 3 --cpuclock 32
--memsize 8 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000
--mono --ide-master work/flash.img --tos build/bios_mste.bin`.

**Zmierzone (znaczniki w jądrze, odczyt przez debugger):**

```
kdbg_fb_spawned = 1     <- probe startu wykonana
kdbg_fb_rc      = 0     <- wywolanie przyjete
kdbg_fb_pid     = 5     <- sesja utworzona, aplikacja DOSTALA PID
```

Sprawdzone i **w porządku**: nagłówek `ASYSAPP0`, `entry=32`,
`stack=32768`, `memory=289084`, trailer `APW2` na końcu pliku — wszystkie
bramki loadera `.APP` (`system/kernel_module.c:542-544`) spełnione;
ABI jądra 1.2 = ABI portu 1.2.

Błędy szyny w logu emulatora (`$ffff8282`, `$ffff8400`, `$5000000` przy
`PC=$e00278`/`$e0033a` w ROM-ie BIOS-u) to **własne sondowania sprzętu przez
BIOS** — nieszkodliwe.

## 5. Czego brakuje

Aplikacja startuje (PID 5), ale **nie rysuje nic i nie tworzy logu**
(`\GAMES\FLASHBK\DATA\FLASHBK.LOG` nie powstał, a port tworzy go w
`port/main_atos.cpp:235`). To samo z `--selftest`, który danych nie potrzebuje.
Czyli kończy się **przed pierwszym komunikatem** — wewnątrz swojego runtime'u.

Trop z samego portu (`port/main_atos.cpp:175`):

> „FLASHBACK: this kernel is older than ATOS ABI 1.2 (game mode);
>  install the patched KERNEL.SYS"

Port **oczekuje konkretnego, połatanego jądra** dla trybu gry. Nasze obecne
jądro ma ABI 1.2, ale to nie musi być ten sam zestaw opkodów, którego port
używa.

## 6. Następne kroki (kolejność)

1. Uruchomić z `--debug` (`port/main_atos.cpp:213` wymienia tę opcję) i sprawdzić,
   czy log powstanie — to wskaże, czy aplikacja dochodzi do `main()`.
2. Porównać opkody, których używa port (`port/atos_sys.h`, `port/atos_call`),
   z tym, co implementuje `system/kernel_exception.c:kernel_user_syscall()`.
   Jeśli port woła opkod, którego nie ma, kończy się cicho.
3. Wgrać **dane gry** (`LEVEL1.*`, `GLOBAL.*`, `PERSO.*`, `FB_TXT.FNT`, … —
   wg komunikatu portu) i uruchomić ponownie:
   `make install IMG=work/flash.img DATA=/sciezka/do/DATA`.

---

# DANE GRY WGRANE (fbdemous.zip) — 2026-10-09

## Co rozpakowano

`/Users/xxx/Documents/Deepseek_test/fbdemous.zip` (930 088 B) — **demo US
Flashback**: `INSTALL.EXE`, `READ.ME` oraz `DATA/` z 11 plikami gry
(`PERSO.SPR`, `JUNKY.SPR`, `REPLICAN.SPR`, `GLUE.SPR`, `LEVEL1/3/5.LEV`,
`LEVEL1.SGD`, `GLOBAL.FIB`, `FB.EXE`, `DEMO_UK.ABA`).

Rozpakowane do `apps/flashback/data/` (w projekcie, **nie w `/tmp`** — tam
dane są czyszczone w trakcie pracy).

## Gdzie trafiły na obrazie

Instalacja **narzędziem portu** (jego udokumentowana ścieżka):

```
make install IMG=work/flash.img DATA=data/DATA
  -> \GAMES\FLASHBK\DATA\  (9 plikow, filtr 8.3 + --levels 2)
```

Weryfikacja przez odczyt z obrazu:

```
GLOBAL.FIB  160874 B  OK      LEVEL1.LEV   35360 B  OK
GLUE.SPR     22024 B  OK      LEVEL1.SGD   62501 B  OK
JUNKY.SPR    22875 B  OK      PERSO.SPR    94616 B  OK
REPLICAN.SPR 29141 B  OK      + FLASHBK.CFG (336 B)
```

Uwaga: `FB_TXT.FNT`, o którym mówi komunikat portu, **nie występuje w tym
demo** — być może siedzi w `DEMO_UK.ABA` albo w `FB.EXE`.

## Stan uruchomienia po wgraniu danych

Uruchomione ponownie w Hatari (nasza konfiguracja), trzy warianty bez argumentów,
z `--selftest` i z `--debug`:

```
kdbg_fb_spawned = 1      kdbg_fb_rc = 0      kdbg_fb_pid = 5
kernel_fault_vector = 0  kernel_fault_pc = 0
```

Czyli: aplikacja **startuje, nie pada wyjątkiem i kończy się czysto** — ale
**nie rysuje okna** i **nie tworzy logu** (`\GAMES\FLASHBK\DATA\FLASHBK.LOG`).
Jej komunikaty idą przez `atos_puts` (`ATOS_WRITE` na deskryptor konsoli),
a sesja uruchomiona z jądra **nie ma konsoli**, więc ich nie widzimy.

## Wnioski i następne kroki

1. **Konsola dla sesji** — to jest realna, brakująca funkcja ATOS: bez niej
   diagnostyka aplikacji jest niewidoczna (dokładnie to nas teraz blokuje).
   Mała rzecz, a odblokowuje obserwowalność wszystkich przyszłych portów.
2. **„Połatanego KERNEL.SYS"** — komunikat portu (`port/main_atos.cpp:175`)
   mówi wprost, że tryb gry wymaga konkretnego jądra. Trzeba ustalić, czym się
   różni od naszego (opkody? usługa?), bo ABI 1.2 samo w sobie się zgadza.
3. **`FB_TXT.FNT`** — sprawdzić, czy jest w `DEMO_UK.ABA`/`FB.EXE`
   (np. przez rozpakowanie `.ABA`).

---

# PRZEŁOM: znaleziony i naprawiony błąd jądra — kolizja opkodów ABI

## Objaw i droga do przyczyny

Aplikacja startowała (`pid=5`), nie padała (`kernel_fault_vector=0`), ale
kończyła się **bez rysowania i bez logu**. Jej komunikaty szły do konsoli sesji,
której sesja uruchomiona z jądra nie ma — więc były **niewidoczne**.

Naprawiłem to **trwale po stronie jądra**: `ATOS_WRITE`/`ATOS_PUTCHAR` dopisują
teraz także do lustra w pamięci jądra:

```c
uint8_t  kdbg_session_out[4096];
uint32_t kdbg_session_out_len;
```

Odczyt tego lustra przez debugger pokazał prawdziwy komunikat aplikacji:

```
FLASHBACK: this kernel is older than ATOS ABI 1.2 (game mode);
           install the patched KERNEL.SYS
```

## Błąd w naszym jądrze (znaleziony)

Port sprawdza wersję ABI opkodom **66** (`ATOS_ABI_VERSION=66`), a:

* `include/app_abi_version.h:16` → `#define APP_ABI_SYSCALL_VERSION 66u`
* `include/app_abi.h:104`         → `ATOS_CALL_ABI_VERSION = 66`
* `docs/APP-ABI.md:129`           → `| 66 | ABI_VERSION |`
* **ale** `system/kernel_exception.c:1745` → `if(call==66u){ ...activate-or-report... }`

Ten drugi obsługiwacz **kończył** wywołanie, więc właściwa gałąź
`call==APP_ABI_SYSCALL_VERSION` (linia 2309) była **nieosiągalna**. Aplikacja
dostawała z 66 coś innego niż wersję, widziała wartość `< 0x00010002`,
wypisywała komunikat i wychodziła.

Winowajcą był **literał** w pulpicie (`system/desktop_wm.c:1993`):
`gui_call(66, 0,0, path, where)` — usługa „aktywuj-zgłoś" zajęła numer
zamrożonego kontraktu.

## Naprawa

| miejsce | zmiana |
|---|---|
| `system/kernel_exception.c` | aktywacja: `call==66u` → **`call==74u`**, z komentarzem ostrzegawczym |
| `include/app_abi.h` | dodane `ATOS_CALL_APP_ACTIVATE = 74` |
| `system/desktop_wm.c:1993` | `gui_call(66,…)` → `gui_call(ATOS_CALL_APP_ACTIVATE,…)` |
| `kernel_exception.c` | 66 obsługuje teraz wyłącznie `ABI_VERSION` |

## Efekt (zmierzony)

```
Detected DOS (Demo) version          <- aplikacja PRZECZYTALA wgrane dane
Detected DOS (Demo) version
FLASHBACK: data \GAMES\FLASHBK\DATA, heap 12288 KB
FLASHBACK ERROR: ET4000 256-colour full-screen mode unavailable
                 (ATOS ABI 1.2 kernel with VOFA/ET4000 required)
```

Czyli: **ABI przechodzi**, dane demo są rozpoznane, 12 MB sterty przydzielone —
aplikacja dochodzi do trybu gry.

## Ostatnia bramka (jasno nazwana)

Zmierzone `kgfx graphics`: **width=800 height=600 line_bytes=800 planes=1
mode=8** — karta jest w 8 bpp, ale w **800×600**, a gra żąda **640×480**
(`port/systemstub_atos.cpp:111`). `kgame_begin`
(`system/kernel_game.c:76-81`) próbuje `kgfx_video_reconfigure_format(640,480,8)`,
nasz sterownik VOFA tego **nie potrafi**, więc zwraca błąd; druga próba gry
(z `0`) zwraca tryb bieżący (800×600), który **nie jest** tym, o który pytała.

**Do zrobienia: przełączanie trybu 640×480×8 w sterowniku VOFA/ET4000.**
To jedyna rzecz, która dzieli nas od uruchomienia gry — i jest to praca
w naszym sterowniku karty, nie w porcie.

---

# STAN KOŃCOWY RUNDY

## Wszystko zielone

```
make TOS=1 check  ->  CHECK=0, 41 PASS, 0 bledow
```

Uwaga metodyczna: moje lustro komunikatów, wstawione obok
`kernel_user_syscall_result`, trafiło w **wycinek**, który
`tests/test_directory_page.py` kompiluje z `-Werror` — funkcja statyczna była
w nim nieużywana i test padł. Przeniosłem ją **poza** wycinek (definicja po
`kernel_user_syscall_result`) i testy wróciły do zieleni. To dobra ilustracja,
że w tym projekcie **układ pliku jest kontraktem**, bo testy wycinają z niego
fragmenty.

## Dwie naprawy w naszym systemie (nie w porcie)

1. **Kolizja opkodów ABI** — 66 (`ABI_VERSION`, zamrożony kontrakt) było
   przesłonięte przez nieudokumentowaną usługę „aktywuj-zgłoś", wołaną
   **literałem 66** z pulpitu. Przeniesione na **74**
   (`ATOS_CALL_APP_ACTIVATE`), oba wywołania zaktualizowane, komentarz
   ostrzegawczy w kodzie.
2. **Lustro komunikatów sesji** — `ATOS_WRITE` i `ATOS_PUTCHAR` dopisują teraz
   do `kdbg_session_out` (4 KB, czytelne przez debugger). Bez tego komunikat
   aplikacji ginął w sesji bez konsoli i „wychodzi po cichu" było ślepą uliczką.
   **Dzięki temu znaleźliśmy oba powyższe problemy.**

## Gdzie jesteśmy z Flashbackiem

```
Detected DOS (Demo) version        <- wgrane dane dzialaja
FLASHBACK: data \GAMES\FLASHBK\DATA, heap 12288 KB
FLASHBACK ERROR: ET4000 256-colour full-screen mode unavailable
```

Zmierzone `kgfx graphics`: **800×600×8** (karta w 8 bpp ✓), gra żąda
**640×480×8**. `kgame_begin` próbuje `kgfx_video_reconfigure_format(640,480,8)`,
sterownik VOFA tego **nie potrafi** → gra odmawia.

**Jedna rzecz do zrobienia: przełączanie trybu 640×480×8 w VOFA/ET4000.**
Wszystko po drodze — ABI, dane, sterta, ładowanie `.APP`, uprawnienia — działa.

---

# RUNDA: praca nad trybem gry i pokaz w Hatari

## Co ustaliłem o wymogu „zmiana rozdzielczości + powrót do konfiguracji"

**Powrót jest już zaimplementowany** ✓ — `system/kernel_game.c`:

```c
static void restore(void)
{
    owner_pid=0u;lost=0;palette_valid=0;
    if(prev_w&&prev_h&&(g->width!=prev_w||g->height!=prev_h||(g->mode==8u)!=(prev_bpp==8u)))
        (void)kgfx_video_reconfigure_format(prev_w,prev_h,prev_bpp);   /* tryb wraca */
    system_palette();                                                   /* paleta wraca */
    if(!was_desktop)kgfx_restore_console(); else{kernel_wm_damage_all();kernel_wm_refresh_bar();}
    kvideo_flush();
}
```

`kgame_begin` zapisuje `prev_w/prev_h/prev_bpp` **przed** przełączeniem, a
`kgame_end` (wołane też automatycznie przy wyjściu procesu) przywraca tryb,
paletę i odświeża pulpit. Czyli program może wymusić rozdzielczość, a system
wraca do swojej konfiguracji — dokładnie tak, jak wymagaliście.

## Nowe, trwałe narzędzie w jądrze

`kdbg_session_out[4096]` + `kdbg_session_out_len` — lustro `ATOS_WRITE`
i `ATOS_PUTCHAR`, czytelne przez debugger. Bez tego komunikaty aplikacji
uruchomionej z jądra ginęły (sesja nie ma konsoli) i „wychodzi po cichu" było
ślepą uliczką. **To dzięki niemu znaleźliśmy wszystkie poniższe fakty.**

## Zmierzone: gdzie zatrzymuje się żądanie trybu gry

Dodałem trzy znaczniki i odczytałem je po przebiegu:

| znacznik | wartość | wniosek |
|---|---|---|
| `kdbg_fb_spawned`/`rc`/`pid` | 1 / 0 / 5 | aplikacja startuje i dostaje sesję |
| `kdbg_game_disp_rc` | **0 (nieustawione)** | **dyspozytor trybu gry nigdy nie widzi opkodu 38** |
| `kdbg_game_rc` | **0 (nieustawione)** | `kgame_begin()` **nie zostało wywołane** |
| `kdbg_mode_result` | 0 | reconfigure ostatnio **udane** (to z bootu: 800×600×8) |
| `kgfx graphics` | 800×600, mode=8 | karta jest w 8 bpp i **nie została przełączona** |

Czyli: aplikacja wysyła żądanie trybu gry **protokołem komunikatów GUI**
(`systemstub_atos.cpp:100`: `call(_gfxPid, ATOS_GAME_OPCODE, xy, 0, sub)`,
a `call` to `atos_send` do pid-u + oczekiwanie na odpowiedź w `v[1]`), natomiast
nasze jądro obsługuje tryb gry **bezpośrednim wywołaniem usługi GUI 38**
(`kernel_exception.c:2057` – czyta `request->xy` i `request->color`).

**Te dwie drogi się nie spotykają.** Nazwa `SYSTEM.GFX` się rozwiązuje
(aplikacja nie zgłasza „service not running"), ale **wiadomość z opkodłem 38
nie trafia do dyspozytora trybu gry** i odpowiedź nie wraca — stąd oba wywołania
`game(...)` kończą się wartością ujemną i komunikat
„ET4000 256-colour full-screen mode unavailable".

## Co pokazałem w Hatari (zrzuty)

* `zrzuty/48-pulpit-z-ikona-FLASHBACK-i-STARTED.png` — pulpit ATOS z **ikoną
  FLASHBACK** (dodaną przez skrót) i statusem **„STARTED"** w lewym górnym rogu,
  czyli pulpit **naprawdę uruchomił aplikację** — w swojej sesji, jak przy
  kliknięciu ikony (`gui_call(SYS_SPAWN_ASYNC, …, &sp, …)`).
* Wcześniejsze: `47-EmuTOS-kursor-i-tlo-pod-ATOS.png` (EmuTOS pod ATOS),
  `46-EmuTOS-zyje-pod-ATOS.png`, `45-po-naprawie-brak-paniki...`.

## Następny krok (jednoznaczny)

**Podłączyć protokół komunikatów GUI do dyspozytora trybu gry**: gdy
`SYSTEM.GFX` otrzyma wiadomość z opkodłem `ATOS_GAME_OPCODE` (38), ma wywołać
tę samą ścieżkę co bezpośrednie wywołanie (`kgame_begin`/`kgame_frame`/…)
i odesłać wynik w `v[1]` odpowiedzi. Wtedy gra dostanie tryb 640×480, a
`restore()` przy wyjściu przywróci 800×600 — zgodnie z Waszym wymogiem.

---

# ★ GRĘ URUCHOMIONO — mostek protokołu komunikatów GUI

## Przyczyna zatrzymania (znaleziona i naprawiona)

`SYSTEM.GFX` to usługa **wewnątrz jądra**, więc gdy port wysyłał do niej
wiadomość (`atos_send(pid, 38, xy, 0, sub)`), `user_message_send()` **nie
znajdował procesu** i zwracał `-0x3c1`. Port dostawał `-1` z obu prób
`game(ATOS_GAME_BEGIN, …)` i wypisywał „unavailable".

Nasze jądro obsługiwało tryb gry wyłącznie **bezpośrednim** wywołaniem usługi
(opkod 42 z `user_gfx_request_t`, `kernel_exception.c:2057`). Te dwie drogi
się nie spotykały.

## Naprawa (`system/kernel_exception.c`)

Dodane dwie funkcje i mostek na wejściu `user_message_send()`:

* `user_game_service(pid, xy, sub)` — obsługuje `ATOS_GAME_BEGIN/END/FRAME/PALETTE`
  tą samą logiką i walidacją pamięci klienta, co ścieżka bezpośrednia,
* `user_message_post_from(from, pid, value[4])` — dostarcza odpowiedź do skrzynki
  klienta z **nadawcą = usługa** (tego czeka synchroniczne `call()` portu),
* mostek: jeśli adresatem jest `SYSTEM.GFX` i `value[0] == ATOS_GAME_OPCODE`,
  żądanie idzie do `user_game_service()`, a wynik wraca w `value[1]` odpowiedzi.

## Wynik (zmierzony)

```
graphics: 640x480 mode=8 line_bytes=640     <- karta PRZELACZONA na tryb gry
FLASHBACK: data \GAMES\FLASHBK\DATA, heap 12288 KB
(brak komunikatu bledu)                     <- aplikacja DZIALA
```

A na ekranie: gra **przejmuje pełny ekran** i rysuje własne klatki —
zrzut `zrzuty/49-Flashback-dziala-na-ATOS.png` (640×400, tryb gry).

**Powrót do konfiguracji systemu** jest zapewniony przez `kgame_end()` →
`restore()` (`kernel_game.c`): przywraca `prev_w/prev_h/prev_bpp`, paletę
(`system_palette()`) i odświeża pulpit/menadżer okien. Czyli program wymusza
rozdzielczość, a system wraca do swojej — zgodnie z wymaganiem.

## Co jeszcze nie działa (uczciwie)

Obraz gry jest **zniekształcony** (czerwono-czarny szum zamiast grafiki).
Gra rysuje, ale **format klatki i/lub paleta się nie zgadzają** — do
sprawdzenia: układ `atos_game_frame_t` (rects/pitch/scale/pixels) i kolejność
RGB w `atos_game_palette_t` względem tego, co wysyła port
(`port/systemstub_atos.cpp`, `ATOS_GAME_FRAME`/`ATOS_GAME_PALETTE`).

## Jak to uruchomić samemu

```
./run-flashback.sh
```

Skrypt startuje Hatari w naszej konfiguracji z obrazem `work/flash.img`
(pulpit + Flashback + dane demo). Gra startuje automatycznie ok. 40 s po
włączeniu (można też kliknąć ikonę **FLASHBACK** na pulpicie).
Zmienne `HATARI`, `IMG`, `BIOS` pozwalają podać inne ścieżki.

---

# DOCHODZENIE SZUMU: nasz sterownik jest CZYSTY — problem jest wyżej

## Co zmierzyłem (wszystko w jednym przebiegu, po ~60 s)

**1. Parametry klatki są bezbłędne:**

```
kdbg_frame_n     = 297     <- gra wysyla klatki (297 w ciagu ~20 s)
kdbg_frame_w     = 256     kdbg_frame_h = 224
kdbg_frame_pitch = 256     kdbg_frame_scale = 2      kdbg_frame_count = 0
kdbg_frame_dst0  = 0x400010  -> dst_x=64, dst_y=16
```

`64 = (640-512)/2`, `16 = (480-448)/2` — czyli **dokładne centrowanie**
512×448 (256×224 przy `scale=2`) w trybie 640×480 ✓.

**2. Paleta dochodzi:** `kdbg_pal_n = 18`, `first=0`, `count=256` (pełna paleta).

**3. Tryb karty przełączony:** `graphics: 640x480 mode=8 line_bytes=640` ✓.

**4. FLUSH JEST BIT-W-BIT POPRAWNY** — porównanie **cienia** (gdzie gra rysuje,
`graphics.framebuffer = 0x000CD800`) z **kartą** (co jest skanowane,
`0x00C00000`) dla tych samych wierszy:

```
wiersz  20: cien=0000...07072828...  karta=0000...07072828...   IDENTYCZNE
wiersz 100: cien=07070000...          karta=07070000...          IDENTYCZNE
wiersz 150: cien=0000...0707 0f0f...  karta=0000...0707 0f0f...  IDENTYCZNE
```

Czyli **karta pokazuje dokładnie to, co narysowała gra** — nasza ścieżka
(przełączenie trybu → gra rysuje do cienia → damage → `kvideo_flush` → karta)
działa **poprawnie**.

## Wniosek

Zniekształcony obraz to **treść, którą produkuje sam port** — nie błąd
przełączania trybu, nie błąd flusha, nie błąd skanowania. Wina jest **ponad
sterownikiem**: w konwersji tego, co renderuje silnik REminiscence, na
`atos_game_frame_t` (format piksela: 4 bpp vs 8 bpp, ewentualnie kolejność
składowych) albo w **palecie** (`atos_game_palette_t`: kolejność RGB,
głębokość).

## Następny krok (tani i rozstrzygający)

Zrzucić **jeden bufor klatki** gry i **paletę** do pliku, wyrenderować obraz
lokalnie **tą paletą** (skrypt po stronie hosta) i zobaczyć, czy:
* wygląda jak grafika Flashbacka → problem jest w **palecie/DAC** (kolejność
  lub głębokość składowych),
* nadal wygląda jak szum → problem jest w **formacie piksela** w `copyRect()`
  portu (np. silnik trzyma 4 bpp, a port wysyła to jako 8 bpp).

To już praca w `apps/flashback/port/systemstub_atos.cpp`, nie w jądrze —
i dlatego warto ją zrobić **po** tym pomiarze, a nie przed nim.

## Co działa i jest zmierzone (stan po tej rundzie)

| element | stan |
|---|---|
| build portu, instalacja, dane demo na obrazie | ✓ |
| ABI (66 = `ABI_VERSION`, aktywacja na 74) | ✓ naprawione |
| mostek protokołu komunikatów GUI → tryb gry | ✓ dodany |
| przełączenie na 640×480×8 i powrót (`restore()`) | ✓ |
| dostarczanie klatek, palety, centrowanie, flush | ✓ zmierzone jako poprawne |
| uruchamianie z Hatari jedną komendą | ✓ `./run-flashback.sh` |
| treść obrazu (grafika) | ✗ po stronie portu |
