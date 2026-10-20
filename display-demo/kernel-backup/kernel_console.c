#include "kernel_console.h"
#include "kernel_memory.h"
#include "kernel_graphics.h"
#include "text_terminal.h"
#include "kernel_sessions.h"
#include "kernel_wm.h"

/* Terminale sesji w TT-RAM (12 KB).  Gdyby alokacja sie nie udala, active=0
   wylacza rysowanie kernela i wszystko idzie sciezka awaryjna (fallback_*). */
static text_terminal_t *consoles;
#define console consoles[target]
static unsigned target;
/* Bring-up status row: while >=0 every kconsole_puts() OVERWRITES this single
   row instead of appending lines, so the boot screen never scrolls (the
   user's "LOADING COMPONENTS: XXX" -> "YYY" idea). */
static int status_row=-1;
static unsigned status_col;
static unsigned status_field;    /* column where the changing VALUE starts */
static uint8_t status_label_done;/* first message is the header, later ones values */
/* Quiet boot: verbose lines are captured (for the ROM mirror) but NOT drawn, so
   the screen keeps only the system table and the LOADING COMPONENTS line. */
static uint8_t quiet,boot_screen;
void kconsole_boot_screen(int on){boot_screen=(uint8_t)(on!=0);}
int kconsole_boot_active(void){return boot_screen!=0;}
static term_display_t display;
static uint8_t active,dirty;
static unsigned pending;
static void (*fallback_clear)(void);
static void (*fallback_puts)(const char *);
static void (*fallback_number)(uint32_t);
static char *capture_buffer;
static uint32_t capture_capacity,capture_length;
static uint8_t capture_done=1;

void kconsole_init(int mono,void (*clear)(void),void (*puts)(const char *),
                   void (*number)(uint32_t))
{
 fallback_clear=clear;fallback_puts=puts;fallback_number=number;
 if(!consoles)consoles=(text_terminal_t *)kmem_alloc(
     (uint32_t)sizeof(text_terminal_t)*(uint32_t)SESSION_COUNT,16u,
     KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
 if(!consoles){active=0;dirty=0;pending=0;target=0;return;}
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
  /* The cell array always uses the fixed TERM_COLS (80) stride, so a grid
     change never moves data.  Clearing here is what wiped the shell's boot log
     the moment the desktop first showed its console window; clamp the cursor
     instead and keep the cells. */
  if(consoles[i].row>=rows)consoles[i].row=(uint16_t)(rows-1u);
  if(consoles[i].column>=cols)consoles[i].column=0;
 }
 term_display_invalidate(&display);
 dirty=active;
}
/* Continue a log that the ROM console already drew: adopt its 80x25 cells and
   cursor so the first render repaints exactly the same characters. */
void kconsole_seed(const char *cells,unsigned column,unsigned row)
{
 unsigned i,s;
 if(column>=TERM_COLS)column=0;
 if(row>=TERM_ROWS)row=0;
 for(s=0;s<SESSION_COUNT;s++){
  for(i=0;i<TERM_CELLS;i++)consoles[s].cells[i]=(term_cell_t)(uint8_t)(cells?cells[i]:' ');
  consoles[s].column=(uint16_t)column;consoles[s].row=(uint16_t)row;
 }
 term_display_invalidate(&display);
 dirty=active;
}
void kconsole_flush(void)
{
 /* Keep background text in cells; never write over a foreground desktop. */
 if(boot_screen||!active||!dirty)return;
 if(kernel_wm_desktop_session())return;   /* desktop session: no text at all */
 /* In desktop mode the console is a window drawn by the compositor. */
 if(kernel_wm_enabled()&&kgfx_desktop_active()){kernel_wm_console_present();dirty=0;pending=0;return;}
 if(kgfx_desktop_active())return;
 if(kgfx_console_present(&display,consoles[ksessions_current()].cells)>=0){dirty=0;pending=0;}
}
/* Diagnostics for the 256-colour boot: the kernel-console output stopped
   reaching the screen.  Report exactly what kconsole_flush() sees, through a
   console that still works (the ROM one). */
