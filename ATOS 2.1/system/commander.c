/* Native Mono Commander. Original implementation, inspired by MC's two-panel
 * interaction model; no Unix/GLib/terminal code is imported. */
#include "gui_app.h"
#define LNG_CMD
#include "app_lng.h"
#include "kernel_fat.h"
#include "text_terminal.h"
#define ROWS 16u
#define MARK_LIMIT 256u
typedef struct {char name[13];uint8_t attr,pad[2];uint32_t size,date;} entry;
typedef struct {char path[64];char focus[13];entry item[ROWS+1];char marked[MARK_LIMIT][13],marked_path[64];unsigned marks,top,sel,count,more,sort;} panel;
static panel panels[2];
static text_terminal_t terminal;
static uint32_t gfx,gui;
static unsigned active,used,quit,dirty,modal,length;
static unsigned menu,menu_sel;   /* 0=closed 1=left 2=right 3=file; sort: 0=name 1=date 2=type */
static int screen_captured;
static unsigned panel_damage,painted_valid;
static char status[64],input[13],source[64],target[64];
/* Norton-style command line, drawn on the former mouse-hint row. */
static char cmdline[64];
static unsigned cmdline_len;
static int fault;
static unsigned batch_copy;
static unsigned painted_active,painted_sel[2],mouse_buttons;
static unsigned last_click_panel,last_click_index,click_valid;
static uint32_t last_click_tick;
#define VIEW_CAP 8192u
static char *viewbuf,viewname[13];static unsigned viewlen,viewtop,viewcap;static int viewing;
#define EDIT_CAP 16384u
static char *editbuf,editname[13];static unsigned editlen,editpos,edittop;static int editing,editdirty;
static void action(unsigned scan,unsigned character);
static void render(void);
static void selection_dirty(void){if(!dirty||dirty==3)dirty=3;}

static gui_message_t pending[16];static unsigned head,tail;
static unsigned len(const char *s){unsigned n=0;while(s[n])n++;return n;}
static void message(const char *s){copytext(status,s,sizeof(status));dirty=1;}
static void queue_input(const gui_message_t *m){unsigned j;gui_message_t *d;
 if(m->sender||(m->v[0]!=0x4b4b4559&&m->v[0]!=0x4b4d4f55))return;
 /* Adjacent mouse motion is state. Preserve clicks while replacing its position. */
 if(m->v[0]==0x4b4d4f55&&tail!=head){d=&pending[(tail-1)&15];
  if(d->v[0]==0x4b4d4f55&&!(m->v[2]&0x1f000000u)&&!(d->v[2]&0x1f000000u)){
   for(j=0;j<sizeof(*m);j++)((volatile uint8_t *)d)[j]=((const uint8_t *)m)[j];
   return;}}
 if(tail-head>=16)return;
 d=&pending[tail++&15];for(j=0;j<sizeof(*m);j++)((volatile uint8_t *)d)[j]=((const uint8_t *)m)[j];
}
static int rpc(uint32_t pid,uint32_t op,uint32_t a,uint32_t b,uint32_t c){gui_message_t m;unsigned tries=0;int r;
 do{r=send(pid,op,a,b,c);if(r!=-0x3c2)break;pause_tick();}while(++tries<100);
 if(r)return r;
 for(;;){if(receive(&m,1))return -1;if(m.sender==pid&&m.v[0]==(op|0x80000000u))return (int32_t)m.v[1];queue_input(&m);}
}
static int join(char *out,const char *dir,const char *name){unsigned n=len(dir),j=0;
 if(n+len(name)+2>64)return 0;
 copytext(out,dir,64);if(n>1)out[n++]='\\';
 while(name[j])out[n++]=name[j++];
 out[n]=0;return 1;}
/* Sorting is applied to the whole directory, not to one kernel page: the
   kernel returns FAT order, so the app collects up to 256 entries, sorts them
   and only then builds the 16-row window. */
static const char *ext_of(const char *name){const char *e=0;unsigned i;for(i=0;name[i];++i)if(name[i]=='.')e=name+i+1;return e?e:name+i;}
static int name_cmp(const char *a,const char *b)
{
    unsigned i=0;
    for(;;){
        int ca=(unsigned char)a[i],cb=(unsigned char)b[i];
        if(ca>='a'&&ca<='z')ca-=32;
        if(cb>='a'&&cb<='z')cb-=32;
        if(ca!=cb)return ca<cb?-1:1;
        if(!ca)return 0;
        ++i;
    }
}
static int entry_cmp(const entry *a,const entry *b,unsigned mode){
    int da=(a->attr&16)?0:1,db=(b->attr&16)?0:1,r;   /* directories first */
    if(da!=db)return da-db;
    if(mode==1&&a->date!=b->date)return a->date>b->date?-1:1;  /* newest first */
    if(mode==2){r=name_cmp(ext_of(a->name),ext_of(b->name));if(r)return r;}
    return name_cmp(a->name,b->name);}
/* Field-by-field copy: a struct assignment would pull in memcpy, and apps are
   freestanding. */
static void copy_entry(entry *d,const entry *s){unsigned k;for(k=0;k<13;k++)d->name[k]=s->name[k];d->attr=s->attr;d->pad[0]=d->pad[1]=0;d->size=s->size;d->date=s->date;}
static void sort_entries(entry *v,unsigned n,unsigned mode){unsigned i,j;entry t;
    for(i=1;i<n;++i){copy_entry(&t,&v[i]);j=i;
        while(j>0&&entry_cmp(&v[j-1],&t,mode)>0){copy_entry(&v[j],&v[j-1]);--j;}
        copy_entry(&v[j],&t);}}
