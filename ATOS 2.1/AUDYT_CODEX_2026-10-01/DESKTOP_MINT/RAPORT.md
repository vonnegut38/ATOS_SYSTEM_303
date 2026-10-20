# ATOS 2.1 — rozruch i pulpit, audyt 2026-10-01

## Najważniejszy wniosek

Są konkretne przyczyny spowolnienia poza samą większą liczbą pikseli. Najważniejsze znalezione w tej wersji to wyłączanie cache instrukcji 68020/030 przy synchronizacji kodu oraz wielokrotne obliczanie rozmiaru bufora podczas jego zerowania. Dodatkowo tekst 8 bpp nadal trafiał do ogólnej pętli pikselowej. Osobną klasą problemów jest zbyt duży obszar odrysowywania po małych zmianach interfejsu.

800×600×8 to 480 000 bajtów obrazu; 640×400×1 to 32 000 bajtów, czyli 15 razy mniej. To wyjaśnia część różnicy, lecz nie usprawiedliwia kosztów opisanych poniżej. Różnica względem konfiguracji użytkownika z MiNT nie została zmierzona bezpośrednio: analizowano źródła MiNT, XaAES, TeraDesk i AES EmuTOS, a uruchamiano ATOS w lokalnym Hatari.

## Co już poprawiono w ATOS

Poprzednie poprawki rozruchu są obecne: ważność cache terminala, pomijanie kopiowania VRAM do samego siebie, poprawne granice damage dla 8 bpp i ograniczenie zbędnego przewijania konsoli. Są też 32-bitowe wypełnienia tła i części procedur kompozytora, bufory aplikacji preferujące TT-RAM oraz wyłączone wymuszanie pełnej kompozycji w każdym obiegu. Nie należy przedstawiać tych rzeczy jako nadal brakujących.

Podczas audytu, o 22:23, zmienił się aktywny `system/kernel_graphics.c`: DeepSeek dodał 32-bitowe wypełnianie `surface_fill`. Paczka uwzględnia tę zmianę. Pomiary bazowe dotyczą kopii sprzed tej równoległej zmiany. Patch zastępuje ten kod wspólną, przetestowaną procedurą; nie przywraca starej pętli bajtowej.

## Poprawki przygotowane w tej paczce

1. **Cache instrukcji CPU.** `system/kernel_entry.S`, `kernel_image_cache_sync`: dotychczasowe `moveq #8` + `movec ...,cacr` kasuje zawartość I-cache i pozostawia go wyłączonym. Procedura jest wywoływana podczas ładowania modułów i programów. Wariant w paczce zapisuje `9`: kasowanie + włączenie I-cache dla 68020/030. Cache danych pozostaje wyłączony, a ścieżki 68000/010 i 040/060 nie zostały zmienione. To jawna polityka ATOS, nie ogólna procedura zachowująca wszystkie ustawienia CACR. Przy przyszłym włączeniu D-cache trzeba ją przebudować oraz rozwiązać DMA/MMIO. Nie zmieniano ROM-u ani przejścia do innego systemu w `boot/ramjump.S`.

2. **Tworzenie powierzchni aplikacji.** `kernel_wm_app_surface` zerował bajty z warunkiem `i < wm_surface_bytes()`. Deasemblacja potwierdziła wywołanie funkcji w pętli, a nie wyniesienie obliczenia przez kompilator. Dla 480 000 bajtów warunek jest sprawdzany 480 001 razy. Poprawka oblicza rozmiar raz, wyrównuje adres i zapisuje po cztery bajty. Pomaga przy pierwszym pulpicie i nowych powierzchniach okien. Nadal używane są bufory wielkości ekranu — zmniejszenie ich do rozmiaru klienta wymaga zmiany kontraktu współrzędnych.

3. **Tekst 8 bpp.** W `kgfx_text` pełny, mieszczący się na powierzchni znak ma osobną procedurę chunky. Opaque text na wyrównanych wierszach zapisuje osiem pikseli dwoma zapisami 32-bitowymi. Tablica 16 wzorców po cztery piksele zajmuje 64 bajty i jest aktualizowana po zmianie kolorów. Przezroczystość i niewyrównane wiersze zachowują poprawną ścieżkę bajtową; znaki przycięte pozostają przy dotychczasowym sprawdzaniu granic. Nie zmieniano renderera mono.

4. **Błędy formatu pikseli.** `kgfx_copy_rect` przekazywał obraz chunky do funkcji planar/mono, bo `planes==1` jest używane również dla 8 bpp. Nowa gałąź jawnie obsługuje chunky i nakładające się prostokąty. Pionowe linie również omijają procedurę planar w 8 bpp. To poprawki poprawności, nie deklaracja szybszego przewijania: kopiowanie chunky jest na razie bezpieczną pętlą bajtową.

