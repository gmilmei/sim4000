/* Test-only batch adapter. Expected values live in the independent Python
 * rational oracle, not in this adapter or the emulator's FP helpers. */
#include "sim4000/decode.h"
#include "sim4000/floating.h"
#include "sim4000/machine.h"

#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    rc_machine_t machine;
    unsigned opcode, reg, low;
    uint32_t left_first, left_second, right_first, right_second;
    int fields;

    if (!rc_machine_init(&machine, 256)) return 1;
    while ((fields = scanf("%u %u %u %" SCNx32 " %" SCNx32
                           " %" SCNx32 " %" SCNx32,
                           &opcode, &reg, &low, &left_first, &left_second,
                           &right_first, &right_second)) != EOF) {
        rc_float_t left, right, result;
        bool overflow, division_by_zero = false;
        unsigned previous, index, scale_register = 0;
        bool conversion = opcode == 32 || opcode == 53;

        if (fields != 7 || (!conversion && opcode != 48 && opcode != 49 &&
                           opcode != 50 && opcode != 52) ||
            reg > 3 || low > 1)
            goto failure;
        left = rc_float_unpack(left_first, left_second);
        right = rc_float_unpack(right_first, right_second);
        if (opcode == 32) {
            overflow = rc_float_from_integer(rc_sign_extend_word(left_second),
                         rc_sign_extend_word(right_first), &result);
        } else if (opcode == 53) {
            int32_t integer = 99; /* Overflow must leave helper output unchanged. */
            overflow = rc_float_to_integer(left, rc_sign_extend_word(right_first),
                                           &integer);
            result = (rc_float_t){integer, 0};
        } else if (opcode == 48 || opcode == 49)
            overflow = rc_float_add(left, right, opcode == 49, &result);
        else if (opcode == 50) overflow = rc_float_multiply(left, right, &result);
        else overflow = rc_float_divide(left, right, &result, &division_by_zero);

        rc_cpu_reset(&machine.cpu);
        machine.cpu.stopped = false;
        machine.cpu.instruction_counter = 32;
        machine.cpu.exception_21 = (uint8_t)low;
        machine.cpu.exception_22 = 1;
        machine.cpu.exception_23 = 1;
        for (index = 0; index < 4; ++index)
            machine.cpu.w[index] = UINT32_C(0x111111) * (index + 1);
        previous = (reg + 3) % 4;
        machine.cpu.w[previous] = left_first;
        machine.cpu.w[reg] = left_second;
        if (conversion) {
            /* Supply the full signed 24-bit effective address via an unused
             * index register, including scales outside a 12-bit displacement. */
            for (scale_register = 1; scale_register < 4; ++scale_register)
                if (scale_register != previous && scale_register != reg) break;
            machine.cpu.w[scale_register] = right_first;
        }
        if (!rc_machine_write_word(&machine, 98, right_first) ||
            !rc_machine_write_word(&machine, 100, right_second) ||
            !rc_machine_write_word(&machine, 32,
                rc_instruction_encode((uint8_t)opcode, (uint8_t)reg,
                                      false, false, (uint8_t)scale_register,
                                      conversion ? 0 : 100)) ||
            rc_machine_step(&machine) != RC_STEP_OK) goto failure;
        (void)printf("%" PRId64 " %" PRId32 " %u %u",
                     result.fraction, result.exponent,
                     overflow ? 1U : 0U, division_by_zero ? 1U : 0U);
        for (index = 0; index < 4; ++index)
            (void)printf(" %" PRIu32, machine.cpu.w[index]);
        (void)printf(" %u %u %u %" PRIu32 " %" PRIu32 "\n",
                     (unsigned)machine.cpu.exception_21,
                     (unsigned)machine.cpu.exception_22,
                     (unsigned)machine.cpu.exception_23,
                     machine.cpu.interrupt_requests,
                     machine.cpu.instruction_counter);
    }
    rc_machine_destroy(&machine);
    return ferror(stdin) || ferror(stdout) ? 1 : 0;
failure:
    rc_machine_destroy(&machine);
    return 1;
}
