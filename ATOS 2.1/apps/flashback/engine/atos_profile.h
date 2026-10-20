/* ATOS frame profiler hooks (implemented in port/systemstub_atos.cpp).
   The kernel's 12288 Hz counter is used, so phases shorter than the 1 ms game
   clock still measure.  Phases: 0 layer memcpy, 1 input+prepare, 2 collision
   prep, 3 object loop, 4 rest of mainLoop, 5 whole mainLoop, 6 Cutscene::play,
   7 Game::playCutscene.  Counters 0..7 count entries into the frame drivers. */
#ifndef ATOS_PROFILE_H
#define ATOS_PROFILE_H

#if ATOS_PROFILE
extern "C" unsigned atos_profile_now(void);
extern "C" void atos_profile_span(int which, unsigned from, unsigned to);
extern "C" void atos_profile_count(int which);

#else
static inline unsigned atos_profile_now() { return 0; }
static inline void atos_profile_span(int, unsigned, unsigned) {}
static inline void atos_profile_count(int) {}
#endif

namespace {
struct AtosSpan {
    int which; unsigned t0;
    explicit AtosSpan(int w) : which(w), t0(atos_profile_now()) {}
    ~AtosSpan() { atos_profile_span(which, t0, atos_profile_now()); }
};
}

#endif
