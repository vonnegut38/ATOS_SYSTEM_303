#ifndef ATOS_THEME_H
#define ATOS_THEME_H
/* ATOS system palette - entries 0..15 of the Shifter/card palette.
 *
 * Only entries 0 and 1 are visible in the 1 bpp mono video modes (the VOFA
 * native renderer reads them), but the whole table is meaningful as soon as a
 * colour mode exists.  Applications should use these indices - or the named
 * constants below - instead of literal colours, so switching the system theme
 * repaints every window consistently. */
#define ATOS_COLOR_BACKDROP    0u   /* desktop dither dark half / window ink    */
#define ATOS_COLOR_PAPER       1u   /* menu bar, window paper, text on backdrop */
#define ATOS_COLOR_FRAME       2u   /* active window frame                      */
#define ATOS_COLOR_FRAME_OFF   3u   /* inactive window frame                    */
#define ATOS_COLOR_SELECTION   4u   /* selected row background                  */
#define ATOS_COLOR_SEL_TEXT    5u   /* selected row text                        */
#define ATOS_COLOR_BUTTON      6u   /* button face                              */
#define ATOS_COLOR_SHADOW      7u   /* bevel shadow                             */
#define ATOS_COLOR_TITLE       8u   /* active title bar                         */
#define ATOS_COLOR_TITLE_TEXT  9u   /* title bar text                           */
#define ATOS_COLOR_STATUS     10u   /* status strip                             */
#define ATOS_COLOR_WARNING    11u   /* errors                                   */
#define ATOS_COLOR_COUNT      16u
#endif
