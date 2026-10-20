#ifndef ASF_BOOT_PROTOCOL_H
#define ASF_BOOT_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

#define BOOT_IMAGE_MAGIC "ASYSBOOT"
#define BOOT_IMAGE_VERSION 1u
#define BOOT_IMAGE_HEADER_SIZE 32u
#define BOOT_HANDOFF_MAGIC 0x4153484fUL /* ASHO */
#define BOOT_HANDOFF_VERSION 20u
#define MODULE_IMAGE_MAGIC "ASYSMOD0"
#define MODULE_ABI_VERSION 3u

/* Absolute address the KERNEL.SYS image is linked for and must be loaded at.
   Single source of truth for the C loader.  The linker script
   (linker/kernel-sys.ld) and system/kernel_entry.S hardcode the same value and
   MUST stay in sync; a mismatch makes load_image() reject the image
   (header_load check).  Guarded by tests/test_kernel_load_addr.py. */
#define KERNEL_IMAGE_LOAD_ADDRESS 0x00020000UL

typedef struct {uint32_t file_size,memory_size,entry_offset;} module_image_info_t;

enum { BOOT_FPU_NONE, BOOT_FPU_EXTERNAL, BOOT_FPU_INTEGRATED };

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t machine_type;
    uint8_t cpu_type;
    uint8_t video_mode;
    uint8_t boot_acsi_id;
    uint8_t boot_partition;
    uint8_t fpu_type;
    uint8_t clock_mhz;
    uint8_t clock_known;
    uint8_t blitter_present;
    uint8_t blitter_enabled;
    uint8_t reserved0; /* v9: bit7 valid, bits0..6 detected graphics adapter */
    uint8_t reserved1; /* v9: detected graphics chipset; valid only with bit7 above */
    uint8_t netusbee_ethernet; /* v10: NetUSBee RTL8019 on the cartridge port */
    uint8_t netusbee_usb;      /* v10: NetUSBee ISP1160 USB controller */
    uint32_t st_ram_bytes;
    uint32_t tt_ram_bytes;
    void (*console_clear)(void);
    void (*console_puts)(const char *);
    void (*console_put_u32)(uint32_t);
    void (*start_kernel)(void);
    int (*load_module)(const char name[11],void *destination,uint32_t capacity,module_image_info_t *info);
    int (*acsi_read_sector)(uint8_t id,uint32_t sector,void *buffer);
    /* v11: 80x25 character mirror of the ROM console plus its cursor, so the
       loader and the kernel can keep one continuous log across the handoff. */
    void (*console_text_copy)(char *destination,uint16_t *column,uint16_t *row);
    /* v12: the ROM already brought the VOFA/ET4000 adapter up and the console
       is writing to its VRAM; the kernel only has to adopt the scanout. */
    uint8_t nova_initialized;
    uint8_t video_force_shifter; /* Alt at power-on / SETUP: never touch the card */
    /* v13: warm reset (peripherals + ROM startup) for Ctrl+Alt+Del. */
    void (*reset)(void);
    /* v14: the kernel reached the shell while the ROM owned the card; the ROM
       confirms the card boot by clearing its failover latch. The kernel has no
       IDE write path when the ACSI module is unavailable, so it cannot do this
       through the FAT layer itself. */
    void (*video_boot_ok)(void);
    /* v15: ROM sector write. The kernel's own write path drives the ST DMA
       (ACSI) registers, which do not reach a TF536 IDE disk; the ROM's IDE
       driver does, so kblock_write falls back to this for IDE devices. */
    int (*acsi_write_sector)(uint8_t id,uint32_t sector,const void *buffer);
    /* v16: read-only floppy (FAT12) access for the kernel/shell/Commander. */
    int (*floppy_mount)(void);
    int (*floppy_dir)(uint32_t index,char name[13],uint32_t *size,uint8_t *attr);
    int (*floppy_read_file)(const char *name,void *buffer,uint32_t capacity,uint32_t *size);
    /* v17: identity of the boot disk, filled by the ROM so the kernel report
       shows the real model/capacity on both ACSI (SCSI INQUIRY) and IDE (ATA
       IDENTIFY).  disk_known=1 means the fields below were queried; an empty
       disk_name with disk_sectors!=0 is normal (model not reported). */
    char disk_name[32];
    uint32_t disk_sectors;
    uint32_t disk_sector_bytes;
    uint8_t disk_known;
    /* v18: present-device bitmaps so the kernel can mount partitions from both
       transports, not only the boot disk. */
    uint8_t acsi_devices;       /* bits 0..7 = ACSI ids */
    uint8_t ide_units;          /* bits 0..1 = IDE master/slave */
    uint8_t card_video_mode;    /* v19: card mode the ROM programmed (CFG_MODE_*),
                                   so the kernel can adopt it at bring-up */
    /* v20: keep early progress on the ROM's working display until the
       kernel video driver takes ownership. Called only during bring-up. */
    void (*boot_progress)(uint16_t percent,const char *label);
} boot_handoff_t;

/* MC68000 faults on unaligned longword access. Keep every 32-bit handoff
   member even-aligned and make ABI drift fail at compile time.  The asserted
   offsets and size are the 32-bit m68k ROM <-> kernel contract; host tests
   include this header for the service types alone and a 64-bit build would
   legitimately produce different numbers, so the check is gated on the target
   instead of being weakened. */
#if defined(__m68k__)
_Static_assert(offsetof(boot_handoff_t,st_ram_bytes)==22u,"boot handoff RAM alignment");
_Static_assert(offsetof(boot_handoff_t,console_clear)==30u,"boot handoff callback alignment");
_Static_assert(sizeof(boot_handoff_t)==132u,"boot handoff ABI size");
#endif

#endif