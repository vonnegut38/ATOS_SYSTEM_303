// FLASHBK --selftest: exercises the whole ATOS path without game data.
//
// 640x480x256 game mode, 256-entry palette upload, partial and full frame
// updates (2x), keyboard and joystick input, fade, session switch recovery.
// A 16x16 block follows the arrow keys / joystick; the eight indicator boxes
// on top light up for UP DOWN LEFT RIGHT SHIFT/FIRE SPACE ENTER BACKSPACE.
// ESC (held) or Alternate+X ends the test; timings are printed afterwards.
#include "systemstub.h"
#include "util.h"
#include "atos_sys.h"
#include "atos_port.h"

extern SystemStub *SystemStub_ATOS_create();

static const int W = 256, H = 224;

static void fill(uint8_t *buf, int x, int y, int w, int h, uint8_t c) {
	for (int j = 0; j < h; ++j) {
		if (y + j < 0 || y + j >= H) continue;
		for (int i = 0; i < w; ++i) {
			if (x + i >= 0 && x + i < W) buf[(y + j) * W + x + i] = c;
		}
	}
}

static uint8_t background(int x, int y) {
	// 16 colour bars, each with a 16-step ramp downwards (palette 16..255)
	return (uint8_t)(16 + ((x / 16) * 15 + (y * 15) / H) % 240);
}

int atos_selftest(void) {
	uint8_t *buf = (uint8_t *)malloc(W * H);
	if (!buf) {
		atos_puts("SELFTEST: no memory\n");
		return 1;
	}
	SystemStub *stub = SystemStub_ATOS_create();
	stub->init("selftest", W, H, true, 0, 0);
	uint8_t pal[256 * 3];
	for (int i = 0; i < 256; ++i) {
		// 0..15 fixed UI colours, 16..255 = 16 hues x 15 brightness steps
		if (i < 16) {
			static const uint8_t ui[16][3] = {
				{0,0,0},{255,255,255},{255,0,0},{0,255,0},{0,0,255},{255,255,0},{0,255,255},{255,0,255},
				{64,64,64},{128,128,128},{192,192,192},{255,128,0},{128,0,255},{0,128,64},{128,64,0},{32,32,96}
			};
			pal[i * 3] = ui[i][0]; pal[i * 3 + 1] = ui[i][1]; pal[i * 3 + 2] = ui[i][2];
		} else {
			const int hue = (i - 16) / 15, lum = 2 + (i - 16) % 15;
			pal[i * 3] = (uint8_t)(((hue & 1) ? 255 : (hue & 8) ? 128 : 40) * lum / 16);
			pal[i * 3 + 1] = (uint8_t)(((hue & 2) ? 255 : (hue & 8) ? 96 : 40) * lum / 16);
			pal[i * 3 + 2] = (uint8_t)(((hue & 4) ? 255 : (hue & 8) ? 64 : 40) * lum / 16);
		}
	}
	stub->setPalette(pal, 256);
	for (int y = 0; y < H; ++y) {
		for (int x = 0; x < W; ++x) {
			buf[y * W + x] = background(x, y);
		}
	}
	stub->copyRect(0, 0, W, H, buf, W);
	stub->updateScreen(0);

	int bx = W / 2 - 8, by = H / 2 - 8;
	uint32_t frames = 0, fullFrames = 0, fullMs = 0, partialMs = 0, partial = 0;
	uint32_t start = stub->getTimeStamp(), lastFull = start, escapeSince = 0;
	bool fadeArmed = true;
	while (!stub->_pi.quit) {
		const uint32_t t0 = stub->getTimeStamp();
		stub->processEvents();
		const PlayerInput &pi = stub->_pi;
		// erase the block, move, redraw
		for (int y = by; y < by + 16; ++y) {
			for (int x = bx; x < bx + 16; ++x) {
				if (x >= 0 && x < W && y >= 0 && y < H) buf[y * W + x] = background(x, y);
			}
		}
		stub->copyRect(bx, by, 16, 16, buf, W);
		if (pi.dirMask & PlayerInput::DIR_LEFT) bx -= 3;
		if (pi.dirMask & PlayerInput::DIR_RIGHT) bx += 3;
		if (pi.dirMask & PlayerInput::DIR_UP) by -= 3;
		if (pi.dirMask & PlayerInput::DIR_DOWN) by += 3;
		bx = CLIP(bx, 0, W - 16);
		by = CLIP(by, 24, H - 16);
		fill(buf, bx, by, 16, 16, (uint8_t)(pi.shift ? 2 : pi.space ? 3 : pi.enter ? 5 : 1));
		stub->copyRect(bx, by, 16, 16, buf, W);
		const bool state[8] = {
			(pi.dirMask & PlayerInput::DIR_UP) != 0, (pi.dirMask & PlayerInput::DIR_DOWN) != 0,
			(pi.dirMask & PlayerInput::DIR_LEFT) != 0, (pi.dirMask & PlayerInput::DIR_RIGHT) != 0,
			pi.shift, pi.space, pi.enter, pi.backspace
		};
		for (int k = 0; k < 8; ++k) {
			fill(buf, 8 + k * 30, 4, 24, 14, (uint8_t)(state[k] ? 3 : 8));
			fill(buf, 10 + k * 30, 6, 20, 10, (uint8_t)(state[k] ? 13 : 0));
		}
		stub->copyRect(0, 0, W, 22, buf, W);
		stub->_pi.backspace = false;
		// every 3 s: repaint everything to measure a full 512x448 update
		const bool full = (t0 - lastFull) > 3000;
		if (full) {
			stub->copyRect(0, 0, W, H, buf, W);
			lastFull = t0;
		}
		const uint32_t u0 = stub->getTimeStamp();
		stub->updateScreen(0);
		const uint32_t u1 = stub->getTimeStamp();
		if (full) { fullMs += u1 - u0; ++fullFrames; } else { partialMs += u1 - u0; ++partial; }
		++frames;
		if (pi.enter && fadeArmed) {
			fadeArmed = false;
			stub->fadeScreen();
			stub->copyRect(0, 0, W, H, buf, W);
			stub->updateScreen(0);   // fades out
			stub->sleep(300);
			stub->copyRect(0, 0, W, H, buf, W);
			stub->updateScreen(0);   // picture back, palette restored
		} else if (!pi.enter) {
			fadeArmed = true;
		}
		if (pi.escape) {
			if (!escapeSince) escapeSince = t0;
			if (t0 - escapeSince > 700) break;
		} else {
			escapeSince = 0;
		}
		const int32_t spent = (int32_t)(stub->getTimeStamp() - t0);
		if (spent < 33) stub->sleep(33 - spent);
	}
	const uint32_t total = stub->getTimeStamp() - start;
	stub->destroy();
	char line[160];
	snprintf(line, sizeof(line), "SELFTEST: %u frames in %u ms (%u fps); partial update avg %u ms, full 512x448 update avg %u ms (%u samples)\n",
		(unsigned)frames, (unsigned)total, (unsigned)(total ? frames * 1000 / total : 0),
		(unsigned)(partial ? partialMs / partial : 0), (unsigned)(fullFrames ? fullMs / fullFrames : 0), (unsigned)fullFrames);
	atos_puts(line);
	return 0;
}
