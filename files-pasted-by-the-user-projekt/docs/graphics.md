# Graphics foundation

## EmuTOS integration boundary

For the baseline MC68000 + Shifter target, proven EmuTOS VDI raster routines,
system resources and cursor/font data may replace equivalent low-level drawing
code incrementally.  Imported or derived material is kept in an explicitly
identified GPL component with its upstream revision and notices recorded.

EmuTOS AES and EmuDesk do not become the scheduler or process owner.  The
Atari System kernel retains preemptive task state, memory ownership, IPC and
foreground policy; `SYSTEM.GUI` remains the single privileged window server.
Applications draw into kernel-owned surfaces through bounded messages, and the
server composes the visible Shifter screen.  This separation permits classic
GEM appearance and mature 68000 raster code without reducing the system to the
traditional single foreground AES execution model.

Integration is deliberately staged: first one-plane Mono primitives, then the
two- and four-plane Shifter paths, then optional Blitter acceleration.  The
existing CPU renderer remains a recovery backend until each imported primitive
passes guard-buffer comparisons and a Hatari visual test.

The first imported module is `emutos_vdi_mono`.  Its solid rectangle fill and
aligned source-only word blit are reduced from EmuTOS VDI's rectangle and
`bit_blt()` paths.  The latter follows EmuTOS's overlap rule by traversing from
the lower-right whenever the destination follows the source in screen memory.
Boot-time differential tests compare both routines against the previous local
renderer, including edge masks and overlapping copies in all directions.

The graphics primitives remain inside the kernel, behind the persistent
`SYSTEM.GFX` user service. Applications discover that broker through the
service registry and exchange bounded IPC messages with it. The text console
remains the recovery interface throughout this transition.

The broker passes the kernel-authenticated PID of the original IPC sender
outside the drawing payload. While a persistent GUI foreground owner exists,
the kernel rejects drawing, desktop activation and console restore requests
from every other PID with `-0x3e5`. Surface create/query/destroy remain
available to background applications but are scoped to that sender. A
scheduled background process therefore cannot alter the visible application.

## Shifter backend

The backend consumes the video mode already detected and selected by the BIOS:

| Mode | Geometry | Planes | Bytes per line |
| --- | ---: | ---: | ---: |
| ST Low | 320 x 200 | 4 | 160 |
| ST Medium | 640 x 200 | 2 | 160 |
| ST High | 640 x 400 | 1 | 80 |

It uses Atari's interleaved 16-pixel planar groups at the existing framebuffer
address. Initialization never reprograms the live Shifter, so loading the
kernel cannot introduce a video-mode transition or an unstable frame.

## Primitive contract

`kernel_graphics.h` exposes clear, pixel, clipped Bresenham line, filled
rectangle, frame, overlap-safe rectangle copy, Shifter palette updates and
opaque or transparent Atari 8x16 text.
Rectangle fills operate on complete bitplane words with edge masks; shifted
Mono copy uses a bounded scanline buffer while colour copy and glyph edges use
individual pixels. All public coordinates
are clipped before framebuffer access. Negative origins are clipped without
negating `INT32_MIN`, avoiding signed overflow when arguments eventually come
from IPC clients.

Rectangle copy now uses one translation-preserving clipper for both endpoints.
Cropping the source advances the destination by the same amount and vice versa,
then both extents are limited to the active Shifter geometry. The startup test
covers simultaneous clipping on different source and destination edges plus a
fully rejected rectangle, so a window move cannot manufacture an out-of-range
framebuffer address.

## Damage tracking

Drawing records at most 16 dirty rectangles. Touching or overlapping regions
are merged. If the list overflows, it collapses to one full-screen rectangle
instead of allocating memory or losing damage. This gives a future compositor
a deterministic upper bound and avoids unconditional full-screen refreshes.

## Boot self-test

The boot test uses small local buffers surrounded by guard words. It verifies
one-plane pixels, four-plane colour encoding, masked fills, clipping, dirty
rectangle merging and overflow collapse. It does not draw into the active
framebuffer. A successful Mono boot reports:

`GRAPHICS BACKEND: SHIFTER 640X400 / CLIP+DIRTY READY`

## Broker boundary

`SYSTEM.GFX` is loaded by the bounded startup-service manager after
`SYSTEM.INIT`. It registers its name and sleeps in blocking IPC while idle.
Only the PID which currently owns that registry name may use graphics syscalls
42--44. Those calls provide bounded commands, mode information and explicit
console restoration; the framebuffer address is never returned to user space.

