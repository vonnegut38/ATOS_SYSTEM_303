#include <stdio.h>
#include <string.h>
#include "config.h"
static int fails;
static void check(int ok,const char *what){if(!ok){printf("FAIL %s\n",what);++fails;}}
int main(void)
{
    bios_config_t c;
    cfg_defaults(&c);
    check(c.magic==CONFIG_MAGIC,"magic");
    check(c.version==CONFIG_VERSION,"version");
    check(c.size==(uint16_t)sizeof(bios_config_t),"size");
    check(c.boot_device==CFG_BOOT_AUTO&&c.video_driver==CFG_VIDEO_AUTO,"defaults auto");
    check(c.boot_delay==4u,"default boot_delay");
    check(c.language[0]=='E'&&c.language[1]=='N',"default language EN");
    check(!cfg_validate(&c),"crc not set yet -> invalid");
    c.crc=cfg_crc(&c);
    check(cfg_validate(&c),"valid after crc");
    c.version=99u;check(!cfg_validate(&c),"wrong version rejected");
    c.version=CONFIG_VERSION;c.boot_device=3u;c.crc=cfg_crc(&c);
    check(cfg_validate(&c),"unknown values still structurally valid");
    c.magic=0u;check(!cfg_validate(&c),"wrong magic rejected");
    c.magic=CONFIG_MAGIC;c.crc=(uint32_t)(c.crc^1u);check(!cfg_validate(&c),"corrupt crc rejected");
    if(fails){printf("CONFIG: %d FAIL\n",fails);return 1;}
    printf("CONFIG: ALL PASS\n");return 0;
}
