# Optymalizacja menedzera okien (WM) — konkretnie, żeby nie zabiło systemu

Punkt odniesienia: XaAES/EmuTOS (mamy źródła w work/emutos-reference/aes/gemwmlib.c)
i EasyMiNT, gdzie okna z zawartością chodzą „dość dobrze” na tym samym sprzęcie.

## 0. Zasada nadrzędna
Kompozytor robi NAJWYŻEJ jedną klatkę na tick (50 Hz), i tylko gdy damage niepuste.
Wszystko poniżej jest po to, żeby ta jedna klatka była mała. XaAES robi dokładnie to:
zbiera uszkodzenia i maluje w wind_update(), a nie przy każdym zdarzeniu.

## 1. Dyscyplina uszkodzeń (damage) — największa dźwignia
- Każda zmiana zgłasza prostokąt, nigdy „całe okno”.
- Scalanie nachodzących prostokątów (mamy) + limit listy.
- Rysuj tylko to, co naprawdę się zmieniło:
  * konsola: obwiednia zmienionych komórek (ZROBIONE) — jeden znak = 8x16 px,
    zamiast 624x384 (~1870x mniej),
  * okno aplikacji: WM_REDRAW z prostokątami (jak XaAES) — aplikacja maluje tylko
    odsłonięte fragmenty, nie całą powierzchnię.
- Backpressure: jeśli damage rośnie ponad próg (np. 60% okna), NIE skalujemy go
  w nieskończoność — robimy pełne odświeżenie okna RAZ i czyścimy listę.

## 2. Powierzchnie per okno (backing store)
- Aplikacja maluje do swojego surface'u: zero sprawdzania zasłonięć przy każdej
  operacji VDI. To jest różnica wobec VDI piszącego wprost w ekran.
- Powierzchnie 32 KB (640x400 mono) na okno; limit + recykling (kmem) — nie
  alokujemy przy każdym otwarciu, trzymamy pulę.

## 3. Listy przycięcia (w_rlist) — liczone rzadko
- clip[] per okno wyliczany TYLKO przy zmianie Z, geometrii albo widoczności
  (mamy wm_rebuild_clip).
- Klatka używa gotowej listy: O(liczba prostokątów), nie „okno po oknie w pętli pikseli”.

## 4. Blity — dopasowane do formatu
| Format | Ścieżka szybka | Stan |
|---|---|---|
| mono planar | bajtowo/słowami; blitter ST dla większych obszarów | ZROBIONE (bajtowo) |
| chunky 8bpp (ET4000) | kopie 32-bit, wyrównane wiersze | ZROBIONE |
| wypełnienie tła | bajtowe span-fill / 32-bit | ZROBIONE |
- Wyrównanie 32-bit jest OBOWIĄZKOWE (68000/030 = address error) — mamy.
- Następny poziom: blitter dla kopii ≥ ~1 KB (planar), bo zwalnia CPU całkowicie.

## 5. Tekst
- Font 8x16, LUT, blit wiersza (16 słów na znak) — ZROBIONE.
- Scroll konsoli: jedno memmove + jeden nowy wiersz, nie 25 blitów glifów.
- Tryb 8x8 (gęstszy) = połowa danych na znak; zoom 2x = 4x danych, więc tylko
  dla widocznego okna i tylko na żądanie.

## 6. Kursor i ramki
- Kursor jako overlay (mamy): zero damage pod kursorem. XaAES odrysowuje tło —
  my tego nie robimy i to jest nasza przewaga.
- Ramka okna: rysowana raz do małego surface'u ramki (tytuł + gadżety) i blitowana;
  ponowne rysowanie tylko gdy okno się zmienia. To tanieje przy przeciąganiu.

## 7. Przeciąganie okna (kluczowy scenariusz)
Algorytm (mamy wm_move):
  now = frame + delta (klamrowane do ekranu)
  damage += stary ∪ nowy
  frame/src zaktualizowane, clip przeliczony
  JEDNA klatka na tick — nie na każdy pakiet myszy
- Pakiety myszy łączymy: sąsiadujące ruchy bez przycisków to stan, nie zdarzenia
  (tak robi też nasz mono_desktop i XaAES).
- Koszt klatki = pole starego ∪ nowego w blitach (na chunky to memcpy).
- Fallback dla słabych maszyn: ruch konturem (cień), włączany progiem.

## 8. Zmiana fokusu / Z-order
- Przełączenie okna = przelicz clip[] + damage tylko obszaru, gdzie zmieniła się
  widoczność (stary ∩ nowy), nie całego ekranu.
- Aktywne okno dostaje priorytet CPU (M1: plaster 16 vs 8 tików) — tło nie zabiera
  klatek kompozytora.

## 9. Pomiar (bez tego nie ma optymalizacji)
- Licznik klatek kompozytora + suma pikseli na klatkę.
- jobs (mamy): CPU ticks per zadanie — widać, czy tło nie zjada CPU.
- Kryteria akceptacji:
  * pisanie w konsoli: < 1 tick na znak,
  * przeciąganie okna 300x200: >= 25 klatek/s na 030+ET4000,
  * fokus w tle: <= 25% CPU.

## 10. Czego świadomie NIE robimy
- Pełnego odświeżania ekranu w pętli zdarzeń.
- Klonowania bufora całego ekranu na każdą klatkę (to koszt 2x pamięci i 2x kopii).
- Kompozycji z przezroczystością/alfa (nie ma jej w GEM, a kosztuje).
