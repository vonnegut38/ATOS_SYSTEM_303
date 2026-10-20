/* Minimal GDOS layer.
 *
 * LDW Power (and every GDOS application) probes the VDI for the fonts the
 * system has loaded:  vst_load_fonts (119) must answer with a COUNT, vqt_name
 * (130) with a font name, vqt_fontinfo (131) with metrics.  Without those the
 * program concludes there is no font system and gives up during start-up.
 *
 * What is implemented:  ASSIGN.SYS parsing, the bitmap .FNT header, the font
 * list and the selection state.  What is NOT:  outline (Speedo) rendering,
 * metafiles and printer drivers - those stay future work and are reported as
 * absent rather than faked.
 */
#include "tos_gdos.h"
#include "kernel_fat.h"

static tos_gdos_t gdos;
uint32_t kdbg_gdos_loaded,kdbg_gdos_rejected;   /* header probe outcome */
uint8_t  kdbg_gdos_head[16];                     /* first bytes the kernel actually read */
uint32_t kdbg_gdos_got,kdbg_gdos_fsize;
static uint16_t   gdos_selected;

/* ---- small helpers ------------------------------------------------------- */
static int ci_eq(char a, char b)
{
    if(a>='a'&&a<='z')a=(char)(a-32);
    if(b>='a'&&b<='z')b=(char)(b-32);
    return a==b;
}

static int str_ends_with_fnt(const char *s, unsigned len)
{
    if(len<4u)return 0;
    return ci_eq(s[len-4],'.')&&ci_eq(s[len-3],'F')&&ci_eq(s[len-2],'N')&&ci_eq(s[len-1],'T');
}

/* ASSIGN.SYS -> devices + font list.  Format (Atari GDOS):
     path = <dir>
     NNp <driver>.sys [; comment]      device NN (01..04 screen, 21.. printer, 31 MF)
     <font>.fnt [; comment]            a font belonging to the device above
   Comments start at ';' and are optional. */
int tos_gdos_assign_parse(const char *text, unsigned len, tos_gdos_t *out)
{
    unsigned i=0;
    uint16_t dev=0;

    if(!text||!out)return 0;
    out->count=0;out->screen_dev=0;out->printer_dev=0;out->path[0]=0;out->parsed=1;

    while(i<len){
        char line[80];
        unsigned n=0,start=i,j;
        while(i<len&&text[i]!='\n'&&n<sizeof(line)-1u){line[n++]=text[i++];}
        while(i<len&&text[i]!='\n')++i;             /* skip an over-long line */
        if(i<len&&text[i]=='\n')++i;
        line[n]=0;
        if(n==start)continue;
        /* strip the comment and trailing whitespace */
        for(j=0;j<n;++j)if(line[j]==';'){n=j;break;}
        while(n&&(line[n-1]==' '||line[n-1]=='\t'||line[n-1]=='\r'))--n;
        line[n]=0;
        /* leading whitespace */
        {unsigned k=0;while(k<n&&(line[k]==' '||line[k]=='\t'))++k;
         if(k){unsigned m;for(m=0;m+k<=n;++m)line[m]=line[m+k];n-=k;line[n]=0;}}
        if(!n)continue;

        if((n>=4u)&&ci_eq(line[0],'p')&&ci_eq(line[1],'a')&&ci_eq(line[2],'t')&&ci_eq(line[3],'h')){
            unsigned k=4;while(k<n&&(line[k]==' '||line[k]=='\t'))++k;
            if(k<n&&line[k]=='=')++k;
            while(k<n&&(line[k]==' '||line[k]=='\t'))++k;
            {unsigned t=0;while(k<n&&t<sizeof(out->path)-1u)out->path[t++]=line[k++];
             out->path[t]=0;}
            continue;
        }
        /* "NNp driver" device line? */
        {unsigned k=0;uint16_t id=0;
         while(k<n&&line[k]>='0'&&line[k]<='9'){id=(uint16_t)(id*10u+(uint16_t)(line[k]-'0'));++k;}
         /* Device lines are "01p screen.sys" for the screen, but this
            ASSIGN.SYS writes the printer/metafile entries as "21 fx80.sys"
            (no 'p'), so accept digits followed by 'p' OR by whitespace. */
         if(k>0&&k<n){
             unsigned q=k;
             if(line[q]=='p'||line[q]=='P')++q;
             if(q<n&&(line[q]==' '||line[q]=='\t')){
                 int known=(id>=1u&&id<=4u)||(id>=21u&&id<=31u)||(id>=36u&&id<=42u);
                 if(known){
                     dev=id;
                     if(id>=1u&&id<=4u&&!out->screen_dev)out->screen_dev=id;
                     if(id>=21u&&id<=30u&&!out->printer_dev)out->printer_dev=id;
                     continue;
                 }
             }
         }}
        /* otherwise a font file name */
        if(str_ends_with_fnt(line,n)&&out->count<TOS_GDOS_MAX_FONTS&&dev){
            tos_gdos_font_t *f=&out->font[out->count];
            unsigned k=0,t=0;
            {unsigned z;for(z=0;z<sizeof(*f);++z)((uint8_t *)f)[z]=0;}
            while(k<n&&t<sizeof(f->file)-1u&&line[k]!=' '&&line[k]!='\t')
                f->file[t++]=line[k++];
            f->file[t]=0;
            f->dev=dev;
            ++out->count;
        }
    }
    return (int)out->count;
}

