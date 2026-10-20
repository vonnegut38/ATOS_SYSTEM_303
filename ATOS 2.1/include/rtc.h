#ifndef ASF_RTC_H
#define ASF_RTC_H
#include <stdint.h>
enum rtc_type { RTC_NONE, RTC_MEGA, RTC_NVRAM };
typedef struct { uint16_t year; uint8_t month,day,hour,minute,second; } rtc_datetime_t;
void rtc_init(void);
/* Phase 2 (v10): true if a Mega ST RP5C15 RTC answers with a VALID time.
   Used by machine_detect to separate Mega ST from plain ST.  A readable
   address is not enough: emulators/open-bus decode it without a clock. */
int rtc_mega_present(void);
int rtc_write(const rtc_datetime_t *dt);
/* Kernel-side read for the desktop clock; 0 = no clock, use 12:00 AM. */
int kernel_rtc_read(rtc_datetime_t *dt);
const char *rtc_type_name(uint8_t type);
#endif
