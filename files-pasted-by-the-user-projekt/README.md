# Atari BIOS 0.02

Minimalny, niezależny BootROM/BIOS dla Atari ST, Mega ST, STE i Mega STE. Obrazy zachowują
48-bajtowy nagłówek wymagany przez sprzęt Atari i loader ROM-u Hatari, ale nie
udostępnia usług TOS. Ten milestone zawiera
start MC68000, inicjalizację STE Shiftera, własny font, ekran POST oraz wejścia
F1/F2 do szkieletów Setup i Diagnostics. Nie zawiera GEM, GEMDOS ani loadera HDD.

Shifter automatycznie wybiera ST High Mono, gdy MFP sygnalizuje monitor
monochromatyczny. Dla monitora kolorowego zachowuje zastany ST Low lub ST
Medium. Konsola przełącza odpowiednio układ 4, 2 albo 1 bitplanu.

## Toolchain i build

Wymagany jest bare-metal GCC/binutils z prefiksem `m68k-elf-` oraz Python 3:

```sh
make toolchain
make
make check
```

Inny prefiks można podać np. `make CROSS=m68k-atari-mint-`, o ile kompilator
obsługuje linkowanie freestanding. Wyniki trafiają do `build/`: pełny ROM
`bios_mste.bin` (262144 B, baza 0xE00000) oraz `bios_st.bin` (196608 B,
baza 0xFC0000). Powstają też obrazy `*_hi.bin` i `*_lo.bin`, mapy
linkera oraz `checksums.txt` z CRC32/SHA256. Kontrola przeplotu:

```sh
python3 tools/splitrom.py join build/bios_mste_hi.bin build/bios_mste_lo.bin /tmp/joined.bin
cmp build/bios_mste.bin /tmp/joined.bin
```

## Hatari

Po instalacji Hatari uruchom `make hatari` albo:

```sh
hatari --configfile hatari/mste.cfg --tos build/bios_mste.bin --patch-tos false
```

Przy GNU Make z Homebrew dostępne są też jawne testy monitora:

```sh
gmake hatari-rgb       # ST Low (lub zastany tryb kolorowy)
gmake hatari-medium    # prośba Hatari o ST Medium
gmake hatari-mono      # ST High Mono
gmake hatari-st        # wariant 192 KiB dla ST/STE/Mega ST
```

Konfiguracja wybiera Mega STE, MC68000, 16 MHz i 4 MiB ST-RAM. Warto także
sprawdzić `--cpuclock 8` oraz `--memsize 1`, `2` i `4`. F1 otwiera Setup, F2
Diagnostics, Escape wraca do POST.

F4 skanuje urządzenia ACSI i wybiera partycję startową. F5 odczytuje z wybranej
partycji FAT16 katalog `TOS_BIOS` i pokazuje maksymalnie osiem obrazów o
rozszerzeniu `.ROM`, `.IMG` lub `.BIN`. Rozpoznawane są obrazy 192, 256 i
512 KiB oraz numer wersji zapisany w nagłówku TOS. Na tym etapie F5 jest
bezpiecznym katalogiem i walidatorem; nie uruchamia jeszcze wybranego obrazu.

## Ważne ograniczenia 0.01

- Bootstrap wykrywa niezależnie oba banki ST-RAM (128 KiB, 512 KiB albo 2 MiB)
  metodą zawijania adresów właściwą dla MMU ST i STe, ustawia końcowy MEMCONF,
  a następnie umieszcza stos 4 KiB poniżej końca wykrytej pamięci.
- Detekcja banków wykorzystuje wzorzec oparty na technice opisanej przez
  Christiana Zietza w `doc/memdetect.txt` projektu EmuTOS. Ekran zaczyna się od
  0x10000; minimalnym praktycznym targetem tego BIOS-u pozostaje 512 KiB.
- Ustawienia zegara/cache są tylko wyświetlane; zmiana i trwały Setup to kolejny
  etap.
- Przed użyciem fizycznych EPROM-ów trzeba zweryfikować kolejność HI/LO dla
  konkretnej płyty/programatora i wykonać testy na emulatorze.

Układ katalogów celowo pozostawia miejsca na dalsze moduły `diag`, `loader`,
`monitor` i pozostałe sterowniki sprzętu.
# Atari System boot chain

The ROM automatically starts booting after a five-second MFP-timed countdown.
The first boot component is a flat, checksummed image named `BOOT.SYS`, loaded
from the `SYSTEM` directory on the selected FAT16 ACSI partition.

Build everything with `gmake`. The generated files are:

- `build/bios_st.bin` (192 KiB, mapped at `0xFC0000`) for ST/Mega ST;
- `build/bios_mste.bin` (256 KiB, mapped at `0xE00000`) for STE/Mega STE/TT/Falcon;
- `build/system/BOOT.SYS` for the disk's `SYSTEM` directory.
- `build/system/KERNEL.SYS` for the first system kernel.
- `build/system/ACSI.SYS` for the ABI 3 sector-read driver module.
- `build/system/CORE.SYS` for the first kernel-loaded resident system module.
- `build/system/VFS.SYS` for the read-only virtual filesystem service.
- `build/system/PROC.SYS` for the initial cooperative process service.
- `build/system/TIMER.SYS` for the polling monotonic timer service.
- `build/system/EVENT.SYS` for bounded inter-task mailboxes.
- `build/system/INPUT.SYS` for non-blocking IKBD keyboard input.
- `build/system/SHELL.SYS` for the first interactive system loop.
- `build/system/HELLO.APP` for the first validated user-program image.
- `build/system/FAULT.APP` for the user-exception containment test.
- `build/system/FILETEST.APP` for the user VFS-syscall test.

To install or replace the loader directly in an AHDI hard-disk image (work on
a backup while developing):

```sh
python3 tools/install_boot_sys.py disk.img build/system/BOOT.SYS
python3 tools/install_boot_sys.py disk.img build/system/KERNEL.SYS
python3 tools/install_boot_sys.py disk.img build/system/ACSI.SYS
python3 tools/install_boot_sys.py disk.img build/system/CORE.SYS
python3 tools/install_boot_sys.py disk.img build/system/VFS.SYS
python3 tools/install_boot_sys.py disk.img build/system/PROC.SYS
python3 tools/install_boot_sys.py disk.img build/system/TIMER.SYS
python3 tools/install_boot_sys.py disk.img build/system/EVENT.SYS
python3 tools/install_boot_sys.py disk.img build/system/INPUT.SYS
python3 tools/install_boot_sys.py disk.img build/system/SHELL.SYS
python3 tools/install_boot_sys.py disk.img build/system/PATH.APP
python3 tools/install_boot_sys.py disk.img build/system/AUTOEXEC.BAT --root
python3 tools/install_boot_sys.py disk.img build/system/HELLO.APP
python3 tools/install_boot_sys.py disk.img build/system/FAULT.APP
python3 tools/install_boot_sys.py disk.img build/system/FILETEST.APP
python3 tools/install_boot_sys.py disk.img build/system/SEEKTEST.APP
python3 tools/install_boot_sys.py disk.img build/system/ARGTEST.APP
python3 tools/install_boot_sys.py disk.img build/system/IOTEST.APP
python3 tools/install_boot_sys.py disk.img build/system/YIELD.APP
python3 tools/install_boot_sys.py disk.img build/system/SLEEP.APP
python3 tools/install_boot_sys.py disk.img build/system/SYSINFO.APP
python3 tools/install_boot_sys.py disk.img build/system/DIRTEST.APP
python3 tools/install_boot_sys.py disk.img build/system/CWDTEST.APP
python3 tools/install_boot_sys.py disk.img build/system/MEMTEST.APP
python3 tools/install_boot_sys.py disk.img build/system/PROCTEST.APP
python3 tools/install_boot_sys.py disk.img build/system/PINFO.APP
python3 tools/install_boot_sys.py disk.img build/system/PTABLE.APP
python3 tools/install_boot_sys.py disk.img build/system/CHILD.APP
python3 tools/install_boot_sys.py disk.img build/system/SPAWN.APP
python3 tools/install_boot_sys.py disk.img build/system/ARGCHILD.APP
python3 tools/install_boot_sys.py disk.img build/system/SPARG.APP
python3 tools/install_boot_sys.py disk.img build/system/FLTCHILD.APP
python3 tools/install_boot_sys.py disk.img build/system/FSPAWN.APP
```

The installer creates `SYSTEM` when needed and updates every FAT copy. On real
media, create `SYSTEM` in TOS/EmuTOS and copy `BOOT.SYS`, `KERNEL.SYS`,
`ACSI.SYS`, `CORE.SYS`, `VFS.SYS`, `PROC.SYS`, `TIMER.SYS`, `EVENT.SYS`, `INPUT.SYS` and `SHELL.SYS` into it. The kernel validates the driver with a sector-0
read into ST-RAM. If `ACSI.SYS` is absent, rejected, or fails I/O, the kernel
keeps using the ROM ACSI service as a boot-safe fallback.

