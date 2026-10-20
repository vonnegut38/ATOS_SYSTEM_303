#ifndef ATOS_APP_ABI_H
#define ATOS_APP_ABI_H
#include <stddef.h>
#include <stdint.h>
#include "app_abi_version.h"
#include "gui_app.h"    /* gui_message_t, gui_spawn_t, gui_call */
#include "gui_draw.h"   /* gui_draw_t */

/* ATOS native application ABI 1.0 — FROZEN 2026-09-26.
 *
 * Contract between the kernel and a native .APP process.  Anything listed here
 * MUST NOT change meaning or layout without bumping ATOS_APP_ABI_MAJOR;
 * additive changes (a new opcode, a new optional field appended to a struct)
 * bump ATOS_APP_ABI_MINOR.  See docs/APP-ABI.md for the normative text.
 *
 * Frozen surface:
 *   - the .APP image header and the .app_policy section (linker/app-sys.ld),
 *   - app_entry/app_main and the trap #0 convention (system/gui_app_runtime.S),
 *   - the syscall numbers below (system/kernel_exception.c:kernel_user_syscall),
 *   - the structs shared with the kernel (guarded by _Static_assert below). */
#define ATOS_APP_ABI_MAJOR APP_ABI_VERSION_MAJOR
#define ATOS_APP_ABI_MINOR APP_ABI_VERSION_MINOR
#define ATOS_APP_ABI_VERSION APP_ABI_VERSION

/* .APP image header: magic (8 B) + version (2 B) + header size (2 B) + five
   longwords (file end, memory end, entry, stack, reserved) = 32 bytes. */
#define ATOS_APP_IMAGE_MAGIC "ASYSAPP0"
#define ATOS_APP_IMAGE_VERSION 1u
#define ATOS_APP_IMAGE_HEADER_SIZE 32u
/* .app_policy magic at the end of the writable part of the image. */
#define ATOS_APP_POLICY_MAGIC 0x41505732UL

/* Syscall returning ATOS_APP_ABI_VERSION; added together with ABI 1.0. */
#define ATOS_APP_SYSCALL_ABI_VERSION APP_ABI_SYSCALL_VERSION

/* trap #0 syscall numbers dispatched by kernel_user_syscall().
   Calling convention (system/gui_app_runtime.S:gui_call):
       gui_call(number, d1, d2, a0, a1)  ->  d0 = result
   i.e. argument0=d1, argument1=d2, pointer0=a0, pointer1=a1.  Negative
   results are errors in the kernel error-code space. */
