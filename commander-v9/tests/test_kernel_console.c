#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../system/kernel_console.c"

static unsigned presents,pairs,clears,numbers,restores;
static unsigned session;
unsigned ksessions_current(void){return session;}
static int desktop;
static uint16_t pixels[16000],saved_pixels[16000];
static uint8_t font[256][16];
static char fallback_text[256];
static void old_clear(void){clears++;fallback_text[0]=0;}
static void old_puts(const char *s){strcat(fallback_text,s);}
static void old_number(uint32_t n){assert(n==123);numbers++;}
int kgfx_desktop_active(void){return desktop;}
int kgfx_console_present(term_display_t *d,const term_cell_t *c)
{assert(!desktop);presents++;pairs=term_render_mono(d,c,pixels,font);return (int)pairs;}
void kgfx_restore_console(void){restores++;desktop=0;kconsole_flush();}
static void initialize(int mono)
{
 session=0;desktop=0;presents=pairs=clears=numbers=restores=0;
 fallback_text[0]=0;
 kconsole_init(mono,old_clear,old_puts,old_number);
}
int main(void)
{
 unsigned i,j;char row[32];text_terminal_t expected;
 for(i=0;i<256;i++)for(j=0;j<16;j++)font[i][j]=(uint8_t)(i^(j*13));
 memset(font[32],0,16);
 initialize(1);kconsole_flush();assert(presents==1&&pairs==1000);
 kconsole_flush();assert(presents==1);
 /* Short module callbacks form one presentation, not one per character. */
 kconsole_puts("A");kconsole_puts("B");kconsole_puts("C");assert(presents==1);
 kconsole_flush();assert(presents==2&&pairs==2);
 kconsole_puts("\bC");kconsole_flush();assert(presents==3&&pairs==0);
 kconsole_puts("\rX\tY\nZ");kconsole_flush();
 assert(console.cells[0]=='X'&&console.cells[8]=='Y'&&console.cells[80]=='Z');
 kconsole_clear();kconsole_put_u32(4294967295u);kconsole_flush();
 for(i=0;i<10;i++)assert(console.cells[i]==(unsigned char)"4294967295"[i]);
 /* Clear resets state but retains the renderer cache for a small diff. */
 kconsole_clear();kconsole_flush();assert(pairs==5);
 kconsole_clear();kconsole_flush();assert(pairs==0);
 presents=0;
 for(i=0;i<319;i++)kconsole_puts(".");assert(presents==0);
 kconsole_puts(".");assert(presents==1); /* bounded synchronous output */
 /* Full desktop lifetime: background writes never touch the bitmap. */
 desktop=1;memcpy(saved_pixels,pixels,sizeof(pixels));i=presents;
 kconsole_clear();term_clear(&expected,0);
 for(j=0;j<80;j++){
  snprintf(row,sizeof(row),"row %u\ttext\n",j);
  kconsole_puts(row);term_write(&expected,row);kconsole_flush();
 }
 assert(presents==i&&!memcmp(saved_pixels,pixels,sizeof(pixels)));
 assert(!memcmp(&console,&expected,sizeof(console)));
 kgfx_restore_console();assert(presents==i+1&&!dirty);
 kconsole_flush();assert(presents==i+1);
 /* Legacy graphics may have overwritten the console: force reconstruction. */
 memset(pixels,0xff,sizeof(pixels));kconsole_invalidate();kconsole_flush();
 assert(pairs==1000);
 /* Writes from another terminal cannot overwrite the selected console. */
 memcpy(saved_pixels,pixels,sizeof(pixels));i=presents;
 kconsole_puts_to(1,"ONLY TTS2");kconsole_flush();
 assert(presents==i&&!memcmp(saved_pixels,pixels,sizeof(pixels)));
 session=1;kconsole_select(1,0);kconsole_flush();
 assert(console.cells[0]=='O'&&console.cells[8]=='2');
 session=0;kconsole_select(0,0);kconsole_flush();
 assert(!memcmp(&console,&expected,sizeof(console)));
 /* Panic must immediately restore ROM output even with a desktop active. */
 desktop=1;i=presents;kconsole_puts("pending");kconsole_emergency();
 kconsole_clear();kconsole_puts("PANIC");kconsole_put_u32(123);kconsole_flush();
 assert(!desktop&&restores==2&&presents==i&&clears==1&&numbers==1);
 assert(!strcmp(fallback_text,"PANIC"));
 initialize(0);kconsole_clear();kconsole_puts("color fallback");
 kconsole_put_u32(123);kconsole_flush();
 assert(clears==1&&numbers==1&&presents==0&&!strcmp(fallback_text,"color fallback"));
 puts("Kernel console PASS: batched writes, bounded flush, CR/Tab/Backspace, scroll, desktop isolation, restore, legacy invalidation, panic and non-mono fallback");
 return 0;
}