The IPC protocol supports mode queries, clear, pixel, line, filled rectangle,
frame, palette and console restore. Mode information includes capability bits
for line drawing, palette access and detected/enabled Blitter state. Every
request is copied into a fixed kernel value object before validation, and
invalid opcodes or coordinates cannot become kernel pointers.

Blitter presence and its Setup enable state now travel in boot handoff ABI 3.
Explicit padding keeps every 32-bit member aligned for MC68000, with compile
time offset and size assertions to prevent a later ABI edit from introducing
an address error. Detection remains hardware-probe based, so an ST without a
Blitter and a Mega ST with an empty Blitter socket use the CPU path.

## Emulator validation

`GFXTEST.APP` discovers the broker rather than assuming a PID, drains bounded
stale keyboard input, queries the active mode and draws a mode-independent test
card. It holds the card for inspection, waits for a fresh key, asks the broker
to restore the text console and exits normally.

Hatari ST High Mono displayed the full-screen inset frame, two filled test
rectangles and both clipped corner-to-corner diagonals. After Space, the
console was restored without stale graphics,
`GFXTEST.APP` exited with code 0 and the `C:\\>` prompt was immediately usable.

## Blitter accelerator

The optional kernel Blitter module accelerates planar rectangle fills without
changing the public graphics API. The BIOS hardware probe and Setup enable bit
must both permit it. Applications supply neither Blitter registers nor memory
addresses: `SYSTEM.GFX` validates and clips a request, and the kernel derives
the destination solely from its trusted Shifter surface.

The driver programs one bounded operation per bitplane. End masks preserve
pixels outside unaligned rectangle edges, and destination increments account
for the interleaved ST bitplanes. Interrupts are masked only while the register
set is owned and completion is awaited. A fixed completion limit aborts and
disables acceleration for the current boot; the caller then repeats the
operation with the 68000 renderer.

Initialization first fills an internal ST-RAM test surface surrounded by guard
words and verifies both rows and edge masks. A failure removes the enabled
capability while retaining the CPU backend. Hatari STE passed this self-test,
rendered the existing graphics card through accelerated fills, restored the
console and returned exit code 0.

## Surface handles

The kernel owns a fixed table of eight lightweight surface descriptors. A
descriptor stores owner PID, geometry, flags and a generation counter; it does
not allocate a full-screen backing buffer. Handles combine slot and generation,
so a destroyed handle cannot silently name a later object in the reused slot.

`SYSTEM.GFX` IPC operations 9--11 create, destroy and query these handles. The
owner value sent to the kernel is always overwritten with the IPC sender PID
supplied by the kernel mailbox. Clients cannot claim another PID. Process exit,
fault cleanup and administrative termination release every owned surface.

`SURFTEST.APP` creates a 64x32 surface, clones a second process and verifies
that the child cannot query the parent's handle. The parent can still query and
destroy it, after which the stale handle is rejected. Hatari Mono printed
`SURFTEST.APP: PID OWNERSHIP / STALE HANDLE OK`; parent and child exited zero.
The accelerated graphics test was then repeated through draw, timed hold,
console restore and a clean exit.

## Minimal GUI service

`SYSTEM.GUI` is the first policy layer above `SYSTEM.GFX`. It is a separate,
persistent user process and therefore cannot write the framebuffer or program
the Blitter directly. Its bounded IPC protocol currently supports registering
and unregistering GUI applications, selecting one foreground application, and
querying the foreground PID and application count. Identity always comes from
the kernel-supplied IPC sender; a client cannot register or foreground another
PID.

The implementation uses a fixed four-entry table with no allocation. Removing
the foreground entry deterministically selects the first remaining client, or
PID zero if none remain.

Process lifecycle cleanup is automatic. Every normal exit, trapped CPU fault
and administrative kill generates a kernel-originated `KEXI` event whose
sender PID is zero and therefore cannot be forged by user software. Four
mailbox entries remain reserved beyond the four-entry client IPC limit, so a
full client queue cannot discard all lifecycle notifications. On receipt the
GUI service removes the dead PID and deterministically transfers foreground to
the first surviving application.

The strengthened `GUITEST.APP` deliberately executes an illegal instruction in
the foreground child without unregistering it. Hatari reported the expected
child fault status `-260`; the parent then observed itself as foreground and
the only registered client, emptied the registry and exited zero. Keyboard and
mouse focus routing can now be added without retaining a dead owner.

