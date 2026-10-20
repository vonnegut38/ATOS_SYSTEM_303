NAPRAWY PULPITU ATOS — 10 października 2026

Zmiany w źródłach ATOS 2.1:
- Podpisy ikon 32x32 i 48x48 wyśrodkowane względem bitmapy. Obszary
  odświeżania, usuwania oraz trafiania myszą uwzględniają nową geometrię.
- PLASMA: faza animacji nie nalicza reszty czasu dwukrotnie; każda narysowana
  klatka oddaje czas schedulerowi, żeby można było obsługiwać pozostałe okna.
- Pulpit uruchamia .PRG przez call 65 z d1=1. d1=0 pozostawia dotychczasowe
  wywołanie blokujące (m.in. istniejący Commander). Zwykła pętla pulpitu
  pozostaje aktywna podczas uruchomienia jednego programu TOS.
- Kontekst TOS ma prywatny stos kontynuacji 32 KB, właściciela okna i kolejki
  klawiatury/myszy. Zmiana kontekstu odbywa się przy wywołaniach OS, po około
  20 ms. Przełączane są USP, rejestry zachowywane przez C, VBR, CACR,
  wektory/niska pamięć 0x8..0x7ff i cel rysowania VDI. Adresów 0..7 nie zapisujemy.
- Podczas TOS IRQ Timer C i ACIA używają osobnego stosu 8 KB. To konieczne:
  pakowany SysInfo kopiuje krótki kod poniżej SP; wcześniej MOVEM i ramki
  funkcji C w ISR nadpisywały ten kod. Sprzętowa ramka wyjątku zostaje na
  stosie programu. Nie uruchamiamy starego tos_task_tick na ramce nowej domeny.
- TOS nie jest wznawiany podczas sesji innego pulpitu ani wyłącznego trybu gry.

Hatari:
Po warm reset ATOS rysował 800x600, ale host zachowywał powierzchnię 640x400.
ConvST_Refresh porównywał tylko zapamiętany tryb karty, który się nie zmienił.
Poprawka sprawdza rozmiar rzeczywistej powierzchni przez ConvGen_SetSize(...,
false) przy odświeżaniu karty. Pasująca powierzchnia nie jest tworzona ponownie.
Źródło poprawki: hatari-reset.patch w katalogu dowodów poniżej. Oryginalne
źródło osobnego drzewa hatari-src nie było zmieniane; przy jego kolejnym
przebudowaniu trzeba zastosować ten patch. Build binarny używa jego obecnych
obiektów oraz przebudowanego conv_st.c.

Potwierdzone testy:
- gmake TOS=1 check: sukces, w tym 50 testów unittest oraz testy C.
- SYSINFO.APP i SYSINFO.PRG równocześnie; zakładka DISK natywnego programu.
- PLASMA obok TOS: 41882 zmienione piksele w obszarze klienta między klatkami.
- Zamknięcie obu aplikacji i trzy kolejne uruchomienia/zamknięcia SysInfo TOS.
- Sonda irq-stack-probe.S: stare jądro kończy ją Illegal Instruction (vector 4),
  nowe wykonuje cztery kolejne próby bez błędu.

Granice rozwiązania:
To współpraca jednej domeny TOS z aplikacjami ATOS, nie izolacja sprzętowa
ani pełny scheduler wielu programów TOS. Gość dalej działa w supervisorze.
Kod nieoddający czasu przez OS może blokować pulpit. Nowa ścieżka przyjmuje
68000..68030; uruchomienia zweryfikowano na emulowanym 68030, bez FPU/MMU.
Pełna zgodność AES/VDI nie jest tutaj poświadczona: dodatkowa próba menu
SysInfo wykazała niepełne napisy. Natywny SYSINFO.APP nadal prezentuje
niespójne dane rozdzielczości/pamięci; rzeczywisty tryb w teście to 800x600x8.

Katalog dowodów, kopii i skryptów: /Users/xxx/Documents/Codex/2026-10-04/na-gorze-kolorysstycznie-kolok-przek-amae/work/desktop-repair-20261010
Patch ATOS: /Users/xxx/Documents/Codex/2026-10-04/na-gorze-kolorysstycznie-kolok-przek-amae/work/desktop-repair-20261010/desktop-repair.patch
Kopia jądra/emulatora sprzed zmian: KERNEL.SYS, hatari-before w tym katalogu.

WDROŻONE:
Obraz: /Users/xxx/Documents/Deepseek_test/ATOS 2.1/work/atos-v2.img
Na obrazie zmieniono tylko KERNEL.SYS, DESKTOP.APP i PLASMA.APP.
Manifest wszystkich 396 plików potwierdza pozostałe pliki bez zmian;
obie kopie FAT zgodne, brak skrzyżowanych łańcuchów.
BIOS, SYSINFO.APP, SYSINFO.PRG, Arkanoid i Flashback nie były podmieniane.
Emulator: /Users/xxx/Desktop/Flashback.app/Contents/MacOS/hatari-bin
Dodatkowo: warm reset 800x600, Arkanoid (HUD 41 FPS w próbie), menu Flashbacka.
Pełne sumy plików i ścieżka kopii obrazu: deployment.json.

Test po instalacji na głównym obrazie: native-first, następnie TOS, następnie
przywrócenie natywnego okna na wierzch — oba okna aktywne, fault_vector=0,
async_state=2. Emulator pozostawiony uruchomiony dla użytkownika.
Dodatkowa próba porównania menu ze starym jądrem nie rozstrzygnęła pochodzenia
brakujących napisów: stary SysInfo zatrzymał się już w rozpakowywaczu (RC -4).
Nie oznaczam więc błędu menu jako naprawionego ani jako potwierdzonej regresji.
Skrypty testowe uruchamiano w podanym wyżej oryginalnym katalogu dowodów;
kopie w work/desktop-repair-20261010 służą do wglądu, a nie jako osobny zestaw danych.
