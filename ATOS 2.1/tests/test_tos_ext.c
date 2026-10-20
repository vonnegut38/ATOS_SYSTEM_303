/* Regression test for the shared TOS program extension matcher
 * (include/tos_ext.h).
 *
 * The three launch points (desktop shortcut, file manager, shell command)
 * must agree on what is a TOS program, and that decision must be
 * case-insensitive.  The previous desktop check compared a 24-bit value and
 * the shell had no check at all, so "TEST.PRG" and "test.prg" routed to two
 * different paths.  This locks in the single contract.
 */
#include <stdint.h>
#include <stdio.h>
#include "tos_ext.h"

static int failures;

#define CHECK(cond, ...) do { \
    if (!(cond)) { ++failures; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                   printf(__VA_ARGS__); printf("\n"); } \
} while (0)

int main(void)
{
    /* Upper-case .PRG/.TOS/.TTP/.GTP are TOS programs. */
    CHECK(tos_program_path_ext("C:\\SYSTEM\\TEST\\TOSCOL.PRG"),
          ".PRG should be a TOS program");
    CHECK(tos_program_path_ext("\\APP.TOS"), ".TOS should be a TOS program");
    CHECK(tos_program_path_ext("\\TOOLS\\X.TTP"), ".TTP should be a TOS program");
    CHECK(tos_program_path_ext("\\DESKTOP.GTP"), ".GTP should be a TOS program");

    /* Lower-case must route identically (the 0x202020 fold). */
    CHECK(tos_program_path_ext("\\app.prg"), "lowercase .prg should be a TOS program");
    CHECK(tos_program_path_ext("\\app.tos"), "lowercase .tos should be a TOS program");
    CHECK(tos_program_path_ext("\\app.ttp"), "lowercase .ttp should be a TOS program");
    CHECK(tos_program_path_ext("\\app.gtp"), "lowercase .gtp should be a TOS program");

    /* Mixed case must also match. */
    CHECK(tos_program_path_ext("\\App.PrG"), "mixed .PrG should be a TOS program");
    CHECK(tos_program_path_ext("\\App.ToS"), "mixed .ToS should be a TOS program");

    /* Native / non-TOS extensions stay out of the TOS personality. */
    CHECK(!tos_program_path_ext("\\SYSTEM\\APP\\DESKTOP.APP"),
          ".APP must NOT be a TOS program");
    CHECK(!tos_program_path_ext("\\SYS.ACC"), ".ACC must NOT be a TOS program");
    CHECK(!tos_program_path_ext("\\CONFIG.CFG"), ".CFG must NOT be a TOS program");

    /* A bare name (no dot) is left to the native .APP resolver. */
    CHECK(!tos_program_path_ext("DESKTOP"), "bare name must NOT be a TOS program");
    CHECK(!tos_program_path_ext("\\DIR\\NAME"), "bare path must NOT be a TOS program");

    /* Too-short, empty, or extensionless paths are rejected safely. */
    CHECK(!tos_program_path_ext(""), "empty path must NOT be a TOS program");
    CHECK(!tos_program_path_ext("PRG"), "no-dot name must NOT be a TOS program");
    CHECK(!tos_program_path_ext("AB.PR"), "2-char extension must NOT be a TOS program");
    CHECK(!tos_program_path_ext(0), "NULL path must NOT be a TOS program");

    /* A single-character basename is still a valid TOS program name. */
    CHECK(tos_program_path_ext("X.PRG"), "1-char name .PRG is a TOS program");

    /* A longer extension with a PRG prefix is not a TOS program. */
    CHECK(!tos_program_path_ext("\\X.PRGG"), ".PRGG must NOT be a TOS program");

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("tos_ext: ALL PASS\n");
    return 0;
}