void kconsole_debug_state(void (*outs)(const char *),void (*outn)(uint32_t),
                         const kconsole_debug_t *d)
{
    outs("KCONSOLE: active=");outn(active);
    outs(" dirty=");outn(dirty);
    outs(" pending=");outn(pending);
    outs(" target=");outn(target);
    outs(" session=");outn(ksessions_current());
    outs(" mode=");outn(d?d->mode:0u);
    outs(" planes=");outn(d?d->planes:0u);
    outs("\nKCONSOLE: desktop_active=");outn(d?d->desktop_active:0u);
    outs(" wm_enabled=");outn(d?d->wm_enabled:0u);
    outs(" wm_session=");outn(d?d->wm_session:0u);
    outs(" framebuffer=");outn(d?d->framebuffer:0u);
    outs(" scanout=");outn(d?d->scanout:0u);
    outs("\n");
}
void kconsole_invalidate(void)
{if(active){term_display_invalidate(&display);dirty=1;}}
void kconsole_clear(void)
{
 if(!active){if(fallback_clear)fallback_clear();return;}
 term_clear(&console,0);if(target==ksessions_current())dirty=1;pending=0;
}
static void capture_append(const char *s)
{
 if(!capture_buffer)return;
 while(*s){
  if(capture_length+1u<capture_capacity)capture_buffer[capture_length++]=*s;
  ++s;
 }
 capture_buffer[capture_length]='\0';
}
void kconsole_capture_begin(char *buffer,uint32_t capacity)
{capture_buffer=buffer;capture_capacity=capacity;capture_length=0;capture_done=0;if(buffer&&capacity)buffer[0]='\0';}
void kconsole_capture_end(void){capture_buffer=0;capture_done=1;}
int kconsole_capture_state(void){return capture_done?(int)capture_length:-1;}
void kconsole_status_row(int row)
{
    status_row=row;status_col=0;status_field=0;status_label_done=0;
    if(row<0)return;
    if((unsigned)row>=TERM_ROWS)status_row=-1;
    else{term_fill(&console,0,(unsigned)status_row,TERM_COLS,1,' ',0);if(target==ksessions_current())dirty=1;}
}
void kconsole_status_here(void){kconsole_status_row((int)console.row);}
void kconsole_quiet(int on){quiet=(uint8_t)(on!=0);}
/* Explicit lines that stay visible even in quiet mode (the system table). */
void kconsole_notice_puts(const char *s){uint8_t q=quiet,b=boot_screen;quiet=boot_screen=0;kconsole_puts(s);kconsole_flush();quiet=q;boot_screen=b;}
void kconsole_notice_u32(uint32_t v){uint8_t q=quiet,b=boot_screen;quiet=boot_screen=0;kconsole_put_u32(v);kconsole_flush();quiet=q;boot_screen=b;}
void kconsole_puts(const char *s)
{
 unsigned written;
 if(!s)return;
 capture_append(s);
 if(!active){if(fallback_puts)fallback_puts(s);return;}
 if(!*s)return;
 if(status_row>=0){
     /* Fixed row.  The FIRST message is the header ("LOADING COMPONENTS:") and
        stays; every later "LABEL: VALUE" message only rewrites the VALUE field,
        so just those few cells change and only they get repainted (the user's
        "erase XXXXX, write YYYYY" idea). */
     const char *text=s;unsigned col,i;
     unsigned limit=(unsigned)-1;      /* how many chars of this message to show */
     if(status_label_done){
         /* Show the module NAME (the part before ": ") in the value field: the
            header stays and only those few cells are repainted. */
         const char *p=s;unsigned n=0;
         while(s[n]&&!(s[n]==':'&&s[n+1]==' '))++n;
         if(s[n])limit=n;
         (void)p;
         col=status_field;
     }else col=0;
     status_col=col;
     for(i=0;text[i]&&i<limit&&status_col<TERM_COLS;++i){
         if(text[i]=='\n'||text[i]=='\r')continue;
         console.cells[(unsigned)status_row*TERM_COLS+status_col++]=(term_cell_t)(uint8_t)text[i];
     }
     if(!status_label_done){status_field=(status_col<TERM_COLS-1u)?(status_col+1u):status_col;
                            status_label_done=1u;}
     while(status_col<TERM_COLS)           /* erase the rest of the field */
         console.cells[(unsigned)status_row*TERM_COLS+status_col++]=(term_cell_t)' ';
     if(target==ksessions_current())dirty=1;
     kconsole_flush();
     return;
 }
 if(quiet)return;                       /* verbose line: keep the screen clean */
 /* term_write() already walks the string; use the count it returns instead of
  * scanning the text a second time just to feed the flush batch counter. */
 written=term_write(&console,s);if(target==ksessions_current())dirty=1;
 pending+=written;
 /* Short writes (including per-character shell output) are batched. Bound
  * latency in long synchronous commands which do not return to the pump. */
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
 boot_screen=0;active=0;dirty=0;pending=0;
 kgfx_restore_console();
}

void kconsole_select(unsigned slot,int reset)
{target=slot;if(reset)term_clear(&console,0);kconsole_invalidate();}
void kconsole_puts_to(unsigned slot,const char *s)
{unsigned previous=target;if(slot>=SESSION_COUNT)return;target=slot;kconsole_puts(s);target=previous;}
