# ATOS language files (v10)

Default language is **English**, built into the binary (no file needed).  Other
languages are **separate text files** loaded at startup; adding one needs **no
recompile**.

## File format (plain text, UTF-8/ASCII)

    # comment
    @code=PL
    BIOS_TITLE=ATARI BIOS PL
    MACHINE=MASZYNA
    CPU=CPU
    ALT_RAM=ALT-RAM

* `@code=XX` sets the language code (default `EN`).
* `KEY=VALUE` overrides one string.  Keys are ASCII, `A-Z0-9_`.
* Unknown keys are ignored.  Missing keys fall back to the built-in English
  default, so a partial translation is always safe.

## Location and loading order

    C:\SYSTEM\LANG\<CODE>.LNG        (preferred)
    C:\SYSTEM\LANG\ATOS.LNG          (generic override)
    C:\SYSTEM\DESKTOP\LANG\<CODE>.LNG  (desktop/applet strings)

Boot order: built-in English (`lang_init`) -> selected `.LNG` overrides.
Language selection comes from `\BIOS\BIOS.CFG` (Phase 3) and later the Desktop.

## API (include/lang.h, ui/lang.c)

    lang_init();                       /* load built-in English */
    lang_parse(data,length,&applied);  /* apply a .LNG from memory */
    lang_str(id);                      /* never NULL; EN fallback */
    lang_set(id,text); lang_is_overridden(id);
    lang_code(); lang_id_for_key(key);

The core is freestanding (no libc) and host-tested: `tests/test_lang.c`
(`make check-lang`).  BIOS, kernel, VDI/AES and Desktop/applets share the same
ID space so one file can translate the whole system.

## Adding a string

1. Add one line to `LANG_TABLE(X)` in `include/lang.h` (id, key, English).
2. Use `lang_str(LANG_ID_...)` at the call site.
3. Translators only edit `.LNG` files.
