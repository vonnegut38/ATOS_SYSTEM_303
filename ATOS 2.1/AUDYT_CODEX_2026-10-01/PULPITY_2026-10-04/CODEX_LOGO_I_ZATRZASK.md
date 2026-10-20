# Przegląd poprawek Codexa + znaleziona przyczyna czarnego ekranu

## 1. Najpierw najważniejsze: testowałeś INNE drzewo

`atari-030-port/run-bios-vga.sh`, linia 19:

```sh
PROJECT=$(cd "$HERE/../ATOS 2.0" && pwd)
```

Uruchamiany przez Ciebie Hatari bierze ROM i obraz z **ATOS 2.0**, a cała praca
(Codex + moja) jest w **ATOS 2.1**:

| | ATOS 2.0 | ATOS 2.1 |
|---|---|---|
| `BOOT_HANDOFF_VERSION` | 18 | **20** |
| `boot_progress` | brak | **jest** |
| `post_draw_logo` | brak | **jest (3×)** |
| zmiany z ostatnich godzin | brak | **Codex + moje** |

Dlatego „nic się nie zmienia" — zmiany były w drugim katalogu.

## 2. Przegląd poprawek Codexa — są dobre

- **`ui/post.c`**: logo Fuji jako **ASCII-art** (`##`) + `ATOS 2.1` + tekstowy pasek
  `[####    ] 45%` z etykietą. Tabelka konfiguracji usunięta — zgodnie z „zamiast
  tabelki logo".
- **`loader/bootstage.c`**: pasek postępu **podczas wczytywania** BOOT.SYS/KERNEL.SYS
  (aktualizacja co ≥5%), `post_boot_progress` przekazany w handoffie, poprawka
  `card_video_mode` 640→800 przy podniesionej karcie.
- **`system/kernel_main.c`**: 11 etapów ładowania → 0–100% na pasku; walidacja
  wersji handoffu.
- **`include/boot_protocol.h`**: handoff v20 + `boot_progress`.
- **`main.c`**: opóźnienie bootu ≥3 s, żeby logo było czytelne.

To dokładnie strategia, którą proponowałeś: **żadnych pikseli, żadnej palety,
wszystko przez istniejącą konsolę** — więc zero ryzyka dla bootu. Bramka:
**43 testy zielone**. System bootuje (biurko o ~15 s, stabilne).

## 3. PRAWDZIWA przyczyna czarnego ekranu: zatrzask failover

Porównanie konfiguracji na obrazach:

```
ATOS 2.0:  video_pending = 0   → ROM podnosi kartę, boot działa
ATOS 2.1:  video_pending = 1   → ROM POMIJA kartę, ekran czarny
```

Dowód z logu emulatora — w 2.1 karta **nigdy** nie była programowana:
`VOFA-MODE: w=0 h=0 ...` (12 odczytów, zero niezerowych).

I to było **zakleszczenie**, nie chwilowy stan:

1. ROM ustawia `video_pending=1` przed próbą karty i kasuje ją tylko, gdy jądro
   dojdzie do powłoki.
2. Jądro kasowało ją **tylko gdy `nova_initialized`** — czyli gdy ROM sam podniósł
   kartę.
3. W trybie failover ROM **nie próbuje** karty → `nova_initialized` zostaje 0 →
   jądro **nie kasuje** → następny boot znowu failover → **na zawsze**.

Jedno nieudane bootowanie na kartę zamurowywało maszynę w trybie „tylko Shifter",
a biurko działało, bo jądro programuje kartę samo.

### Poprawka (jądro)

`system/kernel_main.c`: po dojściu do powłoki zatrzask jest kasowany **zawsze**,
nie tylko gdy ROM podniósł kartę.

**Weryfikacja po poprawce**:
- `video_pending = 0` w konfiguracji,
- log emulatora: `VOFA-MODE: w=800 h=600 stride=800 bpp=8` — **10 niezerowych
  odczytów**, karta programowana przez ROM,
- system bootuje, biurko stabilne do 40 s.

## 4. Co jeszcze zostaje

Tekst startowy (logo + pasek Codexa) **nadal nie jest widoczny** — ekran jest
jednolitą czarną klatką 800×600, mimo że karta jest już programowana poprawnie i
konsola ROM-u pisze w 8bpp z paletą 0=czarny, 1=biały. To zawęża się teraz do
samego renderowania znaków przez konsolę ROM-u na kartę — osobny, mały problem.

## 5. Stan

- Bramka **43 testy zielone**, boot zweryfikowany, zero kodu TEMP.
- **Trwała zmiana**: jądro kasuje zatrzask failover (naprawia realne zakleszczenie).
- Hatari uruchomiony na **ATOS 2.1** (nie 2.0).
