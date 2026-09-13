#!/usr/bin/env python3
from pathlib import Path
import re, shutil
import argparse
ap=argparse.ArgumentParser(description="Import pinned EmuTOS icons and 8x16 font")
ap.add_argument("upstream",type=Path)
args=ap.parse_args()
p=Path(__file__).resolve().parents[1]
u=args.upstream
notice='/* Adapted from EmuTOS e0f0c2333d39a487e73d8b17f62b70a95441cd32.\n * Copyright 2001-2026 The EmuTOS development team.\n * SPDX-License-Identifier: GPL-2.0-or-later */\n'
icons=(u/'desk/icons.c').read_text()
res=notice
for n in (0,1,2,3):
 for kind in ('mask','data'):
  name=f'rs_icon{kind}{n}'
  body=re.search(r'static const WORD '+name+r'\[\] = \{(.*?)\};',icons,re.S).group(1)
  res+=f'static const uint16_t {name}[64] = {{{body}}};\n'
font=(u/'bios/fnt_st_8x16.c').read_text()
vals=[int(x,16) for x in re.findall(r'0x[0-9a-fA-F]+',re.search(r'dat_table\[\] =\s*\{(.*?)\};',font,re.S).group(1))]
b=bytes(v for w in vals for v in (w>>8,w&255))
res+='static const uint8_t emutos_system_font[256][16]={\n'
for c in range(256):res+='{'+','.join(hex(b[y*256+c]) for y in range(16))+'},\n'
res+='};\n'
(p/'system/emutos_resources.inc').write_text(res)
shutil.copyfile(u/'doc/license.txt',p/'licenses/emutos-GPL-2.0.txt')
