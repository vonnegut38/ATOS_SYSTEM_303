# Model systemu: desktop graficzny + shell + wielozadaniowość

Odpowiedź na pytanie: czy da się zrobić system, który po starcie daje **graficzny desktop
(wygląd EmuTOS)**, z którego można **wrócić do shella** (jak w Windows 3.11), a przy tym
ma **wielozadaniowość jak FreeMiNT** — aplikacja GEM działa w oknie jako aktywna, a my
przechodzimy do okna plików i uruchamiamy równolegle drugą, z przyciętymi zasobami.

## 1. Odpowiedź krótka: TAK, i u nas to prostsze

FreeMiNT musiał **dokleić** wywłaszczanie do GEMDOS-u (który był jednowątkowy).
Nasz kernel ASF **już jest** wywłaszczający, ma IPC (mailbox), procesy, timer, usługi,
surfaces, damage i blitter. Do wielozadaniowego GEM brakuje **jednej dużej rzeczy: AES**
(oraz pełnego VDI) — nie brakuje fundamentu.

## 2. Wzorzec, który kopiujemy: FreeMiNT + XaAES

Tak działa realny system, o który pytasz:
- **FreeMiNT** = kernel: wielozadaniowość, GEMDOS-kompatybilny FS, PCI/sterowniki,
  priorytety (nice), kolejki.
- **XaAES** = wielozadaniowy AES: każdej aplikacji GEM daje **własne okna**, własny
  kontekst, własną kolejkę komunikatów; aplikacja śpi w `evnt_multi`, aż przyjdzie zdarzenie.
- **Shell/konsola jako okno** — XaAES ma okna konsol (kms). To dokładnie „DOS box" z Win 3.11.
- **Pełny ekran tekstowy** = powrót do konsoli (u nas: tryb tekstowy kernela, który mamy).

Czyli nie wymyślamy — przenosimy sprawdzony model.

## 3. Trzy stany ekranu (przełączane, nie wykluczające się)

1. **Desktop graficzny** (AES): tło, ikony/dyski, okna aplikacji, pasek menu. Wygląd EmuTOS
   (mamy zasoby/fonty EmuTOS w `emutos_resources.inc`, `emutos_vdi_mono`).
2. **Shell jako OKNO** na tym desktopie: konsola kernela (siatka znaków 80x25) renderowana
   do surface'u okna (`term_render_mono` — już to mamy). Odpowiednik „wiersza poleceń"
   w oknie Windows.
3. **Pełnoekranowy tekst** (konsole kernela + shifter): „powrót do DOS". Uruchamiamy z
   pulpitu, wychodzimy z powrotem — dokładnie jak Alt+Enter w Win 3.11.

Kluczowe: to są **trzy widoki tego samego systemu**, nie trzy systemy. Grafika i tekst
nie wykluczają się, bo tekst to po prostu kolejne okno (albo pełny ekran).

## 4. Wielozadaniowość: co dokładnie trzeba

| Wymóg | Mamy? | Uwaga |
|---|---|---|
| wywłaszczanie + kwant | ✅ PROC (RR, pełny kontekst) | to serce sprawy już jest |
| IPC/mailbox + oczekiwanie | ✅ EVENT/PROC | `evnt_multi` = sen na mailboxie |
| timer 50 Hz, `sleep_ticks` | ✅ TIMER | do budżetowania/odroczeń |
| priorytety (tło nie bierze 100%) | ⚠️ jest RR, brak nice | dodać pole priorytetu w PCB |
| powierzchnie + usługa grafiki | ✅ kgfx + ksurface | |
| damage + kompozytor | ⚠️ damage jest, kompozytora brak | patrz PROJEKT-PULPIT-AES.md |
| AES (okna, komunikaty, obiekty) | ❌ | to główna praca |
| VDI pełne (workstation, clip) | ⚠️ mono VDI + zaślepka w personie | do rozbudowy |
| loader .PRG + trapy GEM | ✅ persona TOS | aplikacje GEM wchodzą „za darmo" |

**Przycinanie zasobów** robimy tak, jak MiNT: pole priorytetu/nice w PCB + kwant.
Aplikacja w tle dostaje mniejszy kwant (np. 1 tick na 4), aktywna — pełny. Dodatkowo
`sleep_ticks` w pętli `evnt_multi` sprawia, że aplikacja bezczynna **nie zjada CPU**
w ogóle — to jest naturalne i nic nie trzeba „przycinać" ręcznie.

