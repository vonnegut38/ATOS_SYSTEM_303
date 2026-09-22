#ifndef ASF_DIAGNOSTICS_H
#define ASF_DIAGNOSTICS_H
#include <stdint.h>
enum diag_result { DIAG_NOT_RUN, DIAG_PASS, DIAG_FAIL, DIAG_NOT_PRESENT, DIAG_UNSUPPORTED };
uint8_t cpu_detect_type(void);
uint8_t cpu_detect_fpu(uint8_t cpu_type);
const char *cpu_type_name(uint8_t type);
enum diag_result diag_cpu(void);
enum diag_result diag_stram(void);
enum diag_result diag_ttram(void);
enum diag_result diag_shifter(void);
enum diag_result diag_blitter(void);
enum diag_result diag_mfp(void);
enum diag_result diag_ikbd(void);
enum diag_result diag_rtc(void);
const char *diag_result_name(enum diag_result result);
#endif
