#include "sim4000/throttle.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { ORIGINAL_IPS = 250000, MAX_IPS = 1000000000 };

static bool equal(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (toupper((unsigned char)*left++) != toupper((unsigned char)*right++)) {
            return false;
        }
    }
    return *left == *right;
}

bool rc_throttle_parse(const char *text, rc_throttle_config_t *config)
{
    rc_throttle_config_t parsed = {RC_THROTTLE_OFF, 0};
    uint64_t value = 0;
    const char *suffix = text;

    if (text == NULL || config == NULL) {
        return false;
    }
    if (equal(text, "OFF")) {
        *config = parsed;
        return true;
    }
    if (equal(text, "ORIGINAL")) {
        parsed.mode = RC_THROTTLE_ORIGINAL;
        parsed.value = ORIGINAL_IPS;
        *config = parsed;
        return true;
    }
    while (*suffix >= '0' && *suffix <= '9') {
        value = value * 10U + (unsigned)(*suffix++ - '0');
        if (value > MAX_IPS) {
            return false;
        }
    }
    if (suffix == text || value == 0) {
        return false;
    }
    if (strcmp(suffix, "%") == 0 && value <= 100) {
        parsed.mode = RC_THROTTLE_PERCENT;
    } else if (equal(suffix, "IPS")) {
        parsed.mode = RC_THROTTLE_IPS;
    } else {
        return false;
    }
    parsed.value = (uint32_t)value;
    *config = parsed;
    return true;
}

void rc_throttle_format(rc_throttle_config_t config, char *text, size_t size)
{
    if (config.mode == RC_THROTTLE_ORIGINAL) {
        (void)snprintf(text, size, "original (approx. 250000 ips)");
    } else if (config.mode == RC_THROTTLE_PERCENT) {
        (void)snprintf(text, size, "%u%% of one host CPU", (unsigned)config.value);
    } else if (config.mode == RC_THROTTLE_IPS) {
        (void)snprintf(text, size, "%u ips", (unsigned)config.value);
    } else {
        (void)snprintf(text, size, "off");
    }
}

void rc_throttle_begin(rc_throttle_t *throttle, rc_throttle_config_t config,
                       uint64_t wall_ns, uint64_t cpu_ns, uint64_t instructions)
{
    *throttle = (rc_throttle_t){config, wall_ns, cpu_ns, instructions};
}

uint64_t rc_throttle_delay(rc_throttle_t *throttle, uint64_t wall_ns,
                           uint64_t cpu_ns, uint64_t instructions)
{
    uint64_t elapsed = wall_ns - throttle->wall_start;
    uint64_t target = 0;
    rc_throttle_config_t config = throttle->config;

    if (config.mode == RC_THROTTLE_PERCENT && config.value < 100) {
        target = (cpu_ns - throttle->cpu_start) * 100U / config.value;
    } else if (config.mode == RC_THROTTLE_IPS ||
               config.mode == RC_THROTTLE_ORIGINAL) {
        uint64_t count = instructions - throttle->instructions_start;
        target = (count / config.value) * UINT64_C(1000000000) +
            (count % config.value) * UINT64_C(1000000000) / config.value;
    }
    if (target > elapsed) {
        return target - elapsed;
    }
    /* No unbounded credit after a debugger stop or host scheduling stall. */
    if (elapsed >= UINT64_C(100000000)) {
        rc_throttle_begin(throttle, config, wall_ns, cpu_ns, instructions);
    }
    return 0;
}

uint64_t rc_throttle_quantum(rc_throttle_config_t config)
{
    if (config.mode == RC_THROTTLE_IPS || config.mode == RC_THROTTLE_ORIGINAL) {
        uint64_t quantum = config.value / 200U;
        return quantum < 1 ? 1 : quantum > 10000 ? 10000 : quantum;
    }
    return 10000;
}
