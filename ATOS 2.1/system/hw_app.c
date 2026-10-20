/* HW.APP: shell-friendly hardware summary. Prints the current configuration
 * to the console (same data as SYSINFO, compact text) and exits. */
#include "gui_app.h"
#include "system_report.h"
static system_report_t info;
static unsigned slen(const char *s){unsigned n=0;while(s[n])n++;return n;}
static void scat(char *d,const char *s,unsigned cap)
{unsigned n=slen(d);if(n>=cap)return;copytext(d+n,s,cap-n);}
static void snum(char *d,uint32_t v,unsigned cap){char t[12];number(t,v);scat(d,t,cap);}
static void put(const char *s){gui_call(1,0,0,(void *)s,0);}
static void field(const char *label,const char *text){put(label);put(text);put("\n");}
static const char *machine_name(uint32_t n){switch(n){case 0:return "Atari ST";case 1:return "Atari Mega ST";case 2:return "Atari STE";case 3:return "Atari Mega STE";case 4:return "Atari TT";case 5:return "Atari Falcon";}return "unknown";}
static const char *cpu_name(uint32_t n){switch(n){case 0:return "MC68000";case 1:return "MC68010";case 2:return "MC68020";case 3:return "MC68030";case 4:return "MC68040";case 5:return "MC68060";}return "unknown";}
static const char *chip_name(uint32_t n){switch(n){case 0:return "none";case 1:return "ET4000";case 2:return "ATI Mach32";case 3:return "ET4000/W32i";}return "unknown";}
static const char *adapter_name(uint32_t n){switch(n){case 0:return "none";case 1:return "Nova VME";case 2:return "CrazyDots VME";case 3:return "VOFA/STGA";}return "unknown";}
int app_main(void)
{
    char s[160],vol[16];unsigned i;uint32_t usable;
    if(gui_call(56,sizeof(info),0,&info,0)||info.magic!=SYSREPORT_MAGIC||
       info.version!=SYSREPORT_VERSION||info.size!=sizeof(info)){
        put("HW: system report unavailable\n");return 0;
    }
    put("ATARI HARDWARE / CURRENT CONFIGURATION\n");
    field("  MACHINE : ",machine_name(info.machine));
    s[0]=0;scat(s,cpu_name(info.cpu),sizeof(s));
    if(info.clock_source==1){scat(s," @ ",sizeof(s));snum(s,info.clock_mhz,sizeof(s));scat(s," MHz (measured)",sizeof(s));}
    else if(info.clock_source==2){scat(s," @ ",sizeof(s));snum(s,info.clock_mhz,sizeof(s));scat(s," MHz (nominal)",sizeof(s));}
    else scat(s," @ clock unknown",sizeof(s));
    field("  CPU     : ",s);
    field("  FPU     : ",info.fpu==2?"internal":info.fpu==1?"68881/68882":"none / not detected");
    s[0]=0;snum(s,info.st_total/1024,sizeof(s));scat(s," KiB total, ",sizeof(s));
    snum(s,info.st_free/1024,sizeof(s));scat(s," KiB free, largest ",sizeof(s));
    snum(s,info.st_largest/1024,sizeof(s));scat(s," KiB",sizeof(s));
    field("  ST-RAM  : ",s);
    if(info.tt_total){
        usable=info.tt_total-(info.tt_reserved<info.tt_total?info.tt_reserved:0u);
        s[0]=0;snum(s,usable/1024,sizeof(s));scat(s,"/",sizeof(s));
        snum(s,info.tt_total/1024,sizeof(s));scat(s," KiB usable",sizeof(s));
        if(info.tt_reserved){scat(s," (",sizeof(s));snum(s,info.tt_reserved/1024,sizeof(s));scat(s," TOS)",sizeof(s));}
        scat(s,", ",sizeof(s));snum(s,info.tt_free/1024,sizeof(s));scat(s," free, largest ",sizeof(s));
        snum(s,info.tt_largest/1024,sizeof(s));scat(s," KiB",sizeof(s));
        field("  TT-RAM  : ",s);
    }else field("  TT-RAM  : ","not present");
    s[0]=0;scat(s,info.disk_name[0]?info.disk_name:"(no model)",sizeof(s));
    if(info.disk_capacity_known){scat(s," / ",sizeof(s));snum(s,info.disk_sectors/2048,sizeof(s));scat(s," MiB / ",sizeof(s));snum(s,info.disk_sectors,sizeof(s));scat(s," sectors",sizeof(s));}
    else scat(s," / capacity unknown",sizeof(s));
    field("  HDD     : ",s);
    s[0]=0;scat(s,info.active_video==1?"VOFA + ET4000":info.active_video==2?"ATI Mach32":"Shifter",sizeof(s));
    scat(s," / ",sizeof(s));snum(s,info.output_width,sizeof(s));scat(s," x ",sizeof(s));snum(s,info.output_height,sizeof(s));
    scat(s," / ",sizeof(s));snum(s,1u<<info.planes,sizeof(s));scat(s," colours",sizeof(s));
    field("  VIDEO   : ",s);
    s[0]=0;scat(s,info.hardware_report?chip_name(info.detected_chip):"not detected",sizeof(s));
    scat(s," / adapter ",sizeof(s));scat(s,info.hardware_report?adapter_name(info.detected_adapter):"unknown",sizeof(s));
    field("  CARD    : ",s);
    s[0]=0;scat(s,info.blitter&8?"enabled":info.blitter&4?"present (CPU fallback)":"not present",sizeof(s));
    scat(s," / cache ",sizeof(s));snum(s,info.cache_sectors,sizeof(s));scat(s,info.cache_tt?" sectors TT-RAM":" sectors ST-RAM",sizeof(s));
    field("  BLITTER : ",s);
    s[0]=0;
    if(info.netusbee_ethernet||info.netusbee_usb){scat(s,"NetUSBee",sizeof(s));if(info.netusbee_ethernet)scat(s," ethernet",sizeof(s));if(info.netusbee_usb)scat(s," USB",sizeof(s));}
    else scat(s,"nothing detected",sizeof(s));
    field("  ROM PORT: ",s);
    for(i=0;i<info.volumes;i++){
        unsigned j;vol[0]=(char)('C'+i);vol[1]=':';vol[2]=' ';vol[3]=0;
        for(j=0;j<3&&info.volume[i].id[j];++j)vol[3+j]=info.volume[i].id[j];
        vol[6]=0;
        s[0]=0;scat(s,vol,sizeof(s));scat(s," ",sizeof(s));snum(s,info.volume[i].sectors/2048,sizeof(s));scat(s," MiB",sizeof(s));
        if(info.volume[i].boot)scat(s," boot",sizeof(s));
        field("  VOLUME  : ",s);
    }
    return 0;
}