## Foreground keyboard routing

GUI IPC command 5 lets only the registered foreground application request one
keyboard event. `SYSTEM.GUI` validates the sender and asks the
kernel to arm that exact PID; background and unregistered clients are rejected.
The kernel temporarily suppresses shell input, delivers one non-forgeable
`KKEY` mailbox event with sender PID zero, and then releases ownership
automatically. Exit of either the target or GUI service also cancels the arm.

The event payload preserves both physical and logical information: key-down or
key-up type, the seven-bit Atari IKBD scan code, independent left Shift, right
Shift, Ctrl and Alt modifier bits, and an optional translated ASCII value.
Applications can therefore implement shortcuts from scan codes while ordinary
text controls use the translated value. Modifier state is updated on both make
and break events.

A five-tick handoff barrier drains complete IKBD make/break traffic after the
main input poll. This prevents the Return key or letters used to launch an
application from becoming its first GUI event. The operation is bounded and
one-shot, so an idle GUI client consumes no polling CPU.

`GUIKEY.APP` registers as foreground, requests a key and accepts only a fresh
K with key-down type, IKBD scan code `0x25`, no modifiers and ASCII `k`. Hatari
Mono first confirmed that an unrelated key is rejected, then validated the
complete K event, printed `FOREGROUND KEY DELIVERED OK`, exited zero and
returned input to a usable `C:\>` prompt.

## Continuous keyboard sessions

GUI commands 6 and 7 begin and end a continuous keyboard session for the
calling foreground PID. While active, the shell does not consume keyboard
traffic and every accepted IKBD make or break event becomes a `KKEY` message.
Changing foreground or unregistering a client cancels the old session first;
normal exit, CPU fault, administrative kill and GUI service death also clear
it in the kernel.

The application mailbox remains fixed and bounded. If a client fails to drain
events quickly enough, the kernel drops excess input rather than blocking or
overwriting memory, counts the loss, and includes that count in the next
successfully delivered event. Modifier state continues to be updated even for
dropped events, preventing a permanently stuck Shift, Ctrl or Alt state.

`INPUT.SYS` now publishes release events for ordinary keys as well as
modifiers. The terminal translator safely consumes and ignores them, retaining
the existing shell behaviour. `GUISTRM.APP` opened a continuous session in
Hatari Mono and validated K make (`0x25`) followed by K break (`0x25`), with no
drops. It closed the session explicitly, unregistered, printed
`K DOWN/UP STREAM OK`, exited zero and returned the keyboard to `C:\>`.

`GUIMOD.APP` additionally validates modifier lifetime. It requires Shift make,
uppercase K make, K break and the eventual release of every active Shift bit;
the two release events may arrive in either legal order. This supports host
keyboards which map one physical modifier to a generalized or dual Atari Shift
state. Hatari Mono printed `SHIFT+K MAKE/BREAK OK`, exited zero and restored
shell input without leaving a stuck modifier.

Relative IKBD mouse packets now use the second kernel event queue, isolated
from keyboard traffic consumed by the shell. A complete `F8`-`FB`, X, Y
packet becomes one kernel-authenticated `KMOU` message only for the foreground
continuous GUI session. It carries a clamped screen position, button state,
signed relative movement and the bounded-queue drop count. Partial packets
retain the existing timeout and overrun quarantine, and process cleanup
releases mouse capture together with keyboard capture.

`GUIMOUSE.APP` validates movement plus button make and break before closing its
session. It is intentionally a drag test, so a single gesture exercises all
three states without adding mouse bytes to the command line.

The first software pointer is owned by the kernel graphics layer. Its fixed
16 by 16 monochrome shape is rendered by inverting covered Shifter pixels, so
it stays visible in mono, medium and low resolution without changing the
palette. A fixed 256-byte buffer preserves the pixels underneath it. Movement
restores only the old clipped rectangle, saves the new one and marks those two
small regions dirty; it never redraws the full screen. The pointer appears on
the first real mouse packet and is hidden automatically when GUI input is
released, the client exits or faults, or `SYSTEM.GUI` disappears.

All public drawing primitives now guard the pointer background. A primitive
whose clipped destination intersects the pointer temporarily restores it,
draws, then saves the new pixels and redraws the pointer. Rectangle copies
also guard intersections with their source, preventing cursor pixels from
being duplicated elsewhere. Frames use one outer guard rather than four
visible hide/show cycles, while non-overlapping operations take only a small
rectangle comparison. The graphics startup self-test changes a live pixel
under a visible pointer, verifies both composed and restored values, and puts
the original pixel back before boot output begins.

