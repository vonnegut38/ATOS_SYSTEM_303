# Jak to zobaczyć samemu (obraz demo)

## Pliki

* obraz: `work/atos-audio-demo.img` — kopia Waszego obrazu roboczego z wgranym
  **pakietem produkcyjnym**: KERNEL.SYS 457 762, DESKTOP.APP 29 914,
  **aktualny ARKANOID.APP 18 448** (ten, który woła audio), `ATEST.APP` (236 B),
  `TOSAUDIO.PRG` / `T.PRG` (sondy),
* BIOS: `work/audio-baseline/BIOS-demo.bin` (262 144) — **trzeba go podać**, bo
  zawiera własną kopię `hw/psg.c`; samo podmienienie KERNEL.SYS nie wystarczy,
* manifest: `work/audio-baseline/demo/manifest-DEMO.md` — 5/5 artefaktów
  ZGODNYCH z zawartością FAT.

## Uruchomienie

```sh
hatari --machine megast --cpulevel 3 --cpuclock 32 --memsize 8 --ttram 64 \
  --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --sound 44100 \
  --ide-master work/atos-audio-demo.img --mmu off \
  --tos work/audio-baseline/BIOS-demo.bin --patch-tos false
```

W Waszej aplikacji wystarczy wskazać ten obraz i ten BIOS.

## Test na Arkanoidzie (to, o co prosiliście)

1. `bEnableSound = TRUE` w konfiguracji (u mnie `atos2.cfg`, sekcja `[Sound]`) —
   bez tego emulator nie wypuszcza dźwięku na głośniki, choć gość i tak gra.
2. Kliknij ikonę **ARKANOID**, potem **Space** (serw).
3. Co widać/słychać: gra rusza, HUD pokazuje ~40 FPS (zrzut
   `10-arkanoid-audio-ON.png`), a dźwięki gry są **słyszalne** — kanały PSG są
   realnie zapisywane przez nowy podsystem.
4. Kontrola licznikowa (w moich przebiegach, ten sam obraz i jądro):
   `kaudio_state_owner` = pid gry, `kaudio_state_queued` rośnie,
   `kaudio_dbg_isr_max` = 2–3, `kaudio_dbg_dosound_rejected` = 0.

## Test „natywny STOP z sesją TOS w środku” (jeśli chcecie liczby)

Na obrazie są: `ATEST.APP` (natywny właściciel audio, cyklicznie planuje STOP)
i `T.PRG` (sonda audio). Uruchomienie: `ATEST` z RUN, potem `T.PRG` z RUN.

Zmierzone przeze mnie (`log-natywny-STOP-12-0.txt`):

```
owner=5   głośność kanału A: 12 → 0 (zaplanowany STOP wykonany w terminie)
rej=2  ref=4   (żądania audio TOS odrzucone)
teardown=1     (sesja TOS posprzątana, właściciel bez zmian)
q=1/0          (kolejka natywna pracuje)
```


## Sesja live (uruchomiona przeze mnie)

Obraz demo jest **już uruchomiony** w widocznym oknie Hatari, z dźwiękiem
(`--sound 44100`) i wystartowanym Arkanoidem — zrzut `20-arkanoid-LIVE.png`.
Okno możecie kliknąć i grać: **Space** = serw/start, **P** = pauza, **R** = nowa
gra, **D** = demo, **ESC** = koniec. Gdyby gra zeszła do ekranu „GAME OVER”, moja
pętla i tak co 30 s wysyła Space, więc wróci do rozgrywki.

Stan liczbowy tego samego zestawu (jądro + gra) zmierzony osobno: `owner` = pid
gry, kolejka rośnie, `isr_max` 2–3, `dosound_rejected` = 0. Czyli dźwięk, który
słychać, faktycznie idzie przez nowy podsystem audio ATOS.
