#include "hardware.h"
#include "system.h"
#include "rtc.h"

static uint8_t mega_reg(uint8_t index) { return REG8(MEGA_RTC_BASE+1u+(uint32_t)index*2u)&15u; }
static void mega_put(uint8_t index,uint8_t value) { REG8(MEGA_RTC_BASE+1u+(uint32_t)index*2u)=value; }
static uint8_t nvget(uint8_t index) { REG8(NVRAM_SELECT)=index; return REG8(NVRAM_DATA); }
static void nvput(uint8_t index,uint8_t value) { REG8(NVRAM_SELECT)=index; REG8(NVRAM_DATA)=value; }
static uint8_t bcd(uint8_t v) { return (uint8_t)((v>>4)*10u+(v&15u)); }
static uint8_t tobcd(uint8_t v) { return (uint8_t)(((v/10u)<<4)|(v%10u)); }
static int valid(const rtc_datetime_t *d)
{ return d->month>=1&&d->month<=12&&d->day>=1&&d->day<=31&&d->hour<24&&d->minute<60&&d->second<60; }

static int read_mega(rtc_datetime_t *d)
{
    uint8_t dummy,mode;
    if(!probe_read8(MEGA_RTC_BASE+1u,&dummy))return 0;
    mode=REG8(MEGA_RTC_BASE+0x1bu); REG8(MEGA_RTC_BASE+0x1bu)=(uint8_t)(mode&~1u);
    d->second=(uint8_t)(mega_reg(0)+10u*mega_reg(1));
    d->minute=(uint8_t)(mega_reg(2)+10u*mega_reg(3));
    d->hour=(uint8_t)(mega_reg(4)+10u*mega_reg(5));
    d->day=(uint8_t)(mega_reg(7)+10u*mega_reg(8));
    d->month=(uint8_t)(mega_reg(9)+10u*mega_reg(10));
    d->year=(uint16_t)(1980u+mega_reg(11)+10u*mega_reg(12));
    return valid(d);
}
static int read_nvram(rtc_datetime_t *d)
{
    uint8_t dummy,b,binary;
    if(!probe_read8(NVRAM_SELECT,&dummy)||!probe_read8(NVRAM_DATA,&dummy))return 0;
    b=nvget(11); binary=(uint8_t)(b&4u);
    d->second=nvget(0);d->minute=nvget(2);d->hour=(uint8_t)(nvget(4)&0x3fu);
    d->day=nvget(7);d->month=nvget(8);d->year=nvget(9);
    if(!binary){d->second=bcd(d->second);d->minute=bcd(d->minute);d->hour=bcd(d->hour);d->day=bcd(d->day);d->month=bcd(d->month);d->year=bcd((uint8_t)d->year);}
    d->year=(uint16_t)(d->year<70u?2000u+d->year:1900u+d->year);
    return valid(d);
}
void rtc_init(void)
{
    g_system.rtc_type=RTC_NONE;
    if(read_mega(&g_system.datetime))g_system.rtc_type=RTC_MEGA;
    else if(read_nvram(&g_system.datetime))g_system.rtc_type=RTC_NVRAM;
    else { g_system.datetime.year=1976;g_system.datetime.month=8;g_system.datetime.day=1;
        g_system.datetime.hour=2;g_system.datetime.minute=30;g_system.datetime.second=0; }
}
int rtc_write(const rtc_datetime_t *d)
{
    uint8_t mode,b,binary,yr;
    if(!valid(d))return 0;
    if(g_system.rtc_type==RTC_MEGA){
        mode=REG8(MEGA_RTC_BASE+0x1bu);REG8(MEGA_RTC_BASE+0x1bu)=(uint8_t)(mode&~9u);
        mega_put(0,d->second%10u);mega_put(1,d->second/10u);mega_put(2,d->minute%10u);mega_put(3,d->minute/10u);
        mega_put(4,d->hour%10u);mega_put(5,d->hour/10u);mega_put(7,d->day%10u);mega_put(8,d->day/10u);
        mega_put(9,d->month%10u);mega_put(10,d->month/10u);yr=(uint8_t)(d->year>=1980?d->year-1980u:0u);
        mega_put(11,yr%10u);mega_put(12,yr/10u);REG8(MEGA_RTC_BASE+0x1bu)=(uint8_t)((mode&~1u)|8u);return 1;
    }
    if(g_system.rtc_type==RTC_NVRAM){b=nvget(11);binary=(uint8_t)(b&4u);nvput(11,(uint8_t)(b|0x80u));yr=(uint8_t)(d->year%100u);
        nvput(0,binary?d->second:tobcd(d->second));nvput(2,binary?d->minute:tobcd(d->minute));nvput(4,binary?d->hour:tobcd(d->hour));
        nvput(7,binary?d->day:tobcd(d->day));nvput(8,binary?d->month:tobcd(d->month));nvput(9,binary?yr:tobcd(yr));nvput(11,b);return 1;}
    return 0;
}
const char *rtc_type_name(uint8_t t){if(t==RTC_MEGA)return "MEGA RTC RP5C15";if(t==RTC_NVRAM)return "MSTE/TT/FALCON NVRAM";return "NO RTC - SOFTWARE DEFAULT";}
