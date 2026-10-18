#include "sim4000/floating.h"

#include <stddef.h>

enum {
    RC_FLOAT_EXPONENT_MIN = -2048,
    RC_FLOAT_EXPONENT_MAX = 2047
};

#define RC_FLOAT_FRACTION_MASK UINT64_C(0x0000000fffffffff)
#define RC_FLOAT_FRACTION_SIGN UINT64_C(0x0000000800000000)
#define RC_FLOAT_ZERO_EXPONENT (-2048)

static uint64_t magnitude(int64_t value)
{
    return value < 0 ? (uint64_t)(-value) : (uint64_t)value;
}

static uint64_t multiply_magnitudes_shifted(uint64_t left, uint64_t right,
                                            bool *discarded_nonzero)
{
    const uint64_t limb_mask = UINT64_C(0x00ffffff);
    uint64_t left_low = left & limb_mask;
    uint64_t left_high = left >> 24;
    uint64_t right_low = right & limb_mask;
    uint64_t right_high = right >> 24;
    uint64_t limb0 = left_low * right_low;
    uint64_t limb1 = left_low * right_high + left_high * right_low;
    uint64_t limb2 = left_high * right_high;

    limb1 += limb0 >> 24;
    limb0 &= limb_mask;
    limb2 += limb1 >> 24;
    limb1 &= limb_mask;
    *discarded_nonzero = limb0 != 0 || (limb1 & UINT64_C(0x1ff)) != 0;
    return (limb2 << 15) | (limb1 >> 9);
}

static int64_t arithmetic_shift_right_one(int64_t value)
{
    if (value >= 0 || value % 2 == 0) {
        return value / 2;
    }
    return value / 2 - 1;
}

static int64_t arithmetic_shift_right(int64_t value, unsigned count)
{
    uint64_t divisor;
    uint64_t absolute;

    if (count == 0U) {
        return value;
    }
    if (count >= 63U) {
        return value < 0 ? -1 : 0;
    }
    divisor = UINT64_C(1) << count;
    if (value >= 0) {
        return value / (int64_t)divisor;
    }
    absolute = (uint64_t)(-value);
    return -(int64_t)((absolute + divisor - 1U) / divisor);
}

static uint64_t arithmetic_shift_right_bits(uint64_t value, unsigned count)
{
    if (count == 0U) {
        return value;
    }
    if ((value & (UINT64_C(1) << 63)) != 0) {
        return (value >> count) | (~UINT64_C(0) << (64U - count));
    }
    return value >> count;
}

static int64_t guard_fraction_from_bits(uint64_t value)
{
    const uint64_t mask = (UINT64_C(1) << 38) - 1U;
    uint64_t guard = (value >> 26) & mask;

    if ((guard & (UINT64_C(1) << 37)) != 0) {
        return -(int64_t)(((mask + 1U) - guard) & mask);
    }
    return (int64_t)guard;
}

static int64_t clear_guard_bits(int64_t value)
{
    int64_t remainder = value % 4;

    if (remainder < 0) {
        remainder += 4;
    }
    return value - remainder;
}

static void normalize_and_round(int64_t *fraction, int32_t *exponent)
{
    const int64_t normalized_positive = INT64_C(1) << 36;
    const int64_t normalized_negative = -(INT64_C(1) << 37);

again:
    if (*fraction >= (INT64_C(1) << 37) ||
        *fraction < normalized_negative) {
        *fraction = arithmetic_shift_right_one(*fraction);
        ++*exponent;
    } else if (*fraction == 0) {
        *exponent = RC_FLOAT_ZERO_EXPONENT;
    } else {
        while ((*fraction > 0 && *fraction < normalized_positive) ||
               (*fraction < 0 && *fraction >= -normalized_positive)) {
            *fraction *= 2;
            --*exponent;
        }
    }
    if (((uint64_t)*fraction & UINT64_C(2)) != 0) {
        *fraction = clear_guard_bits(*fraction + 4);
        goto again;
    }
    *fraction = clear_guard_bits(*fraction);
}

