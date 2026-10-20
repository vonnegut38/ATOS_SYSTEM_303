/*
 * ATOS sound backend for FLASHBK.APP - dzwiek przez EMULOWANE ESP (ASND).
 * 2026-10-15, zastepuje port/audio_null.cpp.
 *
 * Zasada (taka sama jak w PRE2/MODPLAY): silnik nadal wola API Mixer, ale nic
 * nie jest miksowane na 68030.  Kazdy efekt jest RAZ wgrywany do urzadzenia
 * (usluga jadra 86/87 -> NatFeats "NF_ATOS_SOUND") i odtwarzany jednym z 16
 * glosow PCM; urzadzenie miksuje efekty razem z muzyka.
 *
 * Muzyka: urzadzenie ma dekoder modulow (libxmp, AS_CAP_TRACKER).  Dane DOS,
 * ktore ma ten port, zawieraja MIDI/AdLib (.MID/.ADL), a nie moduly, wiec
 * playMusic() probuje modulu pod sciezka MODULE_PATTERN i przy jego braku jest
 * cicho.  Po wrzuceniu danych amigowych (albo wyrenderowanych modulow) muzyka
 * zagra z ESP bez zmian w kodzie.
 *
 * CZESTOTLIWOSC: bank efektow DOS (GLOBAL.FIB) ma 6000 Hz, a urzadzenie
 * przyjmuje 8000..96000 Hz - sample ponizej 8000 Hz sa resample'owane przy
 * wgraniu (patrz upload_sample).
 */
#include "mixer.h"
#include "screenshot.h"
#include "file.h"
#include "atos_sys.h"
/* Portowa libc (port/include/stdint.h) nie definiuje UINT64_MAX, a naglowek
   klienta ASND uzywa go w AS_NOW ("jak najszybciej"). */
#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffULL
#endif
extern "C" {
#include "atos_sound.h"
#include "atos_sound_transport.h"
#include "atos_sound_upload.h"
}
#include <string.h>
#include <stdio.h>

/* ---- gracze, ktorych nie uzywamy (to samo co w audio_null.cpp) ------------ */
CpcPlayer::CpcPlayer(Mixer *mixer, FileSystem *fs) : _mix(mixer), _fs(fs) {}
CpcPlayer::~CpcPlayer() {}
ModPlayer::ModPlayer(Mixer *mixer, FileSystem *fs) : _isAmiga(false), _playing(false), _mix(mixer), _fs(fs), _impl(0) {}
ModPlayer::~ModPlayer() {}
OggPlayer::OggPlayer(Mixer *mixer, FileSystem *fs) : _mix(mixer), _fs(fs), _impl(0) {}
OggPlayer::~OggPlayer() {}
SfxPlayer::SfxPlayer(Mixer *mixer) : _mod(0), _playing(false), _mix(mixer) {}

/* ---- lacze ASND ----------------------------------------------------------- */
#define SFX_SLOTS     96
#define HIRES_TICKS   12288u                 /* atos_hires() tykniec na sekunde */
#define AS_MIN_RATE   8000u
#define AS_MAX_RATE   96000u
/* Moduly XM wygenerowane z muzyki DOS przez tools/midi_to_xm.py; numery 0..30 to
   indeksy tabeli utworow z FB.EXE (patrz raport, sekcja 12). */
/* Sciezka WZGLEDNA od katalogu danych gry: port/fs_atos.cpp dokleja swoj
   katalog root do kazdej nazwy, wiec absolutna dalaby podwojona sciezke. */
#define MODULE_PATTERN "TR%02d.XM"
/* Muzyka poziomow: gra wola MUSIC_TRACK + numer poziomu (>=1000).  Bierzemy
   najdluzsze utwory, zeby tlo nie zapetlalo sie co kilka sekund. */
static const int _level_tracks[] = {0, 25, 8, 26};
#define AUDIO_MARKER  "\\SYSTEM\\FBAUDIO.TXT"   /* nazwa MUSI byc 8.3 */

