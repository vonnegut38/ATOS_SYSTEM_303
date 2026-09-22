#include <stdio.h>
#include "irq.h"
static int fails;
static void check(int ok,const char*w){if(!ok){printf("FAIL %s\n",w);++fails;}}
static int vbl_hits,hbl_hits;static void *last_ctx;
static void vbl_handler(irq_source_t s,void *c){(void)s;vbl_hits+=1;last_ctx=c;}
static void hbl_handler(irq_source_t s,void *c){(void)s;(void)c;hbl_hits+=1;}
int main(void)
{
    int marker=42;
    irq_reset();
    check(irq_register(IRQ_SRC_VBL,vbl_handler,&marker),"register vbl");
    check(irq_register(IRQ_SRC_HBL,hbl_handler,0),"register hbl");
    check(!irq_register(IRQ_SRC_VBL,vbl_handler,&marker),"duplicate rejected");
    check(!irq_register(IRQ_SRC_COUNT,vbl_handler,0),"bad source rejected");
    check(!irq_register(IRQ_SRC_VBL,0,0),"null handler rejected");
    check(irq_handler_count(IRQ_SRC_VBL)==1,"vbl count 1");
    irq_dispatch(IRQ_SRC_VBL);
    irq_dispatch(IRQ_SRC_VBL);
    check(vbl_hits==2,"vbl called twice");
    check(last_ctx==&marker,"context passed");
    check(hbl_hits==0,"hbl not called");
    check(irq_dispatch_count(IRQ_SRC_VBL)==2,"dispatch counter");
    irq_dispatch(IRQ_SRC_HBL);
    check(hbl_hits==1,"hbl called");
    check(irq_unregister(IRQ_SRC_VBL,vbl_handler),"unregister");
    irq_dispatch(IRQ_SRC_VBL);
    check(vbl_hits==2,"no call after unregister");
    /* fill all slots then reject the fifth */
    {int i,ok=1;for(i=0;i<5;++i){if(!irq_register(IRQ_SRC_MFP_C,vbl_handler,(void*)(long)(i+1)))ok=0;}
     check(!ok,"slot overflow rejected");
     check(irq_handler_count(IRQ_SRC_MFP_C)==4,"slots capped at 4");}
    check(irq_source_name(IRQ_SRC_VBL)[0]=='V',"name");
    irq_reset();
    check(irq_handler_count(IRQ_SRC_MFP_C)==0&&irq_dispatch_count(IRQ_SRC_VBL)==0,"reset");
    if(fails){printf("IRQ: %d FAIL\n",fails);return 1;}
    printf("IRQ: ALL PASS\n");return 0;
}
