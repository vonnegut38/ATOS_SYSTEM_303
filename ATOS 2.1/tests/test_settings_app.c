/* Test aplikacji SETTINGS (system/settings_app.c) na maszynie hosta.
 *
 * Test byl NIEAKTUALNY od czasu przejscia na liste 9 trybow: uzywal pol
 * res_sel/col_sel, ktorych aplikacja juz nie ma, wiec `make check-settings`
 * nie kompilowal sie (a przez to nikt nie sprawdzal tej sciezki).  Ten plik
 * sprawdza stan BIEZACY, w tym dwie rzeczy zgloszone w audycie:
 *   - APPLY w trakcie podgladu nie jest cicho ignorowane (jasny komunikat),
 *   - -997 z bramki w jadrze nie konczy sie slepa uliczka: gdy podglad nalezy
 *     do nas, wracamy do okna YES/NO; gdy do innej aplikacji - mowimy o tym.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../system/settings_app.c"
static uint32_t op,aa,bb,cc;
static int rc,guard=1,saves,inject,receive_stage;
static int health=2,repairs,repair_rc;   /* zakladka VOLUME: stan i naprawa */
static char config[1024]="THEME=3\nRES=800x600x8\nTOSTT=1\nSYSINFO=2,4\nUNKNOWN=keep\n";
static char saved[1200];
static gui_draw_t draw_store[LIST_MAX];
int32_t gui_call(uint32_t call,uint32_t a,uint32_t b,void *p,void *q)
{
    if(call==36){uint32_t *v=p;op=v[0];aa=v[1];bb=v[2];cc=v[3];receive_stage=0;return 0;}
    if(call==38){gui_message_t *m=p;zero(m,sizeof *m);
        if(inject&&!receive_stage++){m->v[0]=WCLS;inject=0;return 0;}
        m->sender=(uint32_t)gfx_pid;m->v[0]=op;
        if(op==30)m->v[1]=(800u<<16)|600;
        else if(op==35)m->v[1]=8;
        else if(op==ATOS_GFX_DISPLAY_GUARD){m->v[1]=aa==2?(unsigned)guard:(guard?0u:(uint32_t)-1);if(aa<2)guard=0;}
        else m->v[1]=(uint32_t)rc;
        return 0;
    }
    if(call==ATOS_CALL_STAT){((uint32_t *)q)[0]=(uint32_t)strlen(config);return 0;}
    if(call==ATOS_CALL_OPEN)return 0;
    if(call==ATOS_CALL_READ){memcpy(p,config,b);return (int32_t)b;}
    if(call==ATOS_CALL_CLOSE)return 0;
    if(call==ATOS_CALL_SAVE){memcpy(saved,q,a);saved[a]=0;++saves;return rc;}
    if(call==ATOS_CALL_VOLUME){if(a==1u){++repairs;return repair_rc;}return health;}
    return 0;
}
static void click(int x,int y,unsigned down)
{gui_message_t m={0,{KMOU,((uint32_t)(x+wx)<<16)|(uint16_t)(y+wy+16),down<<24,0}};event(&m);}
/* Klikniecie = zwolnienie + nacisniecie: aplikacja reaguje na ZBOCZE przycisku
   (press), wiec samo powtorzone "w dol" nie jest nowym kliknieciem. */
