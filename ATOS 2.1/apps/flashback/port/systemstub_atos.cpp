// SystemStub for ATOS on an ET4000 (VOFA) card: the current 8 bpp screen,
// the 256x224 game frame doubled to 512x448 and centred, keyboard and
// joystick through the ATOS key stream, no audio.
//
// Screen: GFX opcode 38 (include/atos_game.h).  copyRect() fills a private
// 8 bpp frame and marks 8x8 blocks; updateScreen() turns the marked blocks
// into at most ATOS_GAME_RECTS rectangles (one FRAME request) and uploads the
// palette first when it changed, so the kernel only doubles and flushes what
// the engine actually redrew.
#include "systemstub.h"
#include "util.h"
#include "atos_game.h"
#include "atos_sys.h"
#include "atos_port.h"
#include "font8.h"

enum {
	KKEY = 0x4b4b4559u,
	KMOU = 0x4b4d4f55u,
	GUI_BEGIN_KEYS = 6,
	GUI_END_KEYS = 7,
	kScreenW = 640,
	kScreenH = 480,
	kBlock = 8,
	kMaxW = 320,
	kMaxH = 240,
	kBlocksW = kMaxW / kBlock,
	kBlocksH = kMaxH / kBlock,
	kPending = 32,
};

// Atari IKBD scan codes used by the game.
enum {
	SC_ESC = 0x01, SC_BACKSPACE = 0x0e, SC_TAB = 0x0f, SC_RETURN = 0x1c, SC_CTRL = 0x1d,
	SC_LSHIFT = 0x2a, SC_RSHIFT = 0x36, SC_ALT = 0x38, SC_SPACE = 0x39,
	SC_UP = 0x48, SC_LEFT = 0x4b, SC_RIGHT = 0x4d, SC_DOWN = 0x50,
	SC_KP_7 = 0x67, SC_KP_8 = 0x68, SC_KP_9 = 0x69, SC_KP_4 = 0x6a, SC_KP_6 = 0x6c,
	SC_KP_1 = 0x6d, SC_KP_2 = 0x6e, SC_KP_3 = 0x6f, SC_KP_ENTER = 0x72,
	SC_X = 0x2d, SC_Q = 0x10, SC_F = 0x21,
};

// Joystick fire mapping, chosen in FLASHBK.CFG (joy_fire=shift|space|enter).
int g_atosJoyFire = 0; // 0 shift, 1 space, 2 enter

/* Per-frame phase times [ms] from the engine's profiling lines:
   0 = layer memcpy, 1 = input+prepare, 2 = collision prep, 3 = object loop. */
#if ATOS_PROFILE
static uint32_t g_fb_phase[16];
static uint32_t g_fb_count[16];
#endif

struct SystemStub_ATOS : SystemStub {
	int32_t _gfxPid, _guiPid;
	bool _keysOwned, _gameOn, _hidden, _loading;
	int _w, _h;              // game frame size
	int _modeW, _modeH;      // card mode
	int _scale, _dstX, _dstY;
	uint8_t *_screen;
	uint8_t *_black;         // zero rows for clearing bands
	atos_game_frame_t *_frame;
	atos_game_palette_t *_palMsg;
	uint8_t _pal[256 * 3];
	int _palLo, _palHi;      // dirty palette range, _palLo > _palHi = clean
	bool _fullDirty, _fadePending, _paletteAfterFrame;
	int _lastShake;
	uint8_t _blocks[kBlocksH][kBlocksW];
	atos_message_t _pending[kPending];
	unsigned _pendHead, _pendTail;
	uint32_t _lastHires, _msAcc, _ms;
	bool _alt, _ctrl;
	uint8_t _joyDirs, _keyDirs;

	SystemStub_ATOS()
		: _gfxPid(-1), _guiPid(-1), _keysOwned(false), _gameOn(false), _hidden(false), _loading(false),
		  _w(0), _h(0), _modeW(0), _modeH(0), _scale(2), _dstX(0), _dstY(0),
		  _screen(0), _black(0), _frame(0), _palMsg(0), _palLo(256), _palHi(-1),
		  _fullDirty(true), _fadePending(false), _paletteAfterFrame(false), _lastShake(0),
		  _pendHead(0), _pendTail(0), _lastHires(0), _msAcc(0), _ms(0),
		  _alt(false), _ctrl(false), _joyDirs(0), _keyDirs(0) {
		memset(&_pi, 0, sizeof(_pi));
		memset(_pal, 0, sizeof(_pal));
		memset(_blocks, 0, sizeof(_blocks));
	}
	virtual ~SystemStub_ATOS() {}

