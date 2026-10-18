#include "sim4000/core.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool rc_core_init(rc_core_t *core, size_t halfword_count)
{
    if (core == NULL || halfword_count == 0 ||
        halfword_count > SIZE_MAX / sizeof(*core->data)) {
        return false;
    }

    core->data = calloc(halfword_count, sizeof(*core->data));
    if (core->data == NULL) {
        core->protection_keys = NULL;
        core->halfword_count = 0;
        return false;
    }
    core->protection_keys = calloc(halfword_count / 2 + halfword_count % 2,
                                   sizeof(*core->protection_keys));
    if (core->protection_keys == NULL) {
        free(core->data);
        core->data = NULL;
        core->halfword_count = 0;
        return false;
    }
    core->halfword_count = halfword_count;
    return true;
}

void rc_core_destroy(rc_core_t *core)
{
    if (core == NULL) {
        return;
    }
    free(core->data);
    free(core->protection_keys);
    core->data = NULL;
    core->protection_keys = NULL;
    core->halfword_count = 0;
}

void rc_core_reset(rc_core_t *core)
{
    if (core != NULL && core->data != NULL) {
        memset(core->data, 0, core->halfword_count * sizeof(*core->data));
    }
    if (core != NULL && core->protection_keys != NULL) {
        memset(core->protection_keys, 0,
               core->halfword_count / 2 + core->halfword_count % 2);
    }
}

bool rc_core_read_key(const rc_core_t *core, rc_addr_t address, uint8_t *key)
{
    if (core == NULL || key == NULL || core->data == NULL ||
        core->protection_keys == NULL ||
        (size_t)address >= core->halfword_count) {
        return false;
    }
    *key = core->protection_keys[(size_t)address / 2];
    return true;
}

bool rc_core_write_key(rc_core_t *core, rc_addr_t address, uint8_t key)
{
    if (core == NULL || core->data == NULL || core->protection_keys == NULL ||
        (size_t)address >= core->halfword_count || key > 7U) {
        return false;
    }
    core->protection_keys[(size_t)address / 2] = key;
    return true;
}

bool rc_core_read_halfword(const rc_core_t *core, rc_addr_t address,
                           rc_halfword_t *value)
{
    if (core == NULL || value == NULL || core->data == NULL ||
        (size_t)address >= core->halfword_count) {
        return false;
    }
    *value = core->data[address];
    return true;
}

bool rc_core_write_halfword(rc_core_t *core, rc_addr_t address,
                            rc_halfword_t value)
{
    if (core == NULL || core->data == NULL ||
        (size_t)address >= core->halfword_count) {
        return false;
    }
    core->data[address] = rc_halfword(value);
    return true;
}

bool rc_core_read_word(const rc_core_t *core, rc_addr_t address,
                       rc_word_t *value)
{
    size_t index = (size_t)(address & ~UINT32_C(1));

    if (core == NULL || value == NULL || core->data == NULL ||
        index >= core->halfword_count || core->halfword_count - index < 2) {
        return false;
    }
    *value = ((rc_word_t)core->data[index] << RC_HALFWORD_BITS) |
             (rc_word_t)core->data[index + 1];
    return true;
}

bool rc_core_write_word(rc_core_t *core, rc_addr_t address, rc_word_t value)
{
    size_t index = (size_t)(address & ~UINT32_C(1));

    if (core == NULL || core->data == NULL ||
        index >= core->halfword_count || core->halfword_count - index < 2) {
        return false;
    }
    value = rc_word(value);
    core->data[index] = (rc_halfword_t)(value >> RC_HALFWORD_BITS);
    core->data[index + 1] = (rc_halfword_t)(value & RC_HALFWORD_MASK);
    return true;
}