static int marked_index(const panel *p,const char *name){unsigned i;for(i=0;i<p->marks;i++)if(lng_equal(p->marked[i],name))return (int)i;return -1;}
static void unmark(panel *p,unsigned i){unsigned j;for(j=i+1;j<p->marks;j++)copytext(p->marked[j-1],p->marked[j],13);if(p->marks)p->marks--;}
static entry *sortbuf; /* 256 entries on the app heap: BSS must stay inside the PC-relative range */
static void load(panel *p){unsigned total=0,base=0,parent=p->path[1]!=0,list_len,i;int r=0;
 if(!lng_equal(p->path,p->marked_path)){p->marks=0;copytext(p->marked_path,p->path,64);}
 for(;;){
     r=gui_call(52,base,32,p->path,sortbuf+total);
     if(r<=0)break;
     total+=(unsigned)r;
     if((unsigned)r<32u||total>=256u)break;
     base+=32u;
 }
 if(r<0&&!total)message(lng_text(CMD_ERR_DIR));
 if(total>256u)total=256u;
 sort_entries(sortbuf,total,p->sort);
 list_len=total+(parent?1u:0u);
 if(p->focus[0]){unsigned idx=0xffffffffu;for(i=0;i<total;++i)if(!name_cmp(sortbuf[i].name,p->focus)){idx=i;break;}
     if(idx!=0xffffffffu){unsigned abs=idx+(parent?1u:0u);p->top=(abs>=ROWS)?abs-ROWS+1u:0u;}p->focus[0]=0;}
 if(p->top>=list_len)p->top=list_len>ROWS?list_len-ROWS:0u;
 p->count=0;
 for(i=0;i<ROWS;++i){unsigned abs=p->top+i;if(abs>=list_len)break;
     if(parent&&abs==0){zero(&p->item[i],sizeof(entry));copytext(p->item[i].name,"..",13);p->item[i].attr=16;}
     else copy_entry(&p->item[i],&sortbuf[abs-(parent?1u:0u)]);
     p->count=i+1;}
 p->more=(p->top+p->count)<list_len;
 if(!p->count&&p->top){p->top=0;p->sel=0;load(p);return;}
 if(p->sel>=p->count)p->sel=p->count?p->count-1:0;
 click_valid=0;panel_damage|=1u<<(p==&panels[1]);if(dirty!=1&&dirty!=2)dirty=4;
}
static void refresh(void){load(&panels[0]);load(&panels[1]);}
/* All layout is expressed in character cells. The display server compares
 * the submitted grid and rasterizes only changed character pairs. */
