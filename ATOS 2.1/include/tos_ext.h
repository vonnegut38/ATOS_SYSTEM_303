#ifndef ASF_TOS_EXT_H
#define ASF_TOS_EXT_H
/* Shared, case-insensitive "is this a TOS program?" test.
 *
 * Every launch point (desktop shortcuts, the file manager and the shell
 * RUN/START/bare-name command) must agree on what counts as a TOS .PRG so
 * that a program is always handed to the same personality.  The kernel's
 * module_api.tos_run() accepts any 0x601A image, so the extensions below are
 * the single routing decision; a bare "name" without an extension is left to
 * the native .APP spawn path, exactly like the shell already resolves it.
 *
 * Kept as a header (not a compiled object) so both the -mpcrel shell module
 * and the -mpcrel desktop application can include it without adding a runtime
 * link dependency. */

/* Case-insensitive 3-letter extension compare.  `ext` points at the first
   character AFTER the dot and must be followed by the string terminator. */
static inline int tos_ext_is(const char *ext, const char *want)
{
    unsigned i;
    for (i = 0; i < 3u; ++i) {
        char c = ext[i];
        if (!c) return 0;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c != want[i]) return 0;
    }
    return ext[3] == '\0';
}

/* 1 when `path` ends in a TOS program extension (.PRG/.TOS/.TTP/.GTP),
   comparing the extension case-insensitively.  0 otherwise (including a bare
   name or a native .APP). */
static inline int tos_program_path_ext(const char *path)
{
    unsigned n = 0;
    if (!path) return 0;
    while (path[n]) ++n;
    if (n < 4u || path[n - 4u] != '.') return 0;
    return tos_ext_is(path + n - 3u, "PRG") || tos_ext_is(path + n - 3u, "TOS") ||
           tos_ext_is(path + n - 3u, "TTP") || tos_ext_is(path + n - 3u, "GTP");
}

#endif