After mounting the boot volume read-only, the kernel also checks the FAT16
clean-shutdown and hard-error flags. The bootstrap ACSI driver is loaded by the
ROM service because it is required to construct the kernel block layer. Later
modules no longer depend on the ROM FAT loader. A dirty volume is reported but
is never modified during this boot stage.

`CORE.SYS` is the first module loaded and started entirely by the kernel's own
read-only FAT16 path. Its image is validated before execution and is placed in
TT-RAM when available, with an automatic ST-RAM fallback. The same guarded
loader is used as the base for subsequent device and filesystem modules. The
module registers the versioned `CORE` service in a bounded kernel registry;
duplicate service identifiers, pointers outside the module image and partial
registrations from a failed initializer are rejected.

`VFS.SYS` registers a versioned, read-only filesystem service. Version 2
provides `stat`, whole-file reads and directory enumeration for bounded FAT16
paths; the kernel performs a real `CORE.SYS` lookup through that service before reporting it ready. This keeps
FAT directory and cluster details private to the filesystem layer and gives
future modules a stable interface.

`PROC.SYS` supplies the version 3 process service. During initialization it
creates two task-control records and two independent guarded 1 KiB supervisor
stacks in ST-RAM. One external yield performs a full cooperative round-robin
cycle through both workers and back to the kernel; separate dispatch counters
prove that both contexts ran. The common context preserves the ABI registers
used by 68000/68020/68030, while each task record reserves CPU-extension and
CPU-flags fields for later cache, exception-frame and optional FPU state.
Timer preemption remains disabled until this scheduler passes emulator and
real-hardware tests.

Version 3 adds tick-based sleeping without busy waiting. Sleeping tasks keep
their saved context but are skipped by round-robin selection. A timer callback
decrements the per-task delay and makes a task runnable at zero. The boot test
sleeps task 0 for two ticks, runs four deferred quanta, and requires final
dispatch counts of 5 and 6 for tasks 0 and 1 respectively.

Version 4 adds explicit READY, SLEEPING and WAITING states plus guarded wait
and wake operations. `EVENT.SYS` provides one bounded mailbox per initial task.
Receiving from an empty mailbox changes the caller to WAITING; sending a typed
32-bit message makes it READY again. The boot self-test proves that the waiting
task is skipped, verifies the message payload, wakes it and checks both final
dispatch counters. Full queues and invalid task identifiers are rejected.

`INPUT.SYS` polls the IKBD ACIA without enabling receiver interrupts. Mouse
packet headers and their two payload bytes, plus IKBD status bytes, are kept
out of the keyboard stream. Press and release scan codes become typed EVENT
messages for a selected task. A busy mailbox increments a dropped-event
counter instead of overwriting unread input. `EVENT.SYS` uses a 32-entry ring
per initial recipient, so keyboard bursts cannot overwrite unread events.
Non-Shift key releases are discarded before the event queue because
the shell does not consume them. This prevents a pending Enter release from
occupying the mailbox and dropping the first character of the next command;
left/right Shift releases remain delivered for correct modifier tracking.
The main runtime services input and shell delivery in two bounded phases. It
first drains up to 64 raw IKBD bytes, including key releases, into a static
32-entry event mailbox and then lets the shell consume up to 32 events before
dispatching a user process. This prevents shell rendering or command handling
between individual ACIA reads from unnecessarily shrinking the input window.
The fixed limits use no runtime allocation and retain deterministic scheduler
latency. Completely unpaced host-side injection across multiple commands still
belongs to the later interrupt-driven ACIA stage; ordinary rapid keyboard input
remains lossless while a cooperative background process is active.
The bounded startup drain now recognizes IKBD mouse packet headers and retains
the number of coordinate bytes still expected if a packet is split across
module initialization.  Those trailing bytes can therefore never be decoded
as random keyboard scan codes.  The shell command `inputstat` reports the
cumulative `IKBD DROPPED` counter and the current `EVENTS PENDING` count for
diagnosis on real hardware without exposing writable queue storage.
When the MC6850 reports receiver overrun, INPUT.SYS quarantines at most the
current and following byte because a lost byte may have been a mouse-packet
header.  Both this quarantine and an incomplete framed mouse packet expire
after eight consecutive empty ACIA polls.  Thus delayed coordinate bytes do
not appear as characters, while a later real key is never consumed by stale
parser state.  Each recovered overrun increments `IKBD DROPPED`.

Before starting any module loaded by the kernel, the loader now synchronizes
the instruction cache: this is a no-op on 68000, clears the 020/030 I-cache,
and performs the corresponding 040/060 cache push. This prevents stale code
execution when executable module memory is reused.

`SHELL.SYS` starts the permanent cooperative runtime loop. The kernel
continues polling the timer and IKBD, advances sleeping tasks and performs
deferred scheduling; the shell consumes typed EVENT messages for task 0.
It maintains a bounded 63-character command line and supports Backspace.
Tab completes a file or directory name after at least one character, including
the final component of absolute and relative paths such as `START /S`. A
unique directory match receives a trailing backslash; multiple matches extend
only their common prefix. An eight-entry ring stores non-empty commands:
Up-arrow walks toward older entries, Down-arrow returns toward newer entries,
and moving past the newest entry restores an empty input line. History replaces
only the editable line and never redraws the screen.
The prompt contains the selected boot-volume path, for example `C:\>`. Enter
keeps the existing screen, prints the result below the command and opens the
next prompt. The read-only `dir`, `ls`, `ls -l`, `cd`, `type` and `cat`
commands operate through VFS; `pwd` reports the current path, while `clear`
and `cls` clear the console. Paths use FAT 8.3 components and are limited to
eight levels. VFS version 3 reads files in bounded 512-byte windows, so
`type`/`cat` no longer need to load the whole file into RAM. Uptime comes from TIMER, task information
from PROC, and memory statistics from kernel services. Routine key input only
draws changed glyphs and never redraws or reprograms the video hardware.

After it has initialized, the shell reads the bounded, editable
`C:\AUTOEXEC.BAT`. Startup commands therefore remain on disk instead of being
compiled into the kernel or shell. `PATH.APP` changes the executable search
path through a controlled syscall. `RUN` and `START` search the current
directory first, then retry a bare filename through that path; a name
containing a slash, backslash or drive colon remains explicit and is never
redirected. The planned disk layout separates essential commands,
applications and diagnostics under `C:\SYSTEM\COMMAND`, `C:\SYSTEM\APP` and
`C:\SYSTEM\TEST`.
At the bottom edge, the console scrolls the framebuffer by exactly one text
row and clears only the new bottom row.

The `tasks` command reports the fixed cooperative kernel tasks exported by
`PROC.SYS`. The separate `jobs` command walks the bounded user-process table
and prints the APP filename, `PID`, `PPID`, process memory footprint, open-file
count and one of `READY`,
`RUNNING`, `SLEEPING`, `WAITING` or `TERMINATED`. The table is exposed to the
shell through a read-only module API query; the shell never receives writable
process-table pointers.
`kill PID` terminates a kernel-owned READY or SLEEPING background process and
releases its APP image, user stack, private allocations and file handles. PID
0, child-owned records, RUNNING processes and parents blocked in WAITING state
are rejected rather than being torn down in an unsafe context.
Administrative termination uses the distinct completion class `KILLED` rather
than presenting status -3 as a CPU fault.  The supervisor checks for newly
recorded completions after shell activity as well as after user dispatch, so
`kill PID` and foreground `Ctrl+C` produce an immediate `[PID n KILLED]`
notification.  The record remains available until `wait PID` returns
`PID n KILLED` exactly once.