enum atos_app_syscall {
    ATOS_CALL_EXIT = 0,             /* d1=status; does not return */
    ATOS_CALL_WRITE = 1,            /* a0=text */
    ATOS_CALL_OPEN = 2,             /* a0=path -> fd | <0 */
    ATOS_CALL_READ = 3,             /* d1=fd d2=count a0=buf -> bytes | <0 */
    ATOS_CALL_CLOSE = 4,            /* d1=fd */
    ATOS_CALL_STAT = 5,             /* a0=path a1=uint32[2]{size,attributes} */
    ATOS_CALL_SEEK = 6,             /* d1=fd d2=offset */
    ATOS_CALL_ARGS = 7,             /* d1=capacity a0=buffer -> length */
    ATOS_CALL_PUTCHAR = 8,          /* d1=character */
    ATOS_CALL_GETCHAR = 9,
    ATOS_CALL_TICKS = 10,
    ATOS_CALL_SLEEP = 11,           /* d1=ticks */
    ATOS_CALL_SYSTEM_INFO = 12,     /* a0=uint32[6] */
    ATOS_CALL_DIR_ENTRY = 13,       /* d1=index a0=path a1=dir_entry_t */
    ATOS_CALL_GETCWD = 14,          /* d1=capacity a0=buffer (drive:path) */
    ATOS_CALL_CHDIR = 15,           /* a0=path */
    ATOS_CALL_ALLOC = 16,           /* d1=bytes d2=prefer_tt -> pointer */
    ATOS_CALL_FREE = 17,            /* a0=pointer */
    ATOS_CALL_GETPID = 18,
    ATOS_CALL_RESOURCES = 19,       /* a0=uint32[6] */
    ATOS_CALL_PROC_INFO = 20,       /* d1=slot a0=uint32[4] */
    ATOS_CALL_SPAWN = 21,           /* a0=path a1=uint32[2]{pid,status} */
    ATOS_CALL_SPAWN_REQ = 22,       /* a0=gui_spawn_t* (synchronous) */
    ATOS_CALL_QUEUE = 23,           /* a0=gui_spawn_t* (queued job) */
    ATOS_CALL_JOB_QUERY = 24,       /* d1=pid d2=wait a0=uint32[2] */
    ATOS_CALL_JOB_WAIT = 25,        /* d1=pid a0=int32* status */
    ATOS_CALL_CONTEXT = 27,         /* a0=uint32[3]{usp,pc,sr} */
    ATOS_CALL_CLONE = 31,           /* d1= a0=uint32* child pid */
    ATOS_CALL_CHILD_QUERY = 32,     /* d1=pid d2=wait a0=uint32[2] */
    ATOS_CALL_CHILD_WAIT = 33,      /* d1=pid a0=int32* status */
    ATOS_CALL_SPAWN_ASYNC = 34,     /* a0=gui_spawn_t* (background) */
    ATOS_CALL_SET_PATH = 35,        /* a0=PATH list ('+' prepends) */
    ATOS_CALL_SEND = 36,            /* d1=pid a0=uint32[4] */
    ATOS_CALL_RECV = 37,            /* a0=gui_message_t* (non-blocking) */
    ATOS_CALL_RECV_WAIT = 38,       /* a0=gui_message_t* (blocking) */
    ATOS_CALL_SERVICE_ADD = 39,     /* a0=name */
    ATOS_CALL_SERVICE_GET = 40,     /* a0=name -> pid | <0 */
    ATOS_CALL_SERVICE_DEL = 41,     /* a0=name */
    ATOS_CALL_GFX = 42,             /* a0=gfx_request_t* (SYSTEM.GFX only) */
    ATOS_CALL_GFX_INFO = 43,        /* a0=uint32[6] (SYSTEM.GFX only) */
    ATOS_CALL_GFX_RESTORE = 44,     /* SYSTEM.GFX only */
    ATOS_CALL_GUI_ACQUIRE = 45,     /* d1=pid (SYSTEM.GUI only) */
    ATOS_CALL_GET_PATH = 46,        /* a0=64-byte buffer */
    ATOS_CALL_GUI_ACQUIRE_KEEP = 47,/* d1=pid; persistent (SYSTEM.GUI only) */
    ATOS_CALL_GUI_RELEASE = 48,     /* SYSTEM.GUI only */
    ATOS_CALL_COPY = 49,            /* a0=source a1=target */
    ATOS_CALL_MKDIR = 50,           /* a0=path */
    ATOS_CALL_DELETE = 51,          /* a0=path */
    ATOS_CALL_DIR_PAGE = 52,        /* d1=skip d2=count a0=path a1=entries */
    ATOS_CALL_TRASH = 53,           /* a0=path */
    ATOS_CALL_MOVE = 54,            /* a0=source a1=target */
    ATOS_CALL_PURGE = 55,           /* a0=path */
    ATOS_CALL_SYSTEM_REPORT = 56,   /* a0=system_report_t */
    ATOS_CALL_RAW_BLOCK = 57,       /* d1=sector d2=count a0=buffer */
    ATOS_CALL_CLOCK = 58,
    ATOS_CALL_LANGUAGE_SAVE = 59,
    ATOS_CALL_SAVE = 60,            /* a0=path d1=size a1=buffer */
    ATOS_CALL_SHELL = 61,           /* d1=capacity a0=cmd a1=out */
    ATOS_CALL_SHELL_STATE = 62,
    ATOS_CALL_FLOPPY_DIR = 63,      /* d1=capacity a0=buffer */
    ATOS_CALL_FLOPPY_READ = 64,     /* d1=capacity a0=name a1=buffer */
    ATOS_CALL_TOS_RUN = 65,         /* a0=path; d1=0 blocking, 1 queue cooperative TOS */
    ATOS_CALL_ABI_VERSION = 66,
    ATOS_CALL_HIRES_CLOCK = 67,     /* -> monotonic Timer C counts (~81 us) */
    ATOS_CALL_APP_ACTIVATE = 74,    /* a0=path a1=uint32[2]{pid,status}: activate
                                       a running app or report that it runs
                                       elsewhere; NEVER reuse 66 (ABI_VERSION) */
    /* --- audio (1.3 tones/queue, 1.5 PCM through PSG; no DMA).  The first audio call claims the single sound owner (the calling
       pid); a second process is refused instead of sharing the channels. --- */
    ATOS_CALL_AUDIO_CAPS = 75,      /* -> capability bitmask (0 = no audio) */
    ATOS_CALL_AUDIO_TONE = 76,      /* d1=(period<<16)|(volume<<8)|channel */
    ATOS_CALL_AUDIO_NOISE = 77,     /* d1=(period<<16)|(volume<<8) */
    ATOS_CALL_AUDIO_STOP = 78,      /* d1=channel bit mask */
    ATOS_CALL_AUDIO_QUEUE = 79,     /* a0=kaudio_event_t[] d1=count */
    ATOS_CALL_AUDIO_STATE = 80,     /* a0=uint32[4]{owner,queued,events,rejected} */
    ATOS_CALL_GUI_FRAME = 81,        /* d1=draw count d2=group count;
                                       a0=gui_draw_t[] a1=uint16_t group lengths.
                                       Own window handle 0 only. 0=presented,
                                       1=session hidden, negative=error. */
    ATOS_CALL_PCM_CONTROL = 82, /* d1=0 open,1 run,2 pause,3 close; open -> Hz */
    ATOS_CALL_PCM_SUBMIT = 83,  /* a0=unsigned 8-bit mono d1=bytes; -> accepted */
    ATOS_CALL_PCM_STATE = 84,    /* a0=uint32[8]: owner,active,queued,free,
                                   played,underruns,rate,capacity */
    ATOS_CALL_PCM_CLIP = 85,    /* d1=bytes: allocate looping clip; fill via SUBMIT */

