#ifndef ATOS_SOUND_TRANSPORT_H
#define ATOS_SOUND_TRANSPORT_H
#include "atos_sound.h"
enum { AS_IO_ERROR = -1, AS_IO_TIMEOUT = -2, AS_IO_UNAVAILABLE = -3, AS_IO_PROTOCOL = -4 };
typedef int (*as_exchange_fn)(void *, const uint8_t *, size_t, uint8_t *, size_t, size_t *);
typedef struct {
    as_exchange_fn exchange;
    void *context;
} as_transport;
typedef struct {
    as_transport transport;
    uint32_t session, sequence;
} as_client;
void as_client_init(as_client *, as_transport);
/* >=0 protocol status; <0 transport error. data is response after status.
 * Require AS_MAX_REPLY capacity. Sequence never wraps; RESET before exhaustion.
 * After I/O error caller must retry same command or reestablish session. */
int as_client_command(as_client *, uint16_t, const void *, size_t, uint8_t *, size_t, size_t *);
/* Convenience synchronous uploader. On failure after BEGIN, *id retains the
 * known handle for recovery/UNLOAD (0 if no trustworthy handle was received).
 * Does not automatically retry/abort uncertain I/O or retain the failed packet;
 * a production broker needs a resumable upload transaction. */
int as_client_load_pcm(as_client *, const uint8_t *, uint32_t, uint16_t, uint32_t, uint32_t *);
int as_loopback_exchange(void *, const uint8_t *, size_t, uint8_t *, size_t, size_t *);
/* USB byte stream, also usable over Unix socket. Callbacks return bytes or <0;
 * all calls use a single deadline, partial failures poison this link. */
typedef struct {
    void *context;
    int (*write)(void *, const uint8_t *, size_t, uint32_t timeout_ms);
    int (*read)(void *, uint8_t *, size_t, uint32_t timeout_ms);
    uint64_t (*now_ms)(void *);
    uint32_t timeout_ms;
    int poisoned;
} as_bulk_link;
int as_bulk_exchange(void *, const uint8_t *, size_t, uint8_t *, size_t, size_t *);
/* RX frame assembler; caller owns and serializes with mixer. */
typedef struct {
    as_engine *engine;
    uint8_t rx[AS_MAX_PACKET], tx[AS_MAX_REPLY];
    size_t have, want, tx_n;
    int fault;
} as_stream;
typedef int (*as_emit_fn)(void *, const uint8_t *,
                          size_t); /* 0 accepted, >0 would block, <0 fatal */
void as_stream_init(as_stream *, as_engine *);
int as_stream_feed(as_stream *, const uint8_t *, size_t, size_t *, as_emit_fn, void *);
/* Proposed NF_ATOS_SOUND feature. subid 0 version=0x10000; subid 1 exchange:
 * args: request guest address, byte count, reply guest address, reply capacity.
 * Validate BOTH ranges before applying any command. Return reply bytes or <0.
 * memcpy guest callbacks must not fail after validation (emulator thread owns
 * memory for this call). No guest memory pointer is retained. */
typedef struct {
    as_engine *engine;
    void *context;
    int (*valid)(void *, uint32_t, size_t, int write);
    void (*read)(void *, uint32_t, void *, size_t);
    void (*write)(void *, uint32_t, const void *, size_t);
} as_nf_host;
int32_t as_nf_dispatch(as_nf_host *, uint32_t subid, const uint32_t args[4]);
typedef struct {
    void *context;
    uint32_t feature_id;
    /* Callback bridges platform nf_call and 32-bit guest pointers; no naked
     * illegal-instruction probe is executed by this portable library. */
    int (*call)(void *, uint32_t, const uint8_t *, size_t, uint8_t *, size_t);
} as_nf_link;
int as_nf_exchange(void *, const uint8_t *, size_t, uint8_t *, size_t, size_t *);
#endif