Syscall 35 blocks an independently loaded process in the `INPUT` state instead
of polling the IKBD in a busy loop. The shell remains the terminal owner until
the user explicitly enters `fg PID`; only then does the kernel route the next
keyboard event to that process. While a foreground owner exists the shell does
not consume its keystrokes. Exit, fault or `kill` returns ownership to the
shell. `WAITKEY.APP` requests one character and verifies this handoff without
enabling input interrupts or performing scheduler work in interrupt context.
`WAITLINE.APP` repeats the block/wake cycle for every character while retaining
terminal ownership and its saved D2 line-length state. Backspace edits the
line, Enter completes it, and Escape provides a bounded cancellation path that
returns the terminal to the shell.
`startfg program.app [args]` creates a process and reserves the terminal for it
before its first dispatch, so interactive applications do not require a
separate `jobs`/`fg PID` sequence.  The existing `start` command remains the
non-blocking background launch path.  A foreground reservation accepts only a
new READY process or a process already blocked in INPUT; exit and isolated
fault cleanup release it before the shell redraws its prompt.
The same foreground reservation can attach to READY, SLEEPING or INPUT
processes.  While a READY/SLEEPING foreground process is not requesting text,
ordinary keystrokes are discarded instead of leaking into the hidden shell;
`Ctrl+C` remains active and uses the same bounded cleanup path.
The input path also tracks the IKBD Control modifier.  `Ctrl+C` is handled by
the kernel while a process owns the terminal: it terminates an INPUT-state
foreground process through the normal resource cleanup path, prints `^C`, and
returns a fresh shell prompt.  Control release events are retained while other
non-Shift key releases remain filtered, preventing a stuck modifier without
adding routine release traffic to the event queue.
`Ctrl+Z` changes the foreground record to the non-runnable SUSPENDED state,
prints `^Z`, releases terminal ownership and restores the shell prompt without
destroying the saved register/USP/PC/SR context or allocations.  `jobs` reports
that state explicitly.  A later `fg PID` promotes the same record to READY and
reattaches the terminal, while `kill PID` can discard it through normal cleanup.
`bg PID` provides the complementary transition from SUSPENDED to READY without
claiming terminal ownership.  It rejects missing PIDs and processes in every
other state, so repeated or accidental resumes cannot corrupt a live context.
`stop PID` performs the inverse operation for a background READY, SLEEPING or
INPUT process.  It never tears down the saved context or allocations and
rejects RUNNING, WAITING, TERMINATED, already-SUSPENDED and terminal-owned
records.  The process can subsequently follow either the `bg PID` or `fg PID`
resume path.
The kernel also keeps a bounded four-entry completion queue for automatically
reaped background processes.  `wait PID` reports `PROCESS STILL ACTIVE` while
the PID remains live, consumes a normal result as `PID n EXIT status`, or
consumes an isolated exception as `PID n FAULT vector`.  A result is returned
only once; a repeated query reports `NO COMPLETION RECORD`.  The queue contains
only PID/status pairs, so completed processes never remain as zombies and all
of their image, stack, handle and private-memory resources are still released
immediately.
`PROCSTR.APP` is the bounded lifecycle regression test for this foundation. It
runs eight alternating spawn/wait/reap cycles: four children return status 7
and four deliberately raise illegal-instruction fault -260. It then requires
the free ST-RAM and TT-RAM totals to exactly match their pre-test values. A
successful run prints `PROCSTRESS: 8 SPAWN/WAIT/REAP CYCLES + MEMORY PASS`;
the shell can subsequently consume the test's own completion as exit status 0.
The supervisor loop also reports completion asynchronously in the same
`PID n EXIT status` / `PID n FAULT vector` form.  A per-record notification bit
prevents duplicate messages without consuming the result needed by a later
`wait PID`.  Prompt redraw retains a partially typed command, and cursor
removal clears its glyph before restoring the cell position so asynchronous
output cannot leave pixels over the following character.
Returning terminal ownership also clears the kernel's private Control/Shift
modifier state.  This is required because the physical key-release event can
arrive after `Ctrl+C` or `Ctrl+Z` has already returned input routing to the
shell; a stale Control bit must never turn the first resumed `c` or `z` into a
second signal.

The shell `run` command loads the independent `ASYSAPP0` executable format.
Before execution it validates the ABI version, declared file and memory sizes,
entry point, private-stack bounds and additive checksum. Program storage is
allocated independently (TT-RAM preferred with ST-RAM fallback), BSS is
cleared, the CPU instruction cache is synchronized and the entry point runs
in 68000 user mode on a separate 2--16 KiB user stack. ABI 1 exposes `TRAP #0`
system calls for bounded console output and process exit; applications never
receive a kernel function pointer. Bus error, address error, illegal
instruction and divide-by-zero exceptions terminate the active application,
restore the saved supervisor stack and return to the shell. Both allocations
are then freed. `HELLO.APP` tests normal output and exit, while `FAULT.APP`
deliberately executes an illegal instruction and must return fault code 260
without causing a kernel panic. Memory isolation between user processes still
requires the later MMU/protected-process stage.

The next `TRAP #0` ABI revision adds four per-process read-only file handles.
Applications can open, sequentially read, close and stat files. Every path is
copied into a bounded kernel buffer, every handle is range-checked, and every
destination/result buffer must lie wholly inside the calling application's
declared image memory. Handles are discarded on exit or fault.
`FILETEST.APP` opens `HELLO.APP`, reads and validates its eight-byte magic,
closes the handle and exits normally.
Syscall 6 sets a read handle to a checked absolute offset; positions beyond
EOF are rejected. `SEEKTEST.APP` seeks to byte four of `HELLO.APP`, reads the
second half of its magic and leaves the original file test unchanged.
The module API appends a bounded argument setter without changing the stable
`run_user` call. Syscall 7 copies the command tail into application-owned
memory. `ARGTEST.APP` validates `run argtest.app test`.
Syscall 8 writes one validated character. Syscall 9 polls INPUT/EVENT and
returns one translated ASCII key or `-1` without blocking the kernel.
`IOTEST.APP` waits by polling, echoes one key and exits normally.
Syscall 10 cooperatively yields to TIMER and PROC and returns the current
50 Hz tick count. `YIELD.APP` verifies that time and deferred scheduling
continue to advance while a user application is active.
Syscall 11 performs a bounded cooperative sleep of 0--3000 ticks. It keeps
TIMER and PROC running instead of spinning only inside the application;
`SLEEP.APP` verifies a three-tick delay and safe return to the shell.
Syscall 12 copies a fixed six-longword system-information record into a
validated application buffer: ABI version, highest syscall number, CPU type,
monotonic ticks, free ST-RAM and free TT-RAM. `SYSINFO.APP` verifies the
versioned interface without depending on a particular Atari model.
Syscall 13 enumerates a directory one bounded entry at a time. It returns an
8.3 display name, FAT attributes and file size in a fixed 20-byte record;
paths and the destination record are validated before VFS access.
`DIRTEST.APP` verifies root-directory enumeration.
Syscall 14 returns the active drive and private application working directory.
Syscall 15 changes that directory only after canonicalizing `.` and `..` and
verifying the target through VFS. Application path changes do not alter the
shell prompt. `CWDTEST.APP` verifies relative parent and child navigation.
Syscalls 16 and 17 provide bounded application allocation and exact-pointer
free. Each running application owns at most eight non-executable blocks of up
to 1 MiB each, with optional TT-RAM preference and ST-RAM fallback. Kernel
buffer validation includes these blocks. Exit and CPU-fault paths release all
outstanding allocations automatically; forged and interior pointers are
rejected before reaching the allocator. `MEMTEST.APP` verifies dynamic console
output, rejection of an interior-pointer free and normal release.
Application state is now held in one explicit process record containing its
PID, execution state, image bounds, private path, arguments, handles and owned
memory. Every launch receives a monotonically increasing nonzero PID and a
separate allocator owner. Syscall 18 returns the current PID. `PROCTEST.APP`
checks stable identity and deliberately leaves all eight allocation slots in
use; running it again verifies that process teardown reclaimed every block.
Syscall 19 returns a fixed process-accounting record: PID, state, open-handle
count, allocation count, allocated bytes and working-directory length.
`PINFO.APP` opens a file, allocates ST- and TT-preferred buffers and verifies
that all counters belong to the current process before releasing them.
The user runtime now maintains four process slots with explicit FREE, READY,
RUNNING, SLEEPING, WAITING and TERMINATED states. Allocation rotates through
reusable free or terminated slots. Syscall 20 reads one fixed process-table
entry without exposing kernel pointers. `PTABLE.APP`, run after another app,
verifies one RUNNING record and at least one retained TERMINATED record.
Syscall 21 synchronously starts one validated child application. The parent is
WAITING while the child has its own PID, record, user stack, handles and memory
owner; after termination the parent resumes as RUNNING and receives the child
PID plus exit status. A two-level supervisor return-stack prevents nested user
entry from overwriting the parent's recovery context. `SPAWN.APP` runs
`CHILD.APP`, verifies exit status 7 and confirms that its own PID is unchanged.
Syscall 22 uses one bounded request record containing a child path, a
63-character argument tail and PID/status result fields. Arguments are copied
before switching records, and the child inherits the parent's private working
directory. `SPARG.APP` starts `ARGCHILD.APP`, which verifies `alpha beta`, its
inherited drive/path and exit status 9.
`FSPAWN.APP` starts `FLTCHILD.APP`, which deliberately executes an illegal
instruction. The nested fault must terminate only the child with status -260,
release its resources, restore the parent's PID/context and leave allocation
and the shell operational.
Syscall 23 adds a bounded four-entry deferred process queue. Submission copies
the path, arguments and private working directory into kernel memory and
returns immediately with a nonzero job identifier. A controlled syscall 10
yield dispatches one queued child; nested yields can dispatch the next job.
The supervisor recovery stack and parent stack now both support all four user
process slots. `ASYNC.APP` queues `COOPA.APP` and `COOPB.APP`; their explicit
yield points verify deterministic cooperative nesting and return to the parent.
Syscall 24 queries a queued job without blocking and optionally reaps a
terminated result. It returns QUEUED (1), RUNNING (2), or TERMINATED (3) plus
the signed child status. Reaping releases the bounded result slot; stale and
unknown identifiers are rejected. `WAITTEST.APP` verifies queued observation,
normal status 7, isolated illegal-instruction status -260, and stale-ID denial.
Syscall 25 is the blocking wait operation. It dispatches queued work until the
requested child terminates, copies out its signed status, and atomically reaps
the result. Job results carry their creating PID; query, reap, and wait reject
access from any other process. `BWAIT.APP` tests normal and faulted children,
automatic reap, and rejection of a second wait on the stale identifier.

