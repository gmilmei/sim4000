#include "sim4000/trace.h"

#include <inttypes.h>

bool rc_trace_write_header(FILE *stream)
{
    if (stream == NULL) {
        return false;
    }
    return fprintf(stream, "SIM4000-TRACE\t%u\n",
                   (unsigned)RC_TRACE_FORMAT_VERSION) >= 0 &&
           fputs("# W\tcall\taddress\tprevious\tnew\n", stream) >= 0 &&
           fputs("# S\tcall\tresult\tinstructions\tdelta\taddress\traw"
                 "\topcode\tmode\tindex\tresult-register\tdisplacement"
                 "\teffective\tic\tw0\tw1\tw2\tw3\tir\tim\tex21\tex22"
                 "\tex23\tinterrupts-enabled\tmonitor\treset\tstopped"
                 "\tio-operations\tio-delta\tlast-device\tlast-command"
                 "\tlast-modifier\tinterrupts\tinterrupt-delta\tticks\n",
                 stream) >= 0;
}

bool rc_trace_write_core_write(FILE *stream, uint64_t call,
                               rc_addr_t address, rc_word_t previous_value,
                               rc_word_t new_value)
{
    if (stream == NULL) {
        return false;
    }
    return fprintf(stream, "W\t%" PRIu64 "\t%06" PRIx32
                   "\t%06" PRIx32 "\t%06" PRIx32 "\n",
                   call, address, previous_value, new_value) >= 0;
}

bool rc_trace_write_step(FILE *stream, uint64_t call,
                         const rc_machine_t *machine, rc_step_result_t result,
                         uint64_t instructions_before,
                         uint64_t io_operations_before,
                         uint64_t interrupts_before)
{
    const rc_cpu_t *cpu;
    const rc_instruction_t *instruction;
    uint64_t instruction_delta;
    uint64_t io_delta;
    uint64_t interrupt_delta;

    if (stream == NULL || machine == NULL ||
        machine->instructions_executed < instructions_before ||
        machine->io_operations < io_operations_before ||
        machine->interrupts_serviced < interrupts_before) {
        return false;
    }
    cpu = &machine->cpu;
    instruction = &machine->last_instruction;
    instruction_delta = machine->instructions_executed - instructions_before;
    io_delta = machine->io_operations - io_operations_before;
    interrupt_delta = machine->interrupts_serviced - interrupts_before;

    return fprintf(
               stream,
               "S\t%" PRIu64 "\t%u\t%" PRIu64 "\t%" PRIu64
               "\t%06" PRIx32 "\t%06" PRIx32
               "\t%u\t%u\t%u\t%u\t%" PRId32
               "\t%06" PRIx32 "\t%06" PRIx32
               "\t%06" PRIx32 "\t%06" PRIx32 "\t%06" PRIx32
               "\t%06" PRIx32 "\t%06" PRIx32 "\t%06" PRIx32
               "\t%u\t%u\t%u\t%u\t%u\t%u\t%u"
               "\t%" PRIu64 "\t%" PRIu64 "\t%u\t%u\t%u"
               "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\n",
               call, (unsigned)result, machine->instructions_executed,
               instruction_delta, machine->last_instruction_address,
               instruction->raw, (unsigned)instruction->opcode,
               (unsigned)instruction->address_mode,
               (unsigned)instruction->index_register,
               (unsigned)instruction->result_register,
               instruction->displacement, machine->last_effective_address,
               cpu->instruction_counter, cpu->w[0], cpu->w[1], cpu->w[2],
               cpu->w[3], cpu->interrupt_requests, cpu->interrupt_mask,
               (unsigned)cpu->exception_21, (unsigned)cpu->exception_22,
               (unsigned)cpu->exception_23,
               cpu->interrupts_enabled ? 1U : 0U,
               cpu->monitor_mode ? 1U : 0U, cpu->reset_mode ? 1U : 0U,
               cpu->stopped ? 1U : 0U, machine->io_operations, io_delta,
               (unsigned)machine->last_io_device,
               (unsigned)machine->last_io_command,
               (unsigned)machine->last_io_modifier,
               machine->interrupts_serviced, interrupt_delta,
               machine->virtual_time_ticks) >= 0;
}
