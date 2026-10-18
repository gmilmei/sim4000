#ifndef SIM4000_MONITOR_H
#define SIM4000_MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sim4000/core.h"

typedef struct {
    uint64_t byte_size;
    uint64_t word_count;
    uint32_t trailing_bytes;
    uint64_t checksum_fnv1a;
} rc_monitor_info_t;

int rc_monitor_inspect(const char *path, rc_monitor_info_t *info,
                       char *error, size_t error_size);
int rc_monitor_load(const char *path, rc_core_t *core,
                    bool set_disc_load_flag,
                    rc_addr_t *load_address, rc_addr_t *entry_address,
                    char *error, size_t error_size);

#endif
