// Silent audio back end for the first ATOS build ("bez muzyki i efektów").
// Replaces mixer.cpp and the four music players: the engine keeps calling the
// Mixer API, nothing is decoded or mixed, no audio interrupt is installed.
// Swap this file for mixer.cpp + players once a sound driver exists.
#include "mixer.h"
#include "screenshot.h"

CpcPlayer::CpcPlayer(Mixer *mixer, FileSystem *fs) : _mix(mixer), _fs(fs) {}
CpcPlayer::~CpcPlayer() {}
ModPlayer::ModPlayer(Mixer *mixer, FileSystem *fs) : _isAmiga(false), _playing(false), _mix(mixer), _fs(fs), _impl(0) {}
ModPlayer::~ModPlayer() {}
OggPlayer::OggPlayer(Mixer *mixer, FileSystem *fs) : _mix(mixer), _fs(fs), _impl(0) {}
OggPlayer::~OggPlayer() {}
SfxPlayer::SfxPlayer(Mixer *mixer) : _mod(0), _playing(false), _mix(mixer) {}

Mixer::Mixer(FileSystem *fs, SystemStub *stub)
	: _fs(fs), _stub(stub), _premixHook(0), _premixHookData(0),
	  _backgroundMusicType(MT_NONE), _musicType(MT_NONE),
	  _cpc(this, fs), _mod(this, fs), _ogg(this, fs), _sfx(this), _musicTrack(-1) {
	memset(_channels, 0, sizeof(_channels));
}
void Mixer::init() {}
void Mixer::free() {}
void Mixer::setPremixHook(PremixHook premixHook, void *userData) {
	_premixHook = premixHook;
	_premixHookData = userData;
}
void Mixer::play(const uint8_t *, uint32_t, uint16_t, uint8_t) {}
bool Mixer::isPlaying(const uint8_t *) const { return false; }
uint32_t Mixer::getSampleRate() const { return 22050; }
void Mixer::stopAll() {}
void Mixer::playMusic(int num, int) { _musicTrack = num; }
void Mixer::stopMusic() { _musicTrack = -1; }
void Mixer::mix(int16_t *buf, int len) { memset(buf, 0, len * sizeof(int16_t)); }
void Mixer::mixCallback(void *, int16_t *buf, int len) { memset(buf, 0, len * sizeof(int16_t)); }

// Debug screenshots (Ctrl+S in the SDL build) are not supported.
void saveBMP(const char *, const uint8_t *, const uint8_t *, int, int) {}
