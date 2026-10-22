# Model kompozycji ATOS 2.1 (22.10.2026)

Uruchom z katalogu repozytorium (tam, gdzie jest `ATOS 2.1`):

    sh perf-model-20261022/run.sh

Model buduje sie z PRAWDZIWYCH plikow `wm_core.c`, `wm_blit.c` i `kernel_video.c`
(ten ostatni kopiowany z jednym licznikiem `kdbg_scan_words`). Odtwarza scenariusz
z `AUDYT-WYDAJNOSCI-2026-10-22.md`: 800x600 8 bpp, okna z geometrii zrodel aplikacji
(MODPLAY zaslania SYSINFO, TASKMGR i PLASMA), PLASMA 18 Hz, MODPLAY 25 Hz przez GUI_FRAME.

Kolejnosc ticku przepisana z `kernel_wm_frame_tick()`: damage -> kvideo_damage ->
wm_recompose -> dock_draw -> kvideo_flush. Pominiete: chrome i kursor (1,3% i 1,2% w audycie).
Wyniki to wolumeny pracy na sekunde (piksele, bajty), nie czas na 68030.

Warianty V1..V6 sa przelacznikami w `model.c` (kumulatywne):
V1 przyciecie damage aplikacji do widocznych prostokatow okna, V2 PLASMA bez identycznych
klatek, V3 damage na komende zamiast prostokata calej partii, V4 dock tylko przy zmianie,
V5 jedna kompozycja na tick, V6 zasloniete okno nie animuje.
