/* ATOS Sound 1.0 — usluga jadra dla transportu ASND (NF_ATOS_SOUND).
 *
 * Po co: aplikacja nie moze (i nie powinna) wolac NatFeats bezposrednio.
 * Jadro:
 *   - sprawdza WLASCICIELA AUDIO ta sama regula co ton/kolejka/PCM
 *     (kaudio_claim), wiec dwoch chetnych nie moze jednoczesnie sterowac,
 *   - waliduje bufory uzytkownika (user_range / user_write_range),
 *   - dopiero potem wykonuje wymiane w trybie SUPERVISORA.
 *
 * Bez NatFeats usluga zwraca KSND_E_UNAVAILABLE i NIE wykonuje zadnego opcode:
 * obecnosc funkcji sprawdza raz bezpieczna sonda (system/kernel_sound_nf_probe.S),
 * ktora podmienia na czas proby tylko wektor 4 i natychmiast go przywraca.
 */
#include <stdint.h>
#include <stddef.h>
#include "app_abi.h"

/* Statusy protokolu ASND (include/atos_sound.h w reference-v1).  Kernel nie
 * wciaga tego naglowka, wiec powtarzamy TYLKO te dwie wartosci, ktorych uzywa
 * usluga - z komentarzem, skad sa (audyt r3 pokazal, ze brak definicji wyszedl
 * dopiero przy pelnej przebudowie). */
#define KSND_AS_OK     0
#define KSND_AS_E_BUSY 11 /* 10 to AS_E_STATE! */

extern int ksnd_nf_probe(const char *name, uint32_t *id_out);

#define KSND_MAX_PACKET 4128u /* AS_MAX_PACKET z ASND-v1 */
#define KSND_MIN_REPLY  256u  /* AS_MAX_REPLY */

/* Kody w rodzinie -0x3xx/-0x4xx uzywanej przez pozostale wywolania jadra. */
#define KSND_E_UNAVAILABLE (-0x400) /* nie ma NatFeats / brak NF_ATOS_SOUND */
#define KSND_E_BUFFER      (-0x401) /* zle dlugosci buforow */
#define KSND_E_PROTOCOL    (-0x402) /* odpowiedz sprzeczna z umowa ASND */

static uint32_t ksnd_owner;     /* wlasciciel URZADZENIA PCM (nie PSG!) */
static uint32_t ksnd_user_pid;  /* ostatni pid, ktory uzywal uslugi */
static uint32_t ksnd_session;   /* sesja zaobserwowana w wymianach */
static uint32_t ksnd_last_seq;  /* ostatni seq, ktory przeszedl */
static uint8_t  ksnd_probed;  /* sonda wykonana raz */
static uint32_t ksnd_id;      /* NatFeats ID funkcji (0 = brak) */
static uint32_t ksnd_calls;   /* udane wymiany */
static uint32_t ksnd_errors;  /* odrzucone odpowiedzi/bledy */

uint32_t ksnd_available(void)
{
    return ksnd_id != 0u;
}

static uint32_t ksnd_detect(void)
{
    if (!ksnd_probed) {
        uint32_t id = 0;
        ksnd_probed = 1;
        (void)ksnd_nf_probe("NF_ATOS_SOUND", &id);
        ksnd_id = id;
    }
    return ksnd_id;
}

/* Wymiana NF_CALL w supervisorku.  Uklad stosu zweryfikowany w audycie
 * 2026-10-13: opcode pomija slot adresu powrotu, NatFeat_Call pomija id|subid,
 * argumenty sa od sp+8.  Wynik (dlugosc odpowiedzi) w d0. */
static uint32_t ksnd_nf_call(uint32_t id_sub, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    register uint32_t d0 __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                         "move.l %4,-(%%sp)\n\tmove.l %5,-(%%sp)\n\tclr.l -(%%sp)\n\t"
                         ".word 0x7301\n\tlea 24(%%sp),%%sp"
                         : "=d"(d0)
                         : "r"(a3), "r"(a2), "r"(a1), "r"(a0), "r"(id_sub)
                         : "memory", "cc");
    return d0;
}

/* Wyjscie/awaria wlasciciela audio: jadro zwalnia wlasciciela (kaudio_release),
 * ale silnik NF musi dodatkowo usunac glosy i zasoby, inaczej zostaje dzwiek po
 * aplikacji (audyt r2, punkt 6).  Wolane tam, gdzie jadro robi kaudio_release. */
/* CRC-32 (ten sam wielomian co ASND: 0xedb88320 odbity, init ~0, final ~c). */
static uint32_t ksnd_crc(const uint8_t *p, uint32_t n)
{
    uint32_t c = ~0u, i, k;
    for (i = 0; i < n; ++i) {
        /* Pole CRC (bajty 28..31) liczy sie jako ZERA - tak samo jak packet_crc
         * w wire.c.  Bez tego suma nigdy sie nie zgadzala i jadro nie
         * zapamietywalo sesji do sprzatanie (audyt r4, punkt 2). */
        uint8_t b = (i >= 28u && i < 32u) ? 0u : p[i];
        c ^= b;
        for (k = 0; k < 8u; ++k)
            c = (c >> 1) ^ ((0u - (c & 1u)) & 0xedb88320u);
    }
    return ~c;
}

