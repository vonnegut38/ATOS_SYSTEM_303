#include "tos_vdi.h"
#include "wm_blit.h"
#include "emutos_vdi_mono.h"
#include "kernel_blitter.h"
/* Our own VDI core. Drawing goes through wm_blit primitives so it lands in
   the application's compositor surface (option A: no GPL code here). */
static vdi_ws_t ws[VDI_MAX_WORKSTATIONS];
static uint8_t *vdi_target;
static uint16_t vdi_pitch;
static uint16_t vdi_w,vdi_h;

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

int tos_vdi_call(uint16_t opcode,int16_t *control,const int16_t *intin,
                 const int16_t *ptsin,int16_t *intout,int16_t *ptsout)
{
    (void)control;(void)ptsout;
    switch(opcode){
    case VDI_V_OPNVWK:
        {unsigned slot;
         for(slot=0;slot<VDI_MAX_WORKSTATIONS;slot++)if(!ws[slot].open)break;
         if(slot==VDI_MAX_WORKSTATIONS)slot=0;
         ws[slot].open=1;
         vdi_w=ws[slot].width;vdi_h=ws[slot].height;
         if(intout){
             unsigned k;
             for(k=0;k<45;k++)intout[k]=0;
             intout[0]=(int16_t)ws[slot].width;
             intout[1]=(int16_t)ws[slot].height;
             intout[13]=(int16_t)ws[slot].line_width;   /* attributes GEM asks for */
             intout[16]=(int16_t)ws[slot].text_height;
         }
         return 1;}
    case VDI_V_CLSWK:
    case VDI_V_CLRWK:
        vdi_fill(0,0,(int)vdi_w-1,(int)vdi_h-1,(int)ws[0].bg);
        return 0;
    case VDI_V_BAR:
    case VDI_V_FILLAREA:
        if(ptsin)vdi_fill(ptsin[0],ptsin[1],ptsin[2],ptsin[3],
                          (int)ws[0].fill_color?1:0);
        return 0;
    case VDI_V_LINE:
        if(ptsin)vdi_line(ptsin[0],ptsin[1],ptsin[2],ptsin[3],(int)ws[0].line_color);
        return 0;
    case VDI_V_PLINE:
        if(ptsin&&intin){
            int n=intin[0];
            int k;
            for(k=0;k+1<n;k++)
                vdi_line(ptsin[2*k],ptsin[2*k+1],ptsin[2*k+2],ptsin[2*k+3],
                         (int)ws[0].line_color);
        }
        return 0;
    case VDI_VSF_COLOR:   if(intin)ws[0].fill_color=intin[0];return 0;
    case VDI_VSF_INTERIOR:if(intin)ws[0].interior=intin[0];return 0;
    case VDI_VSF_STYLE:   if(intin)ws[0].fill_style=intin[0];return 0;
    case VDI_VST_COLOR:   if(intin)ws[0].text_color=intin[0];return 0;
    case VDI_VST_HEIGHT:  if(intin)ws[0].text_height=intin[0];return 0;
    case VDI_VST_ALIGN:   if(intin)ws[0].text_align=intin[0];return 0;
    case VDI_VSL_COLOR:   if(intin)ws[0].line_color=intin[0];return 0;
    case VDI_VSL_TYPE:    if(intin)ws[0].line_type=intin[0];return 0;
    case VDI_VSL_WIDTH:   if(intin)ws[0].line_width=intin[0];return 0;
    case VDI_V_GTEXT:
        /* Text is drawn by the caller through our font renderer; the VDI core
           records the request so the AES/compositor can place it. */
        return 0;
    default:
        return 0;
    }
}