static void path_row(unsigned p){panel *q=&panels[p];unsigned n=len(q->path),x=p*40+1;
 term_fill(&terminal,x,2,38,1,' ',p==active?TERM_INVERSE:0);
 term_text(&terminal,x,2,n>38?q->path+n-38:q->path,p==active?TERM_INVERSE:0,38);
}
static void row(unsigned p,unsigned i,unsigned selected){panel *q=&panels[p];unsigned j,n;char line[39],num[12];unsigned x=p*40+1;
 if(i>=ROWS)return;
 for(j=0;j<38;j++)line[j]=' ';
 line[38]=0;
 if(i<q->count){line[0]=(q->item[i].attr&16)?'/':marked_index(q,q->item[i].name)>=0?'*':' ';
  for(j=0;q->item[i].name[j]&&j<12;j++)line[j+1]=q->item[i].name[j];
  if(q->item[i].attr&16)copytext(num,lng_text(CMD_DIRECTORY),12);else number(num,q->item[i].size);
  n=len(num);for(j=0;j<n;j++)line[37-n+j]=num[j];
 }
 term_text(&terminal,x,4+i,line,selected?TERM_INVERSE:0,38);
}
static void panel_frame(unsigned p){unsigned x=p*40,i;term_frame(&terminal,x,1,40,21,0);
 if(p!=active)return;
 for(i=0;i<40;i++){terminal.cells[80+x+i]+=0x20;terminal.cells[21*80+x+i]+=0x20;}
 for(i=2;i<21;i++){terminal.cells[i*80+x]+=0x20;terminal.cells[i*80+x+39]+=0x20;}
}
static void panel_view(unsigned p){unsigned i,x=p*40+1;
 term_fill(&terminal,x,2,38,19,' ',0);path_row(p);
 term_text(&terminal,x,3,lng_text(CMD_COLUMNS),0,38);
 for(i=0;i<panels[p].count;i++)row(p,i,p==active&&i==panels[p].sel);
 term_text(&terminal,x,20,"^ PgUp                         PgDn v",0,38);
}
static const char *sort_label(unsigned m){return m==1?lng_text(CMD_SORT_DATE):m==2?lng_text(CMD_SORT_TYPE):lng_text(CMD_SORT_NAME);}
static void menu_anchor(unsigned p){
    unsigned x=p*40+1,n=0,i=0;char text[16];const char *label=p?lng_text(CMD_MENU_RIGHT):lng_text(CMD_MENU_LEFT);const char *s=sort_label(panels[p].sort);
    text[n++]='[';while(label[i]&&n<12)text[n++]=label[i++];text[n++]=':';
    i=0;while(s[i]&&n<15)text[n++]=s[i++];text[n++]=']';text[n]=0;
    term_fill(&terminal,x,0,15,1,' ',0);
    term_text(&terminal,x,0,text,(menu==(p+1))?TERM_INVERSE:0,15);
}
static void file_anchor(void){
    const char *l=lng_text(CMD_MENU_FILE);char text[10];unsigned n=0,i=0;
    text[n++]='[';while(l[i]&&n<8)text[n++]=l[i++];text[n++]=']';text[n]=0;
    term_fill(&terminal,18,0,10,1,' ',0);
    term_text(&terminal,18,0,text,(menu==3)?TERM_INVERSE:0,10);
}
static void menu_bar(void){menu_anchor(0);menu_anchor(1);file_anchor();}
static void menu_popup(void){
    unsigned p,x,y=1,i;const char *items[3];
    if(!menu)return;
    p=menu-1;x=p*40+1;items[0]=lng_text(CMD_SORT_NAME);items[1]=lng_text(CMD_SORT_DATE);items[2]=lng_text(CMD_SORT_TYPE);
    term_fill(&terminal,x,y,16,5,' ',0);term_frame(&terminal,x,y,16,5,0);
    term_text(&terminal,x+2,y+1,lng_text(CMD_SORT),0,12);
    for(i=0;i<3;i++){
        term_fill(&terminal,x+1,y+2+i,14,1,' ',(i==menu_sel)?TERM_INVERSE:0);
        term_text(&terminal,x+1,y+2+i,(i==panels[p].sort)?"*":" ",0,1);
        term_text(&terminal,x+3,y+2+i,items[i],(i==menu_sel)?TERM_INVERSE:0,10);
    }
}
static void apply_sort(void){unsigned p;if(!menu)return;p=menu-1;panels[p].sort=menu_sel;menu=0;load(&panels[p]);dirty=1;}
static const char *file_item(unsigned i){return i==0?lng_text(CMD_MENU_VIEW):i==1?lng_text(CMD_MENU_EDIT):i==2?lng_text(CMD_MENU_RENAME):i==3?lng_text(CMD_MENU_COPY):i==4?lng_text(CMD_MENU_MKDIR):i==5?lng_text(CMD_MENU_DELETE):lng_text(CMD_MENU_QUIT);}
static void file_popup(void){
    unsigned i;term_fill(&terminal,18,1,20,9,' ',0);term_frame(&terminal,18,1,20,9,0);
    term_text(&terminal,20,2,lng_text(CMD_MENU_FILE),0,14);
    for(i=0;i<7;i++){
        term_fill(&terminal,19,3+i,18,1,' ',(i==menu_sel)?TERM_INVERSE:0);
        term_text(&terminal,20,3+i,file_item(i),(i==menu_sel)?TERM_INVERSE:0,16);
    }
}
static void file_run(unsigned sel){menu=0;if(sel==0)action(0x3d,0);else if(sel==1)action(0x3e,0);else if(sel==2)action(0x40,0);else if(sel==3)action(0x3f,0);else if(sel==4)action(0x41,0);else if(sel==5)action(0x42,0);else action(0x44,0);}
static void view_open(panel *p){
    entry *e;unsigned i;int fd,r;
    if(!p->count)return;
    e=&p->item[p->sel];
    if((e->attr&16)||e->name[0]=='.')return;
    if(!join(source,p->path,e->name)){message(lng_text(CMD_ERR_PATH));return;}
    if(!viewbuf){viewbuf=(char *)(uintptr_t)(uint32_t)gui_call(16,VIEW_CAP,0,0,0);viewcap=VIEW_CAP;}
    if(!viewbuf){message(lng_text(CMD_ERR_OPEN));return;}
    fd=gui_call(2,0,0,source,0);if(fd<0){message(lng_text(CMD_ERR_OPEN));return;}
    viewlen=0;
    while(viewlen<viewcap){r=gui_call(3,(uint32_t)fd,(uint32_t)(viewcap-viewlen),viewbuf+viewlen,0);if(r<=0)break;viewlen+=(unsigned)r;}
    gui_call(4,(uint32_t)fd,0,0,0);
    for(i=0;i<12&&e->name[i];i++)viewname[i]=e->name[i];
    viewname[i]=0;
    viewtop=0;viewing=1;dirty=1;
}
static void view_render(void){
    unsigned line=0,pos=0,row=1;char text[79];
    term_clear(&terminal,0);
    term_text(&terminal,21,0,lng_text(CMD_VIEW_TITLE),0,6);
    term_text(&terminal,28,0,viewname,0,13);
    term_text(&terminal,1,23,lng_text(CMD_VIEW_HELP),0,78);
    while(pos<viewlen&&line<viewtop){if(viewbuf[pos]=='\n')++line;++pos;}
    while(pos<=viewlen&&row<23){
        unsigned n=0;
        while(pos<viewlen&&viewbuf[pos]!='\n'&&n<78){unsigned char c=(unsigned char)viewbuf[pos++];text[n++]=(char)((c>=32&&c<127)?c:'.');}
        if(pos<viewlen&&viewbuf[pos]=='\n')++pos;
        text[n]=0;term_text(&terminal,1,row,text,0,78);++row;
        if(pos>=viewlen)break;
    }
}
static void edit_line_col(unsigned pos,unsigned *line,unsigned *col){
    unsigned i,l=0,c=0;
    for(i=0;i<pos&&i<editlen;i++){if(editbuf[i]=='\n'){l++;c=0;}else c++;}
    *line=l;*col=c;
}
static unsigned edit_line_start(unsigned index){
    unsigned i,l=0;if(!index)return 0;
    for(i=0;i<editlen;i++)if(editbuf[i]=='\n'){l++;if(l==index)return i+1;}
    return editlen;
}
static void edit_move(int dline,int dcol){
    if(dcol){if(dcol<0){if(editpos)editpos--;}else if(editpos<editlen)editpos++;return;}
    {unsigned line,col,start,q;edit_line_col(editpos,&line,&col);
     if(dline<0&&!line)return;
     start=edit_line_start(dline>0?line+1:(line?line-1:0));
     q=start;while(q<editlen&&editbuf[q]!='\n')q++;
     editpos=(start+col<q)?start+col:q;}
}
static void edit_insert(char c){
    unsigned i;if(editlen>=EDIT_CAP)return;
    for(i=editlen;i>editpos;i--)editbuf[i]=editbuf[i-1];
    editbuf[editpos++]=c;editlen++;editdirty=1;
}
static void edit_backspace(void){
    unsigned i;if(!editpos)return;
    for(i=editpos-1;i+1<editlen;i++)editbuf[i]=editbuf[i+1];
    editlen--;editpos--;editdirty=1;
}
static void edit_open(panel *p){
    entry *e;unsigned i;int fd,r;
    if(!p->count)return;
    e=&p->item[p->sel];
    if((e->attr&16)||e->name[0]=='.')return;
    if(!editbuf){editbuf=(char *)(uintptr_t)(uint32_t)gui_call(16,EDIT_CAP,0,0,0);if(!editbuf){message(lng_text(CMD_ERR_OPEN));return;}}
    if(!join(source,p->path,e->name)){message(lng_text(CMD_ERR_PATH));return;}
    {uint32_t st[2];if(gui_call(5,0,0,source,st)==0&&st[0]>EDIT_CAP){message(lng_text(CMD_ERR_OPEN));return;}}
    fd=gui_call(2,0,0,source,0);editlen=0;
    if(fd>=0){while(editlen<EDIT_CAP){r=gui_call(3,(uint32_t)fd,(uint32_t)(EDIT_CAP-editlen),editbuf+editlen,0);if(r<=0)break;editlen+=(unsigned)r;}gui_call(4,(uint32_t)fd,0,0,0);}
    for(i=0;i<12&&e->name[i];i++)editname[i]=e->name[i];
    editname[i]=0;editpos=edittop=0;editdirty=0;editing=1;dirty=1;
}
static void edit_save(void){
    if(!join(source,panels[active].path,editname)){message(lng_text(CMD_ERR_PATH));return;}
    message(lng_text(CMD_SAVING));render();
    if(gui_call(60,editlen,0,source,editbuf))message(lng_text(CMD_ERR_SAVE));else{editdirty=0;message(lng_text(CMD_DONE));}
    dirty=1;
}
static void edit_render(void){
    unsigned line,col,start=0,pos,row,n;char text[79];
    edit_line_col(editpos,&line,&col);
    if(line<edittop)edittop=line;
    if(line>edittop+22)edittop=line-22;
    term_clear(&terminal,0);
    term_text(&terminal,1,0,lng_text(CMD_EDIT_TITLE),0,6);
    term_text(&terminal,8,0,editname,0,12);
    if(editdirty)term_text(&terminal,21,0,"*",0,1);
    term_text(&terminal,1,24,lng_text(CMD_EDIT_HELP),0,78);
    {unsigned i;for(i=0;i<edittop;i++){while(start<editlen&&editbuf[start]!='\n')start++;if(start<editlen)start++;}}
    pos=start;row=1;
    while(row<=22){
        n=0;while(pos<editlen&&editbuf[pos]!='\n'&&n<78){unsigned char c=(unsigned char)editbuf[pos++];text[n++]=(char)((c>=32&&c<127)?c:'.');}
        text[n]=0;term_text(&terminal,1,row,text,0,78);
        if(row==line-edittop+1){unsigned cx=col;if(cx>77)cx=77;term_fill(&terminal,1+cx,row,1,1,' ',TERM_INVERSE);}
        ++row;
        if(pos>=editlen)break;
        ++pos;
    }
    {unsigned cr=line-edittop+1;if(cr>=row&&cr<=22){term_fill(&terminal,1,cr,78,1,' ',0);term_fill(&terminal,1+col,cr,1,1,' ',TERM_INVERSE);}}
}
static void render(void){unsigned p,i,n;int r;
 if(!painted_valid)dirty=1;
 if(viewing){view_render();painted_valid=0;dirty=0;panel_damage=0;
  r=rpc(gfx,19,(uint32_t)(uintptr_t)terminal.cells,TERM_CELLS,0);
  if(r<0)fault=19;else used=(unsigned)r;return;}
 if(editing){edit_render();painted_valid=0;dirty=0;panel_damage=0;
  r=rpc(gfx,19,(uint32_t)(uintptr_t)terminal.cells,TERM_CELLS,0);
  if(r<0)fault=19;else used=(unsigned)r;return;}
 if(dirty==4&&!modal){
  for(p=0;p<2;p++){
   if(panel_damage&(1u<<p))panel_view(p);
   else if(painted_active!=active||painted_sel[p]!=panels[p].sel){
    row(p,painted_sel[p],0);row(p,panels[p].sel,p==active);
   }
   if(painted_active!=active){path_row(p);panel_frame(p);}
  }
 }else if(dirty==3&&!modal){
  row(painted_active,painted_sel[painted_active],0);row(active,panels[active].sel,1);
  if(painted_active!=active){path_row(0);path_row(1);panel_frame(0);panel_frame(1);}
 }else if(!(dirty==2&&modal)){
  term_clear(&terminal,0);
  term_text(&terminal,29,0,"COMMANDER",0,9);
  menu_bar();
  for(p=0;p<2;p++){panel_frame(p);panel_view(p);}
  term_fill(&terminal,1,22,78,1,' ',0);
  term_text(&terminal,1,22,"CMD>",0,4);
  term_text(&terminal,6,22,cmdline,0,70);
  {unsigned n=cmdline_len>70u?70u:cmdline_len;
   term_fill(&terminal,6+n,22,1,1,' ',TERM_INVERSE);}
  term_text(&terminal,1,23,status,0,78);
  for(i=0;i<4;i++)term_fill(&terminal,i*20,24,19,1,' ',TERM_INVERSE);
  term_text(&terminal,1,24,lng_text(CMD_COPY),TERM_INVERSE,18);
  term_text(&terminal,21,24,lng_text(CMD_MKDIR),TERM_INVERSE,18);
  term_text(&terminal,41,24,lng_text(CMD_TRASH),TERM_INVERSE,18);
  term_text(&terminal,61,24,lng_text(CMD_QUIT),TERM_INVERSE,18);
  if(menu==3)file_popup();else menu_popup();
 }
 if(modal){term_fill(&terminal,12,8,56,9,' ',0);term_frame(&terminal,12,8,56,9,0);
  term_text(&terminal,14,9,batch_copy&&modal==5?lng_text(CMD_COPY_GROUP):modal==10?lng_text(CMD_RENAME_TITLE):modal==7?lng_text(CMD_NEW_TITLE):modal==5?lng_text(CMD_COPY_TITLE):modal==6?lng_text(CMD_RESTORE_TITLE):modal==8?lng_text(CMD_TRASH_TITLE):lng_text(CMD_DELETE_TITLE),0,52);
  if(modal==7||modal==10)term_text(&terminal,14,11,input,0,52);
  else{n=len(source);term_text(&terminal,14,11,n>52?source+n-52:source,0,52);
   if(modal==5||modal==6){n=len(target);term_text(&terminal,14,12,n>52?target+n-52:target,0,52);}}
  term_fill(&terminal,14,15,22,1,' ',TERM_INVERSE);term_fill(&terminal,40,15,26,1,' ',TERM_INVERSE);
  term_text(&terminal,15,15,modal==7?lng_text(CMD_CREATE_CONFIRM):lng_text(CMD_YES),TERM_INVERSE,20);
  term_text(&terminal,41,15,lng_text(CMD_CANCEL),TERM_INVERSE,24);
 }
 painted_active=active;painted_sel[0]=panels[0].sel;painted_sel[1]=panels[1].sel;
 dirty=0;panel_damage=0;painted_valid=1;
 r=rpc(gfx,19,(uint32_t)(uintptr_t)terminal.cells,TERM_CELLS,0);
 if(r<0)fault=19;else used=(unsigned)r;
}
static void result(int r){
 if(!r)message(lng_text(CMD_DONE));
 else if(r==-FAT_WRITE_EXISTS)message(lng_text(CMD_ERR_EXISTS));
 else if(r==-FAT_WRITE_FULL)message(lng_text(CMD_ERR_FULL));
 else if(r==-FAT_WRITE_NOT_EMPTY)message(lng_text(CMD_ERR_NOT_EMPTY));
 else if(r==-FAT_WRITE_READ_ONLY)message(lng_text(CMD_ERR_READ_ONLY));
 else if(r==-FAT_WRITE_LIMIT)message(lng_text(CMD_ERR_LIMIT));
 else if(r==-FAT_WRITE_IO)message(lng_text(CMD_ERR_IO));
 else message(lng_text(CMD_ERR_INVALID));
 refresh();
}
static void copy_group(void){panel *p=&panels[active];int r=0;
 while(p->marks){
  if(!join(source,p->path,p->marked[0])||!join(target,panels[active^1].path,p->marked[0])){r=-FAT_WRITE_INVALID;break;}
  r=gui_call(49,0,0,source,target);if(r)break;
  unmark(p,0);
 }
 batch_copy=0;result(r);
}
#ifndef COMMANDER_SELFTEST
/* The TOS launcher is part of the real Commander; COMTEST (the PC-relative
   self-test build) stays small, otherwise the -mpcrel relocation overflows. */
