#ifndef SIM4000_THROTTLE_H
#define SIM4000_THROTTLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    RC_THROTTLE_OFF,
    RC_THROTTLE_PERCENT,
    RC_THROTTLE_IPS,
    RC_THROTTLE_ORIGINAL
} rc_throttle_mode_t;

typedef struct {
    rc_throttle_mode_t mode;
    uint32_t value;
} rc_throttle_config_t;

typedef struct {
    rc_throttle_config_t config;
    uint64_t wall_start;
    uint64_t cpu_start;
    uint64_t instructions_start;
} rc_throttle_t;

bool rc_throttle_parse(const char *text, rc_throttle_config_t *config);
void rc_throttle_format(rc_throttle_config_t config, char *text, size_t size);
void rc_throttle_begin(rc_throttle_t *throttle, rc_throttle_config_t config,
                       uint64_t wall_ns, uint64_t cpu_ns, uint64_t instructions);
uint64_t rc_throttle_delay(rc_throttle_t *throttle, uint64_t wall_ns,
                           uint64_t cpu_ns, uint64_t instructions);
uint64_t rc_throttle_quantum(rc_throttle_config_t config);

#endif
