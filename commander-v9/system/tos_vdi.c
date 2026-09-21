#include "tos_vdi.h"
#include "wm_blit.h"
#include "emutos_vdi_mono.h"
#include "kernel_blitter.h"
#include "font.h"
/* Our own VDI core. Drawing goes through wm_blit primitives so it lands in
   the application's compositor surface (option A: no GPL code here). */
static vdi_ws_t ws[VDI_MAX_WORKSTATIONS];
static uint8_t *vdi_target;
static uint16_t vdi_pitch;
static uint16_t vdi_w,vdi_h;
/* Diagnostic: last v_justified (GDP 11/10) request seen. */
uint32_t tos_vdi_just_n;
int16_t tos_vdi_just_pts[6];
uint16_t tos_vdi_just_ctrl[8];
char tos_vdi_just_txt[24];
uint16_t tos_vdi_just_iw[8];
uint16_t tos_vdi_just_pw[8];
uint32_t tos_vdi_just_intin,tos_vdi_just_ptsin;
uint16_t tos_vdi_just_fiw[16];
int16_t tos_vdi_just_fpts[4];
uint32_t tos_vdi_just_f_n;
/* Ring of the last 8 v_justified strings (12 words) with their origin. */
uint16_t tos_vdi_hist[8][12];
int16_t tos_vdi_hist_pts[8][2];
uint16_t tos_vdi_hist_ctrl[8][6];
uint32_t tos_vdi_hist_i;
/* Ring of the last 8 v_gtext requests: the first 12 int_in words, the origin
   and the contrl block, so the call ABI can be read back from a live run. */
uint16_t tos_vdi_gtext[8][12];
int16_t tos_vdi_gtext_pts[8][2];
uint16_t tos_vdi_gtext_ctrl[8][6];
uint32_t tos_vdi_gtext_i;
/* How often each VDI opcode was called - shows which paint paths SysInfo uses. */
uint32_t tos_vdi_opcount[256];
/* How much the VDI actually painted: lets us compare the ink on screen with
   what went through our VDI at all (the rest is the app drawing itself). */
uint32_t tos_vdi_fill_calls;
uint32_t tos_vdi_fill_pixels;
/* Filled pixels broken down by the VDI opcode that caused them. */
uint32_t tos_vdi_pixels_per_op[256];
static uint16_t tos_vdi_cur_op;

void tos_vdi_set_surface(uint8_t *surface,uint16_t pitch){vdi_target=surface;vdi_pitch=pitch;}
const vdi_ws_t *tos_vdi_workstation(void){return &ws[0];}

void tos_vdi_init(void)
{
    unsigned i;
    for(i=0;i<VDI_MAX_WORKSTATIONS;i++){
        ws[i].width=640;ws[i].height=400;
        ws[i].fg=1;ws[i].bg=0;
        ws[i].line_color=1;ws[i].text_color=1;ws[i].fill_color=1;
        ws[i].line_width=1;ws[i].line_type=1;
        ws[i].text_height=16;ws[i].text_align=0;
        ws[i].interior=1;ws[i].fill_style=1;
        ws[i].clip[0]=0;ws[i].clip[1]=0;ws[i].clip[2]=639;ws[i].clip[3]=399;
        ws[i].open=0;
    }
    vdi_target=0;vdi_pitch=80;vdi_w=640;vdi_h=400;
}

static wm_fb_t vdi_fb(void)
{
    wm_fb_t d;
    d.data=vdi_target;d.pitch=vdi_pitch;
    d.w=(uint16_t)(vdi_w? vdi_w:640);d.h=(uint16_t)(vdi_h? vdi_h:400);d.bpp=1;
    return d;
}

