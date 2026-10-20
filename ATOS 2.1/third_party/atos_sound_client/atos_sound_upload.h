/* ATOS SOUND — transakcja uploadu probki (ASND-v1), wznawialna.
 *
 * Czym rozni sie od as_client_load_pcm(): tamten jest synchroniczna wygoda dla
 * testow i NIE zachowuje pakietu po niejednoznacznym I/O (naglowek
 * atos_sound_transport.h mowi to wprost).  Broker produkcyjny potrzebuje stanu,
 * ktory:
 *
 *   1. PAMIETA ostatni pakiet (bajt w bajt) i numer sekwencji, ktorym poszedl,
 *      zeby po utracie odpowiedzi powtorzyc DOKLADNIE to samo zadanie.  Wire
 *      jest idempotentny per seq (rdzen cache'uje ostatnia odpowiedz), wiec
 *      powtorka nie wykona operacji drugi raz.
 *   2. ZNA SESJE, w ktorej transakcja zyje.  Reconnect/RESET = nowa sesja,
 *      a zasoby poprzedniej nie istnieja; wtedy transakcja jest niewazna i
 *      trzeba ja zaczac od nowa (albo posprzatac przez ABORT/UNLOAD).
 *   3. WALIDUJE potwierdzenia: niezerowy uchwyt z BEGIN, zgodnosc uchwytu i
 *      zgodnosc licznika odebranych bajtow.  Nie ufa samemu statusowi OK.
 *   4. PRZYWRACA sie po utracie odpowiedzi w KAZDEJ fazie (BEGIN/DATA/COMMIT)
 *      bez przesuwania offsetu do czasu potwierdzenia.
 *   5. SPRZATA: as_upload_abort() wysyla ABORT (albo UNLOAD, gdy uchwyt jest
 *      znany, a silnik odrzucil ABORT), zwalniajac arena PCM.
 *
 * API jest uzywane zarowno przez klienta na Atari (bez miksera), jak i przez
 * symulator oraz firmware — nie zawiera zaleznosci od OS ani od miksera.
 */
#ifndef ATOS_SOUND_UPLOAD_H
#define ATOS_SOUND_UPLOAD_H

#include "atos_sound_transport.h"

typedef enum {
    AS_UPLOAD_IDLE = 0,     /* nic nie trwa                     */
    AS_UPLOAD_BEGIN,        /* wyslane BEGIN, czekamy na uchwyt  */
    AS_UPLOAD_DATA,         /* wysylamy dane                     */
    AS_UPLOAD_COMMIT,       /* wyslane COMMIT, czekamy na CRC    */
    AS_UPLOAD_DONE          /* probka opublikowana               */
} as_upload_phase;

typedef struct {
    as_client *client;
    uint32_t session;           /* sesja transakcji (0 = brak)               */
    uint32_t sample_id;         /* uchwyt z BEGIN (0 = jeszcze nieznany)     */
    uint32_t total;             /* bajty calosci                             */
    uint32_t offset;            /* bajty POTWIERDZONE (== received)          */
    uint16_t channels;
    uint32_t rate;
    uint32_t crc;               /* CRC32 narastajaco, start ~0 (jak rdzen)   */
    as_upload_phase phase;
    /* Ostatni pakiet, gotowy do powtorzenia bajt w bajt. */
    uint8_t  pending[AS_MAX_PACKET];
    size_t   pending_len;
    uint16_t pending_op;
    uint32_t pending_seq;
    uint8_t  waiting;           /* 1 = pakiet wyslany, brak potwierdzenia    */
    uint32_t retries;           /* ile powtorzen wykonano                    */
    uint32_t chunks;            /* ile porcji danych przyjetych              */
} as_upload;

void as_upload_init(as_upload *u, as_client *c);

/* Rozpoczyna transakcje: BEGIN(bytes, channels, rate).  0 albo status bledu.
 * Przy bledzie transportu pakiet zostaje zapamietany — uzyj as_upload_retry(). */
int as_upload_begin(as_upload *u, uint32_t bytes, uint16_t channels, uint32_t rate);

/* Wysyla jedna porcje danych od potwierdzonego offsetu.  `bytes` musi byc
 * wielokrotnoscia channels*2 i nie przekraczac AS_UPLOAD_CHUNK. */
#define AS_UPLOAD_CHUNK (AS_MAX_PAYLOAD - 12u)
int as_upload_write(as_upload *u, const uint8_t *pcm, uint32_t bytes);

/* Konczy transakcje: COMMIT z CRC.  Po sukcesie probka jest opublikowana. */
int as_upload_commit(as_upload *u);

/* Ponawia ostatni pakiet DOKLADNIE tak, jak poszedl (ten sam seq). */
int as_upload_retry(as_upload *u);

/* Sprawdza, czy sesja jest ta sama, i ewentualnie ponawia oczekujacy pakiet.
 * Zwraca AS_OK, AS_E_STATE (nowa sesja — transakcja niewazna) albo status I/O. */
int as_upload_resume(as_upload *u);

/* Sprzatanie: ABORT, a gdy silnik go odrzuci — UNLOAD znanego uchwytu. */
int as_upload_abort(as_upload *u);

/* Podglad stanu dla brokera (bez I/O). */
as_upload_phase as_upload_state(const as_upload *u);
uint32_t as_upload_progress(const as_upload *u);   /* potwierdzone bajty */
uint32_t as_upload_handle(const as_upload *u);     /* uchwyt albo 0      */

#endif /* ATOS_SOUND_UPLOAD_H */
