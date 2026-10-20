#include "tos_vdi.h"
void tos_note_dirty_rect(int x0,int y0,int x1,int y1);
#include "wm_blit.h"
#include "emutos_vdi_mono.h"
#include "tos_gdos.h"
#include "kernel_blitter.h"
#include "font.h"
/* LEGACY / EXPERIMENTAL (ATOS v10, Phase 15).
 * Frozen: do NOT add new VDI opcodes, fonts, colours or dialogs here.
 * Only fixes required to run the compatibility tests are allowed.  The future
 * compatibility path is being analysed in docs/EMUTOS-INTEGRATION.md; the new
 * architecture routes drawing through video_driver_t, not through this file. */
/* Our own VDI core. Drawing goes through wm_blit primitives so it lands in
   the application's compositor surface (option A: no GPL code here). */
static vdi_ws_t ws[VDI_MAX_WORKSTATIONS];
static uint8_t *vdi_target;
static uint16_t vdi_pitch;
static uint16_t vdi_w,vdi_h;
/* The handle from v_opnvwk (contrl[6]) selects the workstation; every drawing
   op used ws[0] before, so a second v_opnvwk silently redirected all output
   into the first (or into a zeroed) slot. */
static uint8_t vdi_cur;
static vdi_ws_t *vdi_ws(void){return &ws[vdi_cur<VDI_MAX_WORKSTATIONS?vdi_cur:0u];}
/* vs_clip/ptsin rectangle actually honoured by vdi_fill (the audit: clipping
   and the write modes were stored and then ignored, so a partial redraw could
   overwrite the rest of the client). */
static int vdi_clip_on;
static int16_t vdi_clip[4];
static int vdi_wrt_mode=1;
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
uint32_t tos_vdi_glyph_fast;
uint32_t kdbg_vdi_dbgptr[5];      /* pointers the VDI received */
uint16_t kdbg_vdi_dbgname[4];   /* what vqt_name actually wrote */
uint16_t kdbg_vdi_dbginfo[4];   /* what vqt_fontinfo actually wrote to intout */   /* glyphs drawn through mono_glyph (word masks) */
uint32_t tos_vdi_fill_pixels;
/* Filled pixels broken down by the VDI opcode that caused them. */
uint32_t tos_vdi_pixels_per_op[256];
static uint16_t tos_vdi_cur_op;

void tos_vdi_set_surface(uint8_t *surface,uint16_t pitch){vdi_target=surface;vdi_pitch=pitch;}
/* Read back the current VDI target so a caller can restore it (the TOS window
   redirects the VDI to the program's own screen and must put it back). */
void tos_vdi_get_surface(uint8_t **surface,uint16_t *pitch)
{
    if(surface)*surface=vdi_target;
    if(pitch)*pitch=vdi_pitch;
}
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

/* Set while the VDI target is the TOS program's own screen: then every fill
   reports the rows it touched, and the personality never has to scan 32 KB to
   discover what the program drew. */
static uint8_t vdi_row_dirty_hook;
void tos_vdi_set_dirty_hook(uint8_t on){vdi_row_dirty_hook=on;}
/* Below this many pixels the CPU planar fill beats programming the blitter. */
#define TOS_VDI_BLIT_MIN_PIXELS 64u

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
    if(vdi_clip_on){
        if(x0<vdi_clip[0])x0=vdi_clip[0];
        if(y0<vdi_clip[1])y0=vdi_clip[1];
        if(x1>vdi_clip[2])x1=vdi_clip[2];
        if(y1>vdi_clip[3])y1=vdi_clip[3];
    }
    if(x1<x0||y1<y0)return;
    /* Every VDI fill reports the rows it touched, so a GEM program's changes
       are known WITHOUT scanning the 32 KB screen (only direct screen writers
       need the periodic fallback scan). */
    if(vdi_row_dirty_hook)tos_note_dirty_rect(x0,y0,x1,y1);
    ++tos_vdi_fill_calls;
    {uint32_t npx=(uint32_t)(x1-x0+1)*(uint32_t)(y1-y0+1);
     tos_vdi_fill_pixels+=npx;
     tos_vdi_pixels_per_op[tos_vdi_cur_op&0xffu]+=npx;
     /* blitter only when the area pays for its setup (see the threshold) */
     if(kblit_enabled()&&d.bpp==1&&npx>=TOS_VDI_BLIT_MIN_PIXELS){
        if(kblit_fill_planar((volatile uint16_t *)d.data,(uint16_t)(d.pitch/2u),1u,
                             (uint16_t)x0,(uint16_t)y0,
                             (uint16_t)(x1-x0+1),(uint16_t)(y1-y0+1),
                             (uint8_t)(color?1u:0u)))return;
     }}
    /* Next best: the unrolled planar mono fill the kernel graphics uses. */
    emutos_vdi_mono_fill((volatile uint16_t *)d.data,(uint16_t)(d.pitch/2u),
                         (uint16_t)x0,(uint16_t)y0,
                         (uint16_t)(x1-x0+1),(uint16_t)(y1-y0+1),
                         (uint8_t)(color?1u:0u));
}

