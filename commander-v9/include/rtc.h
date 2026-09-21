#ifndef ASF_RTC_H
#define ASF_RTC_H
#include <stdint.h>
enum rtc_type { RTC_NONE, RTC_MEGA, RTC_NVRAM };
typedef struct { uint16_t year; uint8_t month,day,hour,minute,second; } rtc_datetime_t;
void rtc_init(void);
int rtc_write(const rtc_datetime_t *dt);
const char *rtc_type_name(uint8_t type);
#endif