static void vdi_fill(int x0,int y0,int x1,int y1,int color)
{
    wm_fb_t d=vdi_fb();
    if(!d.data)return;
    if(x0>x1){int t=x0;x0=x1;x1=t;}
    if(y0>y1){int t=y0;y0=y1;y1=t;}
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>(int)d.w-1)x1=(int)d.w-1;
    if(y1>(int)d.h-1)y1=(int)d.h-1;
    if(x1<x0||y1<y0)return;
    ++tos_vdi_fill_calls;
    {uint32_t npx=(uint32_t)(x1-x0+1)*(uint32_t)(y1-y0+1);
     tos_vdi_fill_pixels+=npx;
     tos_vdi_pixels_per_op[tos_vdi_cur_op&0xffu]+=npx;}
    /* Fastest path: hardware blitter, but ONLY when the kernel decided it is
       present and working (accelerator cards are known to disagree with the
       blitter, so this is never assumed). */
    if(kblit_enabled()&&d.bpp==1){
        if(kblit_fill_planar((volatile uint16_t *)d.data,(uint16_t)(d.pitch/2u),1u,
                             (uint16_t)x0,(uint16_t)y0,
                             (uint16_t)(x1-x0+1),(uint16_t)(y1-y0+1),
                             (uint8_t)(color?1u:0u)))return;
    }
    /* Next best: the unrolled planar mono fill the kernel graphics uses. */
    emutos_vdi_mono_fill((volatile uint16_t *)d.data,(uint16_t)(d.pitch/2u),
                         (uint16_t)x0,(uint16_t)y0,
                         (uint16_t)(x1-x0+1),(uint16_t)(y1-y0+1),
                         (uint8_t)(color?1u:0u));
}

/* Bresenham; the same shape EmuTOS uses, written from scratch. */
static void vdi_line(int x0,int y0,int x1,int y1,int color)
{
    int dx=x1-x0;int dy=y1-y0;int sx=1,sy=1;int err;
    if(dx<0){dx=-dx;sx=-1;}
    if(dy<0){dy=-dy;sy=-1;}
    err=dx-dy;
    for(;;){
        vdi_fill(x0,y0,x0,y0,color);
        if(x0==x1&&y0==y1)break;
        {int e2=err+err;
         if(e2>-dy){err-=dy;x0+=sx;}
         if(e2<dx){err+=dx;y0+=sy;}}
    }
}

/* Scanline fill of a polygon given as ptsin-style vertex pairs. */
static void vdi_poly_fill(const int16_t *pts,int n,int color)
{
    int i,k,j,ymin,ymax;
    if(!pts||n<3)return;
    ymin=ymax=pts[1];
    for(i=1;i<n;i++){int y=pts[2*i+1];if(y<ymin)ymin=y;if(y>ymax)ymax=y;}
    for(j=ymin;j<=ymax;j++){
        int xints[32],nx=0;
        for(i=0;i<n&&nx<31;i++){
            int k2=(i+1)%n;
            int y1=pts[2*i+1],y2=pts[2*k2+1];
            if((y1<=j&&y2>j)||(y2<=j&&y1>j)){
                int x1=pts[2*i],x2=pts[2*k2];
                xints[nx++]=x1+(int)(((long)(j-y1)*(x2-x1))/(y2-y1));
            }
        }
        for(i=0;i+1<nx;i++)
            for(k=i+1;k<nx;k++)
                if(xints[k]<xints[i]){int t=xints[i];xints[i]=xints[k];xints[k]=t;}
        for(i=0;i+1<nx;i+=2)vdi_fill(xints[i],j,xints[i+1],j,color);
    }
}

static int vdird16(const uint8_t *b){return (int)(int16_t)(((uint16_t)b[0]<<8)|b[1]);}
static uint32_t vdird32(const uint8_t *b)
{return ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];}
static int vdib_get(const uint8_t *base,int pitch,int w,int h,int x,int y)
{
    if(!base||x<0||y<0||x>=w||y>=h)return 0;
    return (base[(uint32_t)y*pitch+(x>>3)]>>(7-(x&7)))&1u;
}
static void vdib_set(uint8_t *base,int pitch,int w,int h,int x,int y,int v)
{
    uint8_t *p;uint8_t m;
    if(!base||x<0||y<0||x>=w||y>=h)return;
    p=base+(uint32_t)y*pitch+(x>>3);m=(uint8_t)(0x80u>>(x&7));
    if(v)*p|=m;else *p&=(uint8_t)~m;
}
/* VDI 109 vro_cpyfm / 110 vrt_cpyfm: blit between two MFDBs, or between an
   MFDB and the screen when the MFDB address is 0.  The AES uses this for
   bitmaps/icons and SysInfo uses it for its own rendered text - without it
   those parts of the UI simply never appear. */
