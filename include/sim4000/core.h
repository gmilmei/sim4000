#ifndef SIM4000_CORE_H
#define SIM4000_CORE_H

#include <stdbool.h>
#include <stddef.h>

#include "sim4000/types.h"

typedef struct {
    rc_halfword_t *data;
    uint8_t *protection_keys; /* One three-bit key per pair of RC bytes. */
    size_t halfword_count;
} rc_core_t;

bool rc_core_init(rc_core_t *core, size_t halfword_count);
void rc_core_destroy(rc_core_t *core);
void rc_core_reset(rc_core_t *core);

bool rc_core_read_halfword(const rc_core_t *core, rc_addr_t address,
                           rc_halfword_t *value);
bool rc_core_write_halfword(rc_core_t *core, rc_addr_t address,
                            rc_halfword_t value);
bool rc_core_read_word(const rc_core_t *core, rc_addr_t address,
                       rc_word_t *value);
bool rc_core_write_word(rc_core_t *core, rc_addr_t address, rc_word_t value);

/* Host-side key access: either valid halfword selects its containing word.
 * Ordinary data writes preserve keys. Invalid addresses/keys are rejected. */
bool rc_core_read_key(const rc_core_t *core, rc_addr_t address, uint8_t *key);
bool rc_core_write_key(rc_core_t *core, rc_addr_t address, uint8_t key);

#endif
