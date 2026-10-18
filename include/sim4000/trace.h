#ifndef SIM4000_TRACE_H
#define SIM4000_TRACE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "sim4000/machine.h"

enum {
    RC_TRACE_FORMAT_VERSION = 1
};

bool rc_trace_write_header(FILE *stream);
bool rc_trace_write_core_write(FILE *stream, uint64_t call,
                               rc_addr_t address, rc_word_t previous_value,
                               rc_word_t new_value);
bool rc_trace_write_step(FILE *stream, uint64_t call,
                         const rc_machine_t *machine, rc_step_result_t result,
                         uint64_t instructions_before,
                         uint64_t io_operations_before,
                         uint64_t interrupts_before);

#endif