	// ---------------------------------------------------------------- IPC
	/* Called by the engine's per-frame profiling lines (see engine/game.cpp):
	   0 = layer memcpy, 1 = input+prepare, 2 = collision prep, 3 = object loop. */

	void queue(const atos_message_t &m) {
		if (_pendTail - _pendHead < (unsigned)kPending) {
			_pending[_pendTail++ % kPending] = m;
		}
	}
	int32_t call(int32_t pid, uint32_t op, uint32_t a, uint32_t b, uint32_t c) {
		if (pid < 0 || atos_send((uint32_t)pid, op, a, b, c) != 0) {
			return -1;
		}
		for (;;) {
			atos_message_t m;
			if (atos_receive(&m, 1) != 0) {
				return -1;
			}
			if (m.sender == (uint32_t)pid) {
				return (int32_t)m.v[1];
			}
			queue(m);
		}
	}
	int32_t game(uint32_t sub, uint32_t xy) {
		return call(_gfxPid, ATOS_GAME_OPCODE, xy, 0, sub);
	}

	// ------------------------------------------------------------ display
	void layout() {
		_scale = (_modeW >= _w * 2 && _modeH >= _h * 2) ? 2 : 1;
		_dstX = ((_modeW - _w * _scale) / 2) & ~1;
		_dstY = (_modeH - _h * _scale) / 2;
	}

	bool beginScreen() {
		// Keep an existing 256-colour desktop mode. The game still draws only
		// its 512x448 viewport, so a larger border costs no per-frame transfer.
		// A temporary WM resize otherwise reinterprets live desktop surfaces
		// at the game pitch and corrupts their contents on return.
		int32_t r = game(ATOS_GAME_BEGIN, 0);
		if (r < 0) {
			// A mono desktop needs a genuine 8 bpp mode switch.
			r = game(ATOS_GAME_BEGIN, ((uint32_t)kScreenW << 16) | kScreenH);
		}
		if (r < 0) {
			return false;
		}
		_modeW = (int)((uint32_t)r >> 16);
		_modeH = (int)((uint32_t)r & 0xFFFF);
		_gameOn = true;
		return true;
	}

	virtual void init(const char *title, int w, int h, bool fullscreen, int widescreenMode, const ScalerParameters *scalerParameters) {
		_gfxPid = atos_lookup("SYSTEM.GFX");
		_guiPid = atos_lookup("SYSTEM.GUI");
		if (_gfxPid < 0) {
			error("SYSTEM.GFX service not running (start the ATOS desktop first)");
		}
		_frame = (atos_game_frame_t *)calloc(1, sizeof(atos_game_frame_t));
		_palMsg = (atos_game_palette_t *)calloc(1, sizeof(atos_game_palette_t));
		_black = (uint8_t *)calloc(1, kMaxW * kBlock);
		if (!_frame || !_palMsg || !_black) {
			error("out of memory for the frame descriptors");
		}
		// Keyboard: become the session's keyboard owner when nobody else is;
		// on a desktop the kernel routes input to the game anyway.
		_keysOwned = (_guiPid >= 0 && call(_guiPid, GUI_BEGIN_KEYS, 0, 0, 0) == 0);
		if (!beginScreen()) {
			error("ET4000 256-colour full-screen mode unavailable (ATOS ABI 1.2 kernel with VOFA/ET4000 required)");
		}
		setScreenSize(w, h);
		_lastHires = atos_hires();
	}

	virtual void destroy() {
		if (_gameOn) {
			game(ATOS_GAME_END, 0);
			_gameOn = false;
		}
		if (_keysOwned) {
			call(_guiPid, GUI_END_KEYS, 0, 0, 0);
			_keysOwned = false;
		}
	}

	virtual bool hasWidescreen() const { return false; }

