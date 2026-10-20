#ifndef ATOS_SOUND_H
#define ATOS_SOUND_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AS_VOICES 16u
#define AS_SAMPLES 64u
#define AS_EVENTS 128u
/* Miejsca w kolejce trzymane wylacznie dla AS_STOP.  Dzieki temu pelna kolejka
 * NIGDY nie uniemozliwi wlascicielowi wyciszenia swoich glosow: STOP moze zajac
 * rezerwe (AS_VOICES miejsc), a PLAY/SET_* zatrzymuja sie wczesniej.  To wymog
 * bezpiecznego teardownu brokera; protokol i opkody pozostaja bez zmian. */
#define AS_RESERVED_EVENTS AS_VOICES
#define AS_MAX_PAYLOAD 4096u
#define AS_HEADER 32u
#define AS_MAX_PACKET (AS_HEADER + AS_MAX_PAYLOAD)
#define AS_MAX_REPLY 256u
#define AS_DEFAULT_BUDGET (6u * 1024u * 1024u)
#define AS_NOW UINT64_MAX
/* PCM memory contains signed, little-endian 16-bit mono/stereo samples. */
enum as_opcode {
    AS_HELLO = 1,
    AS_GET_CAPS,
    AS_LOAD_SAMPLE,
    AS_UNLOAD_SAMPLE,
    AS_PLAY,
    AS_STOP,
    AS_SET_VOLUME,
    AS_SET_PAN,
    AS_GET_STATUS,
    AS_GET_CLOCK,
    AS_RESET,
    AS_SET_INPUT_ROUTE = 0x20,
    AS_TRACKER_LOAD = 0x30, AS_TRACKER_CONTROL = 0x31, AS_TRACKER_STATUS = 0x32, AS_TRACKER_SEEK = 0x33
};
enum as_error {
    AS_OK = 0,
    AS_E_PACKET = 1,
    AS_E_VERSION,
    AS_E_CRC,
    AS_E_OPCODE,
    AS_E_SESSION,
    AS_E_SEQUENCE,
    AS_E_ARGUMENT,
    AS_E_MEMORY,
    AS_E_HANDLE,
    AS_E_STATE,
    AS_E_BUSY,
    AS_E_QUEUE,
    AS_E_LATE,
    AS_E_LIMIT,
    AS_E_UNSUPPORTED
};
enum as_load_phase { AS_LOAD_BEGIN = 0, AS_LOAD_DATA = 1, AS_LOAD_COMMIT = 2, AS_LOAD_ABORT = 3 };
enum as_cap {
    AS_CAP_PCM = 1,
    AS_CAP_SCHEDULE = 2,
    AS_CAP_LOOP = 4,
    AS_CAP_STEREO_SAMPLE = 8,
    AS_CAP_48000 = 16,
    AS_CAP_LINE_IN = 32,
    AS_CAP_ANALOG_MIX = 64,
    AS_CAP_TRACKER = 128, AS_CAP_TRACKER_SEEK = 256
};
typedef void *(*as_alloc_fn)(void *ctx, size_t bytes);
typedef void (*as_free_fn)(void *ctx, void *ptr);
typedef struct {
    uint32_t output_rate, sample_budget, session_seed;
    as_alloc_fn alloc;
    as_free_fn free;
    void *allocator_context;
} as_config;
typedef struct as_engine as_engine;
/* All calls on one engine MUST be serialized by its owner. No internal locks.
 * mix() never allocates, does I/O or calls external code. Engine metadata uses
 * the allocator too; budget applies ONLY to PCM storage, not metadata. */
as_engine *as_create(const as_config *config);
void as_destroy(as_engine *engine);
/* JEDEN WYKONAWCA (kontrakt).  as_mix() i as_exchange() nie moga byc wywolywane
 * wspolbieznie ani zagniezdzenie.  Rdzen wykrywa zagniezdzone wejscie, odrzuca je
 * BEZ zmiany stanu (as_exchange zwraca 0, as_mix wypelnia bufor cisza) i zwieksza
 * licznik as_reentrancy_refused().
 *
 * Callback audio NIE moze czekac na USB/dysk ani alokowac pamieci: ma tylko wolac
 * as_mix().  Prace administracyjne (as_exchange, upload) wykonuje osobny wykonawca,
 * ktory serializuje sie z callbackiem po stronie platformy.  Do budzetowania
 * obrotu sluza as_events_applied() i as_max_events_mix(). */
void as_mix(as_engine *engine, int16_t *interleaved_stereo, size_t frames);
/* Wyrzuca caly stan silnika (glosy, kolejke, sample) i rotuje sesje - to samo,
 * co komenda AS_RESET.  Dla resetu goscia, snapshotu i wyjscia wlasciciela. */
void as_engine_reset(as_engine *engine);
/* UWAGA (audyt r3, sekcja 10): klient ASND trzyma ramki zadania i odpowiedzi
 * w buforach STATYCZNYCH, zeby stos 68k nie musial pomiescic ~8.9 KB.  To
 * oznacza serializacje WSZYSTKICH klientow korzystajacych z tej samej instancji
 * biblioteki (nie tylko jednego as_engine): jedno polecenie naraz, bez watkow
 * i bez rekurencji.  Jesli kiedys potrzebny bedzie rownolegly klient, trzeba
 * przeniesc ten workspace do kontekstu klienta (alokacja poza stosem 68k). */
uint64_t as_clock(const as_engine *engine); /* next not-yet-rendered frame */
uint32_t as_session(const as_engine *engine);
/* Diagnostyka kontraktu jednego wykonawcy i budzetu administracyjnego. */
uint64_t as_events_applied(const as_engine *engine);
uint32_t as_max_events_mix(const as_engine *engine);
uint64_t as_reentrancy_refused(const as_engine *engine);
/* Diagnostics from platform: output starvation, not idle voices. */
void as_note_underrun(as_engine *engine);
/* One COMPLETE packet in/out. Returns 0 only for invalid API arguments or
 * insufficient reply capacity; no command mutation in either case.
 * Request and reply buffers may overlap; retry cache preserves original bytes. */
size_t as_exchange(as_engine *, const uint8_t *, size_t, uint8_t *, size_t);
uint16_t as_get16(const uint8_t *);
uint32_t as_get32(const uint8_t *);
uint64_t as_get64(const uint8_t *);
void as_put16(uint8_t *, uint16_t);
void as_put32(uint8_t *, uint32_t);
void as_put64(uint8_t *, uint64_t);
uint32_t as_crc32(const void *, size_t);
/* Incremental raw CRC state: initialize ~0u, complement final result. */
uint32_t as_crc32_update(uint32_t, const void *, size_t);
size_t as_packet(uint8_t *out, size_t cap, uint16_t op, uint16_t flags, uint32_t seq,
                 uint32_t session, const void *payload, size_t length);
int as_packet_check(const uint8_t *, size_t); /* as_error */
#ifdef __cplusplus
}
#endif
#endif
