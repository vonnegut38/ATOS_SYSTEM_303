# ATOS 2.0 — kwalifikacja 68030: B4 i szybkie skroty (24.09.2026)

## B4 — odciecie nieaktywnych zaleznosci TOS od natywnego timera

- kernel_entry.S (Timer-C): hooki tos_composite_tick / tos_task_tick sa wolane tylko gdy
  kernel_tos_runtime_enabled != 0, czyli gdy naprawde dziala program TOS.
- tos_prg.c: flaga ustawiana przy wejsciu w program TOS i zerowana po powrocie.
- Natywny profil bez TOS: dostepny przelacznik build-time TOS=0 (bez kodu persony).
- Weryfikacja: build + testy PASS, boot OK, ST-RAM FREE 3655 KB, stress 8 cykli bez strat
  (ACIA/QUAR/PARTIAL/KEY = 0).

## Szybkie kolejne skroty Shift+Alt+F10 (68030)

Pomiar: 3 sesje (tts1/tts2/tts3), start zawsze z tts1 (0). Wynik = aktywna sesja po serii.

| Seria | odstep | wynik |
|---|---|---|
| 1 | 0.35 s | 0 -> 1 |
| 2 | 0.35 s | 0 -> 2 |
| 3 | 0.35 s | 0 -> 0 |
| 2 | bez odstepu | 0 -> 2 |
| 3 | bez odstepu | 0 -> 1 |
| 6 | bez odstepu | 0 -> 2 |
| 2 | 0.08 s | 0 -> 2 |
| 3 | 0.08 s | 0 -> 2 |
| 6 | 0.08 s | 0 -> 1 |

Wnioski:

- Przy odstepie ~0.35 s wszystkie skroty sa dostarczane (n mod 3).
- Przy szybkich seriach (>=3 bez odstepu lub 0.08 s) czesc skrotow jest **scalana**: kolejne
  zadanie nadpisuje poprzednie w pojedynczym slocie request (apply_request czeka na
  oproznienie wejscia). To nie jest strata sprzetowa — liczniki ACIA/QUAR/PARTIAL pozostaja 0.
- Zachowanie jest zgodne z opisem w POPRAWKI_WEJSCIA_I_SESJI_PL.md; nie usuwac zabezpieczenia
  przed autorepeat bez testu.

Opcja na przyszlosc (jesli kazde nacisniecie ma przełączac): zamienic pojedynczy slot request
na mala kolejke (np. 4) w ksessions_key/apply_request. Do decyzji — obecne scalanie jest
celowe i bezpieczne.

## Aktualizacja 27.09.2026 — TOS=0 jako domyslny profil

- `TOS ?= 0` jest teraz domyslne (profil natywny); `TOS=1` przywraca persone.
- Pomiar KERNEL.SYS: domyslnie `text 101 241 / data 128 / BSS 60 232`;
  z persona `125 129 / 157 / 87 308`. Zysk: **−23 888 B text, −27 076 B BSS**.
- Dwa niezmienione call-site (API modulu, syscall 65) obsluguje stub
  `kernel_tos_run_program` pod `#if !CONFIG_TOS`, zwracajacy `-0x2ff`.
- Shell: `TOS PERSONA NOT BUILT IN THIS KERNEL` zamiast `TOS PROGRAM FAILED`.
- Test runtime: `atos2-tools/scenario_tos_profile.py` (TOS=0/1); statyczny
  straznik: `tests/test_tos_profile.py`. `gmake check` 34/34.