rc_float_t rc_float_unpack(rc_word_t first_word, rc_word_t second_word)
{
    rc_float_t result;
    uint64_t fraction = ((uint64_t)rc_word(first_word) << 12) |
                        (uint64_t)(rc_word(second_word) >> 12);

    if ((fraction & RC_FLOAT_FRACTION_SIGN) != 0) {
        result.fraction = -(int64_t)((RC_FLOAT_FRACTION_MASK + 1U) -
                                     fraction);
    } else {
        result.fraction = (int64_t)fraction;
    }
    result.exponent = rc_sign_extend_halfword(
        (rc_halfword_t)(second_word & RC_HALFWORD_MASK));
    return result;
}

void rc_float_pack(rc_float_t value, bool low_precision,
                   rc_word_t *first_word, rc_word_t *second_word)
{
    uint64_t fraction;

    if (first_word == NULL || second_word == NULL) {
        return;
    }
    fraction = (uint64_t)value.fraction & RC_FLOAT_FRACTION_MASK;
    if (low_precision) {
        if ((fraction & UINT64_C(4)) != 0) {
            fraction |= UINT64_C(3);
        } else {
            fraction &= ~UINT64_C(3);
        }
    }
    *first_word = (rc_word_t)(fraction >> 12);
    *second_word = (rc_word_t)(((fraction & RC_HALFWORD_MASK) << 12) |
                               ((uint32_t)value.exponent &
                                RC_HALFWORD_MASK));
}

bool rc_float_multiply(rc_float_t left, rc_float_t right,
                       rc_float_t *result)
{
    uint64_t shifted_product;
    bool discarded_nonzero;
    bool negative;
    int64_t fraction;
    int32_t exponent;

    if (result == NULL) {
        return false;
    }
    negative = (left.fraction < 0) != (right.fraction < 0);
    shifted_product = multiply_magnitudes_shifted(
        magnitude(left.fraction), magnitude(right.fraction),
        &discarded_nonzero);
    if (negative) {
        fraction = -(int64_t)shifted_product;
        if (discarded_nonzero) {
            --fraction;
        }
    } else {
        fraction = (int64_t)shifted_product;
    }
    exponent = left.exponent + right.exponent;
    normalize_and_round(&fraction, &exponent);
    result->fraction = fraction / 4;
    result->exponent = exponent;
    return exponent < RC_FLOAT_EXPONENT_MIN ||
           exponent > RC_FLOAT_EXPONENT_MAX;
}

bool rc_float_divide(rc_float_t left, rc_float_t right, rc_float_t *result,
                     bool *division_by_zero)
{
    uint64_t numerator;
    uint64_t denominator;
    uint64_t quotient;
    uint64_t remainder;
    unsigned bit;
    bool negative;
    int64_t fraction;
    int32_t exponent;

    if (result == NULL || division_by_zero == NULL) {
        return false;
    }
    *division_by_zero = right.fraction == 0;
    if (*division_by_zero) {
        *result = left;
        return true;
    }
    if (left.fraction == 0) {
        *result = (rc_float_t){0, RC_FLOAT_ZERO_EXPONENT};
        return false;
    }

    numerator = magnitude(left.fraction);
    denominator = magnitude(right.fraction);
    exponent = left.exponent - right.exponent;
    /* A non-normalized divisor can produce a fraction ratio as large as 2^35.
     * Scale before appending 37 guard positions: otherwise the host quotient
     * can wrap, or exceed normalize_and_round's single-right-shift range.
     * For architectural 36-bit fractions, denominator stays <= 2^35 and
     * the scaled quotient is < 2^38. Normalized-operand results are unchanged.
     * Keep small quotients unscaled to preserve their truncation contract. */
    while (numerator >= (denominator << 1)) {
        denominator <<= 1;
        ++exponent;
    }
    quotient = (numerator / denominator) << 37;
    remainder = numerator % denominator;
    for (bit = 0; bit < 37U; ++bit) {
        remainder <<= 1;
        if (remainder >= denominator) {
            quotient |= UINT64_C(1) << (36U - bit);
            remainder -= denominator;
        }
    }
    negative = (left.fraction < 0) != (right.fraction < 0);
    if (negative) {
        fraction = -(int64_t)quotient;
        if (remainder != 0) {
            --fraction;
        }
    } else {
        fraction = (int64_t)quotient;
    }
    normalize_and_round(&fraction, &exponent);
    result->fraction = fraction / 4;
    result->exponent = exponent;
    return exponent < RC_FLOAT_EXPONENT_MIN ||
           exponent > RC_FLOAT_EXPONENT_MAX;
}

