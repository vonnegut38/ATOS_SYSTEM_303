#include "desktop_module.h"

#define DESKTOP_MODULE_MAX 8
static desktop_module_t *modules[DESKTOP_MODULE_MAX];
static int module_count;

void desktop_module_register(desktop_module_t *module)
{
    int i;
    if(!module||!module->name)return;
    for(i=0;i<module_count;++i){if(modules[i]==module)return;}
    if(module_count<DESKTOP_MODULE_MAX)modules[module_count++]=module;
}
static int same(const char *a,const char *b){int i=0;if(!a||!b)return 0;while(a[i]&&b[i]&&a[i]==b[i])++i;return a[i]==0&&b[i]==0;}
desktop_module_t *desktop_module_find(const char *name)
{
    int i;if(!name)return 0;
    for(i=0;i<module_count;++i){if(same(modules[i]->name,name))return modules[i];}
    return 0;
}
int desktop_module_count(void){return module_count;}
const char *desktop_module_name(int index){return (index>=0&&index<module_count)?modules[index]->name:"";}
int desktop_module_init_all(void *config)
{
    int i,n=0;
    for(i=0;i<module_count;++i){if(!modules[i]->init||modules[i]->init(config))++n;}
    return n;
}
void desktop_module_tick_all(uint32_t now_ticks)
{
    int i;for(i=0;i<module_count;++i){if(modules[i]->tick)modules[i]->tick(now_ticks);}
}
void desktop_module_draw_all(video_surface_t *surface,int x,int y,int w,int h)
{
    int i;for(i=0;i<module_count;++i){if(modules[i]->draw)modules[i]->draw(surface,x,y,w,h);}
}
void desktop_module_reset(void){module_count=0;}