static int has_tos_ext(const char *path)
{
    unsigned n=len(path);
    char a,b,c;
    if(n<4u||path[n-4u]!='.')return 0;
    a=path[n-3u];
    b=path[n-2u];
    c=path[n-1u];
    if(a>='a')a=(char)(a-32);
    if(b>='a')b=(char)(b-32);
    if(c>='a')c=(char)(c-32);
    if(a=='P'&&b=='R'&&c=='G')return 1;
    if(a=='T'&&b=='O'&&c=='S')return 1;
    return 0;
}
#endif
static void run_program(const char *path)
{
    gui_spawn_t request;uint32_t result[2];gui_message_t mm;unsigned guard;
    /* TOS .PRG/.TOS: hand over to the TOS personality (syscall 65). It runs the
       program synchronously and restores the exception vectors before
       returning, so the Commander redraws and continues afterwards. */
#ifndef COMMANDER_SELFTEST
    if(has_tos_ext(path)){
        char msg[28];char num[12];int32_t rc;
        message(lng_text(CMD_STARTING));render();
        /* The persona owns the console screen while it runs (it draws into the
           same card memory), so do not release/re-acquire through the GUI
           service here: that round trip was tearing the Commander down.  The
           TOS exit code is shown so the return path is visible. */
        rc=gui_call(65,0,0,(void *)path,0);
        copytext(msg,"TOS RC ",sizeof(msg));
        number(num,(uint32_t)(rc<0?-rc:rc));
        copytext(msg+7,num,sizeof(msg)-7);
        message(msg);refresh();render();
        return;
    }
#endif
    zero(&request,sizeof(request));
    copytext(request.path,path,sizeof(request.path));
    message(lng_text(CMD_STARTING));render();
    if(screen_captured)rpc(gui,7,0,0,0);
    if(gui_call(34,0,0,&request,0)){message(lng_text(CMD_ERR_START));}
    else{
        for(guard=0;guard<6000u;++guard){
            result[0]=0;
            if(gui_call(32,request.pid,1,result,0)==0&&result[0]==5)break;
            while(receive(&mm,0)==0)queue_input(&mm);
            pause_tick();
        }
    }
    if(screen_captured)rpc(gui,6,0,0,0);
    refresh();render();
}
static int in_trash(const char *path){const char *t="\\TRASH";unsigned i;
 for(i=0;i<6;i++){char c=path[i];if(c>='a'&&c<='z')c-=32;if(c!=t[i])return 0;}
 return path[6]==0||path[6]=='\\';}
