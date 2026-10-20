#!/usr/bin/env python3
"""Generate a small public-domain icon set (32x32) for ATOS DESKTOP.
Own drawings - no third-party license constraints."""
import pathlib
from PIL import Image, ImageDraw

OUT = pathlib.Path("/tmp/atos-icons"); OUT.mkdir(exist_ok=True)

def base(): return Image.new("RGBA", (32,32), (0,0,0,0))

def disk():
    im=base(); d=ImageDraw.Draw(im)
    d.rectangle([3,3,28,28], outline=(0,0,0,255), width=2)
    d.rectangle([8,6,23,6], fill=(0,0,0,255))
    d.rectangle([8,15,23,26], outline=(0,0,0,255), width=2)
    return im

def trash():
    im=base(); d=ImageDraw.Draw(im)
    d.rectangle([5,4,26,8], outline=(0,0,0,255), width=2)
    d.rectangle([6,9,25,28], outline=(0,0,0,255), width=2)
    for x in (11,16,21): d.line([x,12,x,25], fill=(0,0,0,255), width=2)
    return im

def folder():
    im=base(); d=ImageDraw.Draw(im)
    d.rectangle([3,8,28,26], outline=(0,0,0,255), width=2)
    d.polygon([(3,8),(12,8),(15,4),(26,4),(28,8)], outline=(0,0,0,255))
    return im

def terminal():
    im=base(); d=ImageDraw.Draw(im)
    d.rectangle([3,5,28,26], outline=(0,0,0,255), width=2)
    d.line([8,11,12,14], fill=(0,0,0,255), width=2)
    d.line([12,14,8,17], fill=(0,0,0,255), width=2)
    d.line([15,19,24,19], fill=(0,0,0,255), width=2)
    return im

def gear():
    im=base(); d=ImageDraw.Draw(im)
    d.ellipse([8,8,23,23], outline=(0,0,0,255), width=3)
    for x,y,x2,y2 in [(15,2,15,7),(15,24,15,29),(2,15,7,15),(24,15,29,15)]:
        d.line([x,y,x2,y2], fill=(0,0,0,255), width=3)
    return im

for name, fn in [("disk",disk),("trash",trash),("folder",folder),("term",terminal),("gear",gear)]:
    fn().save(OUT / (name + ".png"))
print("icons in", OUT)