struct SfxSlot {
	const uint8_t *data;                     /* stabilny wskaznik z silnika */
	uint32_t handle, frames, rate, voice, end_tick;
};
static SfxSlot _slot[SFX_SLOTS];
static as_client _client;
static as_upload _upload;
static uint8_t _pcm[AS_UPLOAD_CHUNK & ~1u];
static uint8_t _reply[AS_MAX_REPLY];
static uint8_t _packet[AS_MAX_PAYLOAD];
static int _connected, _tracker_loaded, _music_num = -1, _music_ok, _music_missing;
static uint32_t _caps;
/* Liczniki widoczne dla diagnostyki i testow hostowych. */
uint32_t flashbk_audio_loaded, flashbk_audio_played, flashbk_audio_dropped;
int32_t flashbk_audio_error;

static int asnd_exchange(void *ctx, const uint8_t *req, size_t n, uint8_t *reply, size_t cap, size_t *got)
{
	(void)ctx;
	int32_t r = atos_call(86, (uint32_t)n, (uint32_t)cap, req, reply);
	if (r < (int32_t)AS_HEADER) return AS_IO_UNAVAILABLE;   /* brak urzadzenia/BUSY */
	if ((size_t)r > cap) return AS_IO_PROTOCOL;
	*got = (size_t)r;
	return 0;
}
static int command(uint16_t op, const void *p, size_t n)
{
	size_t got = 0;
	return as_client_command(&_client, op, p, n, _reply, sizeof _reply, &got);
}
/* Znaczniki na dysku ATOS: po jednym pliku na stan (8.3!), zeby kolejne etapy
   nie nadpisywaly sie nawzajem - tak test widzi pelna historie przebiegu. */
static void marker_file(const char *tag)
{
	char path[32], buf[192];
	snprintf(path, sizeof path, "\\SYSTEM\\FB_%s.TXT", tag);
	snprintf(buf, sizeof buf,
	         "state=%s t=%u connected=%d caps=0x%x loaded=%u played=%u dropped=%u error=%d music=%d\n",
	         tag, (unsigned)atos_hires(), _connected, (unsigned)_caps,
	         (unsigned)flashbk_audio_loaded, (unsigned)flashbk_audio_played,
	         (unsigned)flashbk_audio_dropped, (int)flashbk_audio_error, _music_num);
	(void)atos_save(path, buf, (uint32_t)strlen(buf));
}
/* Znacznik muzyki z numerem utworu i sciezka - rozroznia "nie wolano" od
   "nie ma pliku" (brak modulu ma byc cichy, ale musi byc widoczny w testach). */
static void marker_music(const char *tag, int track, const char *path, int rc)
{
	char p[32], buf[192];
	snprintf(p, sizeof p, "\\SYSTEM\\FB_%s.TXT", tag);
	snprintf(buf, sizeof buf, "state=%s t=%u track=%d rc=%d path=%s\n",
	         tag, (unsigned)atos_hires(), track, rc, path);
	(void)atos_save(p, buf, (uint32_t)strlen(buf));
}
static void disconnect(void)
{
	_connected = 0; _caps = 0; _tracker_loaded = 0; _music_num = -1;
	memset(_slot, 0, sizeof _slot);
}
static void connect(void)
{
	uint32_t info[4] = {0};
	if (_connected) return;
	/* 87 = sonda urzadzenia; bez NF_ATOS_SOUND (albo bez --natfeats) nie ma nic. */
	if (atos_call(87, 0, 0, info, 0) < 0 || !info[0]) { flashbk_audio_error = AS_IO_UNAVAILABLE; return; }
	as_client_init(&_client, (as_transport){asnd_exchange, 0});
	int rc = command(AS_HELLO, 0, 0);
	if (rc != AS_OK) { flashbk_audio_error = rc; return; }
	if (command(AS_GET_CAPS, 0, 0) == AS_OK) _caps = as_get32(_reply);
	_connected = 1;
	marker_file("CONN");                         /* dowod: urzadzenie znalezione */
}

