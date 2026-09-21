#include <stdint.h>

int shell_build_path(const char *current,const char *argument,char *output,
                     uint32_t capacity)
{
    uint32_t used=0;
    const char *source;
    if(!current||!argument||!output||capacity<2u)return 0;
    if(argument[0]=='\\'||argument[0]=='/'){
        output[used++]='\\';source=argument+1;
    }else{
        source=current;
        while(*source){if(used+1u>=capacity)return 0;output[used++]=*source++;}
        if(!*argument){output[used]='\0';return 1;}
        if(!used){output[used++]='\\';}
        if(output[used-1u]!='\\'){
            if(used+1u>=capacity)return 0;
            output[used++]='\\';
        }
        source=argument;
    }
    while(*source){
        char c=*source++;
        if(c=='/')c='\\';
        if(used+1u>=capacity)return 0;
        output[used++]=c;
    }
    output[used]='\0';
    return 1;
}