Mouse overflow coalesces into the newest queued `KMOU` event instead of
blindly dropping the final state. This retains the latest absolute position
and, most importantly, a button release when an 8 MHz machine cannot schedule
the client between every host-generated motion packet. Keyboard events are
never overwritten by coalescing.

GUI input uses at most three client mailbox slots. One fourth client slot is
kept free for a synchronous broker reply, while four additional slots remain
reserved for kernel lifecycle messages. This prevents a burst of mouse motion
from deadlocking a foreground client waiting on `SYSTEM.GFX`.

Mouse delivery drains and combines every complete queued IKBD packet in one
kernel pass. The final button state is retained while relative deltas are
accumulated, preventing an old motion backlog from making the pointer trail
behind the physical mouse. In one-plane Shifter Mono, cursor save, XOR draw and
restore operate on one or two 16-bit screen words per row instead of 256
individual pixels. Colour modes keep the conservative planar pixel path.

## First interactive desktop window

`SYSTEM.GFX` operation 12 exposes the kernel's overlap-safe rectangle copy
without exposing framebuffer addresses: source position, dimensions and
destination position fit in the authenticated four-long IPC request. The
surface operations 9--11 retain their sender-derived ownership field.

Mono operation 12 now has a word-copy fast path when source, destination and
width are aligned to the Shifter's 16-pixel word boundary. A 240x140 window
then needs 2,100 word transfers rather than 33,600 pixel get/set pairs.
Traversal reverses independently in X and Y when required, retaining memmove
semantics for overlap. Unaligned Mono copies assemble one destination scanline
from shifted source words in an 80-byte buffer, then apply masks only to its
edge words. This makes arbitrary window positions fast without touching pixels
outside the rectangle. Colour copies retain the conservative planar pixel
path. Boot self-tests compare shifted copies bit-for-bit with the reference
implementation and also cover rightward and downward overlap.

`DESKTOP.APP` is the first interactive desktop client. It claims the GUI
session before changing the screen, draws a mode-independent background and a
240 by 140 window with a title bar, close gadget and resize handle, and exits
through Escape or the close gadget with a console restore. Operation 14 draws
one fixed 8x16 system-font glyph encoded entirely in the IPC request; no user
pointer is ever dereferenced by the kernel. `DESKTOP.APP` uses it to render the
`ATARI DESKTOP` title. Dragging the title bar copies the complete window, then clears only
the old horizontal and vertical strips not covered by its new position. This
keeps work bounded on a plain 8 MHz 68000 and exercises cursor/source guards.

The desktop now owns a fixed 18-pixel system menu bar with `DESK`, `FILE` and
`VIEW` labels rendered black on white in the Atari system font. It is painted
once during the authoritative desktop redraw and is not part of the movable
window. Window dragging clamps Y to 20, preventing rectangle copies and exposed
background cleanup from overwriting the bar. Drop-down interaction is deferred
until this static ownership boundary is verified.

The lower-right gadget tracks bounded variable geometry. Mouse motion changes
only a pending width and height; releasing the button commits one erase of the
old window and one redraw. The minimum is 220 by 70 pixels and the maximum is
clipped to the current Shifter screen from the window origin. Title dragging
and exposed-strip cleanup use the committed dimensions.

The first client-area control is an 80 by 22 pixel push button whose logical
state is separate from the window decoration. It toggles `START`/`READY`
through either a bounded mouse hit-test or Return. A held mouse button is
latched as one activation instead of retriggering for every motion packet, and
only the button rectangle is repainted when its state changes.

A second independent `INFO`/`HIDE` control sits beside it. `Tab`, Left and
Right move the keyboard focus between controls, Return activates the focused
control, and an inner frame identifies that focus. A mouse click transfers
the same focus state. Both controls are restored after a committed resize.

`INFO` now exposes a bounded diagnostic panel rather than only changing the
button label. It reports the selected Shifter mode, its resolution and the
validated logical-surface state. Showing or hiding clears only the 208x52
client rectangle. The minimum window height is 130 pixels so the panel cannot
escape its owner window, and an active panel is reconstructed after resize.

The Blitter driver also handles the hardware's single-word mask rule
explicitly. When `XCOUNT` is one it combines the first and last edge masks;
the startup test now includes an aligned one-pixel vertical fill, preventing a
window edge from expanding to an entire 16-pixel planar word.

