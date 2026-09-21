#ifndef ASF_KERNEL_PARTITION_H
#define ASF_KERNEL_PARTITION_H

#include <stdint.h>

#define KPART_MAX_VOLUMES 14u
enum partition_state {KPART_INVALID,KPART_IO_FAILED,KPART_READY};

enum partition_state kpart_scan(uint8_t boot_root_slot);
uint8_t kpart_count(void);
int kpart_truncated(void);
const char *kpart_id(uint8_t index);
uint32_t kpart_size(uint8_t index);
uint32_t kpart_start(uint8_t index);
int kpart_accessible(uint8_t index);
int kpart_is_boot(uint8_t index);
uint32_t kpart_boot_start(void);
const char *kpart_boot_id(void);

#endif