5. **Wspólne wypełnianie i krawędź tła.** Nowy `include/chunky_raster.h` obsługuje wyrównanie i końcówki. W tle poprawia również lewą, niewyrównaną krawędź, która nadal zapisywała dawny wzór 0/1 zamiast jednolitego koloru 2.

## Największy następny etap: mniej odrysowywania

| Miejsce | Obecne zachowanie | Zalecana zmiana |
|---|---|---|
| `desktop_wm.c`, `paint()` | Lista zaczyna się od `GUI_BG` całego ekranu; wywołują ją m.in. wybór ikony i zamykanie menu. | Osobne malowanie zmienionego elementu i przywrócenie obszaru zamkniętego menu z zachowanej powierzchni pulpitu. |
| `desktop_wm.c`, `fb_paint()` | Zmiana zaznaczenia może odmalować cały klient i wszystkie wiersze. | Odmalować stary i nowy wiersz; przy przewijaniu skopiować zachowaną część i dorysować odsłonięty pas. |
| `kernel_wm.c`, dekoracje | `clip=r` mimo zgromadzonej listy damage; dotknięcie klienta uruchamia malowanie całej dekoracji. | Przeciąć damage z pasami ramki i tytułu; wszystkie pomocnicze funkcje muszą respektować clip. |
| `kernel_wm.c`, koniec przeciągania | `wm_move` już zgłasza stary i nowy obszar, potem `kernel_wm_damage_all` unieważnia cały ekran. | Usunąć pełne odrysowanie dopiero po testach zasłaniania, ramek i kursora. Można zacząć od poprawnego starego + nowego prostokąta, przed wdrożeniem screen blit. |
| `kernel_wm.c`, pasek | `topstrip_hit` jest obliczany, ale ignorowany; pasek jest malowany po każdej kompozycji. | Odświeżać po zmianie zegara/stanu lub przecięciu damage z paskiem, z zachowaniem kolejności save/restore kursora. |
| `kgfx_draw_batch` | Jedna obwiednia całej listy może objąć duży pusty obszar. | Przekazywać małą listę regionów z kontrolowanym scalaniem, a nie zawsze jeden bounding box. |

Przykład skali: dwa wiersze o szerokości 600 i wysokości 16 to 19 200 pikseli zamiast pełnych 480 000 — 25 razy mniej powierzchni. To rachunek obszaru, **nie zmierzony 25-krotny wzrost wydajności systemu**. Nie należy usuwać pełnych odrysowań bez zapewnienia odtworzenia odsłoniętych pikseli.

## Błędy wymagające osobnej poprawki

- **P1: nowy tryb 1024×768×8 przekracza stały bufor pulpitu.** `KGFX_MAX_FRAME_BYTES` wynosi 480 000, a `kgfx_video_reconfigure_format` czyści `line_bytes * height`, czyli 786 432 bajty w tym trybie. Zwiększyć/reallokować bufor przed zmianą trybu oraz obsłużyć niepowodzenie, albo odrzucić tryb przed programowaniem karty. Sama poprawka `wm_surface_bytes` nie wystarcza.
- **P1: awaryjna ścieżka backbuffer w `kgfx_draw_batch`.** Alokacja nadal używa `SCREEN_BYTES` (32 000), a zakres kopii liczy grupami 16 pikseli. To nie pasuje do chunky ani większego mono. Warunek dotyczy docelowego `desktop_framebuffer` lub pracy bez WM; zwykły batch na osobnej powierzchni WM go omija. Nie uznaję tego za udowodnioną przyczynę opisanego spowolnienia, lecz jest to ryzyko wyjścia poza bufor w tej ścieżce.
- **P2: `wm_damage_add` gubi nowy prostokąt po zapełnieniu listy.** W razie przepełnienia trzeba scalić regiony lub przejść do pełnego damage. Test powinien podać ponad limit rozłącznych regionów i sprawdzić pokrycie wszystkich zmian.
- **P2: `wm_move_live` zgłasza tylko pasy różnicy starego i nowego obszaru.** Taka redukcja wymaga poprawnego przeniesienia pikseli obszaru wspólnego albo odrysowania nowego położenia. `wm_move` używane przy zwolnieniu przycisku zgłasza oba pełne prostokąty; to inna ścieżka.
- Liczniki nazwane `*_cycles` korzystają z `kernel_user_hires_clock`: to takty Timer C, nominalnie 12 288/s, nie cykle CPU. Przy przerwach w odpytywaniu dłuższych od okresu timera można zgubić przepełnienia. Nie wyznaczać z nich FPS ani dokładnego czasu długiego renderowania bez naprawy pomiaru.