static void vdi_cpyfm(int is_raster,const int16_t *intin,const int16_t *ptsin,
                      void **addrin)
{
    const uint8_t *smf,*dmf;
    const uint8_t *sbase;uint8_t *dbase;
    int spitch,dpitch,sw,sh,dw,dh,wrmode,fg=1,bg=0;
    int i,j,cols,rows;
    if(!ptsin||!addrin)return;
    smf=(const uint8_t *)(uintptr_t)addrin[0];
    dmf=(const uint8_t *)(uintptr_t)addrin[1];
    if(!smf||!dmf)return;
    spitch=vdird16(smf+8)*2;sw=vdird16(smf+4);sh=vdird16(smf+6);
    dpitch=vdird16(dmf+8)*2;dw=vdird16(dmf+4);dh=vdird16(dmf+6);
    sbase=(const uint8_t *)(uintptr_t)vdird32(smf+12);
    dbase=(uint8_t *)(uintptr_t)vdird32(dmf+12);
    if(!sbase){sbase=vdi_target;spitch=vdi_pitch;sw=vdi_w;sh=vdi_h;}
    if(!dbase){dbase=vdi_target;dpitch=vdi_pitch;dw=vdi_w;dh=vdi_h;}
    if(!sbase||!dbase)return;
    if(is_raster){fg=intin?intin[0]:1;bg=intin?intin[1]:0;wrmode=intin?intin[2]:0;}
    else wrmode=intin?intin[0]:0;
    cols=ptsin[6]-ptsin[4];rows=ptsin[7]-ptsin[5];
    if(cols<0)cols=-cols;
    if(rows<0)rows=-rows;
    for(j=0;j<=rows;j++)for(i=0;i<=cols;i++){
        int sx=ptsin[0]+i,sy=ptsin[1]+j,dx=ptsin[4]+i,dy=ptsin[5]+j,v;
        v=vdib_get(sbase,spitch,sw,sh,sx,sy);
        if(is_raster)v=v?fg:bg;
        if(wrmode==1)v=(v||vdib_get(dbase,dpitch,dw,dh,dx,dy))?1:0;
        else if(wrmode==2)v^=vdib_get(dbase,dpitch,dw,dh,dx,dy);
        else if(wrmode==3)v=(v&&vdib_get(dbase,dpitch,dw,dh,dx,dy))?1:0;
        vdib_set(dbase,dpitch,dw,dh,dx,dy,v);
    }
}