static void parent(panel *p){unsigned n=len(p->path);if(n<=1)return;
 {unsigned e=n,s;while(e>1&&p->path[e-1]=='\\')--e;
  s=e;
  while(s>1&&p->path[s-1]!='\\')--s;
  {unsigned j=0;while(s+j<e&&j<12){p->focus[j]=p->path[s+j];++j;}p->focus[j]=0;}}
 while(n>1&&p->path[n-1]!='\\')n--;
 if(n>1)n--;
 p->path[n]=0;p->top=p->sel=0;load(p);}
/* 'cd' typed on the command line also moves the active panel (like NC). The
   shell still runs it, so the two stay consistent. */
static void command_cd(void)
{
    char path[64];panel *p=&panels[active];const char *arg;unsigned n,i;
    if((cmdline[0]!='c'&&cmdline[0]!='C')||(cmdline[1]!='d'&&cmdline[1]!='D'))return;
    if(cmdline[2]!=' '&&cmdline[2]!='\t'&&cmdline[2])return;
    arg=cmdline+2;while(*arg==' ')++arg;
    if(!*arg)return;
    if(arg[0]=='\\'||arg[1]==':')copytext(path,arg+(arg[1]==':'?2:0),64);
    else if(arg[0]=='.'&&arg[1]=='.'&&arg[2]==0){
        n=len(p->path);while(n>1&&p->path[n-1]=='\\')--n;
        while(n>1&&p->path[n-1]!='\\')--n;
        if(n>1)--n;
        if(n>=64)n=63;
        for(i=0;i<n;++i)path[i]=p->path[i];
        path[n]=0;
        if(!path[0]){path[0]='\\';path[1]=0;}
    }else if(!join(path,p->path,arg))return;
    {uint32_t st[2];if(gui_call(5,0,0,path,st)||!(st[1]&0x10u))return;}
    copytext(p->path,path,64);p->top=p->sel=0;p->focus[0]=0;load(p);
}
/* 'fdd' lists the floppy; 'fdd NAME' views a file.  The kernel forwards to the
   ROM's FAT12 reader (handoff callbacks), so no floppy code lives in the app.
   Kept out of the self-test build: that -mpcrel image is already near its
   PC-relative range and does not exercise the command line. */