	virtual void setScreenSize(int w, int h) {
		if (w > kMaxW) w = kMaxW;
		if (h > kMaxH) h = kMaxH;
		if (w == _w && h == _h && _screen) {
			return;
		}
		free(_screen);
		_screen = (uint8_t *)calloc(1, w * h);
		if (!_screen) {
			error("out of memory for the %dx%d screen", w, h);
		}
		_w = w;
		_h = h;
		layout();
		if (_gameOn) {
			game(ATOS_GAME_BEGIN, 0); // same owner: clears the whole screen
		}
		_fullDirty = true;
	}

	// Uses the ordinary validated game-frame path; no private hardware writes
	// and no disk/font dependencies, so it can precede resource discovery.
	void loading(const char *stage, unsigned progress) {
		if (!_gameOn || !_screen) return;
		_loading = true;
		memset(_screen, 0, _w * _h);
		memset(_pal, 0, sizeof(_pal));
		_pal[0] = 7; _pal[1] = 12; _pal[2] = 25;
		_pal[3] = 225; _pal[4] = 235; _pal[5] = 246;
		_pal[6] = 64; _pal[7] = 204; _pal[8] = 220;
		_pal[9] = 32; _pal[10] = 52; _pal[11] = 72;
		loadingText("FLASHBACK", 72, 1);
		loadingText("LOADING", 100, 2);
		loadingText(stage, 146, 1);
		const int x = 32, width = _w - 64;
		if (progress > 100) progress = 100;
		for (int y = 122; y < 128; ++y) {
			memset(_screen + y * _w + x, 3, width);
			memset(_screen + y * _w + x, 2, width * progress / 100);
		}
		_fadePending = _paletteAfterFrame = false;
		_fullDirty = true;
		markPalette(0, 255);
		updateScreen(0);
	}
	void loadingText(const char *s, int y, uint8_t colour) {
		const int n = (int)strlen(s), x = (_w - n * 8) / 2;
		for (int i = 0; i < n; ++i) {
			const uint8_t *glyph = emutos_font8[(uint8_t)s[i]];
			for (int row = 0; row < 8; ++row)
				for (int bit = 0; bit < 8; ++bit) {
					const int px = x + i * 8 + bit, py = y + row;
					if (px >= 0 && px < _w && py >= 0 && py < _h && (glyph[row] & (0x80 >> bit)))
						_screen[py * _w + px] = colour;
				}
		}
	}

	void markPalette(int lo, int hi) {
		if (lo < _palLo) _palLo = lo;
		if (hi > _palHi) _palHi = hi;
	}
	virtual void setPalette(const uint8_t *pal, int n) {
		if (n > 256) n = 256;
		memcpy(_pal, pal, n * 3);
		markPalette(0, n - 1);
	}
	virtual void getPalette(uint8_t *pal, int n) {
		if (n > 256) n = 256;
		memcpy(pal, _pal, n * 3);
	}
	virtual void setPaletteEntry(int i, const Color *c) {
		_pal[i * 3] = c->r;
		_pal[i * 3 + 1] = c->g;
		_pal[i * 3 + 2] = c->b;
		markPalette(i, i);
	}
	virtual void getPaletteEntry(int i, Color *c) {
		c->r = _pal[i * 3];
		c->g = _pal[i * 3 + 1];
		c->b = _pal[i * 3 + 2];
	}
	virtual void setOverscanColor(int i) {}