## Separate console and desktop screens

The graphics kernel reserves and pins one 32 KB, 256-byte-aligned framebuffer
from ST-RAM during initialization. The firmware/shell console remains at
`0x10000`; graphics operation 13 lets only the current persistent foreground
GUI owner activate the second screen. The kernel clears that buffer before
first display and changes only the Shifter base high and middle bytes while
interrupts are masked. Because the address is 256-byte aligned, this path does
not touch the STE-only low-byte register and remains safe on a plain ST.

Console output may continue in its original framebuffer while the desktop is
visible, but it can no longer damage or flash through the desktop. Operation 6
restores the preserved console without clearing or reconstructing it. Normal
close, process fault, administrative kill, foreground release and GUI-service
failure all restore the console automatically. If the pinned ST-RAM buffer
cannot be allocated, capability `KGFX_CAP_DESKTOP_BUFFER` remains clear and
activation fails without changing the displayed address.

## Bounded surface identities

The kernel now owns a fixed table of eight lightweight surface identities.
Each entry records an owner PID, dimensions, flags and a generation counter;
it intentionally allocates no full-screen backing store. Handles encode the
slot and generation, so a destroyed handle cannot silently refer to a later
surface which reused the same slot.

Creation rejects zero or over-screen dimensions and unknown flags. Query and
destroy require the exact owner PID. Normal process exit, fault cleanup and
administrative termination release all identities owned by that process. The
boot self-test verifies cross-owner rejection, stale-handle rejection, bounded
table exhaustion and owner-wide cleanup, then resets the table before services
start. Hatari Mono reported `SURFACE MANAGER: 8 HANDLES / OWNER GUARDS OK`.

These objects are identities and policy records, not composited pixel buffers.
Create, destroy and query are exposed through `SYSTEM.GFX`, which derives the
owner from the authenticated IPC sender rather than accepting a caller PID.
`DESKTOP.APP` now creates one full-screen logical surface after querying the
active mode and destroys it on every normal and recoverable error exit. Kernel
process cleanup remains the final safety net after abnormal termination.

The imported Mono backend now also covers skewed rectangle copies whose source
and destination have different bit offsets. It assembles a complete destination
scanline before writing it, applies first/last-word masks, and reverses vertical
traversal for downward overlap. Startup differential tests compare it with the
previous local implementation for left, right, up and down copies before the
graphics service is allowed to start.

## EmuTOS desktop resource adaptation (2026-09-09)

The Shifter CPU fill backend now uses the WM_REPLACE path of EmuTOS
`swblit_rect_common`, including its 68000 DBRA centre loop, for 1, 2 and 4
planes. Pattern fills support independent foreground/background pens.
Vertical lines use its solid `vertical_line` specialization; horizontal
lines use the word fill path. Frames preserve original off-screen edges and
use wide arithmetic before clipping, so clipped frames do not invent borders.

The original 256-character EmuTOS 8x16 font is used on the desktop. The
existing private menu-logo glyphs retain their resource mapping. Original
32x32 disk and trash masks/bitmaps are used directly. EmuDesk's default
IP_4PATT background and active AES W_NAME pattern, W_CLOSER and W_SIZER
characters form the first classic window skin. Colour modes use EmuTOS ST
RGB values mapped to this system's black=0, white=1 pen convention, with
green desktop foreground. Console palette values are saved and restored.

Bounded broker operations 15 (repaint desktop damage rectangle) and 16
(render classic window geometry) render trusted resources without accepting
user pointers. They inherit the foreground PID checks. Background repaint
clips icon pixels and labels to the damage rectangle, including when a moving
window uncovers an icon. This is currently a fixed resource renderer inside
the kernel graphics component, not a full AES object tree or window manager.
Desktop interaction, process ownership and scheduling remain unchanged.
The disk/trash icons are visual resources at this stage; opening volumes,
selection and file deletion are not implemented by this change.

`gmake check-graphics` runs 30,000 differential patterned fills and 30,000
vertical lines with guard buffers and host address/undefined sanitizers.
These test the portable equivalent of the centre loop; the real 68000 DBRA
path is covered by boot differential tests and emulator rendering.

Next: move configurable theme composition/resources behind the GUI service,
complete window/menu/icon interactions, and validate low/medium modes on
Hatari as thoroughly as Mono. After desktop stability and appearance: a
process manager with task list, terminate, suspend and resume. Suspension
must account for service dependencies and owned resources to avoid freezing
the desktop when a critical service is paused.

