#ifndef SETTINGS_CONFIG_H
#define SETTINGS_CONFIG_H
/* Shared by SETTINGS, desktop and host tests. No absolute pointer tables. */
static inline int settings_parse_mode(const char *s,unsigned *w,unsigned *h,unsigned *d)
{
    unsigned v[3],i,j=0;
    if(s[0]>='0'&&s[0]<='4'&&!s[1]){
        static const unsigned short ws[5]={640,800,1024,800,1024};
        static const unsigned short hs[5]={400,600,768,600,768};
        i=(unsigned)(s[0]-'0');*w=ws[i];*h=hs[i];*d=i<3?1:8;return 1;
    }
    for(i=0;i<3;++i){
        unsigned n=0;if(s[j]<'0'||s[j]>'9')return 0;
        while(s[j]>='0'&&s[j]<='9'){
            n=n*10u+(unsigned)(s[j++]-'0');if(n>65535u)return 0;
        }
        v[i]=n;
        if(i<2&&s[j++]!='x')return 0;
    }
    if(s[j]&&!(s[j]=='\r'&&!s[j+1]))return 0;
    if(!v[0]||!v[1]||(v[2]!=1&&v[2]!=4&&v[2]!=8&&v[2]!=16))return 0;
    *w=v[0];*h=v[1];*d=v[2];return 1;
}
/* Replace all RES lines; retain every other byte, including unknown keys.
 * Fail on overflow, never persist a truncated configuration. */
static inline int settings_merge_mode(const char *src,unsigned len,const char *mode,
                                      char *dst,unsigned cap)
{
    unsigned at=0,n=0,i;
    for(i=0;mode[i];++i){if(n>=cap)return -1;dst[n++]=mode[i];}
    if(n>=cap)return -1;
    dst[n++]='\n';
    while(at<len){
        unsigned start=at;while(at<len&&src[at]!='\n')++at;if(at<len)++at;
        if(at-start>=4&&src[start]=='R'&&src[start+1]=='E'&&src[start+2]=='S'&&src[start+3]=='=')continue;
        while(start<at){if(n>=cap)return -1;dst[n++]=src[start++];}
    }
    return (int)n;
}
#endif
