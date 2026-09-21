# Memory protection and multitasking direction

Kernel allocations carry an owner ID and access flags (`READ`, `WRITE`,
`EXEC`, `SUPERVISOR`). This metadata is part of the ABI direction, rather than
an afterthought added when processes arrive.

- 68000/68010: ownership and bounds can be enforced by kernel APIs, but these
  CPUs cannot provide hard isolation from arbitrary supervisor-mode code.
- 68020: software ownership checks; external MMU support may be added later.
- 68030/68040/68060: owner IDs will become address-space IDs, and allocation
  flags will be translated into MMU page permissions.
- Drivers stay in a separate privileged owner domain.
- Each future task receives its own user stack, kernel stack, saved register
  context, address-space ID and scheduling state.
- Preemption will be driven by an MFP timer after interrupt dispatch and
  context switching are implemented and tested independently.

The current allocator is deliberately independent of scheduling. It supports
ST-RAM and TT-RAM pools, aligned allocations, ownership-checked freeing,
coalescing and metadata guards.

Loadable modules use ABI version 3 and carry a checksummed `ASYSMOD0` header.
The position-independent `ACSI.SYS` registers its own multi-sector ACSI READ(6)
DMA service, including two bounded retries, and is allocated as a separate
privileged owner. The kernel validates it by reading sector zero into aligned
ST-RAM. The proven ROM path remains active whenever a module is missing,
invalid, or fails I/O validation.

The placement policy is deliberately asymmetric: DMA buffers, framebuffers,
bootstrap code, and compatibility data remain in ST-RAM. Kernel components,
ordinary drivers, caches, and future processes prefer TT-RAM when available
and fall back to ST-RAM. A component is first verified in safe memory before
control can pass to a relocated TT-RAM image.

The kernel block layer uses a permanently ST-RAM-resident, 512-byte-aligned
DMA bounce area. Its eight-sector read cache prefers TT-RAM and falls back to
ST-RAM, so devices never DMA directly into memory outside the Atari DMA bus.
The read-only FAT layer validates the selected AHDI partition entry and FAT16
BPB before exposing a mounted boot volume; malformed geometry is rejected
before any derived disk address is used.
The AHDI scanner recognizes primary GEM, BGM and F32 volumes and follows XGM
descriptor chains with cycle and depth guards. ACSI uses READ(6) for small
addresses and the ICD extended-command form of READ(10) for 32-bit LBAs.