#ifndef COMMANDER_SELFTEST
static int is_fdd_command(void)
{
    return (cmdline[0]=='f'||cmdline[0]=='F')&&(cmdline[1]=='d'||cmdline[1]=='D')&&
           (cmdline[2]=='d'||cmdline[2]=='D')&&(cmdline[3]==0||cmdline[3]==' ');
}
static void command_fdd(void)
{
    char *buf;int n;uint32_t i,m;const char *arg=cmdline+3;
    buf=(char *)(uintptr_t)(uint32_t)gui_call(16,VIEW_CAP,0,0,0);
    if(!buf){message("NO MEMORY");dirty=1;return;}
    message("FLOPPY...");render();
    while(*arg==' ')++arg;
    n=*arg?(int)gui_call(64,VIEW_CAP,0,(void *)arg,buf):(int)gui_call(63,VIEW_CAP,0,buf,0);
    if(n<=0){gui_call(17,0,0,buf,0);message(n<0?"FDD: NO DISK":"FDD: EMPTY");dirty=1;return;}
    if(!viewbuf){viewbuf=(char *)(uintptr_t)(uint32_t)gui_call(16,VIEW_CAP,0,0,0);viewcap=VIEW_CAP;}
    if(viewbuf){m=(uint32_t)n;if(m>viewcap)m=viewcap;for(i=0;i<m;++i)viewbuf[i]=buf[i];viewlen=m;copytext(viewname,"FDD",13);viewtop=0;viewing=1;}
    gui_call(17,0,0,buf,0);
    dirty=1;
}
#endif
/* Run the typed line through SHELL.SYS. The kernel queues it, runs it in the
   shell's own context, and captures the console output into our buffer. */