The keyboard path tracks left and right Shift press/release events and uses a
separate shifted keymap, including `:`. A two-character command such as `G:`
mounts the corresponding scanned AHDI volume as the active read-only FAT16
filesystem and resets the working directory to `\`. If mounting fails, the
kernel remounts the previous volume instead of leaving VFS unusable.

`TIMER.SYS` configures MFP Timer C for approximately 50 Hz while preserving
the Timer-D control nibble. Version 1 deliberately uses polling and never
enables an interrupt. It provides monotonic ticks plus a deferred-switch flag;
the kernel waits for four real hardware counter wraps. Each acknowledged flag
advances the sleep queue and starts a round-robin cycle outside interrupt
context. Exact per-task dispatch counts must match before boot reports success.
This avoids CPU-specific exception frames on 68000/020/030 until
interrupt-driven scheduling is introduced explicitly.
`PROC.SYS` version 5 saves the complete D0-D7/A0-A6 register bank at the
first instruction of every cooperative switch. Its boot-time context test
loads distinct signatures into D1-D7/A0-A6, performs a complete host/task
round trip, and rejects the process service if any value changes. D0 remains
the defined service return register. This validates the low-level context
primitive before user-mode PC/SR/USP frames are attached to it.
Syscall 26 captures the live user exception frame directly in the TRAP #0
entry assembly: USP, the exact post-trap PC, and SR. The kernel rejects odd
stacks, supervisor-mode SR values, malformed SR widths, and PCs outside the
current application image. Syscall 27 returns the stored 12-byte snapshot to
the owning process. `CTXFRAME.APP` verifies the exact continuation address,
user mode, interrupt mask, and stack alignment.
The TRAP entry now saves D0-D7/A0-A6 for every syscall, not only the C ABI
callee-saved subset. Syscall 28 atomically captures that bank together with
USP/PC/SR in the current process record. Syscall 30 restores the saved bank
and USP while retaining the current post-trap continuation PC. `CTXSW.APP`
sets unique register signatures, captures them, destroys every register, and
verifies their exact restoration. This is the process-context format used by
the forthcoming cross-process round-robin switch.
Syscall 31 creates a second user process record over the same validated image,
with a distinct PID, allocator owner, 2 KiB user stack and initial entry PC.
Syscall 10 now rewrites the live exception frame from the next READY record,
including D0-D7/A0-A6, USP, PC and SR, then returns through RTE. Process exit
selects and restores another READY context before unwinding to the shell.
`CTXRR.APP` verifies true A/B/A/B user-mode round-robin execution.
The illegal-instruction handler now preserves the complete faulting register
bank, terminates and cleans only the current process, and rewrites the live
exception frame from another READY process when available. If no peer exists,
the established nested/shell recovery path remains unchanged. `CTXFAULT.APP`
verifies B failing while A resumes, then allocates and frees memory to prove
that the surviving process and allocator remain operational.
Syscall 11 now has a context-aware path. With another READY process present it
saves the caller as SLEEPING with an absolute 50 Hz wake tick and immediately
restores the peer context; it no longer burns CPU in the sleeping process.
Cooperative scheduling polls the monotonic timer, promotes expired sleepers to
READY, and returns the wake tick in D0. The original bounded fallback remains
when there is no runnable peer. `SLPCTX.APP` verifies A running while B sleeps,
then B waking and exiting before A completes.
Syscall 32 queries a real child process by PID and optionally reaps a
TERMINATED record. Only the recorded parent PID can inspect or release it.
Normal syscall exit retains its signed status, while CPU-fault termination now
stores the corresponding negative status (for example -260 for illegal
instruction) before cleanup. `PIDWAIT.APP` verifies status 7, fault status
-260, automatic reap, and rejection of a stale PID.
Syscall 33 provides blocking waitpid for real contexts. A running parent is
saved as WAITING with a validated status destination, then the READY child is
restored immediately. Normal exit or an isolated CPU fault writes the status,
promotes the parent to READY, and atomically releases the child's process
record. `WPID.APP` verifies wakeup with status 9 and with fault status -260.
Syscall 34 asynchronously loads a separate validated `.APP` image, assigns it
an independent PID, image allocation, user stack, working directory and
allocator ownership, then returns its PID without running it synchronously.
The parent can combine it with syscall 33. `RSPAWN.APP` verifies a normal
separate child returning 7 and a separate faulting child returning -260.
When a parent exits first, its children are reparented to the kernel reaper.
Their image, stack and process-table slot are released automatically after
exit or fault. `ORPHAN.APP` exercises this lifecycle without leaving a zombie.
The kernel module API now exposes asynchronous image creation to trusted
system modules. The shell `START program.app [args]` command creates a READY
process and reports its PID; the kernel main loop then enters that context via
a dedicated saved-supervisor-stack/RTE boundary. On final process exit the
dispatcher restores the exact kernel context and resumes shell polling.
Completion of a kernel-dispatched process is tracked independently from its
numeric exit status (including status zero). The shell is notified through its
service interface and redraws the prompt using the current drive and path, so
asynchronous program output never leaves an apparently inactive blank line.
`MULTI.APP` creates two independently loaded children and exits before them.
The kernel reaper retains both runnable contexts; `COOPA.APP` and `COOPB.APP`
then produce the verified A1/B1/A2/B2 round-robin sequence before the final
dispatcher return and prompt redraw. This validates multiple image and stack
allocations across the kernel-to-user dispatch boundary.
When a kernel-dispatched process yields and no user peer is READY, its full
register bank, USP, PC and SR are saved and control returns to the supervisor
scheduler without terminating it. The shell can poll input and execute a
command before the process is dispatched again. `BGYIELD.APP` proves this by
remaining alive for 500 timer ticks while shell commands execute between its start and
finish messages. This is the first kernel/shell-aware cooperative background
execution path; no interrupt-time preemption is enabled yet.
Background console output carries an explicit scheduler return reason. The
shell redraw path begins on a fresh line before rendering the current drive,
path and cursor, avoiding an apparently blank command line after a yielding
program writes. INPUT.SYS also performs a bounded, non-blocking ACIA drain at
module initialization so stale firmware/IKBD bytes cannot consume the first
interactive key after the shell becomes ready.
Mouse packet framing is retained across ordinary empty ACIA polls. IKBD
transmits its header and two relative-coordinate bytes far more slowly than
the kernel's 64-call polling burst, so the former poll-count timeout exposed
coordinate bytes to the keyboard decoder and printed random shell characters.
A partial packet now expires against TIMER.SYS only after two real 50 Hz ticks;
this also prevents a permanently incomplete packet from consuming a later
keypress. An ACIA overrun explicitly resets framing and quarantines the whole
uncertain burst until 40 ms of input silence; a fixed byte-count quarantine
cannot recover safely when several relative-mouse packets have already
overwritten the one-byte ACIA receive register.

Asynchronous user stacks now include 32-bit `STAK` canaries immediately below
and above their usable 2--16 KiB range. Every cooperative context-save path
validates both canaries, USP alignment and USP bounds before retaining the
process. A violation terminates and releases only the offending process with
status `-0x3B0`; the scheduler and shell continue running. `STKFAULT.APP`
deliberately overwrites its lower canary and yields. The Hatari regression test
reports `[PID 1 FAULT 944]`, after which `HELLO.APP` still executes and exits 0.
This is software fault containment and preparation for later MMU-backed memory
protection on 68030-class machines; it does not claim to prevent arbitrary
out-of-range writes on a plain 68000.

System-call pointer validation now recognizes the current asynchronous
process's usable guarded-stack range in addition to its image and explicitly
owned heap blocks. Range checks use subtraction after lower-bound validation,
so addresses near `0xFFFFFFFF` cannot pass through integer wraparound. No
other process's stack or allocations are accepted. `PTRTEST.APP` verifies a
24-byte output buffer on its own stack, rejection of an unrelated address and
rejection of a wrapping range. Hatari reports
`PTRTEST: OWN STACK OK / FOREIGN RANGES REJECTED` and exit status zero; a
subsequent independent `HELLO.APP` run also exits zero.

All system calls that return 32-bit values or structured records now require
an even destination address before the kernel dereferences it. This is the
architectural requirement of the 68000 and prevents a valid writable range
with an odd starting address from turning a bad application argument into a
supervisor-mode address error. Byte buffers remain intentionally unrestricted.
PTRTEST.APP now also passes `writable_info+1` to the system-info call and expects
the normal `-0x280` validation result. The asynchronous Hatari regression run
reported `PTRTEST: STACK OK / FOREIGN, ODD AND RO RANGES REJECTED`, exited zero
and returned to the shell without a kernel panic.

The synchronous `run` path now allocates the same lower and upper stack guards
as an asynchronous process and registers its borrowed stack with the process
record for the duration of execution. Kernel validation can therefore accept
legitimate syscall output buffers on that stack, measure its high-water mark
and verify both guards; process cleanup leaves the borrowed allocation for the
serialized loader to release exactly once. This removes the previous protection
exception where PTRTEST passed under `start` but rejected its own stack under
`run`. In Hatari, both `run \\system\\ptrtest.app` and
`start \\system\\ptrtest.app` printed the complete success message and exited
zero. The kernel also replaced two near-limit PC-relative scheduler flag loads
with absolute 68000 addressing after growth exposed their 32 KiB relocation
limit.

The application linker now appends a checksummed `APW2` policy record carrying
both the end of executable code and the first writable file-backed offset.
Syscalls distinguish input buffers from output buffers: code, constants, the
application header and policy record are read-only to kernel writes, while
`.data`, `.bss`, the guarded user stack and owned heap allocations are valid
output destinations. Process entry, clone entry and captured continuation PCs
must be aligned and lie between the 32-byte header and executable-code end;
addresses in constants, data, BSS, stack or heap cannot become a scheduled PC.
`PTRTEST.APP` asks the system-info syscall to overwrite its entry code and asks
the clone syscall to start in BSS, confirming rejection with `-0x280` and
`-0x360`. The complete rebuilt application set was installed with the kernel.
In Hatari, PTRTEST exits zero and `PROCSTR.APP` still completes all eight
spawn/wait/reap cycles with its memory accounting intact.

Every user heap allocation now reserves 16-byte `HEAP` guard regions before
and after the aligned usable buffer. Process-visible addresses and sizes still
describe only the usable range; the kernel retains the true allocation base.
Syscall 17 validates both guards, releases the true block even after detected
corruption, and returns `-0x3B1`. Process-exit cleanup performs the same check
and converts an otherwise normal exit into that isolated heap-fault status
without leaking the allocation. `HGUARD.APP` overwrites the first byte beyond
a 32-byte block, verifies detection and compares ST/TT free memory before and
after release. Hatari reports `HGUARD: OVERFLOW ISOLATED / MEMORY RELEASED`;
the unchanged `MEMTEST.APP` subsequently passes normal allocation, buffer use
and guarded free. Guard validation also runs at ordinary syscall entry and at
the cooperative yield, sleep, input-wait and waitpid context boundaries. The
current `HGUARD.APP` therefore corrupts its upper guard and merely issues a
yield; it cannot continue or handle the error itself. Hatari reports
`[PID 1 FAULT 945]`, ST-RAM remains exactly 3764 KiB before and after automatic
cleanup, and a following MEMTEST process exits zero.

Every `TRAP #0` now passes through one assembly/C validation gate before the
syscall number is decoded. The gate reads the live USP plus PC and SR from the
CPU exception frame and validates stack bounds/canaries, every owned heap
canary, executable PC policy and user-mode SR. A failed check follows the same
isolated process-exit and scheduler path as a CPU exception, so even ordinary
print, allocation or exit calls cannot bypass protection. Hatari verifies
`HGUARD.APP` as `FAULT 945`, `STKFAULT.APP` as `FAULT 944`, then completes all
eight `PROCSTR.APP` spawn/wait/reap cycles and exits zero.