Operation 17 renders the two client controls and information panel in one
request. This replaces the per-glyph IPC sequence that stalled during mouse
activity in the first integration test. GUI service PID is stored separately
from drawing scratch registers. SYSTEM.GFX discards a reply to an exited
client and continues serving instead of terminating itself.

Emulator check on the revised version: STE, 68000 8 MHz, 4 MiB, Mono; full
background/icons/window, INFO contents, six alternating keyboard activations
(Tab/Return) after mouse input, and Escape to shell with process exit 0.
Automated absolute clicks/dragging are not a conclusive pointer interaction
test because Hatari uses relative IKBD movement.

ST Low RGB emulator check also passed: original icons, green/white desktop
pattern, window frame, full INFO text, then Escape with exit 0 and restored
console palette. ST Medium has host planar coverage but no dedicated visual
run in this change.

ST Medium follow-up (2026-09-09): a separate test ROM selects Medium on RGB.
First launch renders icons/menu/window and INFO reports 640 X 200. Escape
returns exit 0 with console palette restored. However, relaunching DESKTOP.APP
in the same boot leaves a black screen and Escape does not restore the shell.
This is an open regression; it is not yet known whether Medium-specific.
The 8x16 font and icon geometry also need adjustment for Medium pixel aspect.
Do not treat the two-plane host unit tests as proof of desktop lifecycle stability.

Cursor update (2026-09-09): all ST planar modes now use word-oriented
save/restore following the layout described in EmuTOS cur_display(). The
existing XOR shape is retained. Color modes no longer read/write each cursor
pixel independently. The save area holds up to 2 words x 16 rows x 4 planes.
`tests/test_cursor_words.py` compiles the production cursor functions with
ASan/UBSan and compares 6144 positions against a pixel oracle, then checks
exact background restoration including guard memory. Runtime latency and
input queue loss still require emulator measurements. The previously observed
second desktop launch hang and ST Medium layout remain unresolved.

Desktop restart fix (2026-09-09)
-------------------------------
Reproduced a ST Medium freeze during the second launch: background and blank
menu remained, Escape did not exit. A temporary supervisor overlay showed
SYSTEM.GFX READY at image PC +0x94 on successive observations, while DESKTOP
waited for its response. Expensive supervisor work can leave Timer-C pending;
returning to user mode immediately services that old IRQ and preempts the
process before it advances. Repeating the slow supervisor pass causes livelock.

kernel_dispatch_ready_user now raises IPL immediately before restoring the
user frame and writes 0xdf to MFP IPRB, clearing only stale Timer-C pending.
RTE restores the saved user SR, so new Timer-C interrupts still preempt users.
Other pending sources (including IKBD) are preserved. The timer counter and
polled timekeeping are not reset. This is not a remedy for long syscall latency.

The same diagnostic workload completed first and second desktop launches with
this change; overlay removed for release. Final kernel: three consecutive
ST Medium launches and Escape exits (PID 4/5/6, all exit 0), INFO displayed
SHIFTER MEDIUM / 640 X 200. Test environment: Hatari 2.6.1, STE 68000 8 MHz,
4 MiB, RGB, dedicated Medium ROM. Layout proportions remain separate work.

Mono mouse/window correction (2026-09-09)
---------------------------------------
DESKTOP.APP now reloads the gesture-state pointer after menu hit testing;
previously title/resize presses wrote menu_open instead of dragging. Signed
clamps prevent negative title Y from jumping to the bottom and negative resize
extents from becoming maximum-size values. A coalesced release ends an existing
gesture, and resize commits use the release coordinates even without a prior
held-motion packet. The release path reloads the gesture pointer before clearing.

`gmake build/system/DESKTEST.APP` builds a separate regression app from the same
production handlers, with deterministic KMOU messages and real SYSTEM.GFX IPC.
Run `run desktest.app` in Mono 640x400. Seven checks cover START/INFO, dragging
above the top, returning the window, negative resize/minimum, coalesced release,
release-only resize movement, and screen maxima. Hatari 2.6.1 STE 8 MHz/4 MiB:
old handler failed check 3; corrected build printed MOUSE / DRAG / RESIZE PASS
and PID 4 EXIT 0. This tests window logic and IPC/render integration, not host
relative mouse transport, cursor latency, or pixel-exact damage restoration.
Test code is excluded from normal DESKTOP.APP by the preprocessor.
