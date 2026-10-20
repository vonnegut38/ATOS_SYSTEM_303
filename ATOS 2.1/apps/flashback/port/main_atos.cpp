// FLASHBK.APP entry point (replaces main.cpp: no SDL, no getopt, no stat).
//
//   FLASHBK [--datapath=\GAMES\FLASHBK\DATA] [--levelnum=N] [--language=EN]
//           [--cutscenes] [--debug]
//   FLASHBK --selftest      screen/palette/input test, no game data needed
//
// Without --datapath the data directory is searched in: DATA (relative to the
// working directory), \GAMES\FLASHBK\DATA, \FLASHBK\DATA, \FLASHBK.
// FLASHBK.CFG in the data directory uses the REminiscence option names plus
// skip_cutscenes=true|false and joy_fire=shift|space|enter.
#include <ctype.h>
#include "file.h"
#include "fs.h"
#include "game.h"
#include "intern.h"
#include "systemstub.h"
#include "util.h"
#include "atos_sys.h"
#include "atos_port.h"

extern SystemStub *SystemStub_ATOS_create();
extern int g_atosJoyFire;
extern int atos_selftest(void);

Options g_options;
const char *g_caption = "Flashback";

static int detectVersion(FileSystem *fs) {
	static const struct {
		const char *filename;
		int type;
		const char *name;
	} table[] = {
		{ "DEMO_UK.ABA", kResourceTypeDOS, "DOS (Demo)" },
		{ "GLOB_FR.ABA", kResourceTypeDOS, "DOS" },
		{ "INTRO.SEQ", kResourceTypeDOS, "DOS CD" },
		{ "MENU1SSI.MAP", kResourceTypeDOS, "DOS SSI" },
		{ "LEVEL1.MAP", kResourceTypeDOS, "DOS" },
		{ "LEVEL1.BNQ", kResourceTypeDOS, "DOS (Demo)" },
		{ "LEVEL1.LEV", kResourceTypeAmiga, "Amiga" },
		{ "DEMO.LEV", kResourceTypeAmiga, "Amiga (Demo)" },
		{ 0, -1, 0 }
	};
	for (int i = 0; table[i].filename; ++i) {
		if (fs->exists(table[i].filename)) {
			debug(DBG_INFO, "Detected %s version", table[i].name);
			return table[i].type;
		}
	}
	return -1;
}

static Language detectLanguage(FileSystem *fs) {
	static const struct {
		const char *filename;
		Language language;
	} table[] = {
		{ "GLOB_FR.ABA", LANG_FR },
		{ "ENGCINE.TXT", LANG_EN },
		{ "FR_CINE.TXT", LANG_FR },
		{ "RUSCINE.TXT", LANG_RU },
		{ "GERCINE.TXT", LANG_DE },
		{ "SPACINE.TXT", LANG_SP },
		{ "ITACINE.TXT", LANG_IT },
		{ "FRCINE.TXT", LANG_FR },
		{ 0, LANG_EN }
	};
	for (int i = 0; table[i].filename; ++i) {
		if (fs->exists(table[i].filename)) {
			return table[i].language;
		}
	}
	return LANG_EN;
}

static void initOptions(FileSystem *fs) {
	memset(&g_options, 0, sizeof(g_options));
	g_options.bypass_protection = true;
	g_options.enable_password_menu = true;
	g_options.fade_out_palette = true;
	g_options.use_seq_cutscenes = true;
	g_options.restore_memo_cutscene = true;
	g_options.skip_cutscenes = true; // test build: polygon cutscenes off by default
	static const struct {
		const char *name;
		bool *value;
	} opts[] = {
		{ "bypass_protection", &g_options.bypass_protection },
		{ "enable_password_menu", &g_options.enable_password_menu },
		{ "enable_language_selection", &g_options.enable_language_selection },
		{ "fade_out_palette", &g_options.fade_out_palette },
		{ "use_tile_data", &g_options.use_tile_data },
		{ "use_text_cutscenes", &g_options.use_text_cutscenes },
		{ "use_seq_cutscenes", &g_options.use_seq_cutscenes },
		{ "use_words_protection", &g_options.use_words_protection },
		{ "use_white_tshirt", &g_options.use_white_tshirt },
		{ "play_asc_cutscene", &g_options.play_asc_cutscene },
		{ "play_caillou_cutscene", &g_options.play_caillou_cutscene },
		{ "play_metro_cutscene", &g_options.play_metro_cutscene },
		{ "play_serrure_cutscene", &g_options.play_serrure_cutscene },
		{ "play_carte_cutscene", &g_options.play_carte_cutscene },
		{ "restore_memo_cutscene", &g_options.restore_memo_cutscene },
		{ "order_inventory_original", &g_options.order_inventory_original },
		{ "skip_cutscenes", &g_options.skip_cutscenes },
		{ 0, 0 }
	};
	char *path = fs->findPath("FLASHBK.CFG");
	if (!path) {
		return;
	}
	FILE *fp = fopen(path, "rb");
	free(path);
	if (!fp) {
		return;
	}
	char buf[128];
	while (fgets(buf, sizeof(buf), fp)) {
		if (buf[0] == '#') {
			continue;
		}
		const char *p = strchr(buf, '=');
		if (!p) {
			continue;
		}
		++p;
		while (*p && isspace(*p)) {
			++p;
		}
		if (!*p) {
			continue;
		}
		if (strncmp(buf, "joy_fire", 8) == 0) {
			g_atosJoyFire = (*p == 's' && p[1] == 'p') ? 1 : (*p == 'e') ? 2 : 0;
			continue;
		}
		const bool value = (*p == 't' || *p == 'T' || *p == '1');
		bool found = false;
		for (int i = 0; opts[i].name; ++i) {
			if (strncmp(buf, opts[i].name, strlen(opts[i].name)) == 0) {
				*opts[i].value = value;
				found = true;
				break;
			}
		}
		if (!found) {
			warning("Unhandled option '%s', ignoring", buf);
		}
	}
	fclose(fp);
}

