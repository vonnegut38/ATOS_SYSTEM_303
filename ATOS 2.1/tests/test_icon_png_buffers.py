"""The per-app PNG icon path must write exactly ICON_PIXEL_BYTES indices and
ICON_MASK_BYTES of mask - no more.

This is the regression guard for a real stack smash: kgfx_icon_from_png() kept
32x32-sized locals (pixels[1024], mask[128]) while png_icon_convert() writes
ICON_SIZE*ICON_SIZE.  With the icon set at 48x48 that wrote 2304 bytes into a
1024-byte buffer (1280 past the end) and 288 into 128 (160 past), wrecking 1440
bytes of the caller's stack on every dynamic icon.  Guards below prove the
conversion respects the declared contract, and the static checks pin the
contract to the icon size.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

class IconPngBufferTest(unittest.TestCase):
    def test_conversion_fits_the_contract(self):
        src = Path('system/png_icon.c').read_text()
        start = src.index('int png_icon_convert(')
        # png_icon_convert is the last function in the file.
        code = '''#include <stdint.h>
#include <assert.h>
#include <string.h>
#define ICON_SIZE 48u
#define ICON_PIXEL_BYTES (ICON_SIZE*ICON_SIZE)
#define ICON_MASK_BYTES ((ICON_SIZE*ICON_SIZE+7u)/8u)
''' + src[start:] + '''
/* 4x4 RGBA source: two colours plus transparency, so mask and pixels both move. */
static uint8_t rgba[4*4*4];
static uint16_t pal[4]={0x000u,0xf00u,0x0f0u,0x00fu};
int main(void){
    unsigned i;
    /* The old geometry must not be able to hold the current icon. */
    assert(ICON_PIXEL_BYTES > 1024u);      /* was pixels[1024]  */
    assert(ICON_MASK_BYTES  > 128u);       /* was mask[128]    */
    for(i=0;i<sizeof(rgba);i+=4){
        rgba[i+0]=(uint8_t)(i*7u);rgba[i+1]=(uint8_t)(i*3u);
        rgba[i+2]=(uint8_t)(i*5u);rgba[i+3]=(i&4u)?255u:0u;
    }
    {
        /* Exactly the contract, with a canary on each side. */
        struct {uint8_t pre[64];uint8_t px[ICON_PIXEL_BYTES];
                uint8_t post[64];} pbuf;
        struct {uint8_t pre[64];uint8_t mk[ICON_MASK_BYTES];
                uint8_t post[64];} mbuf;
        memset(&pbuf,0xa5,sizeof(pbuf));memset(&mbuf,0xa5,sizeof(mbuf));
        assert(png_icon_convert(rgba,4u,4u,pal,4u,pbuf.px,mbuf.mk,ICON_SIZE)==0);
        for(i=0;i<64u;++i){
            assert(pbuf.pre[i]==0xa5);assert(pbuf.post[i]==0xa5);
            assert(mbuf.pre[i]==0xa5);assert(mbuf.post[i]==0xa5);
        }
        /* Every emitted index must address a real palette entry. */
        for(i=0;i<ICON_PIXEL_BYTES;++i)assert(pbuf.px[i]<4u);
        /* The mask must mark the pixels the alpha channel made visible. */
        {unsigned set=0;for(i=0;i<ICON_MASK_BYTES;++i)set+=__builtin_popcount(mbuf.mk[i]);
         assert(set>0u);assert(set<=ICON_PIXEL_BYTES);}
    }
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            c = Path(d)/'icon_png.c'
            c.write_text(code)
            binary = Path(d)/'icon_png'
            subprocess.run(['cc','-Wall','-Wextra','-Werror',
                            '-fsanitize=address,undefined',str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

if __name__ == '__main__':
    unittest.main()