/* Wgraj jeden efekt (8-bit signed mono) jako sample PCM.  Zwraca uchwyt. */
static uint32_t upload_sample(const uint8_t *data, uint32_t frames, uint16_t freq,
                              uint32_t *out_frames, uint32_t *out_rate)
{
	uint32_t rate = freq ? freq : AS_MIN_RATE;
	uint32_t out = frames;
	if (rate < AS_MIN_RATE) {
		out = (uint32_t)(((uint64_t)frames * AS_MIN_RATE) / rate);
		rate = AS_MIN_RATE;
	} else if (rate > AS_MAX_RATE) {
		rate = AS_MAX_RATE;
	}
	if (!frames || !out) return 0;
	as_upload_init(&_upload, &_client);
	if (as_upload_begin(&_upload, out * 2u, 1, rate) != AS_OK) return 0;
	uint32_t done = 0;
	while (done < out) {
		uint32_t count = out - done;
		if (count > sizeof _pcm / 2u) count = sizeof _pcm / 2u;
		for (uint32_t i = 0; i < count; ++i) {
			int v;
			if (out == frames) {
				v = (int)(int8_t)data[done + i];
			} else {                         /* resampling (np. 6000 -> 8000 Hz) */
				uint32_t pos = (uint32_t)(((uint64_t)(done + i) * frames) / out);
				uint32_t nxt = (pos + 1u < frames) ? pos + 1u : pos;
				v = ((int)(int8_t)data[pos] + (int)(int8_t)data[nxt]) / 2;
			}
			as_put16(_pcm + i * 2u, (uint16_t)(int16_t)(v * 256));
		}
		if (as_upload_write(&_upload, _pcm, count * 2u) != AS_OK) return 0;
		done += count;
	}
	if (as_upload_commit(&_upload) != AS_OK) return 0;
	*out_frames = out; *out_rate = rate;
	return as_upload_handle(&_upload);
}

/* Muzyka: modul -> dekoder w urzadzeniu (tens of KiB, bez dekodowania na 68030). */
static int tracker_song(const char *path, FileSystem *fs)
{
	if (_tracker_loaded) {                        /* 4 = unload */
		as_put32(_packet, 4);
		int rc = command(AS_TRACKER_CONTROL, _packet, 4);
		if (rc != AS_OK) return rc;
		_tracker_loaded = 0;
	}
	File f;
	if (!f.open(path, "rb", fs)) return AS_E_HANDLE;
	uint32_t total = f.size();
	if (total < 1084u || total > 2u * 1024u * 1024u) { f.close(); return AS_E_ARGUMENT; }
	as_put32(_packet, 0u); as_put32(_packet + 4, total); as_put32(_packet + 8, 0u);
	int rc = command(AS_TRACKER_LOAD, _packet, 12);
	if (rc != AS_OK) { f.close(); return rc; }
	uint32_t id = as_get32(_reply), pos = 0, crc = ~0u;
	while (pos < total) {
		uint32_t n = total - pos;
		if (n > AS_MAX_PAYLOAD - 12u) n = AS_MAX_PAYLOAD - 12u;
		if (f.read(_packet + 12, n) != n) { f.close(); return AS_E_PACKET; }
		as_put32(_packet, 1u); as_put32(_packet + 4, id); as_put32(_packet + 8, pos);
		rc = command(AS_TRACKER_LOAD, _packet, n + 12u);
		if (rc != AS_OK) { f.close(); return rc; }
		crc = as_crc32_update(crc, _packet + 12, n);
		pos += n;
		(void)atos_call(10, 0, 0, 0, 0);          /* yield w trakcie wysylania */
	}
	as_put32(_packet, 2u); as_put32(_packet + 4, id); as_put32(_packet + 8, ~crc);
	f.close();
	rc = command(AS_TRACKER_LOAD, _packet, 12);
	if (rc != AS_OK) return rc;
	_tracker_loaded = 1;
	as_put32(_packet, 1u);                        /* 1 = play */
	return command(AS_TRACKER_CONTROL, _packet, 4);
}