static const char *value(const char *arg, const char *name) {
	const size_t n = strlen(name);
	if (strncmp(arg, name, n) == 0 && arg[n] == '=') {
		return arg + n + 1;
	}
	return 0;
}

static bool hasGameData(const char *dir) {
	FileSystem fs(dir);
	return detectVersion(&fs) != -1;
}

extern "C" int atos_main(void) {
	static char args[256];
	static char dataPath[96];
	int levelNum = 0;
	int forcedLanguage = -1;
	int cutscenes = -1;
	bool debugLog = false;
	dataPath[0] = 0;

	if (atos_call(ATOS_ABI_VERSION, 0, 0, 0, 0) < 0x00010002) {
		atos_puts("FLASHBACK: this kernel is older than ATOS ABI 1.2 (game mode); install the patched KERNEL.SYS\n");
		return 1;
	}
	if (atos_call(ATOS_ARGS, sizeof(args), 0, args, 0) < 0) {
		args[0] = 0;
	}
	// split on spaces
	char *argv[16];
	int argc = 0;
	for (char *p = args; *p && argc < 16; ) {
		while (*p == ' ') ++p;
		if (!*p) break;
		argv[argc++] = p;
		while (*p && *p != ' ') ++p;
		if (*p) *p++ = 0;
	}
	for (int i = 0; i < argc; ++i) {
		const char *v;
		if ((v = value(argv[i], "--datapath")) != 0) {
			strncpy(dataPath, v, sizeof(dataPath) - 1);
		} else if ((v = value(argv[i], "--levelnum")) != 0) {
			levelNum = atoi(v);
		} else if ((v = value(argv[i], "--language")) != 0) {
			static const char *names[] = { "FR", "EN", "DE", "SP", "IT", "JP", "RU", 0 };
			for (int k = 0; names[k]; ++k) {
				if (strcasecmp(names[k], v) == 0) {
					forcedLanguage = k;
				}
			}
		} else if (strcmp(argv[i], "--cutscenes") == 0) {
			cutscenes = 1;
		} else if (strcmp(argv[i], "--selftest") == 0) {
			return atos_selftest();
		} else if (strcmp(argv[i], "--debug") == 0) {
			debugLog = true;
		} else if (argv[i][0] != '-') {
			strncpy(dataPath, argv[i], sizeof(dataPath) - 1);
		} else {
			atos_puts("usage: FLASHBK [--datapath=DIR] [--levelnum=N] [--language=EN] [--cutscenes] [--debug] | --selftest\n");
			return 0;
		}
	}
	SystemStub *stub = SystemStub_ATOS_create();
	stub->init(g_caption, 256, 224, true, kWidescreenNone, 0);
	atos_loading("FINDING GAME DATA", 5);
	if (!dataPath[0]) {
		static const char *candidates[] = { "DATA", "\\GAMES\\FLASHBK\\DATA", "\\FLASHBK\\DATA", "\\FLASHBK", 0 };
		for (int k = 0; candidates[k]; ++k) {
			if (hasGameData(candidates[k])) {
				strcpy(dataPath, candidates[k]);
				break;
			}
		}
	}
	g_debugMask = DBG_INFO;
	if (!dataPath[0] || !hasGameData(dataPath)) {
		atos_puts("FLASHBACK: no game data found.  Copy the PC DOS Flashback data (LEVEL1.*, GLOBAL.*, PERSO.*, FB_TXT.FNT, ...)\n");
		atos_puts("           to \\GAMES\\FLASHBK\\DATA or pass --datapath=DIR\n");
		atos_puts("           Starting the screen/joystick test instead (hold ESC to leave).\n");
		stub->destroy();
		return atos_selftest();
	}
	{
		char logPath[96];
		snprintf(logPath, sizeof(logPath), "%s\\FLASHBK.LOG", dataPath);
		if (debugLog) {
			atos_log_to(logPath);
			g_debugMask |= DBG_FILE | DBG_GAME | DBG_RES;
		}
	}
	atos_loading("CONFIGURATION", 15);
	FileSystem fs(dataPath);
	initOptions(&fs);
	if (cutscenes == 1) {
		g_options.skip_cutscenes = false;
	}
	const int version = detectVersion(&fs);
	if (version != kResourceTypeDOS) {
		warning("Only the PC DOS data set is tested on ATOS");
	}
	const Language language = (forcedLanguage == -1) ? detectLanguage(&fs) : (Language)forcedLanguage;
	{
		char b[96];
		snprintf(b, sizeof(b), "FLASHBACK: data %s, heap %u KB\n", dataPath, (unsigned)(atos_heap_size() >> 10));
		atos_puts(b);
	}
	atos_loading("GAME ENGINE", 25);
	Game *g = new Game(stub, &fs, &fs, dataPath, levelNum, (ResourceType)version, language, kWidescreenNone, false, 0);
	stub->setScreenSize(g->_vid._w, g->_vid._h);
	g->run();
	delete g;
	stub->destroy();
	{
		char b[64];
		snprintf(b, sizeof(b), "FLASHBACK: bye (heap peak %u KB)\n", (unsigned)(atos_heap_peak() >> 10));
		atos_puts(b);
	}
	return 0;
}
