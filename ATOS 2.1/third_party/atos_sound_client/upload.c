/* ATOS SOUND — wznawialna transakcja uploadu probki.  Patrz include/atos_sound_upload.h.
 *
 * Kluczowa wlasnosc: po niejednoznacznym I/O pakiet zostaje zapamietany razem z
 * numerem sekwencji, ktorym poszedl, i powtorka jest BAJT W BAJT ta sama.
 * as_client_command() przy bledzie transportu nie przesuwa client->sequence,
 * wiec powtorka trafia w cache ostatniej odpowiedzi rdzenia (idempotencja per
 * seq) i operacja nie wykona sie drugi raz.
 */
#include "atos_sound_upload.h"
#include <string.h>

static void clear_pending(as_upload *u) {
    u->pending_len = 0;
    u->pending_op = 0;
    u->pending_seq = 0;
    u->waiting = 0;
}

static void reset_transaction(as_upload *u) {
    u->session = 0;
    u->sample_id = 0;
    u->total = 0;
    u->offset = 0;
    u->channels = 0;
    u->rate = 0;
    u->crc = ~0u;
    u->phase = AS_UPLOAD_IDLE;
    u->chunks = 0;
    clear_pending(u);
}

void as_upload_init(as_upload *u, as_client *c) {
    if (!u)
        return;
    memset(u, 0, sizeof *u);
    u->client = c;
    u->crc = ~0u;
    u->phase = AS_UPLOAD_IDLE;
}

/* Buduje i wysyla pakiet, zapamietujac go do ewentualnej powtorki. */
static int exchange(as_upload *u, uint16_t op, const uint8_t *payload, size_t n, uint8_t *data,
                    size_t *count) {
    if (!u->client || !u->client->transport.exchange || n > sizeof u->pending)
        return AS_IO_PROTOCOL;
    memcpy(u->pending, payload, n);
    u->pending_len = n;
    u->pending_op = op;
    u->pending_seq = u->client->sequence;
    u->waiting = 1;
    int rc = as_client_command(u->client, op, u->pending, u->pending_len, data, AS_MAX_REPLY, count);
    if (rc >= 0)
        clear_pending(u); /* potwierdzone: nie ma czego powtarzac */
    return rc;
}

/* Waliduje potwierdzenie biezacego kroku i przesuwa stan transakcji.
 * Wywolywane zarowno po normalnej odpowiedzi, jak i po powtorce. */
static int finish_step(as_upload *u, const uint8_t *ack, size_t an) {
    if (an != 8)
        return AS_IO_PROTOCOL; /* kontrakt kazdej fazy LOAD to {id, received} */
    uint32_t id = as_get32(ack), received = as_get32(ack + 4);
    switch (u->phase) {
    case AS_UPLOAD_IDLE: /* trwa BEGIN */
        if (!id || received != 0)
            return AS_IO_PROTOCOL; /* audyt A4: zaufanie tylko sensownemu ack */
        u->sample_id = id;
        u->session = u->client->session;
        u->offset = 0;
        u->crc = ~0u;
        u->chunks = 0;
        u->phase = AS_UPLOAD_DATA;
        return AS_OK;
    case AS_UPLOAD_DATA:
        if (id != u->sample_id)
            return AS_IO_PROTOCOL;
        if (received <= u->offset || received > u->total)
            return AS_IO_PROTOCOL; /* musi wskazac nowy, sensowny offset */
        u->crc = as_crc32_update(u->crc, u->pending + 12, received - u->offset);
        u->offset = received;
        ++u->chunks;
        if (u->offset == u->total)
            u->phase = AS_UPLOAD_COMMIT;
        return AS_OK;
    case AS_UPLOAD_COMMIT:
        if (id != u->sample_id || received != u->total)
            return AS_IO_PROTOCOL;
        u->phase = AS_UPLOAD_DONE;
        return AS_OK;
    default:
        return AS_E_STATE;
    }
}

int as_upload_begin(as_upload *u, uint32_t bytes, uint16_t channels, uint32_t rate) {
    uint8_t p[24], ack[AS_MAX_REPLY];
    size_t an = 0;
    if (!u || !u->client)
        return AS_IO_PROTOCOL;
    if (!bytes || (channels != 1 && channels != 2) || !rate || bytes % (channels * 2u))
        return AS_E_ARGUMENT;
    if (u->phase != AS_UPLOAD_IDLE || u->waiting)
        return AS_E_STATE;
    memset(p, 0, sizeof p);
    as_put32(p, AS_LOAD_BEGIN);
    as_put16(p + 4, 1); /* wersja formatu probki sprawdzana przez rdzen */
    as_put16(p + 6, channels);
    as_put32(p + 8, rate);
    as_put32(p + 12, bytes / (channels * 2u));
    as_put32(p + 16, bytes);
    u->total = bytes;
    u->channels = channels;
    u->rate = rate;
    int rc = exchange(u, AS_LOAD_SAMPLE, p, sizeof p, ack, &an);
    if (rc < 0)
        return rc; /* pakiet zapamietany: as_upload_retry() */
    if (rc != AS_OK) {
        reset_transaction(u);
        return rc;
    }
    rc = finish_step(u, ack, an);
    if (rc != AS_OK)
        reset_transaction(u);
    return rc;
}