Bus error, address error and divide-by-zero now share the same completion
finalizer as illegal-instruction and guarded-memory faults. It records the
signed status, wakes a parent blocked in waitpid, copies the status to the
validated parent buffer and reaps the child atomically before returning to the
supervisor scheduler. `BUSFLT.APP`, `ADRFLT.APP` and `DIVFLT.APP` produce
statuses -258, -259 and -261. `EXWAIT.APP` spawns and synchronously waits for
these faults plus CHK, TRAPV, privilege violation, Line-A and Line-F (-262,
-263, -264, -266 and -267), verifies every status and compares ST/TT free
memory before and after all ten children. The final two children exercise the
endpoints of the otherwise unsupported trap range: `TRAP #1` and `TRAP #15`
(statuses -289 and -303). Hatari reports
`EXWAIT: 10 CHILD FAULTS / WAKE / REAP / MEMORY PASS` and exit status zero.
The shell completion path now suppresses a prompt redraw when a READY user
process is already waiting to resume. Consequently the ten EXWAIT child
faults return directly to their blocked parent without printing misleading
intermediate `C:\>` prompts. A top-level fault still prints its completion and
redraws the prompt; this was verified separately with `DIVFLT.APP`.

Bus error, address error and divide-by-zero now use the same completion
finalizer as illegal-instruction faults. It records the signed vector status,
wakes a parent blocked in waitpid or hands a root-process completion to the
shell, and releases the process record before discarding the CPU-specific
exception frame through the saved supervisor return stack. `BUSFLT.APP`,
`ADRFLT.APP` and `DIVFLT.APP` deliberately trigger vectors 2, 3 and 5.
Hatari reports `FAULT 258`, `FAULT 259` and `FAULT 261` respectively; a
following HELLO process exits zero and ST-RAM returns to 3764 KiB.

Privilege violation, Line-A and Line-F are also installed as protected kernel
vectors and use the same process finalizer. `PRIVFLT.APP`, `LINEA.APP` and
`LINEF.APP` deliberately trigger vectors 8, 10 and 11. In Hatari they terminate
only their own processes as `FAULT 264`, `FAULT 266` and `FAULT 267`; a
subsequent `HELLO.APP` starts normally and exits zero, confirming that the
kernel, scheduler and shell remain usable after every exception.

The asynchronous loader now validates the complete APW2 layout before stack
allocation or PID creation: the file must contain the policy trailer,
`entry < executable_end <= writable_start <= policy`, the entry must remain in
the executable interval, and the existing additive checksum must also match.
`BADPOL.APP` is a deliberately malformed regression fixture whose checksum is
valid but whose executable range overlaps writable data. Hatari reports
`PROCESS START FAILED`; ST-RAM remains 3765 KiB before and after rejection, no
PID is consumed, and the valid PTRTEST application still exits zero afterward.

The first hardware-preemptive scheduler path is now active on the 68000 test
target. The kernel explicitly assigns the MFP vector base to `0x40`, installs
Timer C at vector `0x45` (`0x114`) and enables only that level-6 MFP source;
VBL and unrelated MFP interrupts remain masked while their drivers are still
polling. User SR uses interrupt mask level 5, so Timer C can interrupt user
code but cannot preempt supervisor/kernel work. The IRQ entry validates the
live USP, PC, SR, stack guards and heap guards, saves all D0-D7/A0-A6 registers
without applying the cooperative-yield D0 result, and either resumes another
READY process or returns cleanly to the supervisor scheduler.

`HOGTEST.APP` clones a child and then executes a 20-million-iteration CPU loop
without any yield or syscall. Hatari shows the child running during that loop,
returns a usable `C:\>` prompt while the parent remains READY, and finally
reports `HOGTEST: PREEMPT / CONTEXT / REAP PASS` plus parent exit status zero.
The full exception regression also remains valid under IRQ scheduling:
`DIVFLT.APP` reports `FAULT 261` and `EXWAIT.APP` completes all ten child
fault/wake/reap cases with unchanged memory.

Fresh user entries now use a complete eight-byte format-0 exception frame:
SR, PC and a zero format/vector word. A 68000 consumes its six-byte portion
and safely leaves the unused word on the saved supervisor stack; 68010 and
newer processors consume the full frame required by `RTE`. Dispatcher-created
frames use the same layout, while frames produced by real Timer-C interrupts
retain the CPU-provided format word. Hatari cold-boot tests on both 68000 and
68030 complete `EXWAIT.APP` and `HOGTEST.APP` with exit status zero. The address
fault fixture now jumps to an odd instruction address instead of performing an
odd data access, because 68020/68030 legally support unaligned data; the new
test reliably reports vector 3 on both tested CPU modes.

Administrative termination is now covered by a deliberately non-cooperative
`HANG.APP` fixture. It prints one startup line and then remains in an infinite
CPU loop without issuing another syscall or yield. On a cold 68030 Hatari run,
Timer-C preemption returned control to the shell, `KILL 1` removed only that
READY process and reported `[PID 1 KILLED]`. ST-RAM was 3763 KiB both before
the start and after termination, proving that its image and guarded stack were
released. A following `HELLO.APP` ran as PID 2 and exited zero, confirming that
the scheduler, shell and executable loader remain usable after forced cleanup.

Each process record now also owns a 32-bit CPU-time counter. It is incremented
only when the level-6 Timer-C interrupt actually preempts that process, so
READY, sleeping and suspended time is not charged as CPU execution. The module
API exposes the counter without allowing modules to modify it, and `JOBS`
prints it as `CPU TICKS` next to memory and open-file usage. In the 68030 Hatari
test, the permanently looping HANG process advanced from 533 to 996 ticks
between two queries, remained killable, and released its resources normally.
This accounting is deliberately observational for now; no automatic watchdog
policy can terminate a program until explicit limits and exemptions exist.

Private application heap use is limited to 1 MiB per process across all eight
allocation handles. The check is overflow-safe and runs before the allocator,
so one damaged or hostile program cannot consume the complete ST/TT pool and
starve the kernel or other tasks. Exceeding the quota returns `-0x2C3`; normal
out-of-memory and invalid-request errors remain distinct. `QUOTA.APP` allocates
two guarded 512 KiB blocks, verifies rejection of a further allocation, frees
both blocks and compares the ST/TT pool counters. On the 68030 Hatari test it
reported `QUOTA: 1 MB PROCESS LIMIT / MEMORY RELEASE PASS`, ST-RAM returned
exactly to 3763 KiB, and a following `HELLO.APP` exited zero.