    /* ATOS Sound 1.0 (NF_ATOS_SOUND): transport ASND przez usluge jadra.
       Aplikacja NIE wola NatFeats bezposrednio.  Jadro sprawdza wlasciciela
       audio (ta sama regula co ton/kolejka/PCM), waliduje bufory i dopiero
       wtedy wykonuje wymiane w trybie supervisora. */
    ATOS_CALL_SOUND_XCHG = 86,  /* a0=zadanie d1=dlugosc a1=odpowiedz d2=pojemnosc
                                   -> dlugosc odpowiedzi albo kod ujemny */
    ATOS_CALL_VOLUME = 88,          /* d1=0 stan woluminu (fat_health), d1=1 naprawa */
    ATOS_CALL_SOUND_INFO = 87   /* a0=uint32[4]: dostepnosc, sesja, wersja, liczniki */
};

/* App-visible directory entry (opcodes 13/52); mirrors the kernel's
   user_dir_entry_t. */
typedef struct {
    char name[13];
    uint8_t attributes, padding[2];
    uint32_t size, datetime;
} atos_app_dir_entry_t;

/* ---------------------------------------------------------------- audio (1.3)
   One queued effect for ATOS_CALL_AUDIO_QUEUE.  The kernel applies it when the
   wait (in hires ticks, 12288 per second) has elapsed, so a game can hand over a
   short sequence without owning a timer.  Ops mirror include/kernel_audio.h. */
#define ATOS_AUDIO_CAP_PSG_TONE  (1u<<0)
#define ATOS_AUDIO_CAP_PSG_NOISE (1u<<1)
#define ATOS_AUDIO_CAP_ENVELOPE  (1u<<2)   /* never advertised by this build */
#define ATOS_AUDIO_CAP_QUEUE     (1u<<3)
#define ATOS_AUDIO_CAP_PCM       (1u<<4)   /* unsigned 8-bit mono streaming */

#define ATOS_AUDIO_OP_NONE  0u
#define ATOS_AUDIO_OP_TONE  1u   /* a = channel, b = volume, aux = period      */
#define ATOS_AUDIO_OP_NOISE 2u   /* b = volume, aux = noise period             */
#define ATOS_AUDIO_OP_STOP  3u   /* a = channel bit mask                       */

#define ATOS_AUDIO_CHANNELS 3u

typedef struct {
    uint32_t wait;   /* hires ticks to wait before applying (12288 = 1 s) */
    uint16_t aux;    /* tone/noise period */
    uint8_t op;
    uint8_t a;
    uint8_t b;
    uint8_t pad[3];  /* explicit: m68k aligns uint32 to 2, so 12 needs saying */
} atos_audio_event_t;

/* Compile-time guards: ABI drift breaks the build, not a user's app. */
_Static_assert(sizeof(atos_app_dir_entry_t) == 24u, "dir entry ABI size");
_Static_assert(offsetof(atos_app_dir_entry_t, size) == 16u, "dir entry size offset");
_Static_assert(sizeof(atos_audio_event_t) == 12u, "audio event ABI size");
_Static_assert(sizeof(gui_message_t) == 20u, "message ABI size");
_Static_assert(sizeof(gui_spawn_t) == 136u, "spawn ABI size");
_Static_assert(sizeof(gui_draw_t) == 48u, "draw ABI size");
_Static_assert(offsetof(gui_draw_t, text) == 16u, "draw text offset");

static inline uint32_t atos_app_abi_version(void)
{
    return (uint32_t)gui_call(ATOS_CALL_ABI_VERSION, 0, 0, 0, 0);
}
#endif