	void sendPalette(int lo, int hi, int level /* 0..16 */) {
		if (lo > hi || !_gameOn) {
			return;
		}
		_palMsg->first = (uint16_t)lo;
		_palMsg->count = (uint16_t)(hi - lo + 1);
		for (int i = 0; i < _palMsg->count * 3; ++i) {
			_palMsg->rgb[i] = (uint8_t)((_pal[lo * 3 + i] * level) >> 4);
		}
		game(ATOS_GAME_PALETTE, (uint32_t)(uintptr_t)_palMsg);
	}
	void flushPalette() {
		if (_palLo <= _palHi) {
			sendPalette(_palLo, _palHi, 16);
			_palLo = 256;
			_palHi = -1;
		}
	}

	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) {
		if (x < 0) { w += x; buf -= x; x = 0; }
		if (y < 0) { h += y; buf -= y * pitch; y = 0; }
		if (x + w > _w) w = _w - x;
		if (y + h > _h) h = _h - y;
		if (w <= 0 || h <= 0) {
			return;
		}
		_loading = false; // the engine has supplied its first real pixels
		// Engine damage includes unchanged background, inventory and repeated
		// animation poses. Compare in fast RAM before expanding onto the slow
		// card bus. The generic path below handles unaligned/menu rectangles.
		if (!_fullDirty && !((x | y | w | h | pitch | _w) & 7)
		    && !((uintptr_t)buf & 3u)) {
			for (int by = y; by < y + h; by += kBlock) {
				for (int bx = x; bx < x + w; bx += kBlock) {
					const uint8_t *a = buf + by * pitch + bx;
					uint8_t *b = _screen + by * _w + bx;
					bool changed = false;
					for (int j = 0; j < kBlock; ++j) {
						const uint32_t *sa = (const uint32_t *)(a + j * pitch);
						const uint32_t *sb = (const uint32_t *)(b + j * _w);
						if (sa[0] != sb[0] || sa[1] != sb[1]) { changed = true; break; }
					}
					if (!changed) continue;
					for (int j = 0; j < kBlock; ++j) {
						const uint32_t *sa = (const uint32_t *)(a + j * pitch);
						uint32_t *sb = (uint32_t *)(b + j * _w);
						sb[0] = sa[0]; sb[1] = sa[1];
					}
					_blocks[by / kBlock][bx / kBlock] = 1;
				}
			}
			return;
		}
		const uint8_t *src = buf + y * pitch + x;
		uint8_t *dst = _screen + y * _w + x;
		for (int j = 0; j < h; ++j) {
			memcpy(dst, src, w);
			dst += _w;
			src += pitch;
		}
		const int bx1 = (x + w - 1) / kBlock, by1 = (y + h - 1) / kBlock;
		for (int by = y / kBlock; by <= by1; ++by) {
			memset(&_blocks[by][x / kBlock], 1, bx1 - x / kBlock + 1);
		}
	}
	virtual void copyRectRgb24(int x, int y, int w, int h, const uint8_t *rgb) {
		// Macintosh / CD-i data only; the DOS data used here never calls it.
	}
	virtual void zoomRect(int x, int y, int w, int h) {}
	virtual void copyWidescreenLeft(int w, int h, const uint8_t *buf, bool dark) {}
	virtual void copyWidescreenRight(int w, int h, const uint8_t *buf, bool dark) {}
	virtual void copyWidescreenMirror(int w, int h, const uint8_t *buf) {}
	virtual void copyWidescreenBlur(int w, int h, const uint8_t *buf) {}
	virtual void copyWidescreenCDi(int w, int h, const uint8_t *buf, const uint8_t *pal) {}
	virtual void clearWidescreen() {}
	virtual void enableWidescreen(bool enable) {}

	virtual void fadeScreen() {
		_fadePending = true;
	}

	// Convert the dirty 8x8 blocks into rectangles: horizontal runs per block
	// row, extended downwards while the run below has exactly the same span.
	int collectRects() {
		const int bw = (_w + kBlock - 1) / kBlock, bh = (_h + kBlock - 1) / kBlock;
		int count = 0;
		// Keep the latest run for each x; its bottom edge proves adjacency.
		// This avoids clearing/copying two 160-byte tables on every block row.
		int open[kBlocksW];
		for (int i = 0; i < bw; ++i) open[i] = -1;
		for (int by = 0; by < bh; ++by) {
			for (int bx = 0; bx < bw; ) {
				if (!_blocks[by][bx]) { ++bx; continue; }
				int ex = bx;
				while (ex < bw && _blocks[by][ex]) ++ex;
				const int x = bx * kBlock, w = (ex - bx) * kBlock;
				const int prev = open[bx];
				if (prev >= 0 && _frame->rect[prev].w == w
				    && _frame->rect[prev].y + _frame->rect[prev].h == by * kBlock) {
					_frame->rect[prev].h += kBlock;
				} else {
					if (count == (int)ATOS_GAME_RECTS) {
						return -1;
					}
					atos_game_rect_t &r = _frame->rect[count];
					r.x = (int16_t)x;
					r.y = (int16_t)(by * kBlock);
					r.w = (uint16_t)w;
					r.h = (uint16_t)kBlock;
					open[bx] = count++;
				}
				bx = ex;
			}
		}
		return count;
	}

	// ---- ATOS frame profiler (atos_hires = 12288 Hz) ----
	inline static uint32_t fb_prof_frames, fb_prof_total, fb_prof_transport, fb_prof_max;

	inline static uint32_t fb_prof_last_ts;


	int32_t sendFrame(int count, int shake) {
		_frame->pixels = (uint32_t)(uintptr_t)_screen;
		_frame->width = (uint16_t)_w;
		_frame->height = (uint16_t)_h;
		_frame->pitch = (uint16_t)_w;
		_frame->scale = (uint16_t)_scale;
		_frame->dst_x = (int16_t)_dstX;
		_frame->dst_y = (int16_t)(_dstY + shake * _scale);
		_frame->count = (uint16_t)count;
		_frame->flags = 0;
		return game(ATOS_GAME_FRAME, (uint32_t)(uintptr_t)_frame);
	}

	// Paint `rows` black game rows at screen row y (used after a shake).
	void clearBand(int y, int rows) {
		while (rows > 0) {
			const int n = rows > kBlock ? kBlock : rows;
			_frame->pixels = (uint32_t)(uintptr_t)_black;
			_frame->width = (uint16_t)_w;
			_frame->height = (uint16_t)n;
			_frame->pitch = (uint16_t)_w;
			_frame->scale = (uint16_t)_scale;
			_frame->dst_x = (int16_t)_dstX;
			_frame->dst_y = (int16_t)y;
			_frame->count = 0;
			_frame->flags = 0;
			game(ATOS_GAME_FRAME, (uint32_t)(uintptr_t)_frame);
			y += n * _scale;
			rows -= n;
		}
	}

	void pushPixels(int shakeOffset) {
		int count = 0;
		if (!_fullDirty && shakeOffset == 0 && _lastShake == 0) {
			count = collectRects();
			if (count == 0) {
				return;
			}
			if (count < 0) {
				count = 0; // too fragmented: send the whole frame
			}
		}
		memset(_blocks, 0, sizeof(_blocks));
		#if ATOS_PROFILE
		const uint32_t _tp = atos_hires();
		#endif
		int32_t r = sendFrame(count, shakeOffset);
		#if ATOS_PROFILE
		fb_prof_transport += atos_hires() - _tp;
		#endif
		if (r == ATOS_GAME_FRAME_REPAINT) {
			// we were away (session switch): the screen and our palette were
			// restored by the kernel, now resend everything
			r = sendFrame(0, shakeOffset);
		}
		if (_lastShake > 0 && shakeOffset == 0) {
			clearBand(_dstY + _h * _scale, _lastShake);
		}
		_lastShake = shakeOffset;
		_fullDirty = (r == ATOS_GAME_FRAME_HIDDEN);
		_hidden = (r == ATOS_GAME_FRAME_HIDDEN);
	}

	virtual void updateScreen(int shakeOffset) {
		#if ATOS_PROFILE
		{const uint32_t now = atos_hires();
		 if (fb_prof_last_ts) {
			 const uint32_t total = now - fb_prof_last_ts;
			 fb_prof_total += total;
			 if (total > fb_prof_max) fb_prof_max = total;
			 if (++fb_prof_frames % 300u == 0u) {
				 char b[768]; unsigned n = 0;
				 const char *p1 = "PROF frames=300 avg_ticks=";
				 while (*p1) b[n++] = *p1++;
				 uint32_t v = fb_prof_total / 300u; char d[12]; int k = 0;
				 if (!v) d[k++] = '0';
				 while (v) { d[k++] = (char)('0' + v % 10u); v /= 10u; }
				 while (k) b[n++] = d[--k];
				 p1 = " transport_ticks="; while (*p1) b[n++] = *p1++;
				 v = fb_prof_transport / 300u; k = 0;
				 if (!v) d[k++] = '0';
				 while (v) { d[k++] = (char)('0' + v % 10u); v /= 10u; }
				 while (k) b[n++] = d[--k];
				 p1 = " max_ticks="; while (*p1) b[n++] = *p1++;
				 v = fb_prof_max; k = 0;
				 if (!v) d[k++] = '0';
				 while (v) { d[k++] = (char)('0' + v % 10u); v /= 10u; }
				 while (k) b[n++] = d[--k];
				 for (int q = 0; q < 16; ++q) {
					 p1 = " | f"; while (*p1) b[n++] = *p1++;
					 if (q >= 10) b[n++] = '1';
					 b[n++] = (char)('0' + q % 10); b[n++] = '=';
					 uint32_t v2 = g_fb_phase[q]; k = 0;
					 if (!v2) d[k++] = '0';
					 while (v2) { d[k++] = (char)('0' + v2 % 10u); v2 /= 10u; }
					 while (k) b[n++] = d[--k];
					 g_fb_phase[q] = 0u;
				 }
				 for (int q = 0; q < 16; ++q) {
					 p1 = " | c"; while (*p1) b[n++] = *p1++;
					 if (q >= 10) b[n++] = '1';
					 b[n++] = (char)('0' + q % 10); b[n++] = '=';
					 uint32_t v3 = g_fb_count[q]; k = 0;
					 if (!v3) d[k++] = '0';
					 while (v3) { d[k++] = (char)('0' + v3 % 10u); v3 /= 10u; }
					 while (k) b[n++] = d[--k];
					 g_fb_count[q] = 0u;
				 }
				 b[n++] = '\n'; b[n] = 0;
				 atos_puts(b);
				 fb_prof_total = fb_prof_transport = fb_prof_max = 0u;
			 }
		 }
		 fb_prof_last_ts = now;}
		#endif
		if (!_gameOn) {
			return;
		}
		if (_paletteAfterFrame) {
			// after a fade to black: draw the new picture first, then light it
			pushPixels(shakeOffset);
			_palLo = 0;
			_palHi = 255;
			flushPalette();
			_paletteAfterFrame = false;
			return;
		}
		flushPalette();
		pushPixels(shakeOffset);
		if (_fadePending) {
			_fadePending = false;
			for (int level = 15; level >= 0; --level) {
				sendPalette(0, 255, level);
				sleep(30);
			}
			_paletteAfterFrame = true;
		}
	}

	// ------------------------------------------------------------- input
	void updateDirs() {
		_pi.dirMask = _keyDirs | _joyDirs;
	}
	void setDir(uint8_t &mask, uint8_t bit, bool down) {
		if (down) mask |= bit; else mask &= ~bit;
		updateDirs();
	}
	void fire(int mapping, bool down) {
		switch (mapping) {
		case 1: _pi.space = down; break;
		case 2: _pi.enter = down; break;
		default: _pi.shift = down; break;
		}
	}
	void key(unsigned sc, bool down, unsigned ch) {
		switch (sc) {
		case SC_UP: case SC_KP_8: setDir(_keyDirs, PlayerInput::DIR_UP, down); break;
		case SC_DOWN: case SC_KP_2: setDir(_keyDirs, PlayerInput::DIR_DOWN, down); break;
		case SC_LEFT: case SC_KP_4: setDir(_keyDirs, PlayerInput::DIR_LEFT, down); break;
		case SC_RIGHT: case SC_KP_6: setDir(_keyDirs, PlayerInput::DIR_RIGHT, down); break;
		case SC_KP_7: setDir(_keyDirs, PlayerInput::DIR_UP | PlayerInput::DIR_LEFT, down); break;
		case SC_KP_9: setDir(_keyDirs, PlayerInput::DIR_UP | PlayerInput::DIR_RIGHT, down); break;
		case SC_KP_1: setDir(_keyDirs, PlayerInput::DIR_DOWN | PlayerInput::DIR_LEFT, down); break;
		case SC_KP_3: setDir(_keyDirs, PlayerInput::DIR_DOWN | PlayerInput::DIR_RIGHT, down); break;
		case SC_SPACE: _pi.space = down; break;
		case SC_LSHIFT: case SC_RSHIFT: _pi.shift = down; break;
		case SC_RETURN: case SC_KP_ENTER: _pi.enter = down; break;
		case SC_BACKSPACE: case SC_TAB: if (down) _pi.backspace = true; break;
		case SC_ESC: _pi.escape = down; break;
		case SC_ALT: _alt = down; break;
		case SC_CTRL: _ctrl = down; break;
		case ATOS_KEY_JOY_UP: setDir(_joyDirs, PlayerInput::DIR_UP, down); break;
		case ATOS_KEY_JOY_DOWN: setDir(_joyDirs, PlayerInput::DIR_DOWN, down); break;
		case ATOS_KEY_JOY_LEFT: setDir(_joyDirs, PlayerInput::DIR_LEFT, down); break;
		case ATOS_KEY_JOY_RIGHT: setDir(_joyDirs, PlayerInput::DIR_RIGHT, down); break;
		case ATOS_KEY_JOY_FIRE: fire(g_atosJoyFire, down); break;
		case ATOS_KEY_JOY0_FIRE: fire(g_atosJoyFire == 1 ? 0 : 1, down); break;
		default: break;
		}
		if (!down) {
			return;
		}
		if (_alt && (sc == SC_X || sc == SC_Q)) {
			_pi.quit = true; // Alternate+X / Alternate+Q: leave immediately
			return;
		}
		if (_ctrl && sc == SC_F) {
			_pi.dbgMask ^= PlayerInput::DF_FASTMODE;
		}
		// password / protection screens
		if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
		if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ') {
			_pi.lastChar = (char)ch;
		} else {
			_pi.lastChar = 0;
		}
	}
	void handle(const atos_message_t &m) {
		if (m.sender != 0) {
			return; // a late service reply
		}
		if (m.v[0] == KKEY) {
			const unsigned type = (m.v[1] >> 24) & 0xFF;
			key(m.v[1] & 0x7F, type == 1, m.v[2] & 0xFF);
		}
		// KMOU (mouse) is ignored: the game is keyboard/joystick only
	}
	virtual void processEvents() {
		for (;;) {
			while (_pendHead != _pendTail) {
				handle(_pending[_pendHead++ % kPending]);
			}
			atos_message_t m;
			while (atos_receive(&m, 0) == 0) {
				handle(m);
			}
			if (!_hidden || _pi.quit) {
				break;
			}
			// Another session is on screen: pause, like the SDL build on focus loss.
			atos_sleep_ticks(5);
			pushPixels(0);
		}
	}

	// -------------------------------------------------------------- time
	virtual uint32_t getTimeStamp() {
		const uint32_t now = atos_hires();
		uint32_t d = now - _lastHires;
		_lastHires = now;
		if (d > 1000000u) d = 1000000u;
		_msAcc += d * 1000u;
		_ms += _msAcc / ATOS_HIRES_HZ;
		_msAcc %= ATOS_HIRES_HZ;
		return _ms;
	}
	virtual void sleep(int duration) {
		const uint32_t end = getTimeStamp() + (uint32_t)(duration > 0 ? duration : 0);
		for (;;) {
			const int32_t left = (int32_t)(end - getTimeStamp());
			if (left <= 0) {
				break;
			}
			if (left >= 25) {
				atos_sleep_ticks(1); // 50 Hz scheduler tick, yields the CPU
			}
		}
	}

	// ------------------------------------------------------------- audio
	virtual void startAudio(AudioCallback callback, void *param) {}
	virtual void stopAudio() {}
	virtual uint32_t getOutputSampleRate() { return 22050; }
	virtual void lockAudio() {}
	virtual void unlockAudio() {}
};

