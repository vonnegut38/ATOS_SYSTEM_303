from pathlib import Path
import re, shutil
p=Path('.')
notice='/* Adapted from EmuTOS e0f0c2333d39a487e73d8b17f62b70a95441cd32.\n * Copyright 2001-2026 The EmuTOS development team.\n * SPDX-License-Identifier: GPL-2.0-or-later */\n'
icons=(p/'work/emutos-reference/desk/icons.c').read_text()
res=notice
for n in (0,1,2,3):
 for kind in ('mask','data'):
  name=f'rs_icon{kind}{n}'
  body=re.search(r'static const WORD '+name+r'\[\] = \{(.*?)\};',icons,re.S).group(1)
  res+=f'static const uint16_t {name}[64] = {{{body}}};\n'
font=(p/'work/emutos-reference/bios/fnt_st_8x16.c').read_text()
vals=[int(x,16) for x in re.findall(r'0x[0-9a-fA-F]+',re.search(r'dat_table\[\] =\s*\{(.*?)\};',font,re.S).group(1))]
b=bytes(v for w in vals for v in (w>>8,w&255))
res+='static const uint8_t emutos_system_font[256][16]={\n'
for c in range(256):res+='{'+','.join(hex(b[y*256+c]) for y in range(16))+'},\n'
res+='};\n'
(p/'system/emutos_resources.inc').write_text(res)
shutil.copyfile(p/'work/emutos-reference/doc/license.txt',p/'licenses/emutos-GPL-2.0.txt')
f=p/'include/emutos_vdi_mono.h';s=f.read_text();s=s.replace('#endif','''void emutos_vdi_planar_fill(volatile uint16_t *,uint16_t,uint8_t,
    uint16_t,uint16_t,uint16_t,uint16_t,uint8_t,const uint16_t *,uint8_t);
void emutos_vdi_vertical(volatile uint16_t *,uint16_t,uint8_t,
    uint16_t,uint16_t,uint16_t,uint8_t);
#endif''');f.write_text(s)
f=p/'system/emutos_vdi_mono.c';s=f.read_text();s+='''
/* WM_REPLACE path of vdi_line.c swblit_rect_common(). Keep the original
 * plane/scanline/word traversal and MC68000 DBRA centre loop. Bounds are
 * already clipped by the caller. Patterns are anchored to screen Y. */
void emutos_vdi_planar_fill(volatile uint16_t *words,uint16_t stride,uint8_t planes,
    uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t color,
    const uint16_t *patterns,uint8_t pattern_mask)
{
    uint16_t last=(uint16_t)(x+width-1u),left=(uint16_t)(0xffffu>>(x&15u));
    uint16_t right=(uint16_t)(0xffffu<<(15u-(last&15u)));
    uint16_t groups=(uint16_t)((last>>4)-(x>>4)+1u),row,plane;
    if(groups==1u){left&=right;right=0;}
    for(row=0;row<height;row++)for(plane=0;plane<planes;plane++){
        volatile uint16_t *work=words+(uint32_t)(y+row)*stride+(x>>4)*planes+plane;
        uint16_t pattern=(color&(1u<<plane))?patterns[(y+row)&pattern_mask]:0;
        *work=(uint16_t)((*work&~left)|(pattern&left));work+=planes;
        if(groups>2u){
#ifdef __m68k__
            int16_t n=(int16_t)(groups-3u),step=(int16_t)(2u*planes);
            __asm__ volatile("1:\\n\\tmove.w %2,(%1)\\n\\tadda.w %3,%1\\n\\tdbra %0,1b"
                : "+d"(n),"+a"(work):"r"(pattern),"r"(step):"memory","cc");
#else
            uint16_t n;for(n=2;n<groups;n++){*work=pattern;work+=planes;}
#endif
        }
        if(right)*work=(uint16_t)((*work&~right)|(pattern&right));
    }
}
/* Solid WM_REPLACE specialization of EmuTOS vertical_line(). */
void emutos_vdi_vertical(volatile uint16_t *words,uint16_t stride,uint8_t planes,
    uint16_t x,uint16_t y,uint16_t height,uint8_t color)
{
    uint16_t plane,row,bit=(uint16_t)(0x8000u>>(x&15u));
    for(plane=0;plane<planes;plane++){
        volatile uint16_t *addr=words+(uint32_t)y*stride+(x>>4)*planes+plane;
        if(color&(1u<<plane))for(row=0;row<height;row++){*addr|=bit;addr+=stride;}
        else for(row=0;row<height;row++){*addr&=(uint16_t)~bit;addr+=stride;}
    }
}
''';f.write_text(s)
f=p/'system/kernel_graphics.c';s=f.read_text();s=s.replace('#include "hardware.h"','#include "hardware.h"\n#include "emutos_resources.inc"')
s=s.replace('''    if(surface->planes==1u)
        emutos_vdi_mono_fill(surface->words,surface->line_words,x,y,width,height,color&1u);
    else surface_fill_reference(surface,x,y,width,height,color);''','''    static const uint16_t solid[1]={0xffffu};
    emutos_vdi_planar_fill(surface->words,surface->line_words,surface->planes,
        x,y,width,height,color,solid,0);''')
s=s.replace('    if(!clip_line(&x0,&y0,&x1,&y1))return;','''    if(!clip_line(&x0,&y0,&x1,&y1))return;
    if(y0==y1){kgfx_fill_rect(x0<x1?x0:x1,y0,(x0<x1?x1-x0:x0-x1)+1,1,color);return;}
    if(x0==x1){
        miny=y0<y1?y0:y1;maxy=y0>y1?y0:y1;
        guard=cursor_guard_begin(x0,miny,1,maxy-miny+1);surface=active_surface();
        emutos_vdi_vertical(surface.words,surface.line_words,surface.planes,
            (uint16_t)x0,(uint16_t)miny,(uint16_t)(maxy-miny+1),color);
        mark_dirty(x0,miny,1,maxy-miny+1);cursor_guard_end(guard);return;
    }''')
s=s.replace('glyph=font_glyph(*text++,1u);','glyph=desktop_is_active?emutos_system_font[(uint8_t)*text++]:font_glyph(*text++,1u);')
f.write_text(s)