/* Bresenham; the same shape EmuTOS uses, written from scratch. */
/* Line style: 1 solid, 2 long dash, 3 dot, 4 dash-dot, 5 dash-dot-dot,
   6 user pattern (approximated by solid).  The pattern walks along the line and
   suppresses the pixels of the gaps, which is what a mono VDI does. */
/* One glyph renderer for every text path (v_justified, v_gtext and the AES
   tos_vdi_text).  The font stores row 0 at the TOP of the character, so it must
   land on the HIGHEST row of the cell: the old y-row-1 put row 0 at the bottom
   and mirrored every letter vertically (the audit reproduced readable text by
   switching to y-h+row; the cell still spans [y-h, y-1]).  Metrics/vertical
   alignment belong here once the font tables carry them. */
/* Optional pixel clip for glyph spans.  vdi_fill honours the application's
   vs_clip, but the AES renderer must clip text to objc_draw's rectangle WITHOUT
   disturbing the application's own clip state, so glyph spans intersect this
   context instead.  The audit measured a solid 8x16 glyph with a 1x1 clip
   asking for 128 pixels in 16 spans, all of them outside the clip. */
static int vdi_text_clip_on;
static int16_t vdi_text_clip[4];
void tos_vdi_text_clip(int x0,int y0,int x1,int y1)
{
    vdi_text_clip[0]=(int16_t)x0;vdi_text_clip[1]=(int16_t)y0;
    vdi_text_clip[2]=(int16_t)x1;vdi_text_clip[3]=(int16_t)y1;
    vdi_text_clip_on=1;
}
void tos_vdi_text_clip_off(void){vdi_text_clip_on=0;}

static void vdi_glyph(int x,int y,int h,char ch,int tall,int color)
{
    const uint8_t *g=font_glyph(ch,(uint8_t)tall);
    int row,col;
    /* Word-mask fast path (audit item C): a complete 8x16 cell, mono planar
       surface, replace mode, fully inside the surface AND inside both clips.
       mono_glyph() writes masked words row by row, so a glyph becomes 16 word
       updates instead of one vdi_fill per lit run - and the damage is ONE
       rectangle for the whole cell instead of one per row.  mono_glyph() does
       not clip the buffer, so the position is validated here before calling it
       (the audit's warning); anything else uses the clipped span path below. */
    if((h==8||h==16)&&vdi_ws()->wrt_mode==1u){
        wm_fb_t d=vdi_fb();
        int top=y-h;
        int inside=(d.data&&d.bpp==1&&x>=0&&top>=0&&x+8<=(int)d.w&&y<=(int)d.h);
        if(inside&&vdi_text_clip_on)
            inside=(x>=vdi_text_clip[0]&&x+7<=vdi_text_clip[2]&&
                    top>=vdi_text_clip[1]&&y-1<=vdi_text_clip[3]);
        if(inside&&vdi_clip_on)
            inside=(x>=vdi_clip[0]&&x+7<=vdi_clip[2]&&
                    top>=vdi_clip[1]&&y-1<=vdi_clip[3]);
        if(inside){
            mono_glyph_h((volatile uint16_t *)d.data,(uint16_t)(d.pitch/2u),
                         (uint16_t)x,(uint16_t)top,g,(uint16_t)h,
                         (uint8_t)(color?1u:0u),0u,0);
            if(vdi_row_dirty_hook)tos_note_dirty_rect(x,top,x+7,y-1);
            ++tos_vdi_glyph_fast;
            return;
        }
    }
    for(row=0;row<h;row++){
        uint8_t bits=g[row];
        if(!bits)continue;
        /* One fill per contiguous run of lit pixels instead of one per pixel:
           a letter row is usually a single span, so this turns up to 64 calls
           per glyph into a handful - and each call is a real span fill, not a
           single pixel with its own bounds/clipping/damage bookkeeping. */
        col=0;
        while(col<8){
            int start;
            if(!(bits&(0x80u>>col))){++col;continue;}
            start=col;
            while(col<8&&(bits&(0x80u>>col)))++col;
            {   int sx=x+start,ex=x+col-1,sy=y-h+row;
                if(vdi_text_clip_on){
                    if(sx<vdi_text_clip[0])sx=vdi_text_clip[0];
                    if(ex>vdi_text_clip[2])ex=vdi_text_clip[2];
                    if(sy<vdi_text_clip[1]||sy>vdi_text_clip[3])continue;
                }
                if(sx<=ex)vdi_fill(sx,sy,ex,sy,color);}
        }
    }
}