static void ksnd_put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void ksnd_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t ksnd_get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Wyjscie/awaria wlasciciela: jadro zwalnia wlasciciela (kaudio_release), ale
 * silnik NF trzeba dodatkowo wyczyscic, inaczej zostaje dzwiek po aplikacji
 * (audyt r2, punkt 6).  Kernel nie siega do silnika (ten jest w Hatari), wiec
 * wysyla POPRAWNY AS_RESET: z sesja i seq zaobserwowanymi w wymianach.
 * Wczesniejsza wersja uzywala session=0, co dawalo AS_E_SESSION i nic nie czyscilo. */
void ksnd_owner_exit(uint32_t pid)
{
    uint8_t req[32], reply[KSND_MIN_REPLY];
    uint32_t i, n;

    if (!ksnd_user_pid || pid != ksnd_user_pid)
        return;
    ksnd_user_pid = 0;
    if (!ksnd_id || !ksnd_session)
        return; /* usluga nie byla uzywana albo nie znamy sesji */

    for (i = 0; i < 32u; ++i)
        req[i] = 0;
    req[0] = 'A'; req[1] = 'S'; req[2] = 'N'; req[3] = 'D';
    req[4] = 1;
    ksnd_put16(req + 6, 0);
    ksnd_put16(req + 8, 11u);     /* AS_RESET = 11 (w ASND: HELLO=1 ... RESET=11) */
    ksnd_put16(req + 10, 32u);
    ksnd_put32(req + 12, ksnd_last_seq + 1u);
    ksnd_put32(req + 16, ksnd_session);
    ksnd_put32(req + 20, 0);
    ksnd_put32(req + 24, 0);
    ksnd_put32(req + 28, ksnd_crc(req, 32u));
    n = ksnd_nf_call(ksnd_id | 1u, (uint32_t)req, 32u, (uint32_t)reply, KSND_MIN_REPLY);
    (void)n;
}

/* Wlasciciel URZADZENIA PCM to OSOBNA domena niz wlasciciel PSG (kaudio_claim).
 * Dzieki temu Arkanoid (PSG/YM2149) i odtwarzacz PCM nie blokuja sie wzajemnie,
 * a sterownik Yamahy i zachowanie Arkanoida pozostaja bez zmian (audyt r3, pkt 3). */
int32_t ksnd_claim(uint32_t pid)
{
    if (!ksnd_owner) {
        ksnd_owner = pid ? pid : 1u;
        return KSND_AS_OK;
    }
    return ksnd_owner == pid ? KSND_AS_OK : KSND_AS_E_BUSY;
}

void ksnd_release(uint32_t pid)
{
    if (ksnd_owner == pid)
        ksnd_owner = 0;
}

uint32_t ksnd_owner_pid_get(void)
{
    return ksnd_owner;
}

int32_t ksnd_exchange(uint32_t pid, const void *req, uint32_t req_len, void *reply,
                      uint32_t reply_cap)
{
    uint32_t id = ksnd_detect();
    uint32_t n;

    if (!id)
        return KSND_E_UNAVAILABLE;
    if (!req || req_len < 32u || req_len > KSND_MAX_PACKET)
        return KSND_E_BUFFER;
    if (!reply || reply_cap < KSND_MIN_REPLY || reply_cap > KSND_MAX_PACKET)
        return KSND_E_BUFFER;

    ksnd_user_pid = pid; /* PID jawnie z jadra: wczesniej ta zmienna byla zawsze 0 */
    n = ksnd_nf_call(id | 1u, (uint32_t)req, req_len, (uint32_t)reply, reply_cap);
    if (!n || n > reply_cap) {
        ++ksnd_errors;
        return KSND_E_PROTOCOL;
    }
    /* Sesje zapamietujemy z POPRAWNEJ ODPOWIEDZI (magic + CRC), a nie z zadania:
     * bledne zadanie nie moze zatruc stanu sprzatania (audyt r3, punkt 2). */
    if (n >= 32u && reply) {
        const uint8_t *rp = (const uint8_t *)reply;
        if (ksnd_crc(rp, n) == ksnd_get32(rp + 28) && rp[0] == 'A' && rp[1] == 'S' &&
            rp[2] == 'N' && rp[3] == 'D') {
            ksnd_session = ksnd_get32(rp + 16);
            ksnd_last_seq = ksnd_get32(rp + 12);
        }
    }
    ++ksnd_calls;
    return (int32_t)n;
}

/* a0 = uint32[4]: dostepnosc, ID!=0, udane wymiany, bledy. */
void ksnd_info(uint32_t *out)
{
    /* Najpierw WYKRYCIE: bez tego INFO raportowalo "niedostepne" przed
     * pierwsza sonda i NFTEST konczyl sie na tym kroku (audyt r2, punkt 1). */
    (void)ksnd_detect();
    out[0] = ksnd_available();
    out[1] = ksnd_id;
    out[2] = ksnd_calls;
    out[3] = ksnd_errors;
}