static void run_command_line(void)
{
    char *buf;uint32_t i,n;int state;
    if(!cmdline_len)return;
#ifndef COMMANDER_SELFTEST
    if(is_fdd_command()){command_fdd();cmdline_len=0;cmdline[0]=0;return;}
#endif
    command_cd();
    buf=(char *)(uintptr_t)(uint32_t)gui_call(16,4096u,0,0,0);
    if(!buf){cmdline_len=0;cmdline[0]=0;message("NO MEMORY");dirty=1;return;}
    message("RUNNING...");render();
    if(gui_call(61,4096u,0,cmdline,buf)){gui_call(17,0,0,buf,0);message("COMMAND FAILED");dirty=1;return;}
    cmdline_len=0;cmdline[0]=0;   /* submitted: start the next command clean */
    state=-1;
    for(i=0;i<20000u&&state<0;++i){state=(int)gui_call(62,0,0,0,0);if(state<0)pause_tick();}
    n=state>0?(uint32_t)state:0u;
    if(n){
        if(!viewbuf){viewbuf=(char *)(uintptr_t)(uint32_t)gui_call(16,VIEW_CAP,0,0,0);viewcap=VIEW_CAP;}
        if(viewbuf){
            if(n>viewcap)n=viewcap;
            for(i=0;i<n;++i)viewbuf[i]=buf[i];
            viewlen=n;copytext(viewname,"COMMAND",13);viewtop=0;viewing=1;
        }
    }else message("DONE");
    status[0]=0;               /* the captured text is the feedback */
    gui_call(17,0,0,buf,0);
    dirty=1;
}
static void key(const gui_message_t *m){unsigned scan=m->v[1]&127;char c=(char)m->v[2];panel *p=&panels[active];entry *e;
 if((m->v[1]>>24)!=1)return;
 if(!editing&&!viewing&&!modal&&!menu){
  if(scan==14&&cmdline_len){cmdline[--cmdline_len]=0;dirty=1;return;}
  if(scan==28&&cmdline_len){run_command_line();return;}
  if(scan==1&&cmdline_len){cmdline_len=0;cmdline[0]=0;dirty=1;return;}
  if(c>=32&&c<127&&(cmdline_len||(c!='+'&&c!='-'))){
   if(cmdline_len<63){cmdline[cmdline_len++]=c;cmdline[cmdline_len]=0;dirty=1;}
   return;}
 }
 if(editing){
  if(scan==0x44||scan==1)editing=0;
  else if(scan==0x3c)edit_save();
  else if(scan==0x48)edit_move(-1,0);
  else if(scan==0x50)edit_move(1,0);
  else if(scan==0x4b)edit_move(0,-1);
  else if(scan==0x4d)edit_move(0,1);
  else if(scan==14)edit_backspace();
  else if(scan==28)edit_insert('\n');
  else if(c>=32&&c<127)edit_insert(c);
  dirty=1;return;}
 if(viewing){
  if(scan==1||scan==0x3d||scan==0x44){viewing=0;dirty=1;}
  else if(scan==0x48){if(viewtop)viewtop--;dirty=1;}
  else if(scan==0x50){viewtop++;dirty=1;}
  else if(scan==0x49){viewtop=viewtop>22?viewtop-22:0;dirty=1;}
  else if(scan==0x51){viewtop+=22;dirty=1;}
  else if(scan==0x47){viewtop=0;dirty=1;}
  return;}
 if(modal){dirty=2;
 if(scan==1||((modal!=7&&modal!=10)&&(c=='n'||c=='N'))){modal=0;batch_copy=0;message(lng_text(CMD_CANCELLED));return;}
 if(modal==7||modal==10){if(scan==14){if(length)input[--length]=0;}
 else if(scan==28&&length){
  if(modal==10){if(join(target,p->path,input)){modal=0;message(lng_text(CMD_RENAMING));render();result(gui_call(54,0,0,source,target));}else message(lng_text(CMD_ERR_PATH));}
  else{if(join(target,p->path,input)){modal=0;message(lng_text(CMD_CREATING));render();result(gui_call(50,0,0,target,0));}else message(lng_text(CMD_ERR_PATH));}}
 else if(length<12&&((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')){input[length++]=c;input[length]=0;}}
 else if(c=='y'||c=='Y'){unsigned op=modal;modal=0;message(lng_text(CMD_WORKING));render();if(op==5&&batch_copy){copy_group();return;}result(gui_call(op==5?49:op==6?54:op==8?53:55,0,0,source,(op==5||op==6)?target:0));}
 return;}
 if(menu){
 if(scan==0x4b||scan==0x4d){
  if(scan==0x4b)menu=(menu==1)?3u:(menu==2?1u:2u);
  else menu=(menu==1)?3u:(menu==3?2u:1u);
  menu_sel=(menu==3)?0:panels[menu-1].sort;dirty=1;return;}
 if(menu==3){
  if(scan==1||scan==0x43){menu=0;dirty=1;return;}
  if(scan==0x48){if(menu_sel)menu_sel--;dirty=1;return;}
  if(scan==0x50){if(menu_sel<6)menu_sel++;dirty=1;return;}
  if(scan==28){file_run(menu_sel);return;}
  return;}
 if(scan==1||scan==0x43){menu=0;dirty=1;return;}
 if(scan==0x48){if(menu_sel)menu_sel--;dirty=1;return;}
 if(scan==0x50){if(menu_sel<2)menu_sel++;dirty=1;return;}
 if(scan==28){apply_sort();return;}
 if(c=='n'||c=='N'){menu_sel=0;apply_sort();return;}
 if(c=='d'||c=='D'){menu_sel=1;apply_sort();return;}
 if(c=='t'||c=='T'){menu_sel=2;apply_sort();return;}
 return;}
 if(scan==0x43){menu=active+1;menu_sel=panels[active].sort;dirty=1;return;}
 if(scan==0x44){quit=1;return;}   /* only F10/Quit closes; Esc cancels overlays */
 if(scan==0x3b){message(lng_text(CMD_HELP));return;}
 if(scan==0x3d){view_open(p);return;}
 if(scan==0x3e){edit_open(p);return;}
 if(scan==0x40&&!in_trash(p->path)){if(!p->count)return;e=&p->item[p->sel];if(e->name[0]=='.')return;if(!join(source,p->path,e->name)){message(lng_text(CMD_ERR_PATH));return;}copytext(input,e->name,13);length=len(input);modal=10;dirty=1;return;}
 if(scan==15){active^=1;click_valid=0;selection_dirty();return;}
 if(scan==14){parent(p);return;}
 if(scan==0x48){if(p->sel)p->sel--;else if(p->top){p->top--;load(p);}click_valid=0;selection_dirty();return;}
 if(scan==0x50){if(p->sel+1<p->count)p->sel++;else if(p->more){p->top++;load(p);}click_valid=0;selection_dirty();return;}
 if(scan==0x49){p->top=p->top>ROWS?p->top-ROWS:0;p->sel=0;load(p);return;}
 if(scan==0x51){if(p->more){p->top+=ROWS;p->sel=0;load(p);}return;}
 if(scan==0x47){p->top=p->sel=0;load(p);return;}
 if(scan==0x41){modal=7;length=0;input[0]=0;dirty=1;return;}
 if(scan==0x3f&&p->marks){char n[12];batch_copy=1;modal=5;number(n,p->marks);copytext(source,n,64);copytext(target,panels[active^1].path,64);dirty=1;return;}
 if(!p->count)return;
 e=&p->item[p->sel];
 if(c=='+'||c=='-'||scan==0x4e||scan==0x4a){int at;
  if(e->attr&16)return;
  at=marked_index(p,e->name);
  if(c=='+'||scan==0x4e){if(at<0){if(p->marks==MARK_LIMIT){message(lng_text(CMD_MARK_LIMIT));return;}copytext(p->marked[p->marks++],e->name,13);}}
  else if(at>=0)unmark(p,(unsigned)at);
  panel_damage|=1u<<active;if(dirty!=1)dirty=4;click_valid=0;return;
 }
 if(scan==28){                               /* Enter: katalog -> wejdz, plik -> uruchom */
     if(e->name[0]=='.'){parent(p);return;}
     if(e->attr&16){if(join(source,p->path,e->name)){copytext(p->path,source,64);p->top=p->sel=0;p->focus[0]=0;load(p);}return;}
     if(join(source,p->path,e->name))run_program(source);
     return;}
 if(scan!=0x3f&&scan!=0x40&&scan!=0x42)return;
 if(scan==0x40&&(!in_trash(p->path)||in_trash(panels[active^1].path))){message(lng_text(CMD_RESTORE_HELP));return;}
 if(e->name[0]=='.'){message(lng_text(CMD_SELECT_ITEM));return;}
 if(scan==0x3f&&(e->attr&16)){message(lng_text(CMD_COPY_FILES));return;}
 if(!join(source,p->path,e->name)||!join(target,panels[active^1].path,e->name)){message(lng_text(CMD_ERR_PATH));return;}
 if(scan==0x42&&in_trash(source)&&len(source)==6){message(lng_text(CMD_KEEP_TRASH));return;}
 modal=scan==0x3f?5:scan==0x40?6:in_trash(p->path)?9:8;dirty=1;
}
static void action(unsigned scan,unsigned character){gui_message_t m;zero(&m,sizeof(m));m.v[1]=0x01000000u|scan;m.v[2]=character;key(&m);}
static void mouse(const gui_message_t *m){unsigned buttons=(m->v[2]>>24)&31,down=buttons&3;
 unsigned press=(down&~mouse_buttons);int x=(int)(m->v[1]>>16),y=(int)(m->v[1]&65535);
 unsigned p,index;uint32_t now;
 if(!down&&!mouse_buttons){press=(buttons>>3)&3;if(!press&&(buttons&4))press=2;}
 mouse_buttons=down;
 if(!press||x<0||x>=640||y<0||y>=400)return;
 if(modal){click_valid=0;if(!(press&2))return;
  if(y>=240&&y<256){if(x>=112&&x<288)action(modal==7?28:0,modal==7?0:'y');else if(x>=320&&x<528)action(1,0);}return;}
 if(y>=384){click_valid=0;if(press&2){if(x<160)action(0x3f,0);else if(x<320)action(0x41,0);else if(x<480)action(0x42,0);else action(0x44,0);}return;}
 if(y<16){click_valid=0;if(press){if(x>=144&&x<224){if(menu==3)menu=0;else{menu=3;menu_sel=0;}dirty=1;return;}unsigned p2=(x>=320);if(menu==p2+1)menu=0;else{menu=p2+1;menu_sel=panels[p2].sort;active=p2;}dirty=1;}return;}
 if(menu){unsigned px=(menu==1)?0u:320u;
  if(menu==3){if(x>=144&&x<304&&y>=48&&y<160){menu_sel=(unsigned)(y-48)/16u;if(menu_sel>6)menu_sel=6;file_run(menu_sel);return;}menu=0;dirty=1;return;}
  if(x>=(int)px&&x<(int)(px+128)&&y>=48&&y<96){menu_sel=(unsigned)(y-48)/16u;if(menu_sel>2)menu_sel=2;apply_sort();return;}
  menu=0;dirty=1;return;}
 if(y<16||y>=352)return;
 p=x>=320;if(x-(int)(p*320)<8||x-(int)(p*320)>=312)return;
 if(active!=p){active=p;selection_dirty();}
 if(press&1){click_valid=0;parent(&panels[p]);return;}
 if(!(press&2))return;
 if(y>=320&&y<336){click_valid=0;action((x-(int)(p*320))<160?0x49:0x51,0);return;}
 if(y<64||y>=320)return;
 index=(unsigned)(y-64)/16;if(index>=panels[p].count)return;
 now=(uint32_t)ticks();panels[p].sel=index;selection_dirty();
 if(click_valid&&last_click_panel==p&&last_click_index==panels[p].top+index&&now-last_click_tick<=20u){click_valid=0;action(28,0);}
 else{click_valid=1;last_click_panel=p;last_click_index=panels[p].top+index;last_click_tick=now;}
}
static void handle(const gui_message_t *m){if(m->sender)return;if(m->v[0]==0x4b4b4559)key(m);else if(m->v[0]==0x4b4d4f55)mouse(m);}
#include "../tests/commander_selftest.inc"
int app_main(void){gui_message_t m;int registered=0,captured=0;
 lng_init();
 sortbuf=(entry *)(uintptr_t)(uint32_t)gui_call(16,256u*(uint32_t)sizeof(entry),0,0,0);
 if(!sortbuf)return 1;
 gfx=lookup("SYSTEM.GFX");gui=lookup("SYSTEM.GUI");if((int32_t)gfx<=0||(int32_t)gui<=0)return 1;
 if(rpc(gui,1,0,0,0))return 2;
 registered=1;
 if(rpc(gui,6,0,0,0)){fault=3;goto done;}captured=1;screen_captured=1;
 if(rpc(gfx,13,0,0,0)||rpc(gfx,1,0,0,0)!=(int32_t)((640u<<16)|400u)){fault=4;goto done;}
 copytext(panels[0].path,"\\",64);copytext(panels[1].path,"\\SYSTEM\\APP",64);
 message(lng_text(CMD_WELCOME));refresh();
#ifdef COMMANDER_SELFTEST
 commander_selftest();goto done;
#endif
 while(!quit&&!fault){unsigned n=0;int got;
  do{if(head!=tail){unsigned j;gui_message_t *d=&pending[head++&15];for(j=0;j<sizeof(m);j++)((volatile uint8_t *)&m)[j]=((uint8_t *)d)[j];got=1;}
  else got=receive(&m,0)==0;
  if(got)handle(&m);
  }while(got&&++n<16&&!quit&&!fault);
  if(quit||fault)break;
  if(dirty)render();
  if(head==tail){if(receive(&m,1))break;handle(&m);}
 }

 done:if(captured)rpc(gui,7,0,0,0);if(registered)rpc(gui,2,0,0,0);
#ifdef COMMANDER_SELFTEST
 {char n[12];gui_call(1,0,0,fault?"COMTEST: FAIL\n":"COMTEST: MOUSE/PAGE/DRAW PASS\n",0);
 gui_call(1,0,0,"PAGE TICKS: ",0);number(n,bench_page);gui_call(1,0,0,n,0);
 gui_call(1,0,0," / LEGACY TICKS: ",0);number(n,bench_single);gui_call(1,0,0,n,0);gui_call(1,0,0,"\n",0);}
#endif
 return fault;
}