static SystemStub_ATOS *g_stub;

SystemStub *SystemStub_ATOS_create() {
	g_stub = new SystemStub_ATOS;
	return g_stub;
}

extern "C" int atos_loading_active(void) {
	return g_stub && g_stub->_loading;
}

extern "C" void atos_loading(const char *stage, unsigned progress) {
	if (g_stub) g_stub->loading(stage, progress);
}

extern "C" void atos_port_shutdown(void) {
	if (g_stub) {
		g_stub->destroy();
	}
}

ScalerParameters ScalerParameters::defaults() {
	ScalerParameters params;
	params.type = kScalerTypeInternal;
	params.name[0] = 0;
	params.factor = 2;
	return params;
}

/* Bridge for the engine's profiling lines (engine/game.cpp).  Values are raw
   atos_hires() ticks: 12288 ticks = 1 s, so ms = ticks / 12.288. */
#if ATOS_PROFILE
extern "C" unsigned atos_profile_now(void);
extern "C" unsigned atos_profile_now(void) { return atos_hires(); }
extern "C" void atos_profile_span(int which, unsigned from, unsigned to);
extern "C" void atos_profile_span(int which, unsigned from, unsigned to) {
	if (which >= 0 && which < 16) g_fb_phase[which] += (to - from);
}
extern "C" void atos_profile_count(int which);
extern "C" void atos_profile_count(int which) {
	if (which >= 0 && which < 16) ++g_fb_count[which];
}

#endif // ATOS_PROFILE
