#include "sim4000/decode.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

static const char *const opcode_names[64] = {
    "aw", "io", "bl", "hl", "la", "lo", "lx", "wa",
    "ws", "am", "wm", "al", "ml", "jl", "jd", "je",
    "xl", "bs", "ba", "bz", "rl", "sp", "kl", "rs",
    "wd", "rx", "hs", "xs", "pl", "ps", "ms", "is",
    "ci", "ac", "ns", "nd", "as", "ad", "ls", "ld",
    "sh", "sl", "se", "sn", "so", "sz", "sx", "ic",
    "fa", "fs", "fm", "ks", "fd", "cf", "dl", "ds",
    "aa", "ss", NULL, NULL, NULL, NULL, NULL, NULL
};

rc_instruction_t rc_instruction_decode(rc_word_t word)
{
    rc_instruction_t instruction;

    word = rc_word(word);
    instruction.raw = word;
    instruction.opcode = (uint8_t)((word >> 18) & UINT32_C(0x3f));
    instruction.result_register = (uint8_t)((word >> 16) & UINT32_C(3));
    instruction.relative = (word & UINT32_C(0x00008000)) != 0;
    instruction.indirect = (word & UINT32_C(0x00004000)) != 0;
    instruction.address_mode =
        (uint8_t)((instruction.relative ? 2U : 0U) |
                  (instruction.indirect ? 1U : 0U));
    instruction.index_register = (uint8_t)((word >> 12) & UINT32_C(3));
    instruction.displacement_raw = (rc_halfword_t)(word & RC_HALFWORD_MASK);
    instruction.displacement =
        rc_sign_extend_halfword(instruction.displacement_raw);
    return instruction;
}

rc_word_t rc_instruction_encode(uint8_t opcode, uint8_t result_register,
                                bool relative, bool indirect,
                                uint8_t index_register,
                                int32_t displacement)
{
    rc_word_t word = ((rc_word_t)(opcode & UINT8_C(0x3f)) << 18) |
                     ((rc_word_t)(result_register & UINT8_C(3)) << 16) |
                     ((rc_word_t)(index_register & UINT8_C(3)) << 12) |
                     ((rc_word_t)displacement & RC_HALFWORD_MASK);

    if (relative) {
        word |= UINT32_C(0x00008000);
    }
    if (indirect) {
        word |= UINT32_C(0x00004000);
    }
    return word;
}

const char *rc_opcode_name(uint8_t opcode)
{
    return opcode < 64U ? opcode_names[opcode] : NULL;
}

bool rc_opcode_is_assigned(uint8_t opcode)
{
    return rc_opcode_name(opcode) != NULL;
}

const char *rc_opcode_name_extended(uint8_t opcode, bool ix_enabled)
{
    return opcode == 61 && ix_enabled ? "ix" : rc_opcode_name(opcode);
}

int rc_instruction_format(rc_word_t word, char *text, size_t text_size)
{
    return rc_instruction_format_extended(word, false, text, text_size);
}

int rc_instruction_format_extended(rc_word_t word, bool ix_enabled,
                                   char *text, size_t text_size)
{
    rc_instruction_t instruction = rc_instruction_decode(word);
    const char *name = rc_opcode_name_extended(instruction.opcode, ix_enabled);
    char operand[48];
    int operand_length;
    int length;

    if (text == NULL || text_size == 0) {
        return -1;
    }
    if (instruction.index_register != 0) {
        operand_length = snprintf(
            operand, sizeof(operand), "x%u%+" PRId32,
            (unsigned)instruction.index_register, instruction.displacement);
    } else {
        operand_length = snprintf(operand, sizeof(operand), "%" PRId32,
                                  instruction.displacement);
    }
    if (operand_length < 0 || (size_t)operand_length >= sizeof(operand)) {
        return -1;
    }
    length = snprintf(text, text_size, "%s%s w%u %s%s%s",
                      name != NULL ? name : "??",
                      instruction.relative ? "." : "",
                      (unsigned)instruction.result_register,
                      instruction.indirect ? "(" : "", operand,
                      instruction.indirect ? ")" : "");
    return length < 0 || (size_t)length >= text_size ? -1 : 0;
}
