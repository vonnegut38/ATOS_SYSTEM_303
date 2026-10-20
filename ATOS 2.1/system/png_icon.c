/* Minimal PNG decoder for app icons: 8-bit RGBA / RGB, non-interlaced.
   Decodes a whole image into caller-provided RGBA memory and reports its size.
   Freestanding (no libc): a small self-contained DEFLATE/zlib inflate plus PNG
   unfiltering.  Handles the PNGs mk_icons_color.py consumes (96x96 RGBA). */
#include <stdint.h>

typedef struct {const uint8_t *in;uint32_t len,pos,bitbuf;int bitcnt;} bits_t;
static void bits_init(bits_t *b,const uint8_t *in,uint32_t len)
{b->in=in;b->len=len;b->pos=0;b->bitbuf=0;b->bitcnt=0;}
static uint32_t bits_get(bits_t *b,int n)
{
    uint32_t v=0;
    while(b->bitcnt<n&&b->pos<b->len){b->bitbuf|=(uint32_t)b->in[b->pos++]<<b->bitcnt;b->bitcnt+=8;}
    v=b->bitbuf&((n>=32)?0xffffffffu:((1u<<n)-1u));
    b->bitbuf>>=n;b->bitcnt-=n;
    return v;
}

typedef struct {int16_t count[16];uint16_t symbol[288];} huff_t;
static int huff_build(huff_t *h,const uint8_t *len,int n)
{
    int i,k=0;
    for(i=0;i<16;++i)h->count[i]=0;
    for(i=0;i<n;++i){if(len[i]>15u)return -1;if(len[i])h->count[len[i]]++;}
    h->count[0]=0;
    {int16_t left=1;
     for(i=1;i<16;++i){left<<=1;left-=h->count[i];if(left<0)return -1;}}
    /* Fill by ascending code length, matching huff_decode's cumulative index. */
    for(i=1;i<16;++i){int sym;
        for(sym=0;sym<n;++sym)if(len[sym]==i)h->symbol[k++]=(uint16_t)sym;}
    return 0;
}
static int huff_decode(bits_t *b,const huff_t *h)
{
    int code=0,first=0,index=0,len;
    for(len=1;len<16;++len){
        code|= (int)bits_get(b,1);
        {int c=h->count[len];
         if(code-first<c)return h->symbol[index+(code-first)];
         index+=c;first=(first+c)<<1;code<<=1;}
    }
    return -1;
}