bool rc_float_add(rc_float_t left, rc_float_t right, bool subtract,
                  rc_float_t *result)
{
    const uint64_t sign = UINT64_C(1) << 63;
    int32_t difference;
    int32_t exponent;
    uint64_t left_bits;
    uint64_t right_bits;
    uint64_t result_bits;
    bool overflow_shift;
    int64_t fraction;

    if (result == NULL) {
        return false;
    }
    difference = left.exponent - right.exponent;
    if (difference >= 38) {
        *result = left;
        return false;
    }
    if (difference <= -38 && !subtract) {
        *result = right;
        return false;
    }

    left_bits = (uint64_t)left.fraction << 28;
    right_bits = (uint64_t)right.fraction << 28;
    if (difference <= -38) {
        left_bits = 0;
        exponent = right.exponent;
    } else if (difference > 0) {
        right_bits = arithmetic_shift_right_bits(right_bits,
                                                 (unsigned)difference);
        exponent = left.exponent;
    } else if (difference < 0) {
        left_bits = arithmetic_shift_right_bits(left_bits,
                                                (unsigned)(-difference));
        exponent = right.exponent;
    } else {
        exponent = left.exponent;
    }
    result_bits = subtract ? left_bits - right_bits
                           : left_bits + right_bits;
    if (subtract) {
        overflow_shift = ((left_bits ^ right_bits) &
                          (left_bits ^ result_bits) & sign) != 0;
    } else {
        overflow_shift = (~(left_bits ^ right_bits) &
                          (left_bits ^ result_bits) & sign) != 0;
    }
    if (overflow_shift) {
        bool negative = (left_bits & sign) != 0;

        result_bits >>= 1;
        if (negative) {
            result_bits |= sign;
        }
        ++exponent;
    }
    fraction = guard_fraction_from_bits(result_bits);
    normalize_and_round(&fraction, &exponent);
    result->fraction = fraction / 4;
    result->exponent = exponent;
    return exponent < RC_FLOAT_EXPONENT_MIN ||
           exponent > RC_FLOAT_EXPONENT_MAX;
}

bool rc_float_from_integer(int32_t value, int32_t scale, rc_float_t *result)
{
    int64_t fraction;
    int32_t exponent;

    if (result == NULL) {
        return false;
    }
    if (value == 0) {
        *result = (rc_float_t){0, RC_FLOAT_ZERO_EXPONENT};
        return false;
    }
    fraction = (int64_t)value * 4;
    exponent = 35 + scale;
    normalize_and_round(&fraction, &exponent);
    result->fraction = fraction / 4;
    result->exponent = exponent;
    return exponent < RC_FLOAT_EXPONENT_MIN ||
           exponent > RC_FLOAT_EXPONENT_MAX;
}

bool rc_float_to_integer(rc_float_t value, int32_t scale, int32_t *result)
{
    const int64_t minimum = -INT64_C(8388608);
    const int64_t maximum = INT64_C(8388607);
    int64_t shift = INT64_C(35) - value.exponent - scale;
    int64_t converted;

    if (result == NULL) {
        return false;
    }
    if (value.fraction == 0 || shift >= 63) {
        *result = 0;
        return false;
    }
    if (shift <= 0) {
        uint64_t magnitude_value = magnitude(value.fraction);
        uint64_t limit = value.fraction < 0
            ? (uint64_t)(-minimum) : (uint64_t)maximum;
        uint64_t left = (uint64_t)(-shift);

        if (left >= 63U || magnitude_value > (limit >> left)) {
            return true;
        }
        converted = value.fraction * (int64_t)(UINT64_C(1) << left);
    } else {
        unsigned right = (unsigned)shift;
        uint64_t bits = (uint64_t)value.fraction;
        bool round_up = ((bits >> (right - 1U)) & UINT64_C(1)) != 0;

        converted = arithmetic_shift_right(value.fraction, right);
        if (round_up) {
            ++converted;
        }
    }
    if (converted < minimum || converted > maximum) {
        return true;
    }
    *result = (int32_t)converted;
    return false;
}