static int vdi_line_style_on(int style,int step)
{
    switch(style){
    case 2: return ((step/8)%2)==0;                 /* 8 on / 8 off */
    case 3: return (step%4)==0;                     /* dot */
    case 4: return (step%16)<8||(step%16)==12;      /* dash-dot */
    case 5: return (step%20)<8||(step%20)==12||(step%20)==16;
    default: return 1;                              /* 1 and 6: solid */
    }
}
static void vdi_line(int x0,int y0,int x1,int y1,int color)
{
    int dx=x1-x0;int dy=y1-y0;int sx=1,sy=1;int err,step=0;
    int width=vdi_ws()->line_width;int style=vdi_ws()->line_type;
    int k;
    if(width<1)width=1;
    if(width>16)width=16;
    /* Horizontal and vertical lines as ONE span per width step.  The Bresenham
       loop called vdi_fill once per pixel: a 200x120 rectangle outline cost 644
       fill calls, while four spans draw exactly the same picture.  Dashed/dotted
       patterns keep the per-step path (a span cannot express them) and so do the
       non-replace write modes (overlay semantics, especially XOR). */
    if((x0==x1||y0==y1)&&vdi_ws()->wrt_mode==1u&&vdi_line_style_on(style,0)&&
       vdi_line_style_on(style,1)&&(style<=1||style==6)){
        int k;
        for(k=0;k<width;k++){
            if(y0==y1)vdi_fill(x0<x1?x0:x1,y0+k,x0<x1?x1:x0,y0+k,color);
            else       vdi_fill(x0+k,y0<y1?y0:y1,x0+k,y0<y1?y1:y0,color);
        }
        return;
    }
    if(dx<0){dx=-dx;sx=-1;}
    if(dy<0){dy=-dy;sy=-1;}
    err=dx-dy;
    for(;;){
        if(vdi_line_style_on(style,step)){
            /* A wide line is several parallel one-pixel lines: offset across
               the minor axis (the audit: line_width was stored, never used). */
            for(k=0;k<width;k++){
                if(dx>=dy)vdi_fill(x0,y0+k,x0,y0+k,color);
                else vdi_fill(x0+k,y0,x0+k,y0,color);
            }
        }
        ++step;
        if(x0==x1&&y0==y1)break;
        {int e2=err+err;
         if(e2>-dy){err-=dy;x0+=sx;}
         if(e2<dx){err+=dx;y0+=sy;}}
    }
}

/* Scanline fill of a polygon given as ptsin-style vertex pairs. */
static int vdi_hatch_on(int style,int x,int y);
static void vdi_fill_area(int x0,int y0,int x1,int y1,int style);

/* Same scanline fill, but each span is drawn through the hatch mask. */
static void vdi_poly_fill_styled(const int16_t *pts,int n,int ymin,int ymax,int style);
static void vdi_poly_fill_styled(const int16_t *pts,int n,int ymin,int ymax,int style)
{
    int j;
    for(j=ymin;j<=ymax;j++){
        int xints[32],nx=0,i,k;
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
        for(i=0;i+1<nx;i+=2){
            int x;
            for(x=xints[i];x<=xints[i+1];x++)
                if(vdi_hatch_on(style,x,j))vdi_fill(x,j,x,j,1);
        }
    }
}

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
static void vdi_wr16(uint8_t *b,uint16_t v){b[0]=(uint8_t)(v>>8);b[1]=(uint8_t)v;}
/* Filled areas (v_bar / v_fillarea) honour the interior style: 1 = solid,
   2..7 = the standard hatch patterns, 8 = user pattern (approximated by a sparse
   diagonal).  vdi_fill stays solid because lines and text use it as well. */
static int vdi_hatch_on(int style,int x,int y)
{
    switch(style){
    case 2: return (y&1)==0;
    case 3: return (x&1)==0;
    case 4: return ((x+y)&1)==0;
    case 5: return ((x-y)&1)==0;
    case 6: return ((x&1)==0)||((y&1)==0);
    case 7: return (((x+y)&1)==0)||(((x-y)&1)==0);
    case 8: return ((x+y)%4)==0;
    default: return 1;
    }
}
static void vdi_fill_area(int x0,int y0,int x1,int y1,int style)
{
    int x,y;
    if(style<=1||style>8){vdi_fill(x0,y0,x1,y1,1);return;}
    if(x0>x1){int t=x0;x0=x1;x1=t;}
    if(y0>y1){int t=y0;y0=y1;y1=t;}
    for(y=y0;y<=y1;y++)
        for(x=x0;x<=x1;x++)
            if(vdi_hatch_on(style,x,y))vdi_fill(x,y,x,y,1);
}

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
/* MFDB pointers travel in contrl[7..8] (source) and contrl[9..10]
   (destination) as a 32-bit address split over two 16-bit words; addrin is an
   AES concept and is always NULL on the VDI trap.  Reading them from addrin
   made every vro_cpyfm/vrt_cpyfm a silent no-op. */
static const uint8_t *vdi_mfdb_addr(const int16_t *control,int index)
{
    uint32_t a;
    if(!control)return 0;
    a=((uint32_t)(uint16_t)control[index]<<16)|(uint16_t)control[index+1];
    return a?(const uint8_t *)(uintptr_t)a:0;
}

/* The 16 standard VDI write modes (source S, destination D). */
static int vdi_rop(int mode,int s,int d)
{
    switch(mode&15){
    case 0:  return 0;
    case 1:  return s&d;
    case 2:  return s&(d^1);
    case 3:  return s;
    case 4:  return (s^1)&d;
    case 5:  return d;
    case 6:  return s^d;
    case 7:  return s|d;
    case 8:  return (s|d)^1;
    case 9:  return (s^d)^1;
    case 10: return d^1;
    case 11: return s|(d^1);
    case 12: return s^1;
    case 13: return (s^1)|d;
    case 14: return (s&d)^1;
    default: return 1;
    }
}