int tos_vdi_call(uint16_t opcode,int16_t *control,const int16_t *intin,
                 const int16_t *ptsin,int16_t *intout,int16_t *ptsout,
                 void **addrin,void **addrout)
{
    (void)ptsout;(void)control;(void)addrout;
    ++tos_vdi_opcount[opcode&0xffu];
    tos_vdi_cur_op=(uint16_t)(opcode&0xffu);
    switch(opcode){
    case VDI_V_OPNVWK:
    case VDI_V_OPNWK:
        {unsigned slot;
         for(slot=0;slot<VDI_MAX_WORKSTATIONS;slot++)if(!ws[slot].open)break;
         if(slot==VDI_MAX_WORKSTATIONS)slot=0;
         ws[slot].open=1;
         vdi_w=ws[slot].width;vdi_h=ws[slot].height;
         if(intout){
             unsigned k;
             for(k=0;k<45;k++)intout[k]=0;
             /* Standard DEV_TAB: [0]=max x, [1]=max y, ... [13]=colours-1.
                The workstation handle is returned in contrl[6], NOT intout[0].
                Putting it in intout[0] shifted the whole work_out table, so a
                program reading maxx=work_out[0]/maxy=work_out[1] saw 1 x 640
                and laid its whole page out transposed. */
             intout[0]=(int16_t)(ws[slot].width?ws[slot].width-1:639);
             intout[1]=(int16_t)(ws[slot].height?ws[slot].height-1:399);
             intout[2]=1;intout[3]=1;
             intout[4]=1;intout[5]=1;intout[6]=1;intout[7]=1;
             intout[8]=1;intout[9]=1;intout[10]=1;intout[11]=2;intout[12]=0;
             intout[13]=1;   /* number of colours - 1 (monochrome) */
             intout[14]=1;intout[15]=1;intout[16]=(int16_t)ws[slot].text_height;
             if(control)((int16_t *)(uintptr_t)control)[6]=(int16_t)(control[6]?control[6]:1);
         }
         return 1;}
    case VDI_V_CLSWK:
    case VDI_V_CLSVWK:
    case VDI_V_CLRWK:
        vdi_fill(0,0,(int)vdi_w-1,(int)vdi_h-1,(int)ws[0].bg);
        ws[0].open=0;
        return 0;
    case VDI_V_BAR:
        /* Opcode 11 is the GDP dispatcher: contrl[5] selects the primitive
           (1=v_bar, 10=v_justified, ...).  Treating it as a plain filled bar
           made every SysInfo text draw a bogus rectangle. */
        if(control&&control[5]==10){                 /* v_justified: text */
            int dk;
            ++tos_vdi_just_n;
            if(ptsin)for(dk=0;dk<4;dk++)tos_vdi_just_pts[dk]=ptsin[dk];
            if(control)for(dk=0;dk<6;dk++)tos_vdi_just_ctrl[dk]=(uint16_t)control[dk];
            if(intin)for(dk=0;dk<8;dk++)tos_vdi_just_iw[dk]=(uint16_t)intin[dk];
            if(ptsin)for(dk=0;dk<8;dk++)tos_vdi_just_pw[dk]=(uint16_t)ptsin[dk];
            tos_vdi_just_intin=(uint32_t)(uintptr_t)intin;
            tos_vdi_just_ptsin=(uint32_t)(uintptr_t)ptsin;
            {uint32_t hi=tos_vdi_hist_i%8u;int hk;
             tos_vdi_hist_i++;
             if(intin)for(hk=0;hk<12;hk++)tos_vdi_hist[hi][hk]=(uint16_t)intin[hk];
             if(ptsin){tos_vdi_hist_pts[hi][0]=ptsin[0];tos_vdi_hist_pts[hi][1]=ptsin[1];}
             if(control)for(hk=0;hk<6;hk++)tos_vdi_hist_ctrl[hi][hk]=(uint16_t)control[hk];}
            if(tos_vdi_just_f_n<1u&&intin){
                int fk;
                tos_vdi_just_f_n++;
                for(fk=0;fk<16;fk++)tos_vdi_just_fiw[fk]=(uint16_t)intin[fk];
                if(ptsin)for(fk=0;fk<4;fk++)tos_vdi_just_fpts[fk]=ptsin[fk];
            }
            if(intin){const char *ds=(const char *)(const void *)intin;
                for(dk=0;dk<23&&ds[dk];dk++)tos_vdi_just_txt[dk]=ds[dk];
                tos_vdi_just_txt[dk]=0;}
            if(ptsin&&intin&&vdi_target){
                /* SysInfo's GEM library hands v_justified the text as one
                   16-bit character per word, preceded by two header words.
                   SysInfo packs its strings back to back with no NUL
                   separators, so the only reliable length is contrl[3]
                   (the int_in word count) minus the two header words: scanning
                   for a terminating zero draws stale bytes from the previous
                   longer call.  The kernel font is stored MSB-first, so the
                   character walk must go forwards, left to right, with bit 7
                   as the leftmost pixel -- exactly the v_gtext convention.
                   (The previous LSB-first draw plus a reversed character walk
                   produced a fully mirrored rendering.) */
                const uint16_t *w=(const uint16_t *)(const void *)intin;
                int x=ptsin[0],y=ptsin[1];
                int tall=(ws[0].text_height>=16)?1:0;
                int h=tall?16:8;
                int color=(int)ws[0].text_color?1:0;
                int k,n=(control?(int)control[3]:0)-2;
                if(n<0)n=0;
                if(n>126)n=126;
                for(k=0;k<n;k++){
                    int ch=w[2+k]&0xff;
                    const uint8_t *g;
                    int row,col;
                    if(!ch)continue;
                    g=font_glyph((char)ch,(uint8_t)tall);
                    for(row=0;row<h;row++){
                        uint8_t bits=g[row];
                        if(!bits)continue;
                        for(col=0;col<8;col++)
                            if(bits&(0x80u>>col))
                                vdi_fill(x+col,y-row-1,x+col,y-row-1,color);
                    }
                    x+=8;
                }
            }
            return 0;
        }
        if(ptsin)vdi_fill(ptsin[0],ptsin[1],ptsin[2],ptsin[3],
                          (int)ws[0].fill_color?1:0);
        return 0;
    case VDI_V_FILLAREA:
        /* VDI: contrl[1] is the point count and ptsin holds the vertices.
           Filling only the bounding box made every non-rectangular area a
           solid block, so polygons are scan-filled here. */
        {int n=(control?control[1]:0);
         if(n>=3&&ptsin)vdi_poly_fill(ptsin,n,(int)ws[0].fill_color?1:0);
         else if(ptsin)vdi_fill(ptsin[0],ptsin[1],ptsin[2],ptsin[3],
                                (int)ws[0].fill_color?1:0);}
        return 0;
    case VDI_V_PLINE:
        if(ptsin){
            /* VDI: the number of points is contrl[1] (int_in is unused for
               v_pline).  Reading intin[0] drew a wrong number of segments, so
               SysInfo's own vector drawing came out incomplete/garbled. */
            int n=control?control[1]:0;
            int k;
            if(n<2)n=intin?intin[0]:0;
            for(k=0;k+1<n;k++)
                vdi_line(ptsin[2*k],ptsin[2*k+1],ptsin[2*k+2],ptsin[2*k+3],
                         (int)ws[0].line_color);
        }
        return 0;
    /* --- attribute setters (SysInfo initialises these after v_opnvwk) --- */
    case VDI_VSL_TYPE:    if(intin)ws[0].line_type=intin[0];return 0;
    case VDI_VSL_WIDTH:   if(intin)ws[0].line_width=intin[0];return 0;
    case VDI_VSL_COLOR:   if(intin)ws[0].line_color=intin[0];return 0;
    case VDI_VSL_ENDS:    if(intin){ws[0].ends[0]=intin[0];ws[0].ends[1]=intin[1];}return 0;
    case VDI_VST_COLOR:   if(intin)ws[0].text_color=intin[0];return 0;
    case VDI_VST_FONT:    if(intin)ws[0].font=intin[0];return 0;
    case VDI_VST_EFFECTS: if(intin)ws[0].effects=intin[0];return 0;
    case VDI_VST_ROTATION:if(intin)ws[0].rotation=intin[0];return 0;
    case VDI_VST_HEIGHT:                       /* 2 ptsin points define a box */
        if(ptsin&&ptsout){
            int h=ptsin[1]-ptsin[3];if(h<0)h=-h;
            if(h>0)ws[0].text_height=(int16_t)h;
            ptsout[0]=(int16_t)h;
        }
        return 0;
    case VDI_VST_ALIGN:   if(intin)ws[0].text_align=intin[0];return 0;
    case VDI_VSM_TYPE:    if(intin)ws[0].marker_type=intin[0];return 0;
    case VDI_VSM_HEIGHT:  if(intin)ws[0].marker_height=intin[0];return 0;
    case VDI_VSM_COLOR:   if(intin)ws[0].marker_color=intin[0];return 0;
    case VDI_VSF_INTERIOR:if(intin)ws[0].interior=intin[0];return 0;
    case VDI_VSF_STYLE:   if(intin)ws[0].fill_style=intin[0];return 0;
    case VDI_VSF_COLOR:   if(intin)ws[0].fill_color=intin[0];return 0;
    case VDI_VSF_PERIMETER:if(intin)ws[0].perimeter=intin[0];return 0;
    case VDI_VSWR_MODE:   if(intin)ws[0].wrt_mode=intin[0];return 0;
    case VDI_VS_CLIP:
        if(intin&&ptsin){ws[0].clip_on=(uint8_t)intin[0];
            ws[0].clip[0]=ptsin[0];ws[0].clip[1]=ptsin[1];
            ws[0].clip[2]=ptsin[2];ws[0].clip[3]=ptsin[3];}
        return 0;
    /* --- attribute queries --- */
    case VDI_VQT_ATTRS:
        if(intout){intout[0]=1;intout[1]=ws[0].text_color;intout[2]=ws[0].rotation;
                   intout[3]=ws[0].text_align;intout[4]=0;intout[5]=ws[0].wrt_mode;}
        if(ptsout){int16_t th=ws[0].text_height?ws[0].text_height:16;
                   ptsout[0]=8;ptsout[1]=(int16_t)(th-1);ptsout[2]=8;ptsout[3]=th;}
        return 0;
    case VDI_VQL_ATTRS:
        if(intout){intout[0]=ws[0].line_type;intout[1]=1;intout[2]=ws[0].line_color;
                   intout[3]=ws[0].line_width;}
        if(ptsout){ptsout[0]=1;ptsout[1]=1;}
        return 0;
    case VDI_VQM_ATTRS:
        if(intout){intout[0]=ws[0].marker_type;intout[1]=ws[0].marker_height;
                   intout[2]=ws[0].marker_color;}
        if(ptsout){ptsout[0]=8;ptsout[1]=8;}
        return 0;
    case VDI_VQF_ATTRS:
        if(intout){intout[0]=ws[0].interior;intout[1]=ws[0].fill_style;
                   intout[2]=ws[0].fill_color;intout[3]=0;intout[4]=0;}
        return 0;
    case VDI_VQ_EXTND:
        /* intin[0]=0 -> the standard extended table; intin[0]>=1 -> NVDI's
           vq_scrninfo, which returns the pitch and the physical screen base.
           A program that paints the screen itself (SysInfo) needs both, and
           the earlier all-ones answer gave it a 1-byte stride. */
        if(intout){
            unsigned k;for(k=0;k<45;k++)intout[k]=0;
            if(intin&&intin[0]>=1){
                uint32_t base=(uint32_t)(uintptr_t)vdi_target;
                intout[0]=0;      /* planar, not interleaved */
                intout[1]=1;      /* hardware CLUT */
                intout[2]=1;      /* planes */
                intout[3]=2;intout[4]=2;
                intout[5]=(int16_t)vdi_pitch;
                intout[6]=(int16_t)(base>>16);intout[7]=(int16_t)base;
                intout[8]=1;intout[9]=1;intout[10]=1;
                intout[12]=0;intout[14]=1;
            }else{
                intout[0]=4;intout[1]=2;intout[2]=1;intout[3]=1;intout[4]=1;
                intout[5]=1;intout[8]=0;intout[13]=2;intout[16]=1;
                intout[30]=0;intout[31]=0;
            }
        }
        if(ptsout){ptsout[0]=0;ptsout[1]=0;ptsout[2]=639;ptsout[3]=399;
                   ptsout[4]=639;ptsout[5]=399;ptsout[6]=0;ptsout[7]=0;}
        return 0;
    case VDI_V_GET_PIXEL:
        if(intout)intout[0]=0;
        return 0;
    case VDI_VQT_EXTENT:
        {int n=(control?control[3]:0);int w=n*8;int16_t th=ws[0].text_height?ws[0].text_height:16;
         if(ptsout){ptsout[0]=0;ptsout[1]=0;ptsout[2]=(int16_t)w;ptsout[3]=0;
                    ptsout[4]=(int16_t)w;ptsout[5]=th;ptsout[6]=0;ptsout[7]=th;}}
        return 0;
    case VDI_VQT_WIDTH:
        if(intout){intout[0]=8;intout[1]=8;intout[2]=0;}
        if(ptsout){ptsout[0]=8;ptsout[1]=0;}
        return 0;
    case VDI_V_SHOW_C:
    case VDI_V_HIDE_C:
        return 0;
    case VDI_VQ_MOUSE:
        if(intout)intout[0]=0;
        if(ptsout){ptsout[0]=0;ptsout[1]=0;}
        return 0;
    case VDI_VQ_KEY_S:
        if(intout)intout[0]=0;
        return 0;
    case VDI_V_GTEXT:
        /* v_gtext(x, y, string): the string is a NUL-terminated byte string in
           intin and ptsin[0..1] is the baseline origin.  Without this SysInfo's
           whole UI stayed a black rectangle, because GEM draws all its text
           through here. */
        {uint32_t hi=tos_vdi_gtext_i%8u;int hk;
         tos_vdi_gtext_i++;
         if(intin)for(hk=0;hk<12;hk++)tos_vdi_gtext[hi][hk]=(uint16_t)intin[hk];
         if(ptsin){tos_vdi_gtext_pts[hi][0]=ptsin[0];tos_vdi_gtext_pts[hi][1]=ptsin[1];}
         if(control)for(hk=0;hk<6;hk++)tos_vdi_gtext_ctrl[hi][hk]=(uint16_t)control[hk];}
        if(ptsin&&intin&&vdi_target){
            const char *s=(const char *)(const void *)intin;
            int x=ptsin[0],y=ptsin[1];
            int tall=(ws[0].text_height>=16)?1:0;
            int h=tall?16:8;
            int color=(int)ws[0].text_color?1:0;
            while(*s){
                const uint8_t *g=font_glyph(*s,(uint8_t)tall);
                int row,col;
                for(row=0;row<h;row++){
                    uint8_t bits=g[row];
                    if(!bits)continue;
                    for(col=0;col<8;col++)
                        if(bits&(0x80u>>col))
                            vdi_fill(x+col,y-row-1,x+col,y-row-1,color);
                }
                x+=8; ++s;
            }
        }
        return 0;
    case VDI_VRO_CPYFM:
        vdi_cpyfm(0,intin,ptsin,addrin);
        return 0;
    case VDI_VRT_CPYFM:
        vdi_cpyfm(1,intin,ptsin,addrin);
        return 0;
    default:
        return 0;
    }
}

/* --- helpers used by the AES object library (objc_draw) ------------------- */
void tos_vdi_fill_rect(int x,int y,int w,int h,int color)
{
    if(w<=0||h<=0)return;
    vdi_fill(x,y,x+w-1,y+h-1,color?1:0);
}
void tos_vdi_text(int x,int y,const char *s,int color,int tall)
{
    int h=tall?16:8;
    if(!s)return;
    for(;*s;++s){
        const uint8_t *g=font_glyph(*s,(uint8_t)(tall?1:0));
        int row,col;
        for(row=0;row<h;row++){
            uint8_t bits=g[row];
            if(!bits)continue;
            for(col=0;col<8;col++)
                if(bits&(0x80u>>col))
                    vdi_fill(x+col,y-row-1,x+col,y-row-1,color?1:0);
        }
        x+=8;
    }
}
