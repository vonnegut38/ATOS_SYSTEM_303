# Splash przy starcie — ZROBIONE

## Efekt

Od ~10 s do ~13 s widać ekran startowy, potem pulpit przejmuje ekran w całości:

```
              [logo Fuji]
               ATOS 2.1

         [========== pasek ==========]
         STARTUP / DESKTOP

         ST 4096K  TT 49152K
         VGA ET4000  800x600x256
```

- Logo Fuji rysowane prostokątami (3 zęby + podstawa), bez żadnych plików graficznych.
- Pasek postępu z etykietą etapu (`STARTUP` → `DESKTOP`).
- Linia sprzętu z **prawdziwych** danych: `gui_call(56)` = `system_report_t`
  (`st_total`, `tt_total-tt_reserved`, wykryty adapter, tryb i liczba kolorów
  liczona z głębi — pole `planes` jest 1 dla trybu chunke, więc dałoby „x2").
- Bez trybu tekstowego: pulpit wchodzi od razu.

## Dlaczego splash jest rysowany przez PULPIT, a nie przez jądro

Próby rysowania w jądrze (w `kernel_startup_services` i tuż po adopcji formatu
karty) **nie trafiały na ekran** — sprawdzone najtwardszym testem: wypełnienie
całego ekranu kolorem białym w tym oknie nie zmieniło nic. W tym momencie obraz
widoczny pochodzi z warstwy bootowej, a nie z powierzchni, do której pisze `kgfx`;
usługa VOFA jest podłączana w trakcie startu.

Dodatkowo partia wysłana **przed** pierwszą klatką pulpitu idzie do surowego
framebufferu i jest zamalowana, gdy kompozytor przejmuje ekran (sprawdzone: biały
splash nigdy się nie pojawił). Dlatego splash jest pierwszą rzeczą, jaką rysuje
pulpit **po** swojej pierwszej klatce — dopiero wtedy jego pełnoekranowa kanwa
istnieje i jest widoczna.

## Szczegóły, które trzeba było naprawić po drodze

1. **Teksty były jasne na jasnym tle** (`add(GUI_TEXT,…)` domyślnie ustawia
   `background=1`) — „linia sprzętu" wychodziła jako biały prostokąt, a napisy
   znikały. Rozwiązanie: `list[n-1u].reserved=1` (transparentne glify, mechanizm
   dodany przy motywach).
2. **Pole tekstu ma 32 znaki** — długa linia była ucinana („…80"). Rozbite na dwie
   krótsze linie.
3. **Po splashu pulpit nie zamalowywał ekranu w całości** — normalna ścieżka
   odrysowuje tylko ikony, gdy tło nie jest „brudne". Rozwiązanie: `bg_dirty=1`
   przed finalnym `paint()`.
4. **Limit `-mpcrel` pulpitu**: nowa 436-bajtowa kopia `system_report_t` przełamała
   linkowanie (`relocation truncated to fit: .bss.fb_sort`). Zamiast dokładać
   bufor, splash używa **istniejącego `ma_info`**.

## Stan

- Bramka **43 testy zielone**, zero kodu TEMP.
- `DESKTOP.APP` 28 040 B — limit `-mpcrel` to ~32 KB, więc zapas stopniał do
  ~4 KB. Przy kolejnej funkcji pulpitu trzeba przenieść następną tablicę BSS na
  stertę (jak wcześniej `list[40]`).
- Własne wywołanie 26 (pamięć) zostało usunięte — raport systemu (56) ma wszystko.
