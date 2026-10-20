import sys
from PIL import Image
src=sys.argv[1]; out=sys.argv[2]
x0,y0,x1,y1=[int(v) for v in sys.argv[3:7]]
sc=int(sys.argv[7]) if len(sys.argv)>7 else 3
im=Image.open(src).convert("L")
im=im.crop((x0,y0,x1,y1))
im=im.resize((im.width*sc,im.height*sc),Image.NEAREST)
im.save(out)
print(out, im.size)
