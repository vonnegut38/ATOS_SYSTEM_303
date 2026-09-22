#ifndef ASF_TOS_VDI_H
#define ASF_TOS_VDI_H
#include <stdint.h>
/* Minimal VDI (option A: our own implementation). EmuTOS sources were used
   only as an algorithmic reference for behaviour, never copied. */
#define VDI_MAX_WORKSTATIONS 4
/* Standard VDI opcodes we implement first (GEM application coverage). */
#define VDI_V_OPNVWK   1
#define VDI_V_CLSWK    2
#define VDI_V_CLRWK    3
#define VDI_V_BAR      11
#define VDI_V_ARC      13
#define VDI_V_PLINE    6
#define VDI_V_LINE     7
#define VDI_V_FILLAREA 9
#define VDI_V_GTEXT    8
#define VDI_VSF_COLOR  25
#define VDI_VSF_INTERIOR 23
#define VDI_VSF_STYLE  24
#define VDI_VST_COLOR  22
#define VDI_VST_HEIGHT 19
#define VDI_VST_ALIGN  12
#define VDI_VSL_COLOR  17
#define VDI_VSL_TYPE   15
#define VDI_VSL_WIDTH  16
#define VDI_V_HIDE     28
#define VDI_V_SHOW     29
typedef struct {
    uint16_t width,height;
    int16_t fg,bg;
    int16_t line_color,text_color,fill_color;
    int16_t line_width,line_type,text_height,text_align;
    int16_t interior,fill_style;
    int16_t clip[4];
    uint8_t open;
} vdi_ws_t;
void tos_vdi_init(void);
int tos_vdi_call(uint16_t opcode,int16_t *control,const int16_t *intin,
                 const int16_t *ptsin,int16_t *intout,int16_t *ptsout);
const vdi_ws_t *tos_vdi_workstation(void);
/* Present the workstation's drawing into the compositor surface of the
   application (called after a batch of primitives). */
void tos_vdi_set_surface(uint8_t *surface,uint16_t pitch);
#endif