/* Full Fonthead probe.  Layout from EmuTOS include/fonthdr.h:
     +0 WORD font_id, +2 WORD point, +4 char name[32], +36 first_ade, +38 last_ade,
     +40 top, +42 ascent, +44 half, +46 descent, +48 bottom, +50 max_char_width,
     +52 max_cell_width, ... +66 flags, ... +84 next_font, +88 reserved (90 bytes).
   Byte order is NOT assumed: the GDOS printer fonts shipped with LDW Power are
   little-endian (0e 00 = ID 14, 18 00 = 24 pt) while standard screen fonts are
   Motorola/big-endian.  The header is accepted in whichever order validates. */
static uint16_t be16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint16_t le16(const uint8_t *p){return (uint16_t)(((uint16_t)p[1]<<8)|p[0]);}

int tos_gdos_fnt_header(const uint8_t *data, unsigned len, tos_gdos_font_t *out)
{
    unsigned i,n=0;
    int be_ok,le_ok;
    if(!data||!out||len<90u)return 0;
    /* point must be a plausible size; that alone separates the two orders on
       every real file seen so far (0x1800 = 6144 vs 24). */
    be_ok=(be16(data+2)>=4u&&be16(data+2)<=200u);
    le_ok=(le16(data+2)>=4u&&le16(data+2)<=200u);
    if(be_ok&&le_ok){
        /* both plausible: let the flags decide (F_STDFORM = 4 = Motorola) */
        uint16_t fb=be16(data+66),fl=le16(data+66);
        if((fl&4u)&&!(fb&4u))be_ok=0;
        else if((fb&4u)&&!(fl&4u))le_ok=0;
    }
    if(!be_ok&&!le_ok)return 0;
    out->big_endian=(uint8_t)(be_ok?1u:0u);
    {   uint16_t (*w)(const uint8_t *)=be_ok?be16:le16;
        out->point=w(data+2);
        out->font_id=w(data+0);
        out->first_ade=w(data+36);
        out->last_ade=w(data+38);
        out->top=w(data+40);
        out->ascent=w(data+42);
        out->descent=w(data+46);
        out->bottom=w(data+48);
        out->max_char_width=w(data+50);
        out->max_cell_width=w(data+52);
        out->flags=w(data+66);
        out->form_width=w(data+80);
        out->form_height=w(data+82);
    }
    for(i=0u;i<32u&&i+4u<len&&n+1u<sizeof(out->name);++i){
        char c=(char)data[4u+i];
        if(!c)break;
        if(c<32||c>126)break;
        out->name[n++]=c;
    }
    out->name[n]=0;
    /* Validation beyond the header: a real bitmap font has a sane character
       range and a raster that fits the file.  An outline/metafile entry fails
       this and is reported as not loaded. */
    if(out->first_ade>out->last_ade)return 0;
    if(out->last_ade<32u||out->first_ade>0x200u)return 0;
    if(out->form_height==0u||out->form_height>128u)return 0;
    out->bitmap=1u;
    return 1;
}

int tos_gdos_fnt_probe(const uint8_t *data, unsigned len,
                       char *name, unsigned name_len, uint16_t *point, uint8_t *bitmap)
{
    tos_gdos_font_t tmp;
    unsigned i;
    if(!data||len<90u)return 0;
    for(i=0;i<sizeof(tmp);++i)((uint8_t *)&tmp)[i]=0;
    if(!tos_gdos_fnt_header(data,len,&tmp))return 0;
    if(point)*point=tmp.point;
    if(bitmap)*bitmap=tmp.bitmap;
    if(name&&name_len){
        unsigned n=0;
        while(n+1u<name_len&&tmp.name[n]){name[n]=tmp.name[n];++n;}
        name[n]=0;
    }
    return 1;
}