An optional per-process CPU watchdog is available through
`WATCHDOG PID CPU_TICKS`; a limit of zero disables it. It is disabled for every
new process, accepts at most 180000 charged ticks and can only be configured
for top-level shell processes, preventing one application from controlling a
child owned by another application. Expiry is handled directly at the safe
Timer-C preemption boundary: the process receives status `-4`, its guarded
heap, stack, image and handles are released, and scheduling continues. Both
the asynchronous completion path and `WAIT` identify this outcome as
`WATCHDOG`, not as a CPU fault. In Hatari, a non-cooperative HANG process with
a 100-tick limit ended as `[PID 1 WATCHDOG]` and returned a live shell prompt.

Private heap allocation now also preserves global emergency reserves of at
least 256 KiB ST-RAM and 64 KiB TT-RAM. The reserved allocator accounts for
alignment and a possible split-block header before accepting a request, and
tries TT first only when that pool can retain its own reserve. Trusted kernel
and driver allocations remain separate. `HOLDMEM.APP` requests a 1 MiB ST heap
and then deliberately consumes CPU forever. In the 4 MiB 68030 Hatari test,
three copies retained their heaps while the fourth reported
`HOLDMEM: KERNEL MEMORY RESERVE PROTECTED` and exited normally. Killing the
three holders restored the baseline 3762 KiB free ST-RAM.

That stress test also exposed and fixed supervisor starvation. Previously two
permanently READY CPU-bound processes could switch directly to one another at
every Timer-C interrupt and never return through the kernel main loop. The
applications kept running, but shell and IKBD administration became
unavailable. Hardware preemption now saves the interrupted process and always
returns each quantum through the supervisor loop; normal round-robin dispatch
then resumes a READY process after input, events and shell work have been
serviced. With three simultaneous HOLDMEM loops, the prompt remained visible
and `KILL 1`, `KILL 2` and `KILL 3` all completed without a reset.

The shell service ABI is now version 2 and exposes a read-only `line_active`
query. Kernel completion notices are deferred while the user is editing a
command, preventing an asynchronous `[PID ...]` message from being inserted
inside that command. If the completing application printed output after the
line was entered, the dispatch-completion handler redraws the drive, current
path and the untouched input buffer immediately; the queued PID status is
printed only after the user submits or clears the line. Kernel and SHELL.SYS
validate the matching service version and structure size at boot. A deliberate
version-registration mismatch was rejected as `SHELL UNAVAILABLE`, confirming
that incompatible modules fail closed; after correcting the registration, the
68030 cold boot again reports `PREEMPTIVE / SHELL READY`.

The shell now provides `CPU`, backed by a read-only kernel hardware query
rather than direct privileged-register access from the module. It reports the
detected 68000/010/020/030/040/060-compatible family, address-bus width,
on-CPU MMU and instruction/data-cache capabilities, and FPU presence. A
side-effect-free `FNOP` probe is performed once during firmware startup under
a temporary line-F exception handler; the result is passed through the boot
handoff. External FPUs are deliberately reported as
`68881/68882-COMPATIBLE (TYPE UNKNOWN)`, because distinguishing the physical
part reliably would require unsafe assumptions. The CPU vendor is likewise
reported as not identifiable by the 68k ISA. A mono Hatari cold boot without
an FPU completed normally and `CPU` reported a 68000-compatible, 24-bit system
with no MMU, cache or FPU before returning to a clean prompt.
The same immutable record now also carries the firmware-detected Atari model
and clock result. `CPU` prints the model and a numeric MHz value only when the
machine-specific BIOS probe can establish it (Mega STE, TT or Falcon); plain
ST/STE systems deliberately show `SYSTEM CLOCK: UNKNOWN` rather than assuming
that an accelerator is absent. The added bytes reuse the boot handoff's
reserved area, so all following service pointers retain their established
offsets. A repeated mono Hatari cold boot identified the emulated Atari STE,
reported the clock as unknown, and returned to the prompt normally.

User stacks now carry a non-destructive high-water mark in addition to their
existing guards. On process creation the usable 2 KiB area is initialized
with a fixed byte pattern between the guard words. A read-only kernel query
scans only the untouched prefix and reports maximum stack consumption;
`JOBS` displays it as `STACK used/2048` beside memory, handles and CPU ticks.
The scan is bounded to four 2 KiB stacks and runs only on request, adding no
timer-interrupt or scheduling latency. In Hatari, simultaneous HANG and
HOLDMEM processes remained preemptible and killable, both resource records
were printed without corrupting the prompt, and their allocations were
released through the normal termination path.

Process identity and allocator ownership are now independent. Earlier builds
derived a private-memory owner from the low eight bits of the PID, allowing a
long-lived PID 1 and a later PID 257 to share an allocator identity. Each of
the four concurrently occupied process slots now has a fixed, distinct owner
domain (`0x0301` through `0x0304`), while a bounded PID allocator skips zero
and every identifier still present in the process table after 32-bit wrap.
Slot reuse occurs only after normal cleanup, making the owner domain suitable
for a future MMU address-space identifier. Hatari ran two simultaneous 1 MiB
HOLDMEM processes as PID 1 and PID 2, reported both independently, killed and
released both, restored ST-RAM to the 3761 KiB baseline, and then ran
HELLO.APP successfully as PID 3.

Asynchronous executable images and user stacks now transfer allocator
ownership from the serialized loader domain to the selected process-slot
domain before the process becomes READY. The allocator validates the exact
block header, guard and old owner; a partial two-block transfer rolls the
first block back and returns the process slot to FREE. The boot-time memory
self-test also proves that the old owner can no longer free a transferred
block while the new owner can. This closes the remaining shared-owner gap for
future MMU mappings. In Hatari, `MEMORY SELF-TEST` remained `PASS`, malformed
BADPOL.APP was rejected with ST-RAM unchanged at 3761 KiB, and an asynchronous
HELLO.APP then received PID 1, exited zero and restored the same baseline.

IKBD input now has a kernel-owned 256-entry raw-byte interrupt ring. The MFP
GPIP4 ACIA source is installed at vector `0x46`, configured as a falling-edge
input, and enabled together with Timer C only after INPUT.SYS and the shell
have passed their boot checks. Its bounded ISR saves the registers it uses,
copies only ACIA status/data, and never invokes modules or the scheduler.
INPUT.SYS consumes the ring in supervisor context, where its existing mouse,
modifier and overrun framing remains authoritative. A direct polling fallback
keeps the keyboard usable on hardware that reroutes or lacks this interrupt.
`INPUTSTAT` includes both raw-ring overflow and filtered-event loss. Under two
simultaneous HOLDMEM CPU loops, Hatari accepted the complete fast-typed
`inputstat` and both `kill` commands with `IKBD DROPPED: 0`; moving the mouse
produced no shell characters, and both processes terminated normally.
The interrupt is shared with the MIDI ACIA. Until a dedicated MIDI module is
loaded, firmware resets that device and leaves it at `/16, 8N1` with receiver
interrupts disabled, preventing an inherited MIDI request from holding GPIP4
active and causing an interrupt storm. An intentionally extreme synthetic
mouse drag in Hatari reduced raw loss from 23 bytes with the initial 64-entry
ring to two ACIA-overrun bytes with the 256-entry ring; packet framing contained
them and the shell remained unchanged. Ordinary keyboard stress under two CPU
loops remained at zero dropped bytes.

INPUT.SYS now maintains a 128-bit IKBD pressed-key bitmap. A make code is
delivered only when the key was previously up; further make codes are ignored
until the matching break code clears the bit. This removes uncontrolled
hardware/emulator key repeat from shell command entry. If the raw interrupt
ring reports a dropped byte or ACIA reports an overrun, INPUT.SYS clears the
complete bitmap so a lost break code cannot leave a key permanently suppressed.
This intentionally defers friendly key repeat to a later timer-controlled input
policy. In Hatari, the first `cpu` command, repeated `run` commands beginning
with `r`, and Up-arrow history recall were accepted without missing or duplicated
characters. After a cold boot that recorded one raw IKBD drop, recovery still
accepted `run \\system\\hello.app` and `inputstat`, HELLO exited zero, the shell
returned to a clean prompt and the event queue remained empty.

The original 64-entry raw-ring consumer still masked its tail with `& 63` after
the storage and producer had been enlarged to 256 entries. On every 64th byte it
therefore returned to slot zero and replayed old complete key sequences, which
explains repeated command tails such as `emory`. The consumer now relies on
natural eight-bit wraparound and traverses all 256 slots. A temporary shell
mailbox flush was removed because it treated the symptom and could discard valid
type-ahead. In the final Hatari regression, four consecutive
`run \\system\\hello.app` commands crossed the former 64-byte boundary, all four
programs exited zero, four idle seconds produced no extra text, and `inputstat`
reported `IKBD DROPPED: 0  EVENTS PENDING: 0`.

