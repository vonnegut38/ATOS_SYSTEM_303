#ifndef ASF_TOS_VDI_H
#define ASF_TOS_VDI_H
#include <stdint.h>
/* Minimal VDI (option A: our own implementation). EmuTOS sources were used
   only as an algorithmic reference for behaviour, never copied. */
#define VDI_MAX_WORKSTATIONS 4
/* VDI opcodes = the VDI function numbers (jmptb1 covers 1..39, jmptb2 covers
   100..134).  v_opnvwk/v_clsvwk are the 100-series ones; the earlier table had
   them as 1/2 and swapped vst_height/vsm_height and the alignment opcode, so
   SysInfo's calls fell through and no workstation was ever opened. */
#define VDI_V_OPNWK      1
#define VDI_V_CLSWK      2
#define VDI_V_CLRWK      3
#define VDI_V_PLINE      6
#define VDI_V_PMARKER    7
#define VDI_V_GTEXT      8
#define VDI_V_FILLAREA   9
#define VDI_V_BAR        11
#define VDI_VST_HEIGHT   12
#define VDI_VST_ROTATION 13
#define VDI_VS_COLOR     14
#define VDI_VSL_TYPE     15
#define VDI_VSL_WIDTH    16
#define VDI_VSL_COLOR    17
#define VDI_VSM_TYPE     18
#define VDI_VSM_HEIGHT   19
#define VDI_VSM_COLOR    20
#define VDI_VST_FONT     21
#define VDI_VST_COLOR    22
#define VDI_VSF_INTERIOR 23
#define VDI_VSF_STYLE    24
#define VDI_VSF_COLOR    25
#define VDI_VSWR_MODE    32
#define VDI_VQL_ATTRS    35
#define VDI_VQM_ATTRS    36
#define VDI_VQF_ATTRS    37
#define VDI_VQT_ATTRS    38
#define VDI_VST_ALIGN    39
#define VDI_V_OPNVWK     100
#define VDI_V_CLSVWK     101
#define VDI_VQ_EXTND     102
#define VDI_VSF_PERIMETER 104
#define VDI_V_GET_PIXEL  105
#define VDI_VST_EFFECTS  106
#define VDI_VST_POINT    107
#define VDI_VSL_ENDS     108
#define VDI_VQT_EXTENT   116
#define VDI_VQT_WIDTH    117
#define VDI_V_SHOW_C     122
#define VDI_V_HIDE_C     123
#define VDI_VQ_MOUSE     124
#define VDI_VRO_CPYFM   109
#define VDI_VR_TRNFM    110   /* vr_trnfm: transform (copy/transpose) MFDBs */
#define VDI_VRT_CPYFM   121   /* vrt_cpyfm: raster copy WITH a ROP mode */
#define VDI_VQ_KEY_S     128
#define VDI_VS_CLIP      129
#define VDI_VST_LOAD_FONTS 119   /* GDOS: ile fontow ma system */
#define VDI_VST_UNLOAD_FONTS 120
#define VDI_VQT_NAME     130
#define VDI_VQT_FONTINFO 131
#define VDI_V_FTEXT      132     /* GDOS: tekst wybranym fontem */
#define VDI_V_FTEXT_OFFSET 133
typedef struct {
    uint16_t width,height;
    int16_t fg,bg;
    int16_t line_color,text_color,fill_color;
    int16_t line_width,line_type,text_height,text_align;
    int16_t interior,fill_style;
    int16_t clip[4];
    int16_t marker_type,marker_height,marker_color;
    int16_t wrt_mode,effects,rotation,font,ends[2],perimeter;
    uint8_t open;
    uint8_t clip_on;
} vdi_ws_t;
void tos_vdi_init(void);
int tos_vdi_call(uint16_t opcode,int16_t *control,const int16_t *intin,
                 const int16_t *ptsin,int16_t *intout,int16_t *ptsout,
                 void **addrin,void **addrout);
const vdi_ws_t *tos_vdi_workstation(void);
/* Present the workstation's drawing into the compositor surface of the
   application (called after a batch of primitives). */
void tos_vdi_set_surface(uint8_t *surface,uint16_t pitch);
/* AES objc_draw() helpers: draw through the current VDI workstation/surface. */
void tos_vdi_fill_rect(int x,int y,int w,int h,int color);
void tos_vdi_text(int x,int y,const char *s,int color,int tall);
void tos_vdi_text_clip(int x0,int y0,int x1,int y1);
void tos_vdi_text_clip_off(void);
#endif