## 5. Gdzie pasują sesje `tts` (i to rozwiązuje nasz wcześniejszy problem)

Wcześniej próbowaliśmy dać każdej sesji **osobny ekran graficzny** — to zamroziło system,
bo grafika ma jeden framebuffer. Właściwy model:

> **Sesje to konsole, a konsole to okna na JEDNYM pulpicie.**

Czyli `tts1`, `tts2`, `tts3` = trzy okna konsol (albo zakładki) w jednym desktopie.
Commander może być oknem, SysInfo oknem, a shell oknem obok — **bez walki o framebuffer**,
bo wszystkie rysują do własnych surface'ów, a kompozytor składa ekran.

To jest ta sama architektura, którą opisałem w PROJEKT-PULPIT-AES.md — i ona załatwia
zarówno „wiele konsol", jak i „aplikacja GEM w oknie".

## 6. Scenariusz użytkownika, o który pytasz — krok po kroku

1. Boot → desktop graficzny (EmuTOS look) + okno shella (`tts1`).
2. W shellu: `command.app` → aplikacja GEM/ASF **w oknie**, aktywna.
3. Klik w okno plików (albo w okno shella `tts2`) — fokus się zmienia, Commander
   przestaje być aktywny (dostaje mniej kwantu), ale **żyje i zachowuje stan**.
4. W shellu: `sysinfo.app` → **druga aplikacja w drugim oknie**, równolegle.
5. Obie widoczne, obie reagują po kliknięciu; tło dostaje resztę CPU.
6. Przesuwanie okna z zawartością = blit surface'u (patrz PROJEKT-PULPIT-AES.md §4.5).
7. „Powrót do DOS": pełnoekranowa konsola tekstowa, i z powrotem do pulpitu.

## 7. Pułapki (uczciwie)

1. **Aplikacje piszące wprost do `Physbase()`** — omijają VDI i psują izolację okien.
   Mitygacja: aplikacje okienkowe dostają od AES surface okna jako „ekran"; aplikacje
   pełnoekranowe (gry, dema) uruchamiamy jako pełny ekran, nie w oknie. Tak robi też
   MiNT+XaAES.
2. **Globalny stan AES w aplikacji** — część programów zakłada, że są jedyne. Mitygacja:
   kontekst per PID i `appl_init`/`appl_exit` jak w XaAES.
3. **Blokujące I/O** — nie może wstrzymywać kompozytora: kompozycja w pętli kernela,
   I/O w zadaniach.
4. **Wydajność shiftera planarnego** — pełny redraw zabija płynność; dlatego damage +
   blitter/LUT, a docelowo ET4000 (chunky) jako ścieżka szybka.
5. **Regresje** — każdy etap z testem w Hatari; żadnych zmian w grafice „w locie"
   (nauczka z dzisiejszego zwisu przy per-sesyjnym buforze).

## 8. Kolejność prac (małe kroki, każdy testowalny)

| Krok | Co | Efekt widoczny |
|---|---|---|
| M1 | priorytet/nice w PROC + kwant tła | aplikacja w tle nie zjada CPU |
| M2 | kompozytor + damage na jednym ekranie | dwa okna bez artefaktów |
| M3 | okno konsoli (siatka znaków → surface) | **shell jako okno** (Win 3.11) |
| M4 | VDI: workstation per okno + clip list | aplikacja rysuje w swoim oknie |
| M5 | AES: `wind_*`, mover/closer, `evnt_*`, `WM_*` | **ruch okna z zawartością** |
| M6 | trap #2 → nasz AES dla `.PRG` | **istniejąca aplikacja GEM w oknie** |
| M7 | DESKTOP.APP: ikony, `.INF`, `shel_*` | pulpit jak TOS |
| M8 | pełny ekran tekstowy z pulpitu (toggle) | „powrót do DOS" |

## 9. Wniosek

**Da się, i nie jest to żadna egzotyka** — to architektura FreeMiNT+XaAES, tylko u nas
prostsza, bo wielozadaniowy kernel już mamy, a aplikacje GEM wchodzą przez istniejącą
personę TOS. Największa praca to **AES i VDI**, nie system.