All user-memory interval checks now share one half-open subrange primitive.
It verifies the lower bound first and uses subtraction for the remaining size,
so neither `address + length` nor `base + size` can wrap into an accepted range.
The same rule covers readable images, APW2 writable data, BSS, guarded stacks
and private heap blocks. PTRTEST additionally passes a 24-byte result buffer
starting 16 bytes below the top of its stack: its beginning is legal but its end
crosses the boundary, and the syscall must return `-0x280` without writing.
Both synchronous `run` and asynchronous `start` printed
`PTRTEST: STACK OK / BOUNDARY, FOREIGN, ODD AND RO REJECTED`, exited zero and
left the kernel and allocator operational in Hatari.

Process-slot allocation now treats only `FREE` records as reusable. A
`TERMINATED` child remains reserved until its parent explicitly waits/reaps it,
so a later start cannot destroy an unread exit or fault status. The synchronous
runner also refuses a full four-slot table before entering user mode instead of
overwriting a live record. Its assembly failure path unwinds the nested
runner depth and returns `-2` to the loader. Clone stack allocation now precedes
PID publication, and a rare PID-allocation failure releases the stack instead
of leaving a phantom process. In Hatari, four concurrent HANG.APP instances
held PIDs 1--4; a fifth `run` was rejected with fault code 2 while all four jobs
remained listed and killable. Killing them restored ST-RAM to 3758 KiB, HELLO
then ran as PID 5 and exited zero, and PIDWAIT subsequently passed normal-exit,
fault, ownership, stale-PID and reap checks as PID 6.

The kernel now exposes a validated, read-only process memory-map description
through the append-only module API. The `maps PID` shell command reports the
APW2-derived executable, read-only data, writable data, policy trailer, BSS and
guarded stack ranges in hexadecimal, together with the process ownership domain,
private/shared image state and current stack high-water mark. This is a software
policy model only; it does not enable an MMU and therefore remains compatible
with the 68000. Hatari testing started HOLDMEM.APP, displayed all six ranges for
PID 2, terminated it, rejected a later query as `PROCESS NOT FOUND`, and returned
to the prompt without a leak. Two integration defects found during that test—an
unaligned result buffer and an unsaved A2 register—were corrected and the full
sequence was repeated successfully.

Cloned processes now receive a private copy of the complete APP memory image
and an independent guarded user stack. Kernel-owned image and stack allocations
are pinned while a process is alive, so a shell return or an ordinary allocator
free cannot recycle memory still used by a scheduled child. `SLPCTX.APP` changes
a child-local BSS value after sleep/wakeup, verifies that the parent's value is
unchanged, then checks exit status and reaps the child. This also corrected the
foreground runner's stack allocation to include both four-byte guards; the
former upper guard occupied bytes beyond its allocation and could overlap a
later process stack. Hatari completed the private-image test and a subsequent
independent `HELLO.APP` run without a fault or leak.

Syscalls 36 and 37 provide the first bounded inter-process communication
primitive. Every live process owns a fixed four-entry kernel mailbox. A message
contains the kernel-supplied sender PID and four 32-bit values; send copies the
payload from a validated readable range and receive copies the oldest message
to a validated writable range. No user pointer or dynamically allocated queue
node crosses process ownership domains. Sending to a missing process, sending
to a full mailbox and receiving from an empty mailbox return distinct errors.
`IPCTEST.APP` clones a private child, sends a typed request, validates the sender
and all payload words in the child, returns a typed reply, then verifies child
exit and reap in the parent. In Hatari the test printed
`IPCTEST.APP: BOUNDED PROCESS MAILBOX OK`, both processes exited zero, and the
following `HELLO.APP` regression also exited zero at a clean shell prompt.

Syscall 38 adds blocking mailbox receive without changing syscall 37's
nonblocking semantics. When no message is queued, the kernel saves the complete
user context and validated destination buffer, marks the process as waiting and
runs another READY process. Send delivers directly to such a waiter, records
the sender PID, sets the saved return value to zero and makes the recipient
READY. `IPCTEST.APP` now deliberately yields after cloning so the child blocks
first; the parent wakes it with a request, blocks for the reply, and is woken by
the child without either side polling. Hatari reported
`IPCTEST.APP: BLOCKING PROCESS MAILBOX OK`; a subsequent `HELLO.APP` run also
exited zero and returned to a clean prompt.

Syscalls 39--41 implement bounded user-service discovery. A process can
register, find and explicitly unregister a name of up to twelve characters;
lookup is ASCII case-insensitive and accepts only letters, digits, dot, dash
and underscore. The kernel stores eight fixed entries, prevents a second PID
from claiming an existing name and automatically removes every service owned by
a process during normal exit, CPU-fault cleanup or administrative termination.
No registry operation allocates memory or exposes a kernel pointer.
`SVCTEST.APP` clones a service process, registers `ECHO.TEST`, synchronizes over
blocking IPC, finds it through lowercase `echo.test`, verifies explicit
register/unregister, duplicate-owner rejection, and confirms that lookup fails
after the child exits and is reaped.
Hatari printed `SVCTEST.APP: NAMED SERVICE DISCOVERY / CLEANUP OK`; the earlier
blocking `IPCTEST.APP` regression then passed again and both runs returned to a
clean shell prompt.

The kernel now has a bounded startup-service manager. Its fixed ordered table
loads `\\SYSTEM\\INIT.APP` before exposing the shell prompt, dispatches the new
process until it registers `SYSTEM.INIT`, and accepts readiness only when the
registered PID matches the PID returned by the loader. Every entry has a
tick-based five-second deadline; load failure or timeout is reported explicitly,
the timed-out process is terminated when possible, and shell startup continues
instead of hanging forever. The table contains no dynamic state and can be
extended with later desktop, graphics and compatibility services.

`INIT.APP` is a minimal persistent service host. After registering it blocks in
syscall 38 and therefore consumes no polling time; a typed `STOP` IPC message is
reserved for orderly shutdown. The shell's process display now names state 8 as
`MESSAGE WAIT`. On the final Hatari boot the kernel printed
`STARTUP SERVICES: SYSTEM.INIT READY` before `C:\>`. `jobs` reported INIT as PID
1, PPID 0, `MESSAGE WAIT`, one CPU tick and a 0/2048-byte used stack. With that
permanent service still occupying one process slot, `SVCTEST.APP` used a parent
and cloned child, passed discovery, blocking IPC and cleanup, exited zero and
returned to a clean prompt. This completes the command-line process/service
foundation intended to precede the desktop layer.

The first graphics milestone adds a Shifter backend and a versioned internal
graphics contract in `kernel_graphics.h`. It supports ST Low 320x200x4, ST
Medium 640x200x2 and ST High 640x400x1 planar layouts without reprogramming the
active video mode. Implemented primitives are clear, pixel, word-masked filled
rectangle, frame, overlap-safe rectangle copy and Atari 8x16 text. Every public
operation clips to the framebuffer, including overflow-safe handling of
negative origins. Damage tracking merges touching regions in a fixed 16-entry
list and collapses safely to one full-screen region on overflow.

The graphics boot self-test uses guarded local buffers rather than the live
screen. It checks monochrome and four-plane colour encoding, masked writes,
clipping, dirty-region merging and bounded overflow. Hatari Mono reported
`GRAPHICS BACKEND: SHIFTER 640X400 / CLIP+DIRTY READY`; startup services then
reached `SYSTEM.INIT READY`, and a subsequent `HELLO.APP` regression exited zero
at a clean prompt. Detailed design notes are in `docs/graphics.md`.

The second graphics milestone adds the persistent `SYSTEM.GFX` broker. It is
started after INIT, registers through the existing bounded service registry and
blocks in IPC when idle. Graphics syscalls 42--44 accept commands, return mode
metadata and restore the console, but authorize only the PID which owns the
`SYSTEM.GFX` registry entry. No application receives the physical framebuffer
address. `GFXTEST.APP` discovers the broker by name and exercises clear, frame
and fill operations using the detected screen geometry. Hatari Mono showed the
complete test card; pressing Space restored an undamaged console and returned
exit code 0 at a clean prompt.

The broker/kernel boundary now also carries the authenticated PID of the
original client. With a persistent GUI session active, drawing, desktop entry
and console restore are accepted only from its foreground PID; background
clients receive `-0x3e5`. Logical surface operations remain usable in the
background but retain strict sender ownership. This closes the screen-write
race before multi-application GUI switching is introduced.

The Shifter contract now also supplies clipped Bresenham lines and guarded
palette writes. Boot handoff ABI 3 carries the hardware-probed Blitter presence
and Setup enable state into the kernel; explicit padding and compile-time
layout assertions keep all longwords safe for MC68000. Graphics capability bits
expose these facts without assuming that every STE-class configuration has a
usable accelerator. The extended Hatari Mono test drew both full-screen
diagonals, restored the console and exited zero.

The first Blitter driver now accelerates planar rectangle fills. It owns all
register programming inside the kernel, derives addresses only from the trusted
Shifter surface, preserves unaligned edges with end masks and uses a bounded
completion wait. Its guarded ST-RAM startup test disables acceleration on any
failure, after which the operation is repeated by the 68000 renderer. Hatari
STE completed the hardware test and accelerated `GFXTEST.APP`, then restored
the shell with exit code 0.

