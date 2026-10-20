#include "lang.h"

typedef struct { const char *key; const char *en; } lang_def_t;

static const lang_def_t lang_defs[LANG_ID_COUNT] = {
#define LANG_DEF(id,key,en) {key,en},
    LANG_TABLE(LANG_DEF)
#undef LANG_DEF
};

static char lang_buf[LANG_ID_COUNT][LANG_MAX_LEN];
static uint8_t lang_over[LANG_ID_COUNT];
static char lang_code_buf[8];

static void copy_bounded(char *dst,const char *src,uint32_t cap)
{
    uint32_t i=0u;
    if(!dst||!cap)return;
    if(!src){dst[0]='\0';return;}
    while(src[i]&&i+1u<cap){dst[i]=src[i];++i;}
    dst[i]='\0';
}

void lang_init(void)
{
    int i;
    for(i=0;i<LANG_ID_COUNT;++i){copy_bounded(lang_buf[i],lang_defs[i].en,LANG_MAX_LEN);lang_over[i]=0u;}
    copy_bounded(lang_code_buf,"EN",sizeof lang_code_buf);
}

int lang_set(lang_id_t id,const char *text)
{
    if(id<0||id>=LANG_ID_COUNT)return 0;
    copy_bounded(lang_buf[id],text,LANG_MAX_LEN);
    lang_over[id]=1u;
    return 1;
}

const char *lang_str(lang_id_t id){if(id<0||id>=LANG_ID_COUNT)return "?";return lang_buf[id];}
const char *lang_key(lang_id_t id){if(id<0||id>=LANG_ID_COUNT)return "";return lang_defs[id].key;}
int lang_is_overridden(lang_id_t id){return (id>=0&&id<LANG_ID_COUNT)?(int)lang_over[id]:0;}
void lang_set_code(const char *code){copy_bounded(lang_code_buf,code,sizeof lang_code_buf);}
const char *lang_code(void){return lang_code_buf;}

int lang_id_for_key(const char *key)
{
    int i;
    if(!key)return -1;
    for(i=0;i<LANG_ID_COUNT;++i){
        const char *k=lang_defs[i].key;
        int j=0;
        while(k[j]&&key[j]&&k[j]==key[j]){
            ++j;
        }
        if(!k[j]&&!key[j])return i;
    }
    return -1;
}

static int key_char(char c)
{
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';
}

int lang_parse(const char *data,uint32_t length,int *applied)
{
    uint32_t i=0u;
    int n=0;
    if(!data){if(applied)*applied=0;return 0;}
    while(i<length){
        uint32_t ks,ke,vs,ve;
        while(i<length&&(data[i]=='\r'||data[i]=='\n'||data[i]==' '||data[i]=='\t')){++i;}
        if(i>=length)break;
        if(data[i]=='#'){
            while(i<length&&data[i]!='\n'){++i;}
            continue;
        }
        if(data[i]=='@'){
            uint32_t a;char code[8];uint32_t k=0u;
            ++i;
            a=i;
            while(a<length&&data[a]!='\n'&&data[a]!='\r'){++a;}
            {
                uint32_t b=i;
                if(b<a&&data[b]=='c'){while(b<a&&data[b]!='='){++b;}if(b<a){++b;}}
                while(b<a&&(data[b]==' '||data[b]=='\t'||data[b]=='=')){++b;}
                while(b<a&&k+1u<sizeof code&&data[b]!=' '&&data[b]!='\t'){code[k++]=data[b++];}
            }
            code[k]='\0';
            if(k)lang_set_code(code);
            i=a;
            continue;
        }
        ks=i;
        while(i<length&&key_char(data[i])){++i;}
        ke=i;
        while(i<length&&(data[i]==' '||data[i]=='\t')){++i;}
        if(i>=length||data[i]!='='){
            while(i<length&&data[i]!='\n'){++i;}
            continue;
        }
        ++i;
        while(i<length&&(data[i]==' '||data[i]=='\t')){++i;}
        vs=i;
        while(i<length&&data[i]!='\r'&&data[i]!='\n'){++i;}
        ve=i;
        while(ve>vs&&(data[ve-1u]==' '||data[ve-1u]=='\t')){--ve;}
        if(ke>ks){
            char key[24];char val[LANG_MAX_LEN];
            uint32_t k=0u,v=0u;int id;
            while(ks<ke&&k+1u<sizeof key){key[k++]=data[ks++];}
            key[k]='\0';
            id=lang_id_for_key(key);
            if(id>=0){
                while(vs<ve&&v+1u<sizeof val){val[v++]=data[vs++];}
                val[v]='\0';
                lang_set((lang_id_t)id,val);
                ++n;
            }
        }
    }
    if(applied)*applied=n;
    return n;
}