static const uint16_t len_base[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint16_t len_extra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t dist_base[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint16_t dist_extra[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int inflate_block(bits_t *b,uint8_t *out,uint32_t *op,uint32_t cap,int *final)
{
    uint32_t steps=0;
    int type;
    *final=(int)bits_get(b,1);
    type=(int)bits_get(b,2);
    if(type==0){
        uint32_t len,nlen,i;
        b->bitbuf=0;b->bitcnt=0;
        if(b->pos+4>b->len)return -1;
        len=(uint32_t)b->in[b->pos]|((uint32_t)b->in[b->pos+1]<<8);
        nlen=(uint32_t)b->in[b->pos+2]|((uint32_t)b->in[b->pos+3]<<8);
        b->pos+=4;
        if(((len^0xffffu)&0xffffu)!=nlen)return -1;
        if(len>cap-*op||b->pos+len>b->len)return -1;
        for(i=0;i<len;++i)out[(*op)++]=b->in[b->pos++];
        return 0;
    }
    if(type==3)return -1;
    if(type==1){
        uint8_t ll[288];uint8_t dl[30];int i;huff_t hl,hd;
        for(i=0;i<144;++i)ll[i]=8;
        for(;i<256;++i)ll[i]=9;
        for(;i<280;++i)ll[i]=7;
        for(;i<288;++i)ll[i]=8;
        for(i=0;i<30;++i)dl[i]=5;
        if(huff_build(&hl,ll,288)||huff_build(&hd,dl,30))return -1;
        for(;;){
            int sym;
            if(++steps>1000000u)return -1;
            sym=huff_decode(b,&hl);if(sym<0)return -1;
            if(sym<256){if(*op>=cap)return -1;out[(*op)++]=(uint8_t)sym;continue;}
            if(sym==256)return 0;
            if(sym>285)return -1;
            {int lc=sym-257;uint32_t len=len_base[lc]+bits_get(b,len_extra[lc]);
             int dc=(int)huff_decode(b,&hd);if(dc<0||dc>29)return -1;
             uint32_t dist=dist_base[dc]+bits_get(b,dist_extra[dc]);
             uint32_t i;if(dist>*op||len>cap-*op)return -1;
             for(i=0;i<len;++i){out[*op]=out[*op-dist];(*op)++;}}
        }
    }
    {
        int hlit=(int)bits_get(b,5)+257,hdist=(int)bits_get(b,5)+1,hclen=(int)bits_get(b,4)+4;
        int i;uint8_t cl[19];uint8_t ll[288];uint8_t dl[30];huff_t hc,hl,hd;
        static const int order[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
        for(i=0;i<19;++i)cl[i]=0;
        for(i=0;i<hclen;++i)cl[order[i]]=(uint8_t)bits_get(b,3);
        if(huff_build(&hc,cl,19))return -1;
        i=0;
        while(i<hlit+hdist){
            int sym=huff_decode(b,&hc);if(sym<0)return -1;
            if(sym<16){
                if(i<hlit)ll[i]=(uint8_t)sym;else dl[i-hlit]=(uint8_t)sym;
                ++i;
                continue;
            }
            {int rep;uint8_t v;
             if(sym==16){if(!i)return -1;rep=(int)bits_get(b,2)+3;v=(i<=hlit)?ll[i-1]:dl[i-hlit-1];}
             else if(sym==17){rep=(int)bits_get(b,3)+3;v=0;}
             else{rep=(int)bits_get(b,7)+11;v=0;}
             if(i+rep>hlit+hdist)return -1;
             while(rep--){if(i<hlit)ll[i]=(uint8_t)v;else dl[i-hlit]=(uint8_t)v; ++i;}}
        }
        if(ll[256]==0)return -1;
        if(huff_build(&hl,ll,hlit)||huff_build(&hd,dl,hdist))return -1;
        for(;;){
            int sym;
            if(++steps>1000000u)return -1;
            sym=huff_decode(b,&hl);if(sym<0)return -1;
            if(sym<256){if(*op>=cap)return -1;out[(*op)++]=(uint8_t)sym;continue;}
            if(sym==256)return 0;
            if(sym>285)return -1;
            {int lc=sym-257;uint32_t len=len_base[lc]+bits_get(b,len_extra[lc]);
             int dc=(int)huff_decode(b,&hd);if(dc<0||dc>29)return -1;
             uint32_t dist=dist_base[dc]+bits_get(b,dist_extra[dc]);
             uint32_t i;if(dist>*op||len>cap-*op)return -1;
             for(i=0;i<len;++i){out[*op]=out[*op-dist];(*op)++;}}
        }
    }
}
static int zlib_inflate(const uint8_t *in,uint32_t len,uint8_t *out,uint32_t cap,uint32_t *outlen)
{
    bits_t b;uint32_t op=0;int final=0;
    if(len<6)return -1;
    if(in[0]!=0x78)return -1;
    if(((uint16_t)in[0]<<8|in[1])%31u)return -1;
    bits_init(&b,in+2,len-2);
    do{if(inflate_block(&b,out,&op,cap,&final))return -1;}while(!final);
    *outlen=op;
    return 0;
}

static int png_unfilter(uint8_t *scan,const uint8_t *prev,uint32_t len,uint8_t bpp,uint8_t filter)
{
    uint32_t i;
    switch(filter){
        case 0:break;
        case 1:for(i=bpp;i<len;++i)scan[i]=(uint8_t)(scan[i]+scan[i-bpp]);break;
        case 2:if(prev)for(i=0;i<len;++i)scan[i]=(uint8_t)(scan[i]+prev[i]);break;
        case 3:for(i=0;i<len;++i){uint8_t a=i>=bpp?scan[i-bpp]:0,p=prev?prev[i]:0;
                 scan[i]=(uint8_t)(scan[i]+((uint16_t)a+p)/2);}break;
        case 4:for(i=0;i<len;++i){int a=i>=bpp?scan[i-bpp]:0,b2=prev?prev[i]:0,c=i>=bpp&&prev?prev[i-bpp]:0;
                 int p=a+b2-c,pa=p>a?p-a:a-p,pb=p>b2?p-b2:b2-p,pc=p>c?p-c:c-p;
                 int pr=(pa<=pb&&pa<=pc)?a:(pb<=pc?b2:c);
                 scan[i]=(uint8_t)(scan[i]+pr);}break;
        default:return -1;
    }
    return 0;
}

int png_decode_rgba(const uint8_t *data,uint32_t len,uint8_t *rgba,uint32_t *w,uint32_t *h,uint8_t *work)
{
    static const uint8_t sig[8]={0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a};
    uint32_t pos=8,wid=0,hei=0,color=0,bitdepth=0,interlace=0;
    uint8_t *idat=work;uint32_t idlen=0;uint8_t *raw=work+16384u;uint32_t rawlen=0;
    uint8_t channels,bpp;uint32_t stride,y,x,op=0;
    if(!data||!work||len<33)return -1;
    {uint32_t i;for(i=0;i<8;++i)if(data[i]!=sig[i])return -1;}
    while(pos+12<=len){
        uint32_t clen=((uint32_t)data[pos]<<24)|((uint32_t)data[pos+1]<<16)|((uint32_t)data[pos+2]<<8)|data[pos+3];
        if(clen>65536u||pos+12+clen>len)return -1;
        if(data[pos+4]=='I'&&data[pos+5]=='H'&&data[pos+6]=='D'&&data[pos+7]=='R'){
            wid=((uint32_t)data[pos+8]<<24)|((uint32_t)data[pos+9]<<16)|((uint32_t)data[pos+10]<<8)|data[pos+11];
            hei=((uint32_t)data[pos+12]<<24)|((uint32_t)data[pos+13]<<16)|((uint32_t)data[pos+14]<<8)|data[pos+15];
            bitdepth=data[pos+16];color=data[pos+17];interlace=data[pos+20];
        }else if(data[pos+4]=='I'&&data[pos+5]=='D'&&data[pos+6]=='A'&&data[pos+7]=='T'){
            if(idlen+clen>16384u)return -1;
            {uint32_t i;for(i=0;i<clen;++i)idat[idlen++]=data[pos+8+i];}
        }
        pos+=12+clen;
    }
    if(!wid||!hei||wid>512u||hei>512u)return -1;
    if(bitdepth!=8u||interlace||(color!=6u&&color!=2u))return -1;
    channels=(color==6u)?4u:3u;bpp=channels;
    if(zlib_inflate(idat,idlen,raw,49152u,&rawlen))return -1;
    stride=wid*channels;
    if(rawlen!=(stride+1u)*hei)return -1;
    {uint8_t *prev=0;
     for(y=0;y<hei;++y){
         uint8_t filter=raw[op++];
         uint8_t *scan=raw+op;op+=stride;
         if(png_unfilter(scan,prev,stride,bpp,filter))return -1;
         prev=scan;
         for(x=0;x<wid;++x){
             uint8_t r,g,b,a;
             if(channels==4u){r=scan[x*4];g=scan[x*4+1];b=scan[x*4+2];a=scan[x*4+3];}
             else{r=scan[x*3];g=scan[x*3+1];b=scan[x*3+2];a=255;}
             rgba[(y*wid+x)*4]=r;rgba[(y*wid+x)*4+1]=g;rgba[(y*wid+x)*4+2]=b;rgba[(y*wid+x)*4+3]=a;
         }
     }}
    *w=wid;*h=hei;
    return 0;
}

/* ---- icon conversion (runtime counterpart of mk_icons_color.py) ---------- */
/* Box-filter an RGBA image down to out_size x out_size, then quantize to the
   given palette (12-bit RGB entries) and emit a 1 bpp mask (alpha >= 128).
   pixels[out_size*out_size] holds palette indices; mask[out_size*out_size/8]
   holds the mask bits. */
int png_icon_convert(const uint8_t *rgba,uint32_t w,uint32_t h,
                     const uint16_t *palette,uint8_t npalette,
                     uint8_t *pixels,uint8_t *mask,uint32_t out_size)
{
    /* All requests are serialized by SYSTEM.GFX, like its PNG work buffers.
       Cache the EXACT nearest palette entry for each RGB444 input. Palette
       changes invalidate it by content, including changes at the same address.
       Squared distances use a table: -m68000 otherwise calls __mulsi3 for each
       of three channel differences, for every palette entry of every pixel. */
    static uint8_t nearest[4096],valid[512],cached_count;
    static uint16_t cached_palette[255];
    static uint8_t red[255],green[255],blue[255];
    static const uint8_t square[31]={225,196,169,144,121,100,81,64,49,36,25,16,9,4,1,0,
                                   1,4,9,16,25,36,49,64,81,100,121,144,169,196,225};
    uint32_t ox,oy,pi;int changed;
    if(!rgba||!w||!h||!palette||!npalette||!pixels||!mask||!out_size)return -1;
    changed=cached_count!=npalette;
    for(pi=0;pi<npalette&&!changed;++pi)if(cached_palette[pi]!=palette[pi])changed=1;
    if(changed){
        for(pi=0;pi<sizeof(valid);++pi)valid[pi]=0;
        for(pi=0;pi<npalette;++pi){
            uint16_t c=palette[pi];cached_palette[pi]=c;
            red[pi]=(uint8_t)((c>>8)&15);green[pi]=(uint8_t)((c>>4)&15);blue[pi]=(uint8_t)(c&15);
        }
        cached_count=npalette;
    }
    for(oy=0;oy<out_size;++oy){
        uint32_t sy0=oy*h/out_size,sy1=(oy+1u)*h/out_size;
        if(sy1<=sy0)sy1=sy0+1u;
        for(ox=0;ox<out_size;++ox){
            uint32_t sx0=ox*w/out_size,sx1=(ox+1u)*w/out_size,sy,sx;
            uint32_t sr=0,sg=0,sb=0,sa=0;
            unsigned best;
            if(sx1<=sx0)sx1=sx0+1u;
            for(sy=sy0;sy<sy1;++sy)for(sx=sx0;sx<sx1;++sx){
                const uint8_t *p=rgba+(((uint32_t)sy*w+sx)*4u);
                sr+=p[0]*p[3];sg+=p[1]*p[3];sb+=p[2]*p[3];sa+=p[3];
            }
            if(sa>=128u){
                uint32_t r=sr/sa,g=sg/sa,b=sb/sa,pi;
                uint8_t alpha=(uint8_t)(sa/((sy1-sy0)*(sx1-sx0)));
                unsigned key=((r>>4)<<8)|((g>>4)<<4)|(b>>4);
                uint8_t bit=(uint8_t)(1u<<(key&7u));
                if(valid[key>>3]&bit)best=nearest[key];
                else{
                    unsigned bestd=676u;best=0;
                    int rr=(int)(r>>4)+15,gg=(int)(g>>4)+15,bb=(int)(b>>4)+15;
                    for(pi=0;pi<npalette;++pi){
                        unsigned dd=(unsigned)square[rr-red[pi]]+square[gg-green[pi]]+square[bb-blue[pi]];
                        if(dd<bestd){bestd=dd;best=(unsigned)pi;if(!dd)break;}
                    }
                    nearest[key]=(uint8_t)best;valid[key>>3]|=bit;
                }
                pixels[oy*out_size+ox]=(uint8_t)best;
                if(alpha>=128u)mask[(oy*out_size+ox)>>3]|=(uint8_t)(0x80u>>(ox&7u));
                else mask[(oy*out_size+ox)>>3]&=(uint8_t)~(0x80u>>(ox&7u));
            }else{
                pixels[oy*out_size+ox]=0;
                mask[(oy*out_size+ox)>>3]&=(uint8_t)~(0x80u>>(ox&7u));
            }
        }
    }
    return 0;
}