int as_upload_write(as_upload *u, const uint8_t *pcm, uint32_t bytes) {
    /* Static, jak w as_client_command: inaczej sam ten bufor to 4096 B stosu
     * (audyt 2026-10-13, point E).  Biblioteka jest jednokrotnego wykonawcy. */
    static uint8_t p[12u + AS_UPLOAD_CHUNK], ack[AS_MAX_REPLY];
    size_t an = 0;
    if (!u || !u->client)
        return AS_IO_PROTOCOL;
    if (u->phase != AS_UPLOAD_DATA || u->waiting)
        return AS_E_STATE;
    if (!pcm || !bytes || bytes > AS_UPLOAD_CHUNK || bytes % (u->channels * 2u))
        return AS_E_ARGUMENT;
    if ((uint64_t)u->offset + bytes > u->total)
        return AS_E_ARGUMENT; /* bez zawijania u64 -> u32 */
    as_put32(p, AS_LOAD_DATA);
    as_put32(p + 4, u->sample_id);
    as_put32(p + 8, u->offset);
    memcpy(p + 12, pcm, bytes);
    int rc = exchange(u, AS_LOAD_SAMPLE, p, 12u + bytes, ack, &an);
    if (rc < 0)
        return rc;
    if (rc != AS_OK)
        return rc;
    return finish_step(u, ack, an);
}

int as_upload_commit(as_upload *u) {
    uint8_t p[12], ack[AS_MAX_REPLY];
    size_t an = 0;
    if (!u || !u->client)
        return AS_IO_PROTOCOL;
    if (u->phase != AS_UPLOAD_COMMIT || u->waiting)
        return AS_E_STATE;
    as_put32(p, AS_LOAD_COMMIT);
    as_put32(p + 4, u->sample_id);
    as_put32(p + 8, ~u->crc); /* rdzen porownuje z ~s->crc */
    int rc = exchange(u, AS_LOAD_SAMPLE, p, sizeof p, ack, &an);
    if (rc < 0)
        return rc;
    if (rc != AS_OK)
        return rc;
    return finish_step(u, ack, an);
}

int as_upload_retry(as_upload *u) {
    uint8_t ack[AS_MAX_REPLY];
    size_t an = 0;
    if (!u || !u->client || !u->waiting || !u->pending_len)
        return AS_E_STATE;
    /* Ten sam seq => ten sam pakiet => cache odpowiedzi rdzenia. */
    uint32_t seq = u->pending_seq;
    u->client->sequence = seq;
    int rc =
        as_client_command(u->client, u->pending_op, u->pending, u->pending_len, ack, AS_MAX_REPLY, &an);
    ++u->retries;
    if (rc < 0) {
        u->client->sequence = seq; /* nadal nie wiemy, czy doszlo */
        return rc;
    }
    clear_pending(u);
    if (rc != AS_OK)
        return rc;
    return finish_step(u, ack, an);
}

int as_upload_resume(as_upload *u) {
    if (!u || !u->client)
        return AS_IO_PROTOCOL;
    if (!u->waiting && u->phase == AS_UPLOAD_IDLE)
        return AS_E_STATE; /* brak transakcji */
    if (u->waiting) {
        /* Sesja mogla sie zmienic, zanim cokolwiek powtorzymy. */
        if (u->client->session != (u->session ? u->session : u->client->session))
            return AS_E_STATE;
        return as_upload_retry(u);
    }
    if (u->session && u->client->session != u->session)
        return AS_E_STATE; /* nowa sesja: zasoby poprzedniej nie istnieja */
    return AS_OK;
}

int as_upload_abort(as_upload *u) {
    int rc = AS_OK;
    if (!u || !u->client)
        return AS_IO_PROTOCOL;
    if (u->sample_id && u->phase != AS_UPLOAD_DONE) {
        uint8_t p[8], ack[AS_MAX_REPLY];
        size_t an = 0;
        as_put32(p, AS_LOAD_ABORT);
        as_put32(p + 4, u->sample_id);
        rc = exchange(u, AS_LOAD_SAMPLE, p, sizeof p, ack, &an);
        if (rc == AS_E_STATE || rc == AS_E_HANDLE) {
            /* Silnik odrzuca ABORT dla opublikowanej probki — wtedy UNLOAD.
             * AS_E_BUSY oznacza, ze graja glosy: najpierw STOP, potem UNLOAD. */
            uint8_t q[4];
            as_put32(q, u->sample_id);
            rc = as_client_command(u->client, AS_UNLOAD_SAMPLE, q, sizeof q, ack, AS_MAX_REPLY, &an);
        }
    }
    reset_transaction(u);
    return rc;
}

as_upload_phase as_upload_state(const as_upload *u) {
    return u ? u->phase : AS_UPLOAD_IDLE;
}
uint32_t as_upload_progress(const as_upload *u) {
    return u ? u->offset : 0;
}
uint32_t as_upload_handle(const as_upload *u) {
    return u ? u->sample_id : 0;
}
