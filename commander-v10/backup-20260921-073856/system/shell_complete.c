#include <stdint.h>

typedef struct {
    const char *prefix;
    uint32_t prefix_length;
    uint32_t matches;
    char completion[64];
    uint32_t attributes;
} shell_completion_t;

int shell_prepare_completion(const char *current,const char *token,
                             char *directory,uint32_t capacity,
                             uint32_t *leaf_offset)
{
    uint32_t i,last=0,used=0;const char *source;
    if(!current||!token||!directory||capacity<2u||!leaf_offset)return 0;
    for(i=0;token[i];++i)if(token[i]=='\\'||token[i]=='/')last=i+1u;
    *leaf_offset=last;
    if(last){
        if(token[0]=='\\'||token[0]=='/'){directory[used++]='\\';source=token+1;}
        else{
            source=current;
            while(*source){if(used+1u>=capacity)return 0;directory[used++]=*source++;}
            if(!used)directory[used++]='\\';
            if(directory[used-1u]!='\\'){if(used+1u>=capacity)return 0;directory[used++]='\\';}
            source=token;
        }
        while((uint32_t)(source-token)<last&&*source){
            char c=*source++;if(c=='/')c='\\';
            if(c=='\\'&&(uint32_t)(source-token)==last)break;
            if(used+1u>=capacity)return 0;
            directory[used++]=c;
        }
    }else{
        source=current;
        while(*source){if(used+1u>=capacity)return 0;directory[used++]=*source++;}
    }
    if(!used)directory[used++]='\\';
    directory[used]='\0';return 1;
}

static uint8_t folded(uint8_t c)
{
    if(c>='a'&&c<='z')c=(uint8_t)(c-'a'+'A');
    return c;
}

int shell_completion_entry(const char *name,uint32_t size,
                           uint32_t attributes,void *raw_context)
{
    shell_completion_t *context=(shell_completion_t *)raw_context;
    uint32_t i;
    (void)size;(void)attributes;
    if(!name||!context||!context->prefix_length)return 1;
    for(i=0;i<context->prefix_length;++i)
        if(!name[i]||folded((uint8_t)name[i])!=folded((uint8_t)context->prefix[i]))return 1;
    if(!context->matches){
        for(i=0;i<63u&&name[i];++i)context->completion[i]=name[i];
        context->completion[i]='\0';
        context->attributes=attributes;
    }else{
        for(i=0;i<63u&&context->completion[i]&&name[i]&&
            folded((uint8_t)context->completion[i])==folded((uint8_t)name[i]);++i){}
        context->completion[i]='\0';
        if(context->attributes!=attributes)context->attributes=0;
    }
    ++context->matches;
    return 1;
}