/* ---- boot-time loading --------------------------------------------------- */
static const char *const assign_candidates[]={
    "\\ASSIGN.SYS","\\SYSTEM\\ASSIGN.SYS","\\SYSTEM\\TEST\\ASSIGN.SYS",
    "\\GDOS.FNT\\ASSIGN.SYS", 0
};

/* Fonts for one device.  ASSIGN.SYS assigns four sizes to the printer (21) and
   four to the metafile device (31); the SCREEN (1..4) has no extra fonts, so a
   screen workstation must report only the built-in one. */
uint16_t tos_gdos_device_count(uint16_t dev)
{
    unsigned i;uint16_t n=0;
    for(i=0u;i<gdos.count;++i)if(gdos.font[i].dev==dev)++n;
    return n;
}
uint16_t tos_gdos_loaded_count(uint16_t dev)
{
    unsigned i;uint16_t n=0;
    for(i=0u;i<gdos.count;++i)if(gdos.font[i].dev==dev&&gdos.font[i].loaded)++n;
    return n;
}

/* Retryable: a failure before the disk is ready must not latch forever, so the
   state is NOT_READY (0) -> READY (1) or FAILED (2) and only READY stops us. */
void tos_gdos_boot_load(void)
{
    static uint8_t buffer[4096];
    uint32_t size=0;uint8_t attr=0;int i;
    if(gdos.state==1u)return;                 /* already loaded */
    for(i=0;assign_candidates[i];++i){
        uint32_t got=0;
        if(kfat_stat_path(assign_candidates[i],&size,&attr)!=FAT_FILE_FOUND)continue;
        if(size<4u||size>sizeof(buffer))continue;
        if(kfat_load_path(assign_candidates[i],buffer,size,&got)!=FAT_FILE_FOUND||got<4u)continue;
        (void)tos_gdos_assign_parse((const char *)buffer,(unsigned)got,&gdos);
        /* Probe every font header with kfat_read_path_at(): kfat_load_path()
           demands room for the WHOLE file, so a 64-byte header buffer rejected
           all eight fonts of the LDW disk (536..29696 bytes each). */
        {unsigned k;
         for(k=0u;k<gdos.count;++k){
             char path[96];unsigned p=0,q=0;uint32_t fsize=0;uint8_t fa=0;
             static uint8_t head[90];
             uint32_t got2=0;
             while(gdos.path[p]&&p<sizeof(path)-24u)path[p]=gdos.path[p],++p;
             if(p&&path[p-1]!='\\'&&path[p-1]!='/')path[p++]='\\';
             while(gdos.font[k].file[q]&&p<sizeof(path)-1u)path[p++]=gdos.font[k].file[q++];
             path[p]=0;
             if(kfat_stat_path(path,&fsize,&fa)!=FAT_FILE_FOUND){gdos.font[k].loaded=0u;continue;}
             if(fsize<90u){gdos.font[k].loaded=0u;continue;}
             if(kfat_read_path_at(path,0u,head,sizeof(head),&got2)!=FAT_FILE_FOUND||got2<90u){
                 gdos.font[k].loaded=0u;continue;
             }
             {unsigned dbg;for(dbg=0u;dbg<16u;++dbg)kdbg_gdos_head[dbg]=head[dbg];
              kdbg_gdos_got=got2;kdbg_gdos_fsize=fsize;}
             if(!tos_gdos_fnt_header(head,(unsigned)got2,&gdos.font[k])){
                 gdos.font[k].loaded=0u;++kdbg_gdos_rejected;continue;   /* not a usable bitmap font */
             }
             gdos.font[k].loaded=1u;++kdbg_gdos_loaded;
         }}
        gdos.loaded_count=tos_gdos_loaded_count(gdos.screen_dev);
        gdos.state=1u;
        return;
    }
    gdos.state=2u;                            /* FAILED: may be retried later */
}

uint16_t tos_gdos_font_count(void)
{
    /* What vst_load_fonts reports: the ADDITIONAL fonts of the screen
       workstation.  The LDW disk has none (its entries belong to the printer
       and metafile devices), so an honest answer here is 0. */
    tos_gdos_boot_load();
    return tos_gdos_loaded_count(gdos.screen_dev);
}
const tos_gdos_font_t *tos_gdos_font(unsigned index)
{
    if(index>=gdos.count)return 0;
    return &gdos.font[index];
}
void tos_gdos_select(uint16_t id){gdos_selected=id;}
uint16_t tos_gdos_selected(void){return gdos_selected;}
