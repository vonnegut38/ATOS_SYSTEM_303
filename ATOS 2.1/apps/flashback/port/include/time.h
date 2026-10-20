#ifndef ATOS_TIME_H
#define ATOS_TIME_H
typedef long time_t;
#ifdef __cplusplus
extern "C"
#endif
time_t time(time_t *t);
#endif