/* MFDB: fd_addr +0, fd_w +4, fd_h +6, fd_wdwidth +8 (in WORDS), fd_stand +10,
   fd_nplanes +12.  The old code read the address from +12, i.e. from
   nplanes/reserved, and therefore copied from and to arbitrary memory. */
static void vdi_cpyfm(int is_raster,const int16_t *control,const int16_t *intin,
                      const int16_t *ptsin)
{
    const uint8_t *smf=vdi_mfdb_addr(control,7);
    const uint8_t *dmf=vdi_mfdb_addr(control,9);
    const uint8_t *sbase=0;uint8_t *dbase=0;
    int spitch,dpitch,sw,sh,dw,dh,mode=0,fg=1,bg=0;
    int i,j,cols,rows,backwards;
    if(!ptsin)return;
    if(smf){sbase=(const uint8_t *)(uintptr_t)vdird32(smf+0);
            sw=vdird16(smf+4);sh=vdird16(smf+6);spitch=vdird16(smf+8)*2;}
    else{sw=sh=0;spitch=0;}
    if(dmf){dbase=(uint8_t *)(uintptr_t)vdird32(dmf+0);
            dw=vdird16(dmf+4);dh=vdird16(dmf+6);dpitch=vdird16(dmf+8)*2;}
    else{dw=dh=0;dpitch=0;}
    if(!sbase){sbase=vdi_target;spitch=vdi_pitch;sw=vdi_w;sh=vdi_h;}
    if(!dbase){dbase=vdi_target;dpitch=vdi_pitch;dw=vdi_w;dh=vdi_h;}
    if(!sbase||!dbase)return;
    if(is_raster){                       /* vrt_cpyfm: mode FIRST, then colours */
        mode=intin?intin[0]:0;
        fg=intin?intin[1]:1;
        bg=intin?intin[2]:0;
    }else mode=intin?intin[0]:0;         /* vro_cpyfm: only the mode */
    cols=ptsin[6]-ptsin[4];rows=ptsin[7]-ptsin[5];
    if(cols<0)cols=-cols;
    if(rows<0)rows=-rows;
    /* Overlapping copy: going forward when the destination sits after the
       source overwrites source pixels that have not been read yet.  The old
       condition required dx>sx AND dy>=sy, so a straight downward shift at
       dx==sx copied top-down and smeared the source (the audit's finding).
       Memory order decides: later row wins, and only for the same row does the
       column decide. */
    backwards=(sbase==dbase)&&(ptsin[5]>ptsin[1]||(ptsin[5]==ptsin[1]&&ptsin[4]>ptsin[0]));
    /* Word-at-a-time fast path (audit item D): plain 1-bit copy (vro_cpyfm,
       ROP 3) between surfaces of the same geometry and the same bit alignment,
       fully inside both.  A shifted variant needs separate source/destination
       strides, so unequal alignment stays on the pixel path. */
    if(!is_raster&&mode==3&&spitch==dpitch&&sw==dw&&sh==dh&&spitch>0&&
       ((ptsin[0]&15)==(ptsin[4]&15))&&
       ptsin[0]>=0&&ptsin[1]>=0&&ptsin[4]>=0&&ptsin[5]>=0&&
       ptsin[0]+cols<sw&&ptsin[4]+cols<dw&&ptsin[1]+rows<sh&&ptsin[5]+rows<dh){
        int sy0=ptsin[1],dy0=ptsin[5];
        int sx0=ptsin[0],dx0=ptsin[4];
        int g0=sx0>>4,g1=(sx0+cols)>>4,g;
        for(j=0;j<=rows;j++){
            int jj=backwards?rows-j:j;
            const uint16_t *srow=(const uint16_t *)(sbase+(uint32_t)(sy0+jj)*spitch);
            uint16_t *drow=(uint16_t *)(dbase+(uint32_t)(dy0+jj)*dpitch);
            for(g=g0;g<=g1;g++){
                uint16_t mask=(uint16_t)((g==g0?(0xffffu>>(sx0&15)):0xffffu)&
                                         (g==g1?(0xffffu<<(15u-((sx0+cols)&15u))):0xffffu));
                uint16_t v=(uint16_t)(srow[g]&mask);
                drow[g]=(uint16_t)((drow[g]&~mask)|v);
            }
        }
        if(vdi_row_dirty_hook&&dbase==vdi_target)
            tos_note_dirty_rect(dx0<ptsin[0]?dx0:ptsin[0],ptsin[5],ptsin[4]+cols,ptsin[5]+rows);
        return;
    }
    for(j=0;j<=rows;j++){
        int jj=backwards?rows-j:j;
        for(i=0;i<=cols;i++){
            int ii=backwards?cols-i:i;
            int sx=ptsin[0]+ii,sy=ptsin[1]+jj;
            int dx=ptsin[4]+ii,dy=ptsin[5]+jj;
            int v=vdib_get(sbase,spitch,sw,sh,sx,sy);
            if(is_raster)v=v?fg:bg;
            vdib_set(dbase,dpitch,dw,dh,dx,dy,vdi_rop(mode,v,vdib_get(dbase,dpitch,dw,dh,dx,dy)));
        }
    }
    if(vdi_row_dirty_hook&&dbase==vdi_target)
        tos_note_dirty_rect(ptsin[4],ptsin[5],ptsin[4]+cols,ptsin[5]+rows);

}

