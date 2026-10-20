# ATOS 030 — przekazanie prac DeepSeek

## Szybki kontekst

Cel użytkownika: własny system ATOS na Atari klasy ST/STE z 68030/PMMU i ET4000/W32i, startujący z własnego ROM, z kernelem i większymi elementami na HDD. TOS/EmuTOS nie są bazą nowego systemu. Zgodność TOS jest późniejszym, osobnym projektem. Dokładny akcelerator, RAM i adapter wymagają potwierdzenia.

Repozytorium: `/Users/xxx/Documents/Deepseek_test`; główny projekt: `commander-v10`. Audyt z 24.09.2026 dotyczy HEAD `2861cec` plus lokalne zmiany zapisane w evidence/. Nie zakładaj czystego Git. Nie używaj `git add .`, reset/clean ani nadpisywania obrazów użytkownika. Jeżeli pracujesz gdzie indziej, poproś o źródła zgodne z manifestem. Ten pakiet zawiera dokumentację i dowody, nie pełne źródła systemu.

Werdykt: zachować kernel jako bazę warunkową. Są już natywne procesy, preemption, IPC, syscall trap #0, allocator blokowy ST/TT, loader ASYSAPP0, moduły ASYSMOD0, FAT, cache, GUI/WM i VOFA mono. Brakuje kwalifikowanej izolacji PMMU i kompletnej produkcyjnej grafiki 8 bpp. Nie pisać tych istniejących podsystemów od nowa.

## Co wykonano

Wyłącznie audyt oraz testy osobnej kopii. Nie naprawiono kodu produkcyjnego. `gmake -j4 all` przechodzi. `gmake check`: 10 C PASS; discovery 15 testów, 4 błędy. Dodatkowe testy wykazały realny overflow w terminalu, kilka nieaktualnych harnessów i rozjazdy kontraktów FAT/video. Dostępne logi w evidence/.

Nie wykonano nowego startu Hatari ani testu sprzętowego. Screenshoty/commit „desktop działa” nie potwierdzają aktualnego obrazu natywnego ATOS. Nie zmieniono żadnych nośników.

## Najważniejsze fakty

1. `text_terminal.c:term_scroll` czyta `cells[i+cols]` do granicy `i+1<total`, poza buforem; dodatkowo fill używa stride 80, inne operacje cols.
2. `exception_format.h` błędnie interpretuje ramki 030; obecny test potwierdza błędny model. Niezależny repro jest w pakiecie.
3. `kernel_entry.S:kernel_timer_c` wywołuje personę TOS. Są trzy modele zadań: PROC.SYS, natywne procesy i TOS scheduler.
4. `kernel_exception.c` ma 6-slotową tabelę natywnych procesów, IPC, syscalle i GUI. Ma być stopniowo rozdzielony po zabezpieczeniu testami, nie przepisany.
5. Flagi kmem nie są ochroną PMMU. Allocator rezerwuje pamięć dla TOS; ma ryzyka zakresów i przepełnień.
6. `boot/mmu_boot.c` to eksperyment bootowania ROM, z błędnym endian CRP i częściowym mapowaniem. Nie używać go jako gotowego VM.
7. Nowe `video/et4000.c` to szkielet, nie działające 1024×768×8. Działająca statycznie ścieżka VOFA jest mono 640×480 z widokiem 640×400.
8. `gmake check` nie obejmuje wszystkich dodatkowych testów. Nie wolno ogłaszać pełnego PASS po samym tym celu.

## Co przeczytać

1. Ten plik.
2. Wybraną kartę w ZAKRES_PRAC.md.
3. Odpowiedni fragment ATOS_030_V2_AUDIT.md i log konkretnego testu.
4. Tylko potrzebne źródła. Szerszy audyt dopiero, gdy pojawi się nowa sprzeczność.

## Pierwsze zadanie, jeżeli przekazanie nastąpi teraz

Wykonaj wyłącznie B1: naprawa przewijania i jednoznacznego układu komórek terminala. Najpierw odtwórz `gmake check-terminal` na izolowanej kopii. Zachowaj kompatybilność istniejących odbiorców terminala. Dodaj testy pełnej i zwężonej geometrii, przewijania, czyszczenia i granic. Uruchom sanitizers, check-commander i check-gui-damage. Nie ruszaj kernela, MMU, persony TOS ani wyglądu desktopu. Opisz różnicę i wyniki. Nie zaczynaj B2 bez kolejnego zadania.

Jeżeli B1 jest już zakończone, kieruj się AKTUALNY_STAN.md; nigdy nie powtarzaj naprawy tylko dlatego, że ten historyczny handoff ją opisuje.

## Raport po każdym zadaniu

- Identyfikator zadania i baza Git/hash.
- Zmienione pliki oraz powód.
- Komendy testów i wyniki; osobno niewykonane testy.
- Zmiany API/ABI — domyślnie brak.
- Pozostałe ryzyka i dokładny następny krok.
- Commit lub patch ograniczony do zadania; bez obcych zmian użytkownika.

Nie osłabiaj testów dla zielonego wyniku, nie deklaruj obsługi sprzętu na podstawie mocka, nie dodawaj wielozadaniowego GEM/ST Box. Cała komunikacja i dokumentacja po polsku.