/* ---- API Mixer ------------------------------------------------------------ */
Mixer::Mixer(FileSystem *fs, SystemStub *stub)
	: _fs(fs), _stub(stub), _premixHook(0), _premixHookData(0),
	  _backgroundMusicType(MT_NONE), _musicType(MT_NONE),
	  _cpc(this, fs), _mod(this, fs), _ogg(this, fs), _sfx(this), _musicTrack(-1) {
	memset(_channels, 0, sizeof(_channels));
}
void Mixer::init()
{
	connect();
#if FLASHBK_AUDIO_SELFTEST
	marker_file("INIT");
	/* Test end-to-end (tylko build testowy): krotki ton 441 Hz wgrany i zagrany
	   od razu po polaczeniu - dowod, ze cala sciezka dziala, bez czekania na
	   efekt w grze.  Bufor jest statyczny, bo cache sampli kluczuje po wskazniku. */
	{
		static uint8_t tone[4410];                 /* 200 ms @ 22050 Hz, 8-bit */
		for (unsigned i = 0; i < sizeof tone; ++i) {
			tone[i] = (uint8_t)(int8_t)(((i / 25u) & 1u) ? 100 : -100);
		}
		play(tone, sizeof tone, 22050, 48);
		/* Test przejmowania glosow: 24 efekty naraz przy 16 glosach urzadzenia.
		   Bez polityki "victim" urzadzenie odrzuca nadmiarowe jako AS_E_BUSY. */
		{
			static uint8_t burst[24][1103];            /* 50 ms @ 22050 Hz */
			marker_file("PREB");                       /* stan przed testem glosow */
			for (unsigned k = 0; k < 24u; ++k) {
				for (unsigned i = 0; i < sizeof burst[k]; ++i)
					burst[k][i] = (uint8_t)(int8_t)(((i / (3u + (k & 7u))) & 1u) ? 90 : -90);
				play(burst[k], (uint32_t)sizeof burst[k], 22050, 40);
			}
			marker_file("BRST");                       /* played/dropped po tescie */
		}
		marker_file("DONE");
	}
#endif
}
void Mixer::free()
{
	if (_connected) (void)command(AS_RESET, 0, 0);
	marker_file("EXIT");
	disconnect();
}
void Mixer::setPremixHook(PremixHook premixHook, void *userData)
{
	_premixHook = premixHook;
	_premixHookData = userData;
}
uint32_t Mixer::getSampleRate() const { return 22050; }
void Mixer::mix(int16_t *buf, int len) { memset(buf, 0, len * sizeof(int16_t)); }
void Mixer::mixCallback(void *, int16_t *buf, int len) { memset(buf, 0, len * sizeof(int16_t)); }
void Mixer::stopAll()
{
	if (!_connected) return;
	for (int i = 0; i < SFX_SLOTS; ++i) {
		if (!_slot[i].voice) continue;
		uint8_t p[16] = {0};
		as_put32(p, _slot[i].voice); as_put64(p + 8, AS_NOW);
		(void)command(AS_STOP, p, sizeof p);
		_slot[i].voice = 0; _slot[i].end_tick = 0;
	}
}
bool Mixer::isPlaying(const uint8_t *data) const
{
	if (!data) return false;
	uint32_t now = atos_hires();
	for (int i = 0; i < SFX_SLOTS; ++i) {
		if (_slot[i].data != data) continue;
		return _slot[i].voice && (int32_t)(now - _slot[i].end_tick) < 0;
	}
	return false;
}
void Mixer::play(const uint8_t *data, uint32_t len, uint16_t freq, uint8_t volume)
{
#if FLASHBK_AUDIO_SELFTEST
	if (!_connected) { marker_file("NOCN"); return; }
	if (!data || !len) { marker_file("NOARG"); return; }
#endif
	if (!_connected || !data || !len) return;
	int slot = -1, spare = -1;
	for (int i = 0; i < SFX_SLOTS; ++i) {
		if (_slot[i].data == data) { slot = i; break; }
		if (!_slot[i].data && spare < 0) spare = i;
	}
	if (slot < 0) {
		if (spare < 0) {
#if FLASHBK_AUDIO_SELFTEST
			marker_file("NSLT");
#endif
			return; }
		slot = spare;
		uint32_t frames = 0, rate = 0;
		uint32_t handle = upload_sample(data, len, freq, &frames, &rate);
		if (!handle) { ++flashbk_audio_dropped; flashbk_audio_error = AS_E_HANDLE; marker_file("FAIL"); return; }
		_slot[slot].data = data; _slot[slot].handle = handle;
		_slot[slot].frames = frames; _slot[slot].rate = rate;
		++flashbk_audio_loaded;
	}
	uint8_t p[32] = {0};
	as_put32(p, _slot[slot].handle);             /* +0 uchwyt sampla */
	/* +4 = 0: rate bierz z sampla (tak jak PRE2) */
	as_put64(p + 8, AS_NOW);
	as_put16(p + 16, (uint16_t)(volume >= 64u ? 65535u : (uint32_t)volume * 1024u));
	/* +20 flagi petli, +24/+28 lo/hi.  Urzadzenie odrzuca (AS_E_ARGUMENT)
	   NIEZEROWE lo/hi przy flagach == 0 - dlatego efekt jednorazowy zostawia
	   je wyzerowane, a dlugosc wynika z sampla (PRE2 robi tak samo). */
	int rc = command(AS_PLAY, p, sizeof p);
	if (rc == AS_E_BUSY || rc == AS_E_LIMIT) {
		/* Wszystkie 16 glosow urzadzenia zajete.  Bez tego czesc efektow w ogole
		   nie brzmiala: w dlugiej sesji gubilo sie 506 z ~3050 dzwiekow (17%).
		   Zatrzymujemy glos, ktory konczy sie najwczesniej, i probujemy raz jeszcze. */
		int victim = -1;
		for (int i = 0; i < SFX_SLOTS; ++i) {
			if (!_slot[i].voice) continue;
			if (victim < 0 || (int32_t)(_slot[i].end_tick - _slot[victim].end_tick) < 0) victim = i;
		}
		if (victim >= 0) {
			uint8_t s[16] = {0};
			as_put32(s, _slot[victim].voice); as_put64(s + 8, AS_NOW);
			(void)command(AS_STOP, s, sizeof s);
			_slot[victim].voice = 0; _slot[victim].end_tick = 0;
			rc = command(AS_PLAY, p, sizeof p);
		}
	}
	if (rc != AS_OK) {
		flashbk_audio_error = rc; ++flashbk_audio_dropped;
		if (rc < 0) { _connected = 0; marker_file("IO"); }
		return;
	}
	_slot[slot].voice = as_get32(_reply);
	_slot[slot].end_tick = atos_hires() +
	    (uint32_t)(((uint64_t)_slot[slot].frames * HIRES_TICKS) / (_slot[slot].rate ? _slot[slot].rate : AS_MIN_RATE));
	++flashbk_audio_played;
	/* Dowod dla testu: pierwszy zaakceptowany efekt zostawia slad na dysku,
	   nawet gdyby gre ubila zamknieciem okna (wtedy Mixer::free() nie zadziala). */
	if (flashbk_audio_played == 1u) marker_file("PLAY");
}
void Mixer::playMusic(int num, int tempo)
{
	(void)tempo;
	_musicTrack = num;
	if (!_connected || !(_caps & AS_CAP_TRACKER) || num < 0) return;
	int track = num;
	if (num >= 1000) track = _level_tracks[(unsigned)(num - 1000) % (sizeof _level_tracks / sizeof _level_tracks[0])];
	char path[64];
	snprintf(path, sizeof path, MODULE_PATTERN, track);
	int rc = tracker_song(path, _fs);            /* brak pliku = cicho, bez bledu gry */
	if (rc == AS_OK) { if (!_music_ok) { _music_ok = 1; marker_music("MUS", track, path, rc); } }
	else if (rc == AS_E_HANDLE) { if (!_music_missing) { _music_missing = 1; marker_music("MISS", track, path, rc); } }
	else { flashbk_audio_error = rc; marker_music("MBAD", track, path, rc); }
}
void Mixer::stopMusic()
{
	if (_connected && _tracker_loaded) {
		uint8_t p[4]; as_put32(p, 2u);           /* 2 = pauza */
		(void)command(AS_TRACKER_CONTROL, p, 4);
		_tracker_loaded = 0;
	}
	_musicTrack = -1;
}

/* Zrzuty ekranu (Ctrl+S w wersji SDL) nie sa obslugiwane. */
void saveBMP(const char *, const uint8_t *, const uint8_t *, int, int) {}