extern uint32_t kdbg_vdi_dbgptr[5];
int tos_vdi_call(uint16_t opcode,int16_t *control,const int16_t *intin,
                 const int16_t *ptsin,int16_t *intout,int16_t *ptsout,
                 void **addrin,void **addrout)
{
    (void)ptsout;(void)addrout;(void)addrin;
    ++tos_vdi_opcount[opcode&0xffu];
    kdbg_vdi_dbgptr[0]=(uint32_t)(uintptr_t)control;kdbg_vdi_dbgptr[1]=(uint32_t)(uintptr_t)intin;
    kdbg_vdi_dbgptr[2]=(uint32_t)(uintptr_t)ptsin;kdbg_vdi_dbgptr[3]=(uint32_t)(uintptr_t)intout;
    kdbg_vdi_dbgptr[4]=(uint32_t)(uintptr_t)ptsout;
    tos_vdi_cur_op=(uint16_t)(opcode&0xffu);
    switch(opcode){
    case VDI_V_OPNVWK:
    case VDI_V_OPNWK:
        {unsigned slot;
         for(slot=0;slot<VDI_MAX_WORKSTATIONS;slot++)if(!ws[slot].open)break;
         if(slot==VDI_MAX_WORKSTATIONS)slot=0;
         ws[slot].open=1;
         vdi_cur=(uint8_t)slot;                 /* contrl[6] selects it from now on */
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
             intout[13]=2;   /* NUMBER of colours (mono = 2), not colours-1 */
             intout[14]=1;intout[15]=1;intout[16]=(int16_t)ws[slot].text_height;
             /* The handle is the slot + 1 so it is never zero, and contrl[6]
                carries it back to the caller (standard VDI). */
             if(control)((int16_t *)(uintptr_t)control)[6]=(int16_t)(slot+1u);
         }
         return 1;}
    case VDI_V_CLSWK:
    case VDI_V_CLSVWK:
        /* Closing the workstation is NOT the same as clearing it.  Clearing used
           to mark the slot closed, so the next drawing call operated on a
           workstation the VDI considered gone. */
        vdi_ws()->open=0;
        return 0;
    case VDI_V_CLRWK:
        vdi_fill(0,0,(int)vdi_w-1,(int)vdi_h-1,(int)vdi_ws()->bg);
        return 0;
    case VDI_V_BAR:
        /* Opcode 11 is the GDP dispatcher: contrl[5] selects the primitive
           (1=v_bar, 10=v_justified, ...).  Treating it as a plain filled bar
           made every SysInfo text draw a bogus rectangle. */
        if(control&&control[5]==10){                 /* v_justified: text */
            int dk;
            ++tos_vdi_just_n;
            /* Diagnostics must respect the DECLARED lengths (the audit: fixed
               copies of 8/12/16 intin words and 8 ptsin words read past short
               buffers).  contrl[1] = ptsin words, contrl[3] = intin words. */
            {   int np=(control?(int)(control[1]&0x7fffu):0);
                int ni=(control?(int)(control[3]&0x7fffu):0);
                if(ptsin)for(dk=0;dk<4&&dk<np;dk++)tos_vdi_just_pts[dk]=ptsin[dk];
                if(control)for(dk=0;dk<6;dk++)tos_vdi_just_ctrl[dk]=(uint16_t)control[dk];
                if(intin)for(dk=0;dk<8&&dk<ni;dk++)tos_vdi_just_iw[dk]=(uint16_t)intin[dk];
                if(ptsin)for(dk=0;dk<8&&dk<np;dk++)tos_vdi_just_pw[dk]=(uint16_t)ptsin[dk];}
            tos_vdi_just_intin=(uint32_t)(uintptr_t)intin;
            tos_vdi_just_ptsin=(uint32_t)(uintptr_t)ptsin;
            {uint32_t hi=tos_vdi_hist_i%8u;int hk;
             tos_vdi_hist_i++;
             {int ni=(control?(int)(control[3]&0x7fffu):0);
              if(intin)for(hk=0;hk<12&&hk<ni;hk++)tos_vdi_hist[hi][hk]=(uint16_t)intin[hk];}
             if(ptsin){tos_vdi_hist_pts[hi][0]=ptsin[0];tos_vdi_hist_pts[hi][1]=ptsin[1];}
             if(control)for(hk=0;hk<6;hk++)tos_vdi_hist_ctrl[hi][hk]=(uint16_t)control[hk];}
            if(tos_vdi_just_f_n<1u&&intin){
                int fk,ni=(control?(int)(control[3]&0x7fffu):0),np=(control?(int)(control[1]&0x7fffu):0);
                tos_vdi_just_f_n++;
                for(fk=0;fk<16&&fk<ni;fk++)tos_vdi_just_fiw[fk]=(uint16_t)intin[fk];
                if(ptsin)for(fk=0;fk<4&&fk<np;fk++)tos_vdi_just_fpts[fk]=ptsin[fk];
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
                int tall=(vdi_ws()->text_height>=16)?1:0;
                int h=tall?16:8;
                int color=(int)vdi_ws()->text_color?1:0;
                int k,n=(control?(int)control[3]:0)-2;
                if(n<0)n=0;
                if(n>126)n=126;
                for(k=0;k<n;k++){
                    int ch=w[2+k]&0xff;
                    if(!ch)continue;
                    vdi_glyph(x,y,h,(char)ch,tall,color);
                    x+=8;
                }
            }
            return 0;
        }
        if(ptsin)vdi_fill_area(ptsin[0],ptsin[1],ptsin[2],ptsin[3],(int)vdi_ws()->fill_style);
        return 0;
    case VDI_V_FILLAREA:
        /* VDI: contrl[1] is the point count and ptsin holds the vertices.
           Filling only the bounding box made every non-rectangular area a
           solid block, so polygons are scan-filled here. */
        {int n=(control?control[1]:0);
         if(n>=3&&ptsin){
             /* the hatch styles need a per-pixel pass; solid keeps the fast
                scanline filler */
             int st=(int)vdi_ws()->fill_style;
             if(st>1&&st<=8){
                 int ymin=ptsin[1],ymax=ptsin[1],q;
                 for(q=1;q<n;q++){if(ptsin[2*q+1]<ymin)ymin=ptsin[2*q+1];
                                  if(ptsin[2*q+1]>ymax)ymax=ptsin[2*q+1];}
                 vdi_poly_fill_styled(ptsin,n,ymin,ymax,st);
             }else vdi_poly_fill(ptsin,n,1);
         }else if(ptsin)vdi_fill_area(ptsin[0],ptsin[1],ptsin[2],ptsin[3],(int)vdi_ws()->fill_style);}
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
                         (int)vdi_ws()->line_color);
        }
        return 0;
    /* --- attribute setters (SysInfo initialises these after v_opnvwk) --- */
    case VDI_VSL_TYPE:    if(intin)vdi_ws()->line_type=intin[0];return 0;
    case VDI_VSL_WIDTH:
        /* Standard VDI passes the width in ptsin[0] (a WORD in intin[] was
           silently accepted before, so a correct caller changed nothing). */
        if(ptsin)vdi_ws()->line_width=ptsin[0];
        else if(intin)vdi_ws()->line_width=intin[0];
        return 0;
    case VDI_VSL_COLOR:   if(intin)vdi_ws()->line_color=intin[0];return 0;
    case VDI_VSL_ENDS:    if(intin){vdi_ws()->ends[0]=intin[0];vdi_ws()->ends[1]=intin[1];}return 0;
    case VDI_VST_COLOR:   if(intin)vdi_ws()->text_color=intin[0];return 0;
    case VDI_VST_FONT:    /* 21: select a font id; 0 and 1 mean the system font */
        if(intin){
            vdi_ws()->font=intin[0];
            tos_gdos_select((uint16_t)(intin[0]>1?intin[0]:0));
        }
        return 0;
    case VDI_VST_EFFECTS: if(intin)vdi_ws()->effects=intin[0];return 0;
    case VDI_VST_ROTATION:if(intin)vdi_ws()->rotation=intin[0];return 0;
    case VDI_VST_HEIGHT:
        /* VDI passes the requested height in ptsin[1]; the old code took the
           difference ptsin[1]-ptsin[3], which a standard call (no second point)
           turned into the negative of the height.  ptsout reports the resulting
           character cell, not just the height. */
        {int h=ptsin?(int)ptsin[1]:0;
         if(h>0)vdi_ws()->text_height=(int16_t)h;
         if(ptsout){int th=vdi_ws()->text_height?vdi_ws()->text_height:8;
                    ptsout[0]=8;ptsout[1]=(int16_t)(th-1);ptsout[2]=8;ptsout[3]=th;}
        }
        return 0;
    case VDI_VST_ALIGN:   if(intin)vdi_ws()->text_align=intin[0];return 0;
    case VDI_VSM_TYPE:    if(intin)vdi_ws()->marker_type=intin[0];return 0;
    case VDI_VSM_HEIGHT:  if(intin)vdi_ws()->marker_height=intin[0];return 0;
    case VDI_VSM_COLOR:   if(intin)vdi_ws()->marker_color=intin[0];return 0;
    case VDI_VSF_INTERIOR:if(intin)vdi_ws()->interior=intin[0];return 0;
    case VDI_VSF_STYLE:   if(intin)vdi_ws()->fill_style=intin[0];return 0;
    case VDI_VSF_COLOR:   if(intin)vdi_ws()->fill_color=intin[0];return 0;
    case VDI_VSF_PERIMETER:if(intin)vdi_ws()->perimeter=intin[0];return 0;
    case VDI_VSWR_MODE:   if(intin){vdi_ws()->wrt_mode=intin[0];vdi_wrt_mode=intin[0];}return 0;
    case VDI_VS_CLIP:
        if(intin&&ptsin){
            vdi_ws()->clip_on=(uint8_t)intin[0];
            vdi_ws()->clip[0]=ptsin[0];vdi_ws()->clip[1]=ptsin[1];
            vdi_ws()->clip[2]=ptsin[2];vdi_ws()->clip[3]=ptsin[3];
            vdi_clip_on=intin[0]?1:0;
            vdi_clip[0]=ptsin[0];vdi_clip[1]=ptsin[1];
            vdi_clip[2]=ptsin[2];vdi_clip[3]=ptsin[3];
        }
        return 0;
    /* --- attribute queries --- */
    case VDI_VQT_ATTRS:
        if(intout){intout[0]=1;intout[1]=vdi_ws()->text_color;intout[2]=vdi_ws()->rotation;
                   intout[3]=vdi_ws()->text_align;intout[4]=0;intout[5]=vdi_ws()->wrt_mode;}
        if(ptsout){int16_t th=vdi_ws()->text_height?vdi_ws()->text_height:16;
                   ptsout[0]=8;ptsout[1]=(int16_t)(th-1);ptsout[2]=8;ptsout[3]=th;}
        return 0;
    case VDI_VQL_ATTRS:
        if(intout){intout[0]=vdi_ws()->line_type;intout[1]=1;intout[2]=vdi_ws()->line_color;
                   intout[3]=vdi_ws()->line_width;}
        if(ptsout){ptsout[0]=1;ptsout[1]=1;}
        return 0;
    case VDI_VQM_ATTRS:
        if(intout){intout[0]=vdi_ws()->marker_type;intout[1]=vdi_ws()->marker_height;
                   intout[2]=vdi_ws()->marker_color;}
        if(ptsout){ptsout[0]=8;ptsout[1]=8;}
        return 0;
    case VDI_VQF_ATTRS:
        if(intout){intout[0]=vdi_ws()->interior;intout[1]=vdi_ws()->fill_style;
                   intout[2]=vdi_ws()->fill_color;intout[3]=0;intout[4]=0;}
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
        {int n=(control?control[3]:0);int w=n*8;int16_t th=vdi_ws()->text_height?vdi_ws()->text_height:16;
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
        /* Contract: intout[0] = button state, ptsout[0..1] = x,y.  Reporting
           (0,0) told a file manager the pointer never moved. */
        {extern void tos_mouse_state_for_vdi(int16_t *out);
         int16_t m[5];tos_mouse_state_for_vdi(m);
         if(intout)intout[0]=m[3];
         if(ptsout){ptsout[0]=m[1];ptsout[1]=m[2];}}
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
            /* VDI passes the characters as 16-bit WORDS and the word count in
               contrl[3] (EmuTOS vdi_v_gtext -> output_text(CONTRL[3], INTIN)).
               Reading intin as a NUL-terminated byte string saw the high zero
               byte of 'V' first and drew nothing. */
            const uint16_t *w=(const uint16_t *)(const void *)intin;
            int n=control?(int)control[3]:0;
            int gi=0;
            int x=ptsin[0],y=ptsin[1];
            int tall=(vdi_ws()->text_height>=16)?1:0;
            int h=tall?16:8;
            int color=(int)vdi_ws()->text_color?1:0;
            if(n>126)n=126;
            while(gi<n){
                int ch=w[gi++]&0xff;
                if(!ch)continue;
                vdi_glyph(x,y,h,(char)ch,tall,color);
                x+=8;
            }
        }
        return 0;
    case VDI_VRO_CPYFM:
        vdi_cpyfm(0,control,intin,ptsin);
        return 0;
    case VDI_VRT_CPYFM:
        vdi_cpyfm(1,control,intin,ptsin);
        return 0;
    case VDI_VR_TRNFM:
        /* vr_trnfm (110) converts an MFDB between the STANDARD and the DEVICE
           format (and is NOT vrt_cpyfm).  For a single-plane mono bitmap both
           representations are the same word-per-row raster, so the transform is
           a row copy plus publishing the geometry into the destination MFDB.
           The 2/4-plane interleave belongs to the colour work. */
        {const uint8_t *smf=vdi_mfdb_addr(control,7);
         uint8_t *dmf=(uint8_t *)(uintptr_t)vdi_mfdb_addr(control,9);
         if(smf&&dmf){
             const uint8_t *sb=(const uint8_t *)(uintptr_t)vdird32(smf+0);
             uint8_t *db=(uint8_t *)(uintptr_t)vdird32(dmf+0);
             int w=vdird16(smf+4),h=vdird16(smf+6),np=vdird16(smf+12);
             int sp=vdird16(smf+8)*2,dp=vdird16(dmf+8)*2;
             if(np<=0)np=1;
             if(sp<=0)sp=(w+15)/16*2;
             if(dp<=0)dp=sp;
             if(sb&&db&&w>0&&h>0){
                 int y,k,bytes=np*((w+15)/16)*2;
                 if(bytes>sp)bytes=sp;
                 if(bytes>dp)bytes=dp;
                 for(y=0;y<h;y++)
                     for(k=0;k<bytes;k++)db[y*dp+k]=sb[y*sp+k];
             }
             /* Publish the resulting geometry (a real VDI updates the MFDB). */
             vdi_wr16(dmf+4,(uint16_t)w);vdi_wr16(dmf+6,(uint16_t)h);
             vdi_wr16(dmf+8,(uint16_t)((w+15)/16));
             vdi_wr16(dmf+10,vdird16(smf+10));
             vdi_wr16(dmf+12,(uint16_t)np);
         }}
        return 0;
    /* ---- GDOS: font system -------------------------------------------- *
       An application asks how many fonts exist (vst_load_fonts), then queries
       their names and metrics.  Returning zero made GDOS-aware programs give
       up during start-up (LDW Power exits before its first GEM call).  The
       numbers come from the module that parses ASSIGN.SYS and the .FNT
       headers; outlines, metafiles and printers are NOT claimed. */
    case VDI_VST_LOAD_FONTS:                     /* 119: 0 in, 1 out */
        tos_gdos_boot_load();
        if(intout)intout[0]=(int16_t)tos_gdos_font_count();
        return 0;
    case VDI_VST_UNLOAD_FONTS:                   /* 120 */
        if(intout)intout[0]=0;
        return 0;
    case VDI_VQT_NAME:                           /* 130: element -> font ID + name */
        /* EmuTOS vdi_vqt_name(): intout[0] is the FONT ID, then the name goes out
           as ONE CHARACTER PER WORD, zero-padded to FONT_NAME_LEN (32).  The old
           code wrote the length first and packed two chars per word, so a normal
           binding read "ytm" out of "System". */
        {const tos_gdos_font_t *f=0;uint16_t id=6u;unsigned k,n=0;const char *nm="System";
         tos_gdos_boot_load();
         if(intin){
             int element=(int)(uint16_t)intin[0];
             if(element>0){f=tos_gdos_font((unsigned)(element-1));}
         }
         if(f){
             id=f->font_id;
             if(f->name[0])nm=f->name;
         }
         while(nm[n]&&n<32u)++n;
         if(intout){
             intout[0]=(int16_t)id;
             for(k=0u;k<32u;++k)intout[1u+k]=(int16_t)(uint8_t)(k<n?nm[k]:0);
             {unsigned d;for(d=0u;d<4u;++d)kdbg_vdi_dbgname[d]=(uint16_t)intout[1u+d];}
         }}
        return 0;
    case VDI_VQT_FONTINFO:                       /* 131: 2 intout + 10 ptsout */
        /* EmuTOS vdi_vqt_fontinfo(): INTOUT = first_ade, last_ade;
           PTSOUT = max_cell_width, bottom, thicken/0, descent,
                    left/0, half, right/0, ascent, 0, top.
           The old code wrote five intout words and eight ptsout words, leaving
           max_width at 0 and the last ptsout words untouched. */
        {uint16_t first=32u,last=126u,cellw=8u,bottom=16u,descent=2u,half=8u,ascent=12u,top=14u;
         const tos_gdos_font_t *f=0;
         tos_gdos_boot_load();
         /* EmuTOS uses the workstation's current font; the element is selected
            with vst_font, not passed here.  Use the GDOS font that vst_font
            chose, falling back to the built-in metrics. */
         {uint16_t sel=tos_gdos_selected();
          if(sel>1u)f=tos_gdos_font((unsigned)(sel-2u));}
         if(f){
             first=f->first_ade;last=f->last_ade;
             if(f->max_cell_width)cellw=f->max_cell_width;
             if(f->bottom)bottom=f->bottom;
             descent=f->descent;ascent=f->ascent;top=f->top;
             half=(uint16_t)((f->ascent+f->descent)/2u);
         }
         if(intout){intout[0]=(int16_t)first;intout[1]=(int16_t)last;
                    kdbg_vdi_dbginfo[0]=(uint16_t)intout[0];kdbg_vdi_dbginfo[1]=(uint16_t)intout[1];}
         if(ptsout){
             ptsout[0]=(int16_t)cellw;ptsout[1]=(int16_t)bottom;
             ptsout[2]=0;ptsout[3]=(int16_t)descent;
             ptsout[4]=0;ptsout[5]=(int16_t)half;ptsout[6]=0;
             ptsout[7]=(int16_t)ascent;ptsout[8]=0;ptsout[9]=(int16_t)top;
         }}
        return 0;
    case VDI_V_FTEXT:                            /* 132: draw with the current font */
    case VDI_V_FTEXT_OFFSET:                     /* 133 */
        /* Bitmap/outline font bodies are not rendered yet, so the text goes out
           through the system glyph writer.  The position and the string come in
           the same way as v_gtext. */
        if(ptsin&&intin&&vdi_target){
            const uint16_t *w=(const uint16_t *)(const void *)intin;
            int n=control?(int)control[3]:0,gi=0;
            int x=ptsin[0],y=ptsin[1];
            int tall=(vdi_ws()->text_height>=16)?1:0;
            int h=tall?16:8;
            int color=(int)vdi_ws()->text_color?1:0;
            if(n>126)n=126;
            while(gi<n){
                int ch=w[gi++]&0xff;
                if(!ch)continue;
                vdi_glyph(x,y,h,(char)ch,tall,color);
                x+=8;
            }
        }
        return 0;
    default:
        {extern void tos_note_unsupported(int,uint32_t);
         tos_note_unsupported(5,(uint32_t)opcode);}
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
        vdi_glyph(x,y,h,*s,tall?1:0,color?1:0);
        x+=8;
    }
}