The first surface-management layer adds eight fixed, allocation-free GUI
surface identities. Generation-tagged handles prevent stale reuse, every
operation checks the owning PID, dimensions cannot exceed the detected screen,
and process exit or termination releases all owned entries. Its boot self-test
checks cross-owner access, stale handles, exhaustion and cleanup. Hatari Mono
reported `SURFACE MANAGER: 8 HANDLES / OWNER GUARDS OK` before starting INIT,
GFX and the shell normally.

Surface operations are now available through `SYSTEM.GFX` IPC. Create, query
and destroy take ownership exclusively from the kernel-supplied mailbox sender;
the client cannot submit a PID. `SURFTEST.APP` used the last two process slots
for a parent and private cloned child, rejected the child's access to the
parent handle, retained parent access, destroyed it and rejected its stale
generation. Both processes exited zero. This work also corrected the broker's
command colour copy, which had previously read beyond the four-word message;
the complete Blitter graphics regression and console restore passed afterward.

The first desktop policy service is now present as `SYSTEM.GUI`. It starts
after INIT and GFX, keeps a fixed application registry, derives client identity
from the kernel mailbox sender and permits exactly one foreground PID. Its
register, unregister, foreground and query operations remain separate from the
privileged graphics broker. `GUITEST.APP` validated a two-process foreground
handoff and deterministic fallback to the parent in Hatari Mono; the parent
exited with code 0 and the shell remained usable.

GUI registrations now also survive abnormal application behaviour safely.
Normal exit, trapped CPU faults and administrative termination emit a
kernel-authenticated lifecycle message to `SYSTEM.GUI`; reserved mailbox
capacity prevents ordinary client traffic from consuming the event budget.
The expanded GUI test lets its foreground child execute `illegal` without
unregistering, verifies fault status `-260`, automatic removal and foreground
fallback to the parent, and exits zero in Hatari Mono. This removes the stale
owner hazard before keyboard and mouse events are assigned to GUI clients.

Foreground keyboard routing is now connected end to end. A registered GUI
client requests one key through `SYSTEM.GUI`; the service verifies that the
mailbox sender is the foreground PID, and the kernel delivers a single `KKEY`
event directly to that process before returning ownership to the shell. A
bounded five-tick IKBD handoff barrier removes launch-command residue, while
process and service exit cancel any armed target. `GUIKEY.APP` rejected a wrong
key and then accepted a fresh K in Hatari Mono, reported
`FOREGROUND KEY DELIVERED OK`, exited zero and left the command prompt usable.

`KKEY` now carries the full Atari keyboard event rather than only a character:
make/break type, IKBD scan code, left/right Shift, Ctrl and Alt state, plus an
optional translated ASCII value. The decoder updates modifiers on both press
and release. The Hatari regression validates a K key-down as scan code `0x25`,
modifier mask zero and ASCII `k`, while retaining the one-shot ownership and
failure cleanup rules.

Foreground clients can now open a continuous GUI keyboard session. During the
session all IKBD make/break events are delivered as `KKEY` messages and shell
input is suspended; explicit close, foreground change, unregister, process
exit, fault or kill restores ownership safely. The fixed mailbox never blocks
the kernel: overflow is counted and reported with the next delivered event,
while modifier state stays synchronized. `INPUT.SYS` now retains ordinary key
release events, which the shell ignores and GUI clients can consume.
`GUISTRM.APP` validated K down and K up in one session in Hatari Mono, closed
the session, exited zero and returned to a working prompt.

Modifier lifetime is covered by `GUIMOD.APP`. The test accepts either legal
release order and host mappings which expose a generalized or dual Shift
state, while still requiring Shift make, uppercase K make, K break and a final
zero Shift mask. Hatari Mono reported `SHIFT+K MAKE/BREAK OK`, exit code 0 and
a usable shell, confirming that no modifier remains stuck after a GUI session.

Mouse routing uses complete three-byte relative IKBD packets and a dedicated
event queue, so coordinate bytes never enter the shell keyboard stream. During
a continuous foreground GUI session the kernel emits `KMOU` messages containing
clamped screen coordinates, button state, signed deltas and bounded-loss count.
Partial packets retain timeout and overrun quarantine. `GUIMOUSE.APP` exercises
movement, button-down and button-up with one drag gesture and verifies cleanup
back to the command prompt.

The kernel now also owns a clipped 16x16 software pointer. It preserves its
background in a fixed 256-byte buffer, uses colour inversion for visibility in
all Shifter modes and damages only the old and new cursor rectangles. It is
shown only after actual motion and removed on every GUI-session cleanup path,
so neither a crashed application nor a foreground handoff can leave cursor
pixels stamped onto the screen.

Every graphics primitive is now cursor-aware. Overlapping output temporarily
restores the saved background and redraws the pointer over the new content;
rectangle copies protect both source and destination so a cursor image cannot
be copied into a window. Non-overlapping drawing stays on the fast path. A
startup framebuffer probe validates the composed pixel and the final restored
pixel, then restores the original value before any status text is displayed.

Full mouse queues coalesce the newest mouse state rather than losing a final
button release. The latest position and buttons therefore remain authoritative
under heavy motion on an 8 MHz CPU, without overwriting keyboard messages.

GUI input is capped at three mailbox entries, leaving room for an authenticated
broker response. A mouse burst therefore cannot deadlock an application while
it waits for `SYSTEM.GFX`; lifecycle messages retain their separate reserve.
Queued IKBD motion packets are now accumulated in one kernel pass, and the
Shifter Mono cursor uses direct one/two-word save, XOR and restore operations.
The guarded planar pixel implementation remains the fallback for colour modes.

`DESKTOP.APP` provides the first interactive desktop window. It acquires GUI
ownership before drawing, renders a resolution-independent 240x140 window with
an `ATARI DESKTOP` title, close gadget and resize handle, and moves it by
overlap-safe rectangle copy plus at most two exposed background strips. Escape
and the close gadget share the same safe console-restore path. `SYSTEM.GFX`
IPC operation 12 supplies the copy without exposing screen memory; operation
14 renders one request-encoded 8x16 glyph without accepting a user pointer. The Blitter
self-test now covers its special one-word edge-mask rule so one-pixel window
borders remain one pixel wide.

A classic 18-pixel Atari-style menu bar is now drawn as desktop-owned chrome,
with `DESK`, `FILE` and `VIEW` labels. Movable windows are clamped below Y=20,
so neither fast rectangle copying nor exposed-strip cleanup can damage this
global area. The initial milestone keeps the bar static before adding bounded
drop-down hit testing.

Aligned Mono rectangle copies now operate on complete 16-pixel Shifter words
with overlap-safe forward or reverse traversal. For the default 240x140
desktop window this reduces the copy body from 33,600 pixel operations to
2,100 word transfers. Arbitrary Mono offsets use a bounded 80-byte scanline
buffer to combine adjacent source words and mask the two destination edges;
tests compare the result bit-for-bit with the pixel implementation. Colour
operations deliberately retain the fully clipped pixel fallback.

The resize gadget records pending geometry while the button is held and
commits it only on release, avoiding repeated redraws on an 8 MHz 68000.
Dimensions are constrained to 220x70 and the visible Shifter area; subsequent
title dragging uses the committed size for copying and strip cleanup.

The first client-area widget is a bounded `START`/`READY` push button. It can
be activated by mouse or Return, debounces held-button packets and repaints
only its own 80x22 rectangle. Application controls therefore remain logically
separate from the system-owned title, close gadget and resize gadget.

An independent `INFO`/`HIDE` widget now validates multiple-control dispatch.
`Tab` and the horizontal arrow keys move a visible focus frame, Return changes
only the focused widget, and mouse selection updates the same focus state.

The widget now shows a compact diagnostic panel containing the active Shifter
mode, resolution and `SURFACE READY` state. Its transition updates only a
208x52 client region; resize reconstruction retains the state, and a
130-pixel minimum height keeps it inside the window in every Shifter mode.

The desktop process now registers a full-screen logical surface through
`SYSTEM.GFX`. Ownership comes from its authenticated sender PID; the handle is
released on normal close and recoverable failures, while kernel owner cleanup
covers abnormal process death. The identity is metadata-only and consumes no
additional full-screen backing buffer.

Console and desktop now use separate Shifter framebuffers. The kernel pins a
256-byte-aligned 32 KB screen in ST-RAM, authorizes only the persistent
foreground GUI owner to activate it and switches the hardware base without
touching the STE-only low-byte register. Escape, exit, fault, kill or GUI
service failure returns to the untouched console automatically. Hatari Mono
showed a clean window-only desktop, then restored the exact command history;
`pwd` confirmed that shell input remained operational.

# Font license

The firmware UI uses the Atari ST 8x16 system-style bitmap font from
`ntwk/atarist-font`, distributed under the BSD 3-Clause license.  The license
text is included in `licenses/atarist-font-LICENSE`.
