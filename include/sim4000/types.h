#ifndef SIM4000_TYPES_H
#define SIM4000_TYPES_H

#include <stdint.h>

typedef uint16_t rc_halfword_t;
typedef uint32_t rc_word_t;
typedef uint32_t rc_addr_t;

enum {
    RC_HALFWORD_BITS = 12,
    RC_WORD_BITS = 24
};

#define RC_HALFWORD_MASK UINT16_C(0x0fff)
#define RC_WORD_MASK UINT32_C(0x00ffffff)

static inline rc_halfword_t rc_halfword(rc_halfword_t value)
{
    return (rc_halfword_t)(value & RC_HALFWORD_MASK);
}

static inline rc_word_t rc_word(rc_word_t value)
{
    return value & RC_WORD_MASK;
}

static inline int32_t rc_sign_extend_word(rc_word_t value)
{
    value = rc_word(value);
    return (value & UINT32_C(0x00800000)) != 0
        ? (int32_t)(value | UINT32_C(0xff000000))
        : (int32_t)value;
}

static inline int32_t rc_sign_extend_halfword(rc_halfword_t value)
{
    value = rc_halfword(value);
    return (value & UINT16_C(0x0800)) != 0
        ? (int32_t)((uint32_t)value | UINT32_C(0xfffff000))
        : (int32_t)value;
}

#endif