Te punkty nie zostały po cichu włączone do patcha wydajnościowego. Wymagają testów zmiany rozdzielczości, ograniczonej pamięci, zasłaniania i kursora.

## Co wynika ze źródeł AES / XaAES / TeraDesk / MiNT

- **TeraDesk:** `screen.c:move_screen` używa `vro_cpyfm(S_ONLY)`. `window.c:w_scroll` przechodzi po widocznych prostokątach, kopiuje pozostałą część okna i odrysowuje odsłoniętą linię/kolumnę. `window.c` podczas redraw przecina żądany obszar z widoczną listą i ustawia clip. To dobry wzorzec dla `fb_paint`, nie powód do portowania całego desktopu. [screen.c](https://github.com/freemint/teradesk/blob/8035b6e157bd34cb2bd72a26473990c7711501de/screen.c), [window.c](https://github.com/freemint/teradesk/blob/8035b6e157bd34cb2bd72a26473990c7711501de/window.c).
- **XaAES:** `move_window` / `set_and_update_window` obsługują widoczne regiony, przenoszenie i odświeżanie okien poniżej. `xa_vdi.c` korzysta z VDI także przy kopiowaniu ekranu. ATOS powinien przenieść zasadę ograniczania operacji, z własnymi procedurami mono/chunky i kontrolą widoczności. [c_window.c](https://github.com/freemint/freemint/blob/4fdd4167065c1247f6449fc23779321b9064f4f8/xaaes/src.km/c_window.c), [xa_vdi.c](https://github.com/freemint/freemint/blob/4fdd4167065c1247f6449fc23779321b9064f4f8/xaaes/src.km/xa_vdi.c).
- **AES EmuTOS:** `gemwmlib.c` w ścieżce przesuwania używa `bb_screen` do przeniesienia dostępnego obrazu i naprawia odsłonięte brzegi. Ta optymalizacja nie wymaga wielozadaniowego MiNT. [gemwmlib.c](https://github.com/emutos/emutos/blob/e0f0c2333d39a487e73d8b17f62b70a95441cd32/aes/gemwmlib.c).
- **FreeMiNT:** `sys/arch/cpu.S` rozdziela politykę cache według procesora; procedury flush zachowują/odtwarzają ustawienia CACR. `sys/proc.c` realizuje planowanie i oczekiwanie. Sam scheduler nie zastąpi sprawnego renderera i sterownika VDI. Nie ma podstaw, aby proponować wymianę kernela jako pierwszą naprawę grafiki. [cpu.S](https://github.com/freemint/freemint/blob/4fdd4167065c1247f6449fc23779321b9064f4f8/sys/arch/cpu.S), [proc.c](https://github.com/freemint/freemint/blob/4fdd4167065c1247f6449fc23779321b9064f4f8/sys/proc.c).

Wnioski są oparte na kodzie źródłowym, nie na pomiarze konkretnego zestawu MiNT + VDI użytkownika. Nie kopiowano kodu tych projektów do poprawki.

## Kolejność dla DeepSeek

1. Scalić i zbudować tę paczkę na osobnej kopii. Sprawdzić cache instrukcji na docelowym 030 i wielokrotne ładowanie/relokację programów. Zachować możliwość powrotu do bazowego kernela.
2. Naprawić granice buforów i przepełnienie damage przed dalszym rozszerzaniem trybów.
3. Dodać mierzenie etapów: wejście w kernel, wejście w pulpit, pierwszy batch, zakończenie kompozycji i flush, zdarzenie wejścia → koniec odpowiadającego mu flush. Mierzyć procent powierzchni odrysowanej, bajty do VRAM, liczbę batchy i kompozycji.
4. Wprowadzić lokalne odrysowanie zaznaczenia i menu. Następnie przewijanie z kopią istniejących pikseli. Potem ograniczenie dekoracji i paska.
5. Dopiero po tych zmianach profilować wariant kodu dla 68030, rozmieszczenie buforów, obsługę niezgodnego wyrównania źródła/celu i ręczny assembler. Nie włączać automatycznie blittera ST dla pamięci karty ET4000/TT-RAM.
6. Przenieść długie diagnostyki startowe do osobnego trybu dopiero po pomiarze ich udziału. Pozostawić kontrolę formatu, rozmiaru, sumy kontrolnej i wymaganych usług modułów.

Kryterium odbioru: ten sam ROM, CPU, RAM, sterownik, rozdzielczość i obraz dysku; trzy powtórzenia. Testować otwieranie/zamykanie menu, wybór pliku, przewijanie, nakładające się okna, przeciąganie poza ekran, kursor nad paskiem, mono↔kolor i brak wolnej pamięci. Raportować medianę i najgorszy czas odpowiedzi, nie sam wynik małego fill benchmarku.
