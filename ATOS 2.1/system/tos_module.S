/* TOS.SYS - the TOS compatibility module (working-set stage).
 *
 * Scope, stated honestly: this module owns the TOS personality's WORKING SET -
 * the private 640x400 screen the personality renders into.  It does not contain
 * GEMDOS/BIOS/XBIOS/AES/VDI; those are still compiled into the kernel.  What it
 * provides is the on-demand load path and the memory placement, which is what
 * the SETTINGS option controls.
 *
 * Loaded on demand: the kernel calls kernel_load_system_module("TOS     SYS")
 * when a TOS program starts, or right after START when the user ticked
 * "Load TOS Compatibility after START to TT RAM" (prefer_tt=1 then places this
 * code and its buffer in TT RAM, if TT RAM is available - the allocator may
 * still fall back to ST RAM, which is why the kernel reports the real address
 * rather than claiming TT RAM).
 *
 * Module ABI: header "ASYSMOD0", ABI 3, then kernel_module_api_t:
 *   +8  alloc(size,align,owner,flags,prefer_tt)
 *   +12 free(ptr,owner)
 *   +24 register_service(magic,version,ptr)
 *   +28 find_service(magic,version)
 * Arguments are pushed right to left, each in a full 32-bit slot; the caller
 * pops.  D2-D7/A2-A6 are callee-saved, so D3 (used here as the placement flag)
 * is restored on every return path.
 */
#define TOS_CFG_MAGIC 0x544f5343   /* "TOSC": {version,prefer_tt} from the kernel */
#define TOS_SVC_MAGIC 0x544f5320   /* "TOS ": {screen,...} back to the kernel   */
#define TOS_SCREEN_BYTES 65536

    .section .module_header,"a"
    .ascii  "ASYSMOD0"
    .word   3
    .word   32
    .long   __image_file_end
    .long   0
    .long   tos_module_entry
    .long   __image_memory_end
    .long   0

    .section .text.startup,"ax"
    .globl tos_module_entry
tos_module_entry:
    move.l 4(%sp),%a0                 /* api pointer, read before any push */
    move.l %d3,-(%sp)                 /* callee-saved: restore on every exit */
    cmpi.l #0x4b4d4150,(%a0)          /* KAPI */
    bne    tos_fail
    cmpi.w #3,4(%a0)
    bne    tos_fail
    cmpi.w #32,6(%a0)
    blo    tos_fail
    lea    tos_api(%pc),%a1
    move.l %a0,(%a1)

    /* prefer_tt = 0, then adopt the kernel's TOSC config when it is present. */
    moveq  #0,%d3
    move.l #1,-(%sp)                  /* version */
    move.l #TOS_CFG_MAGIC,-(%sp)      /* magic   */
    move.l 28(%a0),%a1                /* find_service */
    jsr    (%a1)
    addq.l #8,%sp
    tst.l  %d0
    beq    tos_alloc
    move.l %d0,%a1
    move.l 8(%a1),%d3                 /* cfg->prefer_tt */

tos_alloc:
    /* alloc(65536, 16, KMEM_OWNER_KERNEL, READ|WRITE, prefer_tt) */
    move.l %d3,-(%sp)                 /* prefer_tt */
    move.l #3,-(%sp)                  /* READ|WRITE */
    move.l #1,-(%sp)                  /* KMEM_OWNER_KERNEL */
    move.l #16,-(%sp)                 /* align */
    move.l #TOS_SCREEN_BYTES,-(%sp)   /* size */
    lea    tos_api(%pc),%a0
    move.l (%a0),%a0
    move.l 8(%a0),%a1                 /* alloc */
    jsr    (%a1)
    lea    20(%sp),%sp
    tst.l  %d0
    beq    tos_fail
    lea    tos_screen(%pc),%a1
    move.l %d0,(%a1)
    lea    tos_service(%pc),%a1
    move.l %d0,8(%a1)                 /* publish the buffer to the kernel */

    /* register_service(TOS_SVC_MAGIC, 1, &tos_service) */
    pea    tos_service(%pc)
    move.l #1,-(%sp)
    move.l #TOS_SVC_MAGIC,-(%sp)
    lea    tos_api(%pc),%a0
    move.l (%a0),%a0
    move.l 24(%a0),%a1                /* register_service */
    jsr    (%a1)
    lea    12(%sp),%sp
    tst.l  %d0
    beq    tos_free_fail              /* registration must succeed */

    moveq  #1,%d0                     /* non-zero: module init succeeded */
    bra    tos_out

tos_free_fail:
    /* free(screen, KMEM_OWNER_KERNEL): a module that fails to publish must not
       leak the buffer it just took. */
    move.l #1,-(%sp)
    lea    tos_screen(%pc),%a0
    move.l (%a0),-(%sp)
    lea    tos_api(%pc),%a0
    move.l (%a0),%a0
    move.l 12(%a0),%a1                /* free */
    jsr    (%a1)
    addq.l #8,%sp
tos_fail:
    moveq  #0,%d0
tos_out:
    move.l (%sp)+,%d3
    rts

    .section .data
    .align 2
tos_service:
    .long TOS_SVC_MAGIC
    .word 1
    .word 16
    .long 0                           /* screen buffer (filled in above) */
    .long 0

    .section .bss
    .align 2
tos_api:
    .space 4
tos_screen:
    .space 4
