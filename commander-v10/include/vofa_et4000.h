#ifndef ASF_VOFA_ET4000_H
#define ASF_VOFA_ET4000_H
#include <stdint.h>
#define VOFA_IO 0x00d00000UL
#define VOFA_MEM 0x00c00000UL
#define VOFA_BYTES 38400UL
/* Every bus operation may fail; no card access bypasses these callbacks. */
typedef struct {
 int (*read)(uint32_t,uint8_t *);
 int (*write)(uint32_t,uint32_t);
 void (*delay)(void);
 uint32_t (*ticks)(void); /* polling clock, 50 Hz */
} vofa_io_t;
enum {VOFA_OK=1,VOFA_ABSENT=0,VOFA_IO_ERROR=-1,VOFA_CLOCK_ERROR=-2,VOFA_MEMORY_ERROR=-3};
int vofa_et4000_init(const vofa_io_t *io);
#endif
