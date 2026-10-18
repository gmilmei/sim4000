#ifndef SIM4000_FLOATING_H
#define SIM4000_FLOATING_H

#include <stdbool.h>
#include <stdint.h>

#include "sim4000/types.h"

typedef struct {
    int64_t fraction;
    int32_t exponent;
} rc_float_t;

rc_float_t rc_float_unpack(rc_word_t first_word, rc_word_t second_word);
void rc_float_pack(rc_float_t value, bool low_precision,
                   rc_word_t *first_word, rc_word_t *second_word);
bool rc_float_multiply(rc_float_t left, rc_float_t right,
                       rc_float_t *result);
bool rc_float_divide(rc_float_t left, rc_float_t right, rc_float_t *result,
                     bool *division_by_zero);
bool rc_float_add(rc_float_t left, rc_float_t right, bool subtract,
                  rc_float_t *result);
bool rc_float_from_integer(int32_t value, int32_t scale, rc_float_t *result);
bool rc_float_to_integer(rc_float_t value, int32_t scale, int32_t *result);

#endif
