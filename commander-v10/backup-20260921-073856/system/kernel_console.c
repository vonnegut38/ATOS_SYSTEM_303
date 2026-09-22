#include "kernel_console.h"
#include "kernel_graphics.h"
#include "text_terminal.h"
#include "kernel_sessions.h"
#include "kernel_wm.h"

static text_terminal_t consoles[SESSION_COUNT];
#define console consoles[target]
static unsigned target;
static term_display_t display;
static uint8_t active,dirty;
static unsigned pending;
static void (*fallback_clear)(void);
static void (*fallback_puts)(const char *);
static void (*fallback_number)(uint32_t);

void kconsole_init(int mono,void (*clear)(void),void (*puts)(const char *),
                   void (*number)(uint32_t))
{
 fallback_clear=clear;fallback_puts=puts;fallback_number=number;
 active=mono!=0;dirty=active;pending=0;target=0;
 {unsigned i;for(i=0;i<SESSION_COUNT;i++)term_clear(&consoles[i],0);}term_display_invalidate(&display);
}
const void *kconsole_cells(unsigned session){return session<SESSION_COUNT?consoles[session].cells:0;}
void kconsole_set_grid(unsigned cols,unsigned rows)
{
 unsigned i;
 if(cols<8u)cols=8u;
 if(cols>80u)cols=80u;
 if(rows<2u)rows=2u;
 if(rows>25u)rows=25u;
 /* Repeating the same geometry must be a no-op: otherwise the caller (which
    runs every frame) would wipe the console continuously. */
 if(cols==consoles[0].cols&&rows==consoles[0].rows)return;
 for(i=0;i<SESSION_COUNT;i++){
  consoles[i].cols=(uint16_t)cols;
  consoles[i].rows=(uint16_t)rows;
  /* Old cells are laid out for the previous width; they are meaningless now. */
  term_clear(&consoles[i],0);
 }
 term_display_invalidate(&display);
 dirty=active;
}
void kconsole_flush(void)
{
 /* Keep background text in cells; never write over a foreground desktop. */
 if(!active||!dirty)return;
 if(kernel_wm_desktop_session())return;   /* desktop session: no text at all */
 /* In desktop mode the console is a window drawn by the compositor. */
 if(kernel_wm_enabled()&&kgfx_desktop_active()){kernel_wm_console_present();dirty=0;pending=0;return;}
 if(kgfx_desktop_active())return;
 if(kgfx_console_present(&display,consoles[ksessions_current()].cells)>=0){dirty=0;pending=0;}
}
void kconsole_invalidate(void)
{if(active){term_display_invalidate(&display);dirty=1;}}
void kconsole_clear(void)
{
 if(!active){if(fallback_clear)fallback_clear();return;}
 term_clear(&console,0);if(target==ksessions_current())dirty=1;pending=0;
}
void kconsole_puts(const char *s)
{
 const char *p;
 if(!s)return;
 if(!active){if(fallback_puts)fallback_puts(s);return;}
 if(!*s)return;
 term_write(&console,s);if(target==ksessions_current())dirty=1;
 /* Short writes (including per-character shell output) are batched. Bound
  * latency in long synchronous commands which do not return to the pump. */
 for(p=s;*p&&pending<320u;p++)pending++;
 if(pending>=320u)kconsole_flush();
}
void kconsole_put_u32(uint32_t value)
{
 char digits[11];unsigned i=10;
 if(!active&&fallback_number){fallback_number(value);return;}
 digits[i]=0;
 do{digits[--i]=(char)('0'+value%10u);value/=10u;}while(value);
 kconsole_puts(digits+i);
}
void kconsole_emergency(void)
{
 /* A panic must be visible even if the normal pump or terminal is broken. */
 active=0;dirty=0;pending=0;
 kgfx_restore_console();
}

void kconsole_select(unsigned slot,int reset)
{target=slot;if(reset)term_clear(&console,0);kconsole_invalidate();}
void kconsole_puts_to(unsigned slot,const char *s)
{unsigned previous=target;if(slot>=SESSION_COUNT)return;target=slot;kconsole_puts(s);target=previous;}
