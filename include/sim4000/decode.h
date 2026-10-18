#ifndef SIM4000_DECODE_H
#define SIM4000_DECODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sim4000/types.h"

typedef struct {
    rc_word_t raw;
    uint8_t opcode;
    uint8_t result_register;
    uint8_t address_mode;
    uint8_t index_register;
    rc_halfword_t displacement_raw;
    int32_t displacement;
    bool relative;
    bool indirect;
} rc_instruction_t;

rc_instruction_t rc_instruction_decode(rc_word_t word);
rc_word_t rc_instruction_encode(uint8_t opcode, uint8_t result_register,
                                bool relative, bool indirect,
                                uint8_t index_register,
                                int32_t displacement);
const char *rc_opcode_name(uint8_t opcode);
const char *rc_opcode_name_extended(uint8_t opcode, bool ix_enabled);
bool rc_opcode_is_assigned(uint8_t opcode);
int rc_instruction_format(rc_word_t word, char *text, size_t text_size);
int rc_instruction_format_extended(rc_word_t word, bool ix_enabled,
                                   char *text, size_t text_size);

#endif
