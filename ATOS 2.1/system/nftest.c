/* NFTEST.APP — CIENKA OTOCZKA.
 *
 * Cala sekwencja akceptacyjna (HELLO -> CAPS -> ton 441 Hz -> PLAY+petla ->
 * dwa odczyty zegara -> STOP -> UNLOAD -> trzy tony -> STATUS=3 -> ~3 s na
 * odsluch -> STOP x3 -> UNLOAD x3 -> DIAG zerowych zasobow) jest w PRZENOSNYM
 * module reference-v1/src/guest_seq.c.  TEN SAM kod przechodzi test hostowy
 * tests/test_guest_mirror.c, wiec bledy protokolu po stronie aplikacji sa
 * wykrywane bez emulatora (audyt r5).
 *
 * Otoczka dostarcza tylko to, co zalezne od ATOS:
 *   - transport: usluga jadra (ATOS_CALL_SOUND_XCHG), nie NatFeats wprost,
 *   - raport: NF_STDERR (diagnostyka, dozwolona w trybie uzytkownika),
 *   - oddanie CPU: gui_call(11).
 */
#include "app_abi.h"
#include "gui_app.h"
#include "atos_sound_guest.h"
#include "atos_sound_transport.h"
#include <string.h>

/* Aplikacja ATOS nie linkuje libc, a biblioteka klienta uzywa tych funkcji.
 * -fno-builtin jest w CFLAGS, wiec wlasne definicje sa bezpieczne. */
void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *dd = (unsigned char *)d;
    const unsigned char *ss = (const unsigned char *)s;
    while (n--)
        *dd++ = *ss++;
    return d;
}
void *memset(void *d, int c, size_t n)
{
    unsigned char *dd = (unsigned char *)d;
    while (n--)
        *dd++ = (unsigned char)c;
    return d;
}
int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
    while (n--) {
        if (*x != *y)
            return (int)*x - (int)*y;
        ++x;
        ++y;
    }
    return 0;
}

/* NF_ID/NF_CALL tylko dla NF_STDERR (diagnostyka).  Wymiana protokolu idzie
 * przez usluge jadra - NF_ATOS_SOUND jest super-only. */
static uint32_t nf_id(const char *name)
{
    register uint32_t d0 __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\tclr.l -(%%sp)\n\t.word 0x7300\n\taddq.l #8,%%sp"
                         : "=d"(d0)
                         : "r"(name)
                         : "memory", "cc");
    return d0;
}
static int32_t nf_call(uint32_t id_sub, const uint32_t *a)
{
    register uint32_t d0 __asm__("d0");
    d0 = id_sub;
    __asm__ __volatile__("move.l %1,-(%%sp)\n\tmove.l %2,-(%%sp)\n\tmove.l %3,-(%%sp)\n\t"
                         "move.l %4,-(%%sp)\n\tmove.l %5,-(%%sp)\n\tclr.l -(%%sp)\n\t"
                         ".word 0x7301\n\tlea 24(%%sp),%%sp"
                         : "=d"(d0)
                         : "r"(a[3]), "r"(a[2]), "r"(a[1]), "r"(a[0]), "r"(id_sub)
                         : "memory", "cc");
    return (int32_t)d0;
}
static void nf_say(void *ctx, const char *line)
{
    uint32_t a[4];
    uint32_t sid = nf_id("NF_STDERR");
    (void)ctx;
    if (!sid)
        return;
    a[0] = (uint32_t)(uintptr_t)line;
    a[1] = 0;
    a[2] = 0;
    a[3] = 0;
    (void)nf_call(sid, a);
}

/* Transport: usluga jadra.  Kontrakt as_transport.exchange: 0 = OK, <0 = blad. */
static int svc_exchange(void *ctx, const uint8_t *req, size_t n, uint8_t *rep, size_t cap,
                        size_t *out)
{
    int32_t rc;
    (void)ctx;
    *out = 0;
    rc = gui_call(ATOS_CALL_SOUND_XCHG, (uint32_t)n, (uint32_t)cap, (void *)req, (void *)rep);
    if (rc < 0)
        return (int)rc;
    if ((size_t)rc > cap)
        return AS_IO_PROTOCOL;
    *out = (size_t)rc;
    return 0;
}

static void gyield(void *ctx)
{
    (void)ctx;
    (void)gui_call(11, 1, 0, 0, 0);
}

int app_main(void)
{
    static uint32_t info[4];
    as_client c;
    as_guest_t g;

    if (gui_call(ATOS_CALL_SOUND_INFO, 0, 0, info, 0) != 0 || !info[0]) {
        nf_say(0, "NFTEST: FAIL ATOS Sound niedostepny (usluga jadra)");
        return 1;
    }
    as_client_init(&c, (as_transport){svc_exchange, 0});
    g.client = &c;
    g.ctx = 0;
    g.say = nf_say;
    g.yield = gyield;
    return as_guest_run(&g) ? 1 : 0;
}
