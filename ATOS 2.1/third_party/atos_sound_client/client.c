#include "atos_sound_transport.h"
#include <string.h>
/* Response data lengths in the frozen ASND-v1 contract (excluding status). */
static size_t success_bytes(uint16_t op) {
    switch (op) {
    case AS_TRACKER_LOAD: case AS_TRACKER_SEEK: return 8;
    case AS_TRACKER_CONTROL: return 0;
    case AS_TRACKER_STATUS: return 208;
    case AS_HELLO:
        return 32;
    case AS_GET_CAPS:
    case AS_GET_STATUS:
        return 64;
    case AS_LOAD_SAMPLE:
    case AS_STOP:
    case AS_SET_VOLUME:
    case AS_SET_PAN:
        return 8;
    case AS_PLAY:
    case AS_GET_CLOCK:
        return 16;
    case AS_UNLOAD_SAMPLE:
        return 0;
    case AS_RESET:
        return 4;
    default:
        return SIZE_MAX; /* no supported success contract */
    }
}
void as_client_init(as_client *c, as_transport t) {
    memset(c, 0, sizeof *c);
    c->transport = t;
    c->sequence = 1;
}
int as_client_command(as_client *c, uint16_t op, const void *p, size_t n, uint8_t *data, size_t cap,
                      size_t *count) {
    /* Single-executor library (see the ownership contract in atos_sound.h):
     * one command at a time.  Static frames keep the 68k app stack small - an
     * 8192 B APP stack cannot hold 4480 B here plus 4432 B in as_upload_write
     * (audit 2026-10-13, point E). */
    static uint8_t req[AS_MAX_PACKET], reply[AS_MAX_REPLY];
    size_t rn = 0;
    if (!count)
        return AS_IO_PROTOCOL;
    *count = 0;
    if (!c || !c->transport.exchange || !data || cap < AS_MAX_REPLY || !c->sequence)
        return AS_IO_PROTOCOL;
    size_t q =
        as_packet(req, sizeof req, op, 0, c->sequence, op == AS_HELLO ? 0 : c->session, p, n);
    if (!q)
        return AS_IO_PROTOCOL;
    int rc = c->transport.exchange(c->transport.context, req, q, reply, sizeof reply, &rn);
    if (rc)
        return rc;
    if (rn > sizeof reply || as_packet_check(reply, rn) || as_get16(reply + 6) != 1 ||
        as_get16(reply + 8) != op || as_get32(reply + 12) != c->sequence || rn < AS_HEADER + 4)
        return AS_IO_PROTOCOL;
    uint32_t status = as_get32(reply + AS_HEADER);
    if (status > AS_E_UNSUPPORTED ||
        (status ? rn != AS_HEADER + 4
                : success_bytes(op) == SIZE_MAX || rn != AS_HEADER + 4 + success_bytes(op)))
        return AS_IO_PROTOCOL;
    if (status == 0) {
        if (op == AS_HELLO || op == AS_RESET) {
            if (!as_get32(reply + 16) || as_get32(reply + AS_HEADER + 4) != as_get32(reply + 16))
                return AS_IO_PROTOCOL;
            c->session = as_get32(reply + 16);
        } else if (as_get32(reply + 16) != c->session)
            return AS_IO_PROTOCOL;
    }
    if (op == AS_HELLO && status == 0) {
        c->sequence = as_get32(reply + AS_HEADER + 4 + 12);
    } else if (op == AS_RESET && status == 0)
        c->sequence = 1;
    else
        ++c->sequence;
    *count = rn - AS_HEADER - 4;
    memcpy(data, reply + AS_HEADER + 4, *count);
    return (int)status;
}
int as_client_load_pcm(as_client *c, const uint8_t *pcm, uint32_t bytes, uint16_t channels,
                       uint32_t rate, uint32_t *id) {
    /* Static, jak w as_client_command (audyt 2026-10-13, point E): ten bufor
     * sam w sobie to 4096 B stosu i nie miesci sie z ramka 4.4 KB. */
    static uint8_t p[AS_MAX_PAYLOAD], r[AS_MAX_REPLY];
    memset(p, 0, 24);
    size_t n;
    int rc;
    if (id)
        *id = 0;
    if (!pcm || !id || (channels != 1 && channels != 2) || !bytes || bytes % (channels * 2u))
        return AS_E_ARGUMENT;
    as_put16(p + 4, 1);
    as_put16(p + 6, channels);
    as_put32(p + 8, rate);
    as_put32(p + 12, bytes / (channels * 2u));
    as_put32(p + 16, bytes);
    rc = as_client_command(c, AS_LOAD_SAMPLE, p, 24, r, sizeof r, &n);
    if (rc)
        return rc;
    if (n != 8 || !as_get32(r) || as_get32(r + 4) != 0)
        return AS_IO_PROTOCOL;
    *id = as_get32(r);
    for (uint32_t off = 0; off < bytes;) {
        uint32_t part = bytes - off;
        if (part > 4000)
            part = 4000;
        as_put32(p, AS_LOAD_DATA);
        as_put32(p + 4, *id);
        as_put32(p + 8, off);
        memcpy(p + 12, pcm + off, part);
        rc = as_client_command(c, AS_LOAD_SAMPLE, p, part + 12, r, sizeof r, &n);
        if (rc)
            return rc;
        if (n != 8 || as_get32(r) != *id || as_get32(r + 4) != off + part)
            return AS_IO_PROTOCOL;
        off += part;
    }
    as_put32(p, AS_LOAD_COMMIT);
    as_put32(p + 4, *id);
    as_put32(p + 8, as_crc32(pcm, bytes));
    rc = as_client_command(c, AS_LOAD_SAMPLE, p, 12, r, sizeof r, &n);
    if (rc)
        return rc;
    return n == 8 && as_get32(r) == *id && as_get32(r + 4) == bytes ? AS_OK : AS_IO_PROTOCOL;
}
