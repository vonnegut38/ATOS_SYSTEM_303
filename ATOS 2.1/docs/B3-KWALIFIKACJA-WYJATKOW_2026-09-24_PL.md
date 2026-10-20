# ATOS 2.0 — B3: kwalifikacja wyjatkow (24.09.2026, zaktualizowane 27.09.2026)

Metoda: aplikacje wywolujace bledy + odczyt kernel_fault_* przez memdump + trace
Hatari --trace cpu_exception dla porownania z rzeczywistym SR/PC.

## B3.1 68030 — PASS (10/10)

| App | wektor OS (Hatari offset/4) | PC OS / Hatari | SR OS / Hatari |
|---|---|---|---|
| busflt | 2 (8/4=2) | 50B30 / 50B30 | 0500 / 500 |
| adrflt | 3 (C/4=3) | 50B36 / 50B36 | 0500 / 500 |
| divflt | 5 (14/4=5) | 50B36 / 50B36 | 0506 / 506 |
| chkflt | 6 (18/4=6) | 50B36 / 50B36 | 0501 / 501 |
| trapv | 7 (1C/4=7) | 50B36 / 50B36 | 050A / 50A |
| privflt | 8 (20/4=8) | 50B30 / 50B30 | 0500 / 500 |
| linea | 10 (28/4=10) | 50B30 / 50B30 | 0500 / 500 |
| linef | 11 (2C/4=11) | 50B30 / 50B30 | 0500 / 500 |
| trap1 | 33 (84/4=33) | 50B32 / 50B32 | 0500 / 500 |
| trap15 | 47 (BC/4=47) | 50B32 / 50B32 | 0500 / 500 |

Wniosek: przechwycone SR/PC/wektor sa dokladnie zgodne z ramka CPU. Parser
(include/exception_format.h) juz czyta SR+0, PC+2, format+6 i formaty A/B — audytowe
F02 jest historyczne. tests/test_exception.c pokrywa formaty 0, A, B i przypadki brzegowe.

## B3.2 supervisor fault — PASS

Wymuszenie przez debugger (po starcie): hatari-debug r sr=0x2500, r pc=0x00f00000.
Ekran:

    ATARI SYSTEM KERNEL PANIC
    FATAL CPU EXCEPTION: UNKNOWN
    FAULT vec=11 pc=15728640 sr=9476 usp=0 frame: 621019376 0 0 0 0 0
    SYSTEM HALTED SAFELY / PRESS RESET

pc = 0x00F00000 (dokladnie ustawiony), sr = 0x2504 -> S=1. Blad supervisora nie jest
pomylony z uzytkownika (kernel_try_user_fault testuje bit S), jest raportowany
i system zatrzymuje sie bezpiecznie. Dodano raport SR/PC do sciezki paniki
(wczesniej panika nie pokazywala vec/pc/sr).

## B3.3 68000 — NAPRAWIONE (27.09.2026)

### Objaw
Na 68000 uruchomienie busflt.app nie bylo izolowane: kernel panikowal z
FAULT vec=2 pc=0xF00000 sr=0x2031 (S=1). div0 i address error byly izolowane.

### Zawezenie
Trace pokazywal jeden blad uzytkownika (SR 500, opcode 2039 = move.l $f00000),
a nastepnie panike. Odczyt kernel_fault_* po busflt dawal
frame[0]=0x203100F0 ("SR"=0x2031, "PC"=0xF00000), a po adrflt
frame[0]=0x4ED20005 ("SR"=0x4ED2 = opcode jmp (a2)). Zadna z tych wartosci nie byla
prawdziwym SR — to nie byla sciezka recovery, tylko zly odczyt ramki sprzetowej.

### Przyczyna
Ramka group-1 68000 (bus error i address error) NIE ma SR na +0.
atari-030-port/hatari-src/src/cpu/newcpu.c, funkcja Exception(), galez
cpu_model == 68000 && (nr == 2 || nr == 3):

    +0  mode word  (fc | RW | N/I | nieudokumentowane bity opcode)
    +2  adres bledu (high:low)
    +6  opcode (IR)
    +8  SR w chwili bledu
    (brak zlozonego PC)

Kernel czytal SR z +0, a PC z +2. Dla busflt opcode 0x2039 daje mode = 0x2031;
jego bit 13 (= bit 5 gornego bajtu +0) jest zapalony, wiec kernel_try_user_fault
bral blad uzytkownika za nadzorczy i szedl do paniki. "PC"=0xF00000 to w
rzeczywistosci adres bledu z +2, a nie PC instrukcji.

### Poprawka (system/kernel_entry.S)
- kernel_exception_install zapisuje kernel_cpu_type (0 = 68000) z cpu_type
  przekazanego przez kernel_main.
- Nowe makro kernel_capture_group1 dla wektorow 2 i 3: gdy kernel_cpu_type==0
  czyta SR z 8(%sp), a na pozostalych CPU z 0(%sp). kernel_capture_fault
  pozostaje bez zmian dla wszystkich innych wektorow.
- kernel_try_user_fault: dla wektorow 2/3 na 68000 testuje bit S z 12(%sp)
  (SR pod ramka+8). Dla pozostalych ramek — jak dotad — z 4(%sp).

### Weryfikacja (27.09.2026)
- atos2-tools/scenario_faults_cpu.py (CPU=0 i CPU=3, dynamiczne symbole z
  build/system/kernel.elf): 10/10 PASS na obu CPU.
  68000: busflt sr=0500 pc=00F00000 (adres bledu), adrflt sr=0500
  (wczesniej mode word 4ED2), reszta wektorow sr=0500 + CCR.
  68030 bez zmian: busflt sr=0500 pc=00053EC0 (adres instrukcji).
- atos2-tools/scenario_supervisor_fault.py (CPU=0 i CPU=3): PASS —
  wymuszony blad nadzorcy (r sr=0x2500, r pc=0x00f00000) nadal panikuje
  (S=1, kernel_fault_vector wyzerowany przez kernel_fault_report).
- gmake check: 24/24 OK.

### Poza zakresem
- tos_fault_stub (system/tos_entry.S) dla wektorow 2/3 na 68000 nadal uzywa
  ogolnego zapisu (SR+0 / PC+2). Programy TOS dzialaja w trybie nadzorcy, wiec
  containment nie zalezy od bitu S, ale diagnostyczny kernel_fault_pc pokaze
  adres bledu zamiast PC instrukcji. Do poprawy razem z B4 (persona TOS).
- Optymalizacja samego procesu bootowania na 68000 — osobne zadanie.

## Pliki

- regresja: atos2-tools/scenario_faults_cpu.py (CPU=0/3),
  atos2-tools/scenario_faults.py (CPU=3),
  atos2-tools/scenario_supervisor_fault.py (CPU=0/3).
- zmiana: system/kernel_entry.S (kernel_cpu_type, kernel_capture_group1,
  kernel_try_user_fault).
- referencja: atari-030-port/hatari-src/src/cpu/newcpu.c, Exception().