static void press(int x,int y){click(x,y,0);click(x,y,1);}
static void key(unsigned s)
{gui_message_t m={0,{KKEY,0x01000000u|s,0,0}};event(&m);}
static int has_text(const char *s){unsigned i;for(i=0;i<n;++i)if(strstr(list[i].text,s))return 1;return 0;}
int main(void)
{
    gui_message_t m;
    gfx_pid=3;list=draw_store;
    /* 1. Lista 9 gotowych trybow + APPLY/CLOSE; radio stoi na trybie z karty
          (opcode 30 daje geometrie, 35 - glebie KARTY, nie format RAM). */
    tab=0;focus_row=0;query_selection();assert(focus_row==6);   /* 800x600x256 */
    paint();assert(n<LIST_MAX);
    assert(has_text("APPLY [ENTER]")&&has_text("CLOSE [ESC]"));
    assert(has_text("1024 x 768 - 16 COLORS")&&has_text("800 x 600 - 65K COLORS"));
    assert(has_text("SELECT MODE, THEN APPLY"));
    /* 2. Okno podgladu rysuje YES/NO (to ono czeka na decyzje uzytkownika). */
    confirm_pending=1;paint();assert(n<LIST_MAX);
    assert(has_text("YES [ENTER]")&&has_text("NO [ESC]"));
    assert(has_text("KEEP MODE? 4 SEC TO CONFIRM"));confirm_pending=0;
    /* 3. Klik w wiersz ustawia radio, klik poza oknem nie zmienia niczego. */
    focus_row=0;press(40,TOP+4*ROW_H+8);assert(focus_row==4);
    press(-20,TOP+7*ROW_H+8);assert(focus_row==4);
    /* 4. Przesuniecie okna: wspolrzedne myszy sa globalne, rysowania lokalne. */
    m.sender=0;m.v[0]=WMOV;m.v[1]=(80u<<16)|50;m.v[2]=(WW<<16)|(WH+16);m.v[3]=0;event(&m);
    press(40,TOP+7*ROW_H+8);assert(focus_row==7);
    /* 5. Klawiatura: strzalki po liscie, TAB przelacza zakladke. */
    focus_row=0;key(0x50);assert(focus_row==1);key(0x48);assert(focus_row==0);
    key(0x0f);assert(tab==1);key(0x0f);assert(tab==2);   /* RES->THEMES->VOLUME */
    key(0x0f);assert(tab==0);
    /* 6. APPLY: opcode 40 z GLEBIA KARTY | PREVIEW, jeszcze bez zapisu. */
    focus_row=5;rc=0;confirm_pending=0;saves=0;op=0;
    apply();assert(op==ATOS_GFX_DISPLAY_SET&&confirm_pending==1&&saves==0);
    assert((aa>>16)==640u&&(uint16_t)aa==480u);
    assert((bb&255u)==8u&&(bb&ATOS_DISPLAY_PREVIEW));
    assert(!strcmp(status,"WAITING FOR CONFIRMATION"));
    /* 7. APPLY w trakcie podgladu: NIE cicho - mowimy, ze czeka YES/NO. */
    op=0;apply();assert(op==0&&confirm_pending==1);
    assert(!strcmp(status,"PREVIEW RUNNING - ANSWER YES/NO"));
    /* 8. -997, a podglad nalezy DO NAS (op 39 sub 2 == 1): to nie blad, a stan -
          wracamy do okna YES/NO i podglad zostaje potwierdzalny. */
    confirm_pending=0;guard=1;rc=RC_PREVIEW_ACTIVE;
    apply();assert(confirm_pending==1);
    assert(!strcmp(status,"PREVIEW RUNNING - ANSWER YES/NO"));
    /* 9. -997, ale podglad nalezy do INNEJ aplikacji: mowimy wprost, bo APPLY
          nie ma szans, a "MODE REFUSED" bez wyjasnienia bylo mylace. */
    confirm_pending=0;guard=0;rc=RC_PREVIEW_ACTIVE;
    apply();assert(confirm_pending==0);
    assert(!strcmp(status,"PREVIEW BUSY (OTHER APP) rc=-997"));
    /* 10. Zwykle odmowy: kod MUSI byc widoczny (to byla prosba z audytu r2). */
    rc=RC_UNKNOWN_OPCODE;apply();assert(!confirm_pending);
    assert(!strcmp(status,"MODE REFUSED rc=-994"));
    rc=RC_MODE_UNSUPPORTED;apply();assert(!confirm_pending);
    assert(!strcmp(status,"MODE REFUSED rc=-2"));
    /* 11. YES: tryb zostaje, konfiguracja zapisana z glebia KARTY, a nieznane
           klucze (THEME/TOSTT/UNKNOWN) przechodza bez zmian. */
    confirm_pending=0;guard=1;rc=0;apply();assert(confirm_pending==1);
    key(0x1c);assert(!confirm_pending&&saves==1);
    assert(strstr(saved,"RES=640x480x8\n")&&strstr(saved,"THEME=3\n")&&
           strstr(saved,"TOSTT=1\n")&&strstr(saved,"UNKNOWN=keep\n"));
    /* 12. NO: powrot do poprzedniego trybu, bez zapisu. */
    guard=1;rc=0;apply();assert(confirm_pending==1);
    press(180,166);assert(!confirm_pending&&saves==1);
    assert(!strcmp(status,"PREVIOUS MODE RESTORED"));
    /* 13. Powrot, ktory sie NIE udal: kod widoczny, a blokada w aplikacji zdjeta
           (jadro zdejmuje ja samo - patrz tests/test_video_boot.c). */
    confirm_pending=1;guard=0;finish_preview(0);assert(!confirm_pending);
    assert(!strcmp(status,"PREVIEW EXPIRED/FAILED rc=-1"));
    /* 14. Klawisze w oknie podgladu: ESC = NO, ENTER = YES. */
    guard=1;rc=0;apply();assert(confirm_pending==1);key(1);
    assert(!confirm_pending&&saves==1);
    guard=1;rc=0;apply();assert(confirm_pending==1);key(0x1c);
    assert(!confirm_pending&&saves==2);
    /* 15. Przyciski na dole listy: APPLY i CLOSE (ESC zamyka aplikacje). */
    confirm_pending=0;rc=0;op=0;press(40,TOP+(int)MODE_COUNT*ROW_H+8);
    assert(op==ATOS_GFX_DISPLAY_SET&&confirm_pending==1);
    confirm_pending=0;quit=0;press(200,TOP+(int)MODE_COUNT*ROW_H+8);assert(quit==1);
    quit=0;guard=0;m.sender=0;m.v[0]=WCLS;m.v[1]=0;event(&m);assert(quit==1);quit=0;
    /* 16. Powiadomienie jadra o zmianie trybu wymusza przemalowanie. */
    dirty=0;m.sender=0;m.v[0]=ATOS_DISPLAY_CHANGED;m.v[1]=0;event(&m);assert(dirty==1);
    /* 17. Zakladka THEMES (opcode 37: 0 = zapytanie, 1 = zastosowanie). */
    tab=1;th_count=5;rc=0;set_theme(3);assert(op==37&&aa==3&&cc==1&&th_cur==3);
    rc=-1;set_theme(4);assert(th_cur==3&&!strcmp(status,"THEME REFUSED"));
    rc=0;set_theme(4);assert(th_cur==4);
    /* 18. Zdarzenie obce (nie od brokera) NIE jest gubione, tylko kolejkuje sie. */
    inject=1;(void)gfx(30,0,0,0);assert(pop(&m)&&m.v[0]==WCLS);
    /* 19. Zapis nieudany: kod bledu MUSI byc widoczny (audyt r2).  Przy -6
           (wolumin "dirty" po nieczystym resecie) aplikacja oferuje naprawe
           klawiszem R przez ATOS_CALL_VOLUME - bez tego uzytkownik nie mial
           z GUI zadnej drogi wyjscia z tego stanu. */
    rc=-1;(void)save_cfg();assert(!strcmp(status,"SAVE FAILED rc=1"));
    rc=-6;repairs=0;repair_rc=0;(void)save_cfg();
    assert(!strcmp(status,"SAVE FAILED rc=6 - PRESS R TO REPAIR"));
    key(0x13);assert(repairs==1);
    assert(!strcmp(status,"VOLUME REPAIRED - PRESS APPLY AGAIN"));
    rc=-6;repair_rc=-6;(void)save_cfg();key(0x13);
    assert(strstr(status,"REPAIR FAILED rc=6")!=0);
    puts("PASS settings app: 9-mode list, hit tests, APPLY/confirm/preview states, -997 route, config preservation, themes, pending IPC");
    
    /* 12. Zakladka VOLUME: pokazuje stan woluminu i naprawia znacznik FAT[1]
           przez ATOS_CALL_VOLUME (d1=0 odczyt, d1=1 naprawa). */
    tab=2;health=2;repairs=0;repair_rc=0;status[0]=0;paint();assert(n<LIST_MAX);
    assert(has_text("DIRTY - WRITES BLOCKED"));
    assert(has_text("REPAIR [ENTER]")&&has_text("CLOSE [ESC]"));
    key(0x1c);assert(repairs==1);
    assert(!strcmp(status,"VOLUME REPAIRED - WRITES ENABLED"));
    health=1;paint();assert(has_text("CLEAN - WRITES ENABLED"));
    health=3;paint();assert(has_text("HARD ERROR - WRITES BLOCKED"));
    health=0;paint();assert(has_text("UNKNOWN - NO VOLUME"));
    repair_rc=-6;health=2;paint();key(0x1c);       /* nieudana naprawa: kod widoczny */
    assert(strstr(status,"REPAIR FAILED rc=6")!=0);
return 0;
}
