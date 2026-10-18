#define _POSIX_C_SOURCE 200809L

#include "sim4000/catalog.h"
#include "sim4000/config.h"
#include "sim4000/core.h"
#include "sim4000/core_image.h"
#include "sim4000/cpu.h"
#include "sim4000/decode.h"
#include "sim4000/floating.h"
#include "sim4000/image.h"
#include "sim4000/machine.h"
#include "sim4000/monitor.h"
#include "sim4000/trace.h"
#include "sim4000/types.h"
#include "telnet.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned failures;

#define CHECK(condition) do {                                                \
    if (!(condition)) {                                                      \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n",                 \
                      __FILE__, __LINE__, #condition);                       \
        ++failures;                                                          \
    }                                                                        \
} while (0)

static void test_types(void)
{
    CHECK(rc_halfword(UINT16_C(0xffff)) == UINT16_C(0x0fff));
    CHECK(rc_word(UINT32_C(0xffffffff)) == UINT32_C(0x00ffffff));
    CHECK(rc_sign_extend_word(UINT32_C(0x007fffff)) == INT32_C(8388607));
    CHECK(rc_sign_extend_word(UINT32_C(0x00800000)) == INT32_C(-8388608));
    CHECK(rc_sign_extend_word(UINT32_C(0x00ffffff)) == INT32_C(-1));
    CHECK(rc_sign_extend_halfword(UINT16_C(0x07ff)) == INT32_C(2047));
    CHECK(rc_sign_extend_halfword(UINT16_C(0x0800)) == INT32_C(-2048));
    CHECK(rc_sign_extend_halfword(UINT16_C(0x0fff)) == INT32_C(-1));
}

static void test_core(void)
{
    rc_core_t core = {0};
    rc_halfword_t halfword = 0;
    rc_word_t word = 0;

    CHECK(rc_core_init(&core, 16));
    CHECK(rc_core_write_halfword(&core, 0, UINT16_C(0xffff)));
    CHECK(rc_core_read_halfword(&core, 0, &halfword));
    CHECK(halfword == RC_HALFWORD_MASK);
    CHECK(rc_core_write_word(&core, 3, UINT32_C(0xabcdef)));
    CHECK(rc_core_read_word(&core, 3, &word));
    CHECK(word == UINT32_C(0xabcdef));
    CHECK(rc_core_read_word(&core, 2, &word));
    CHECK(word == UINT32_C(0xabcdef));
    CHECK(rc_core_write_word(&core, 15, 0));
    CHECK(!rc_core_write_word(&core, 16, 0));
    CHECK(!rc_core_read_halfword(&core, 16, &halfword));
    rc_core_reset(&core);
    CHECK(rc_core_read_word(&core, 3, &word));
    CHECK(word == 0);
    rc_core_destroy(&core);
}

static void test_architectural_trace(void)
{
    rc_machine_t machine;
    FILE *stream;
    char output[4096];
    size_t length;

    CHECK(rc_machine_init(&machine, 64));
    machine.instructions_executed = 12;
    machine.io_operations = 4;
    machine.interrupts_serviced = 2;
    machine.virtual_time_ticks = 9;
    machine.last_instruction_address = UINT32_C(0x20);
    machine.last_instruction = rc_instruction_decode(UINT32_C(0x123456));
    machine.last_effective_address = UINT32_C(0x2a);
    machine.last_io_device = 15;
    machine.last_io_command = 2;
    machine.last_io_modifier = 3;
    machine.cpu.instruction_counter = UINT32_C(0x22);
    machine.cpu.w[0] = UINT32_C(0xabcdef);
    machine.cpu.interrupt_requests = UINT32_C(0x11);
    machine.cpu.interrupt_mask = UINT32_C(0x22);
    machine.cpu.exception_21 = 1;
    machine.cpu.interrupts_enabled = true;
    machine.cpu.monitor_mode = true;

    stream = tmpfile();
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK(rc_trace_write_header(stream));
        CHECK(rc_trace_write_core_write(stream, 7, UINT32_C(0x24),
                                        UINT32_C(0x010203),
                                        UINT32_C(0xa0b0c0)));
        CHECK(rc_trace_write_step(stream, 7, &machine, RC_STEP_OK, 10, 3, 1));
        CHECK(fflush(stream) == 0);
        CHECK(fseek(stream, 0, SEEK_SET) == 0);
        length = fread(output, 1, sizeof(output) - 1U, stream);
        output[length] = '\0';
        CHECK(strncmp(output, "SIM4000-TRACE\t1\n",
                      sizeof("SIM4000-TRACE\t1\n") - 1U) == 0);
        CHECK(strstr(output,
                     "W\t7\t000024\t010203\ta0b0c0\n") != NULL);
        CHECK(strstr(output,
                     "S\t7\t0\t12\t2\t000020\t123456\t") != NULL);
        CHECK(strstr(output, "\t4\t1\t15\t2\t3\t2\t1\t9\n") != NULL);
        (void)fclose(stream);
    }
    rc_machine_destroy(&machine);
}

static void test_core_image(void)
{
    char path[] = "/tmp/sim4000-core-image-XXXXXX";
    const unsigned char input[] = {
        0x12, 0x34, 0x56, 0xab, 0xcd, 0xef
    };
    unsigned char output[sizeof(input)] = {0};
    char error[256];
    rc_machine_t machine;
    rc_word_t word = 0;
    size_t word_count = 0;
    int descriptor = mkstemp(path);
    FILE *file;

    CHECK(descriptor >= 0);
    if (descriptor < 0) {
        return;
    }
    file = fdopen(descriptor, "wb");
    CHECK(file != NULL);
    if (file == NULL) {
        (void)close(descriptor);
        (void)unlink(path);
        return;
    }
    CHECK(fwrite(input, 1, sizeof(input), file) == sizeof(input));
    CHECK(fclose(file) == 0);

    CHECK(rc_machine_init(&machine, 16));
    machine.cpu.w[2] = UINT32_C(0x777777);
    CHECK(rc_core_image_load(path, &machine, 4, &word_count,
                             error, sizeof(error)) == 0);
    CHECK(word_count == 2);
    CHECK(rc_machine_read_word(&machine, 4, &word));
    CHECK(word == UINT32_C(0x123456));
    CHECK(rc_machine_read_word(&machine, 6, &word));
    CHECK(word == UINT32_C(0xabcdef));
    CHECK(rc_core_image_save(path, &machine, 4, 2,
                             error, sizeof(error)) == 0);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fread(output, 1, sizeof(output), file) == sizeof(output));
        CHECK(fgetc(file) == EOF);
        CHECK(fclose(file) == 0);
        CHECK(memcmp(input, output, sizeof(input)) == 0);
    }

    file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fwrite(input, 1, sizeof(input) - 1, file) == sizeof(input) - 1);
        CHECK(fclose(file) == 0);
    }
    machine.cpu.w[2] = UINT32_C(0x777777);
    CHECK(rc_core_image_load(path, &machine, 4, &word_count,
                             error, sizeof(error)) != 0);
    CHECK(rc_machine_read_word(&machine, 4, &word));
    CHECK(word == UINT32_C(0x777777));
    CHECK(rc_core_image_load(path, &machine, 5, &word_count,
                             error, sizeof(error)) != 0);
    CHECK(rc_core_image_save(path, &machine, 14, 2,
                             error, sizeof(error)) != 0);

    rc_machine_destroy(&machine);
    CHECK(unlink(path) == 0);
}

static void test_cpu_state(void)
{
    rc_cpu_t cpu;
    unsigned interrupt = 99;

    rc_cpu_reset(&cpu);
    CHECK(cpu.instruction_counter == 8);
    CHECK(cpu.interrupt_mask == UINT32_C(0x00800000));
    CHECK(cpu.monitor_mode);
    CHECK(cpu.reset_mode);
    CHECK(cpu.stopped);
    rc_cpu_request_interrupt(&cpu, 3);
    CHECK(!rc_cpu_next_interrupt(&cpu, &interrupt));
    cpu.interrupt_mask |= UINT32_C(1) << 20;
    cpu.interrupts_enabled = true;
    CHECK(rc_cpu_next_interrupt(&cpu, &interrupt));
    CHECK(interrupt == 6);
    rc_cpu_clear_interrupt(&cpu, 3);
    CHECK(!rc_cpu_next_interrupt(&cpu, &interrupt));
    rc_cpu_request_interrupt(&cpu, 0);
    cpu.interrupts_enabled = false;
    CHECK(rc_cpu_next_interrupt(&cpu, &interrupt));
    CHECK(interrupt == 0);
}

static void test_decode(void)
{
    rc_instruction_t instruction;
    rc_word_t word;
    char text[80];

    word = rc_instruction_encode(11, 2, true, true, 3, -17);
    instruction = rc_instruction_decode(word);
    CHECK(instruction.raw == word);
    CHECK(instruction.opcode == 11);
    CHECK(instruction.result_register == 2);
    CHECK(instruction.address_mode == 3);
    CHECK(instruction.relative);
    CHECK(instruction.indirect);
    CHECK(instruction.index_register == 3);
    CHECK(instruction.displacement == -17);
    CHECK(strcmp(rc_opcode_name(9), "am") == 0);
    CHECK(rc_opcode_is_assigned(57));
    CHECK(!rc_opcode_is_assigned(58));
    CHECK(rc_instruction_format(
        rc_instruction_encode(19, 3, true, true, 2, -17),
        text, sizeof(text)) == 0);
    CHECK(strcmp(text, "bz. w3 (x2-17)") == 0);
    CHECK(rc_instruction_format(
        rc_instruction_encode(11, 0, false, false, 0, 42),
        text, sizeof(text)) == 0);
    CHECK(strcmp(text, "al w0 42") == 0);
    CHECK(rc_instruction_format(word, text, 4) != 0);
}

typedef struct {
    unsigned count;
    rc_addr_t address;
    rc_word_t previous;
    rc_word_t value;
} write_observer_capture_t;

static void capture_machine_write(void *context, rc_addr_t address,
                                  rc_word_t previous, rc_word_t value)
{
    write_observer_capture_t *capture = context;

    ++capture->count;
    capture->address = address;
    capture->previous = previous;
    capture->value = value;
}

static void test_machine_write_observer(void)
{
    rc_machine_t machine;
    write_observer_capture_t capture = {0};
    rc_device_t device = {0};
    rc_device_t other_device = {0};

    CHECK(rc_machine_init(&machine, 32));
    rc_machine_set_write_observer(&machine, capture_machine_write, &capture);
    CHECK(rc_machine_write_word(&machine, 10, UINT32_C(0xabcdef)));
    CHECK(capture.count == 1);
    CHECK(capture.address == 10);
    CHECK(capture.previous == 0);
    CHECK(capture.value == UINT32_C(0xabcdef));
    CHECK(rc_machine_write_halfword(&machine, 11, UINT16_C(0x0123)));
    CHECK(capture.count == 2);
    CHECK(capture.address == 10);
    CHECK(capture.previous == UINT32_C(0xabcdef));
    CHECK(capture.value == UINT32_C(0xabc123));
    CHECK(rc_machine_write_word(&machine, 0, UINT32_C(0x123456)));
    CHECK(capture.count == 2);
    rc_machine_set_write_observer(&machine, NULL, NULL);
    CHECK(rc_machine_write_word(&machine, 12, UINT32_C(0x654321)));
    CHECK(capture.count == 2);
    CHECK(rc_machine_attach_device(&machine, 5, &device));
    CHECK(machine.devices[5] == &device);
    CHECK(!rc_machine_detach_device(&machine, 5, &other_device));
    CHECK(machine.devices[5] == &device);
    CHECK(rc_machine_detach_device(&machine, 5, &device));
    CHECK(machine.devices[5] == NULL);
    CHECK(!rc_machine_detach_device(&machine, 5, &device));
    CHECK(!rc_machine_detach_device(&machine, RC_DEVICE_COUNT, &device));
    rc_machine_destroy(&machine);
}

static void put_instruction(rc_machine_t *machine, rc_addr_t address,
                            uint8_t opcode, uint8_t result_register,
                            bool relative, bool indirect,
                            uint8_t index_register, int32_t displacement)
{
    CHECK(rc_core_write_word(
        &machine->core, address,
        rc_instruction_encode(opcode, result_register, relative, indirect,
                              index_register, displacement)));
}

static void test_machine_addressing(void)
{
    rc_machine_t machine;

    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.w[2] = 20;
    put_instruction(&machine, 32, 11, 1, true, false, 2, -4);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == 48);
    CHECK(machine.cpu.instruction_counter == 34);

    machine.cpu.instruction_counter = 34;
    CHECK(rc_core_write_word(&machine.core, 60, 100));
    put_instruction(&machine, 34, 11, 0, false, true, 2, 40);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == 100);

    machine.cpu.instruction_counter = 36;
    put_instruction(&machine, 36, 9, 0, false, false, 0, 10);
    put_instruction(&machine, 38, 11, 3, false, false, 0, 5);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[3] == 15);
    CHECK(machine.cpu.instruction_counter == 40);
    CHECK(machine.instructions_executed == 4);
    rc_machine_destroy(&machine);
}

static void test_all_address_modes(void)
{
    rc_machine_t machine;
    unsigned relative;
    unsigned indirect;
    unsigned indexed;

    CHECK(rc_machine_init(&machine, 512));
    machine.cpu.stopped = false;
    for (relative = 0; relative < 2; ++relative) {
        for (indirect = 0; indirect < 2; ++indirect) {
            for (indexed = 0; indexed < 2; ++indexed) {
                rc_word_t expected = 10U + (relative != 0 ? 100U : 0U) +
                                     (indexed != 0 ? 20U : 0U);
                machine.cpu.instruction_counter = 100;
                machine.cpu.w[1] = 20;
                machine.cpu.w[2] = 0;
                put_instruction(&machine, 100, 11, 2, relative != 0,
                                indirect != 0, indexed != 0 ? 1 : 0, 10);
                if (indirect != 0) {
                    CHECK(rc_machine_write_word(&machine, expected,
                                                200U + expected));
                    expected = 200U + expected;
                }
                CHECK(rc_machine_step(&machine) == RC_STEP_OK);
                CHECK(machine.cpu.w[2] == expected);
            }
        }
    }
    rc_machine_destroy(&machine);
}

static void test_maximum_architectural_address(void)
{
    /* Manual sections 3.2-3.5 and 14.5/14.8: unsigned 24-bit byte addresses,
       ignored overflow, aligned words, and instruction fetch/execution. */
    static const struct {
        rc_addr_t ic;
        rc_word_t index;
        int32_t displacement;
        bool relative;
        bool indirect;
        rc_word_t expected;
    } cases[] = {
        {32, 0, -1, false, false, UINT32_C(0xffffff)},
        {32, UINT32_C(0xffffff), 1, false, false, 0},
        {32, UINT32_C(0xffffff), 2047, false, false, 2046},
        {32, 0, -2048, false, false, UINT32_C(0xfff800)},
        {32, UINT32_C(0x800000), -2048, false, false,
         UINT32_C(0x7ff800)},
        {UINT32_C(0xfffffe), 0, 4, true, false, 2},
        {UINT32_C(0xfffffe), UINT32_C(0xffffff), 4, true, false, 1},
        {32, UINT32_C(0xffffff), 0, false, true, UINT32_C(0xabcdef)},
        {UINT32_C(0xfffffc), 0, 2, true, true, UINT32_C(0xabcdef)}
    };
    const rc_addr_t last_byte = UINT32_C(0xffffff);
    const rc_addr_t last_word = UINT32_C(0xfffffe);
    const size_t address_count = (size_t)UINT32_C(0x1000000);
    rc_machine_t machine;
    rc_word_t word = 0;
    rc_halfword_t halfword = 0;

    if (!rc_machine_init(&machine, address_count)) {
        CHECK(false);
        return;
    }
    CHECK(rc_machine_write_word(&machine, last_byte, UINT32_C(0xabcdef)));
    CHECK(rc_machine_read_word(&machine, last_word, &word));
    CHECK(word == UINT32_C(0xabcdef));
    CHECK(rc_machine_read_halfword(&machine, last_byte, &halfword));
    CHECK(halfword == UINT16_C(0xdef));
    CHECK(rc_machine_write_halfword(&machine, last_byte, UINT16_C(0xffff)));
    CHECK(rc_machine_read_word(&machine, last_byte, &word));
    CHECK(word == UINT32_C(0xabcfff));
    /* Host accessors reject invalid addresses; only guest arithmetic wraps. */
    machine.cpu.w[0] = UINT32_C(0x123456);
    word = UINT32_C(0x654321);
    halfword = UINT16_C(0x765);
    CHECK(!rc_machine_read_word(&machine, UINT32_C(0x1000000), &word));
    CHECK(!rc_machine_read_halfword(&machine, UINT32_MAX, &halfword));
    CHECK(!rc_machine_write_word(&machine, UINT32_MAX, 0));
    CHECK(!rc_machine_write_halfword(&machine, UINT32_C(0x1000000), 0));
    CHECK(word == UINT32_C(0x654321));
    CHECK(halfword == UINT16_C(0x765));
    CHECK(machine.cpu.w[0] == UINT32_C(0x123456));
    CHECK(rc_machine_read_word(&machine, last_word, &word));
    CHECK(word == UINT32_C(0xabcfff));

    machine.cpu.stopped = false;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        machine.cpu.instruction_counter = cases[i].ic;
        machine.cpu.w[1] = cases[i].index;
        if (cases[i].indirect) {
            /* Keep an indirect pointer distinct from the instruction. */
            CHECK(rc_machine_write_word(&machine, last_word, cases[i].expected));
        }
        put_instruction(&machine, cases[i].ic, 11, 2, cases[i].relative,
                        cases[i].indirect, 1, cases[i].displacement);
        CHECK(rc_machine_step(&machine) == RC_STEP_OK);
        CHECK(machine.last_effective_address == cases[i].expected);
        CHECK(machine.cpu.w[2] == cases[i].expected);
    }

    /* A final-word fetch advances IC to zero, where W0 is executable. */
    put_instruction(&machine, last_word, 11, 2, false, false, 0, 37);
    machine.cpu.w[0] = rc_instruction_encode(11, 2, false, false, 0, 19);
    machine.cpu.instruction_counter = last_word;
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[2] == 37);
    CHECK(machine.cpu.instruction_counter == 0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[2] == 19);
    CHECK(machine.cpu.instruction_counter == 2);

    /* A taken skip and an AM chain also cross the 24-bit boundary. */
    put_instruction(&machine, last_word - 2U, 42, 2, false, false, 0, 19);
    machine.cpu.instruction_counter = last_word - 2U;
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.instruction_counter == 0);
    put_instruction(&machine, last_word, 9, 0, false, false, 0, -1);
    machine.cpu.w[0] = rc_instruction_encode(11, 2, false, false, 0, 2);
    machine.cpu.instruction_counter = last_word;
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.last_instruction_address == 0);
    CHECK(machine.cpu.w[2] == 1);
    CHECK(machine.cpu.instruction_counter == 2);

    /* Odd operand addresses select the same final word; high addresses
       must not be treated as negative host indexes. */
    machine.cpu.w[1] = last_byte;
    machine.cpu.w[2] = UINT32_C(0xaabbcc);
    put_instruction(&machine, 32, 23, 2, false, false, 1, 0);
    machine.cpu.instruction_counter = 32;
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_word(&machine, last_word, &word));
    CHECK(word == UINT32_C(0xaabbcc));
    put_instruction(&machine, 34, 20, 3, false, false, 1, 0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[3] == UINT32_C(0xaabbcc));
    put_instruction(&machine, 36, 26, 2, false, false, 1, 0);
    machine.cpu.w[2] = UINT32_C(0x123456);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_word(&machine, last_word, &word));
    CHECK(word == UINT32_C(0xaab456));
    put_instruction(&machine, 38, 54, 3, false, false, 1, 0);
    CHECK(rc_machine_write_word(&machine, last_word - 2U, UINT32_C(0x112233)));
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[2] == UINT32_C(0x112233));
    CHECK(machine.cpu.w[3] == UINT32_C(0xaab456));
    put_instruction(&machine, 40, 55, 3, false, false, 1, 0);
    machine.cpu.w[2] = UINT32_C(0x445566);
    machine.cpu.w[3] = UINT32_C(0x778899);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_word(&machine, last_word - 2U, &word));
    CHECK(word == UINT32_C(0x445566));
    CHECK(rc_machine_read_word(&machine, last_word, &word));
    CHECK(word == UINT32_C(0x778899));
    put_instruction(&machine, 42, 13, 2, false, false, 1, 0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.instruction_counter == last_word);
    CHECK(machine.cpu.w[2] == 44);
    rc_machine_destroy(&machine);
}

static void test_missing_storage_boundary(void)
{
    rc_core_t core = {0};
    rc_machine_t machine;
    rc_word_t word = UINT32_C(0x123456);
    rc_halfword_t halfword = 0;

    /* An unpaired final halfword is valid as a byte, but not as a word. */
    CHECK(rc_core_init(&core, 127));
    CHECK(rc_core_write_halfword(&core, 126, UINT16_C(0xabc)));
    CHECK(!rc_core_write_word(&core, 126, 0));
    CHECK(!rc_core_read_word(&core, 127, &word));
    CHECK(word == UINT32_C(0x123456));
    CHECK(rc_core_read_halfword(&core, 126, &halfword));
    CHECK(halfword == UINT16_C(0xabc));
    rc_core_destroy(&core);

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    CHECK(rc_machine_write_word(&machine, 12, 64));
    machine.cpu.instruction_counter = 32;
    machine.cpu.w[1] = 128;
    machine.cpu.w[2] = UINT32_C(0xabcdef);
    put_instruction(&machine, 32, 20, 2, false, false, 1, 0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[2] == UINT32_C(0xabcdef));
    CHECK(machine.cpu.instruction_counter == 64);
    CHECK(machine.interrupts_serviced == 1);
    CHECK(rc_machine_read_word(&machine, 8, &word));
    CHECK(word == 0);
    CHECK(rc_machine_read_word(&machine, 10, &word));
    CHECK(word == 34);

    /* A failed fetch must not advance IC, including a missing second byte. */
    machine.cpu.instruction_counter = 128;
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_word(&machine, 10, &word));
    CHECK(word == 128);
    machine.cpu.instruction_counter = 127;
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_word(&machine, 10, &word));
    CHECK(word == 127);
    CHECK(machine.interrupts_serviced == 3);
    machine.cpu.instruction_counter = 128;
    machine.cpu.interrupt_mask = 0;
    CHECK(rc_machine_step(&machine) == RC_STEP_ADDRESS_EXCEPTION);
    CHECK(machine.cpu.instruction_counter == 128);
    CHECK((machine.cpu.interrupt_requests & UINT32_C(0x800000)) != 0);
    CHECK(machine.interrupts_serviced == 3);
    rc_machine_destroy(&machine);
}

static void test_config_core_limits(void)
{
    rc_config_t config;
    rc_config_error_t error;

    /* Preserve the delivered configuration policy separately from the
       unsigned 24-bit address calculations exercised above. */
    CHECK(rc_config_parse_text("CORESIZE=0,16777216\n", &config, &error) == 0);
    CHECK(config.core_min_halfwords == 16384);
    CHECK(config.core_max_halfwords == 8388607);
    CHECK(rc_config_parse_text("CORESIZE = 8388607\n", &config, &error) == 0);
    CHECK(config.core_min_halfwords == 8388607);
    CHECK(config.core_max_halfwords == 8388607);
    CHECK(rc_config_parse_text("CORESIZE=8388608\n", &config, &error) != 0);
    CHECK(error.line == 1);
    CHECK(rc_config_parse_text("CORESIZE 0,1048576\n", &config, &error) == 0);
    CHECK(config.core_min_halfwords == 16384);
    CHECK(config.core_max_halfwords == 1048576);
    CHECK(rc_config_parse_text("CORESIZE=\n", &config, &error) != 0);
    CHECK(rc_config_parse_text("CORESIZE==1048576\n", &config, &error) != 0);
}

static void test_machine_register_aliases(void)
{
    rc_machine_t machine;
    rc_halfword_t halfword = 0;
    rc_word_t word = 0;

    CHECK(rc_machine_init(&machine, 64));
    CHECK(rc_machine_write_word(&machine, 3, UINT32_C(0xabcdef)));
    CHECK(machine.cpu.w[1] == UINT32_C(0xabcdef));
    CHECK(rc_machine_read_halfword(&machine, 2, &halfword));
    CHECK(halfword == UINT16_C(0xabc));
    CHECK(rc_machine_write_halfword(&machine, 3, UINT16_C(0x123)));
    CHECK(machine.cpu.w[1] == UINT32_C(0xabc123));
    CHECK(rc_machine_read_word(&machine, 2, &word));
    CHECK(word == UINT32_C(0xabc123));
    rc_machine_destroy(&machine);
}

static void test_machine_transfers_and_exceptions(void)
{
    rc_machine_t machine;
    unsigned interrupt = 99;

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 16;
    CHECK(rc_core_write_word(&machine.core, 80, UINT32_C(0x654321)));
    put_instruction(&machine, 16, 20, 2, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[2] == UINT32_C(0x654321));

    put_instruction(&machine, 18, 23, 2, false, false, 0, 90);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_core_read_word(&machine.core, 90, &machine.last_effective_address));
    CHECK(machine.last_effective_address == UINT32_C(0x654321));

    machine.cpu.w[1] = UINT32_C(0x00800800);
    put_instruction(&machine, 20, 33, 0, false, false, 1, -2048);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == UINT32_C(0x00800000));
    CHECK(machine.cpu.exception_22 == 1);
    machine.cpu.interrupt_mask |= UINT32_C(0x00400000);
    machine.cpu.interrupts_enabled = true;
    CHECK(rc_cpu_next_interrupt(&machine.cpu, &interrupt));
    CHECK(interrupt == 2);
    rc_cpu_clear_interrupt(&machine.cpu, 1);
    machine.cpu.interrupts_enabled = false;

    machine.cpu.instruction_counter = 22;
    CHECK(rc_machine_write_word(&machine, 12, 24));
    put_instruction(&machine, 22, 58, 0, false, false, 0, 0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.instruction_counter == 24);
    CHECK((machine.cpu.interrupt_requests & UINT32_C(0x00800000)) == 0);
    rc_machine_destroy(&machine);
}

static void test_machine_arithmetic_and_flow(void)
{
    rc_machine_t machine;

    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.w[0] = UINT32_C(0x007fffff);
    CHECK(rc_core_write_word(&machine.core, 80, 1));
    put_instruction(&machine, 32, 7, 0, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == UINT32_C(0x00800000));
    CHECK(machine.cpu.exception_22 == 1);
    CHECK(machine.cpu.exception_23 == 0);

    machine.cpu.w[0] = RC_WORD_MASK;
    put_instruction(&machine, 34, 7, 0, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == 0);
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 1);

    machine.cpu.w[2] = 5;
    put_instruction(&machine, 36, 40, 2, false, false, 0, 4);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.instruction_counter == 40);

    put_instruction(&machine, 40, 13, 3, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[3] == 42);
    CHECK(machine.cpu.instruction_counter == 100);

    put_instruction(&machine, 100, 15, 1, false, false, 0, 122);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.interrupts_enabled);
    CHECK(machine.cpu.w[1] == 102);
    CHECK(machine.cpu.instruction_counter == 122);

    put_instruction(&machine, 122, 14, 0, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(!machine.cpu.interrupts_enabled);
    CHECK(machine.cpu.instruction_counter == 100);

    machine.cpu.w[1] = UINT32_C(0x00aa5500);
    CHECK(rc_core_write_word(&machine.core, 120, UINT32_C(0x0000ffff)));
    put_instruction(&machine, 100, 4, 1, false, false, 0, 120);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == UINT32_C(0x00005500));
    rc_machine_destroy(&machine);
}

static void test_machine_byte_arithmetic(void)
{
    rc_machine_t machine;

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;

    machine.cpu.w[0] = UINT32_C(0x007fffff);
    CHECK(rc_machine_write_halfword(&machine, 81, 1));
    put_instruction(&machine, 32, 18, 0, false, false, 0, 81);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == UINT32_C(0x00800000));
    CHECK(machine.cpu.exception_22 == 1);
    CHECK(machine.cpu.exception_23 == 0);

    machine.cpu.w[1] = 0;
    CHECK(rc_machine_write_halfword(&machine, 83, UINT16_C(0x0fff)));
    put_instruction(&machine, 34, 17, 1, false, false, 0, 83);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == 1);
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 0);
    rc_machine_destroy(&machine);
}

static void test_machine_double_transfers(void)
{
    rc_machine_t machine;
    rc_word_t word = 0;

    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    CHECK(rc_core_write_word(&machine.core, 78, UINT32_C(0x112233)));
    CHECK(rc_core_write_word(&machine.core, 80, UINT32_C(0x445566)));
    put_instruction(&machine, 32, 54, 2, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == UINT32_C(0x112233));
    CHECK(machine.cpu.w[2] == UINT32_C(0x445566));

    machine.cpu.w[2] = UINT32_C(0xaabbcc);
    machine.cpu.w[3] = UINT32_C(0xddeeff);
    put_instruction(&machine, 34, 55, 3, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_core_read_word(&machine.core, 98, &word));
    CHECK(word == UINT32_C(0xaabbcc));
    CHECK(rc_core_read_word(&machine.core, 100, &word));
    CHECK(word == UINT32_C(0xddeeff));

    machine.cpu.w[0] = UINT32_C(0x010203);
    machine.cpu.w[3] = UINT32_C(0xabcdef);
    machine.cpu.instruction_counter = 36;
    put_instruction(&machine, 36, 54, 0, false, false, 0, 0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == UINT32_C(0x010203));
    CHECK(machine.cpu.w[3] == UINT32_C(0xabcdef));
    rc_machine_destroy(&machine);
}

static void test_machine_multiply_and_divide(void)
{
    rc_machine_t machine;

    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.w[2] = rc_word((rc_word_t)-7);
    CHECK(rc_machine_write_word(&machine, 80, 9));
    put_instruction(&machine, 32, 10, 2, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == RC_WORD_MASK);
    CHECK(machine.cpu.w[2] == rc_word((rc_word_t)-63));

    machine.cpu.w[1] = 0;
    machine.cpu.w[2] = 100;
    CHECK(rc_machine_write_word(&machine, 82, 7));
    put_instruction(&machine, 34, 24, 2, false, false, 0, 82);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == 2);
    CHECK(machine.cpu.w[2] == 14);
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 0);

    machine.cpu.w[1] = UINT32_C(0x123456);
    machine.cpu.w[2] = UINT32_C(0x654321);
    CHECK(rc_machine_write_word(&machine, 84, 0));
    put_instruction(&machine, 36, 24, 2, false, false, 0, 84);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == UINT32_C(0x123456));
    CHECK(machine.cpu.w[2] == UINT32_C(0x654321));
    CHECK(machine.cpu.exception_22 == 1);
    rc_machine_destroy(&machine);
}

static void test_machine_shifts(void)
{
    rc_machine_t machine;

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.w[0] = UINT32_C(0x00800000);
    put_instruction(&machine, 32, 36, 0, false, false, 0, -23);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == RC_WORD_MASK);

    machine.cpu.w[0] = 1;
    put_instruction(&machine, 34, 36, 0, false, false, 0, 23);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == UINT32_C(0x00800000));
    CHECK(machine.cpu.exception_22 == 1);

    machine.cpu.w[1] = UINT32_C(0x123456);
    put_instruction(&machine, 36, 38, 1, false, false, 0, -4);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == UINT32_C(0x012345));

    machine.cpu.w[1] = 0;
    machine.cpu.w[2] = 1;
    put_instruction(&machine, 38, 39, 2, false, false, 0, 24);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == 1);
    CHECK(machine.cpu.w[2] == 0);

    machine.cpu.w[1] = RC_WORD_MASK;
    machine.cpu.w[2] = 0;
    put_instruction(&machine, 40, 37, 2, false, false, 0, -47);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == RC_WORD_MASK);
    CHECK(machine.cpu.w[2] == RC_WORD_MASK);
    rc_machine_destroy(&machine);
}

static void test_machine_normalization(void)
{
    rc_machine_t machine;
    rc_halfword_t halfword = 0;

    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.w[0] = UINT32_C(0x00100000);
    put_instruction(&machine, 32, 34, 0, false, false, 0, 81);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == UINT32_C(0x00400000));
    CHECK(rc_machine_read_halfword(&machine, 81, &halfword));
    CHECK(halfword == UINT16_C(0x0ffe));

    machine.cpu.w[1] = 0;
    put_instruction(&machine, 34, 34, 1, false, false, 0, 83);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == 0);
    CHECK(rc_machine_read_halfword(&machine, 83, &halfword));
    CHECK(halfword == UINT16_C(0x0800));

    machine.cpu.w[1] = 0;
    machine.cpu.w[2] = 1;
    put_instruction(&machine, 36, 35, 2, false, false, 0, 85);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == UINT32_C(0x00400000));
    CHECK(machine.cpu.w[2] == 0);
    CHECK(rc_machine_read_halfword(&machine, 85, &halfword));
    CHECK(halfword == UINT16_C(0x0fd2));
    rc_machine_destroy(&machine);
}

static void test_db_sx_and_nd_regressions(void)
{
    rc_machine_t machine;
    rc_halfword_t halfword;
    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    /* V_1.2_DB repairs the assembly SX bit extraction. The native three-bit
       representation already matches it; exhaust all mask/state pairs. */
    for (unsigned exceptions = 0; exceptions < 8; ++exceptions) {
        for (unsigned mask = 0; mask < 8; ++mask) {
            machine.cpu.instruction_counter = 32;
            machine.cpu.exception_23 = (uint8_t)(exceptions & 1U);
            machine.cpu.exception_22 = (uint8_t)((exceptions >> 1) & 1U);
            machine.cpu.exception_21 = (uint8_t)((exceptions >> 2) & 1U);
            machine.cpu.w[2] = UINT32_C(0x123456);
            put_instruction(&machine, 32, 46, 2, false, false, 0, (int32_t)mask);
            CHECK(rc_machine_step(&machine) == RC_STEP_OK);
            CHECK(machine.cpu.instruction_counter == ((exceptions & mask) == 0 ? 36U : 34U));
            CHECK(machine.cpu.w[2] == UINT32_C(0x123456));
            CHECK(machine.cpu.exception_23 == (exceptions & 1U));
            CHECK(machine.cpu.exception_22 == ((exceptions >> 1) & 1U));
            CHECK(machine.cpu.exception_21 == ((exceptions >> 2) & 1U));
        }
    }
    /* DB shares the ND/NS count store. Exercise both halves, signed and zero
       inputs, while ensuring the neighbouring halfword and exceptions survive. */
    const struct { rc_word_t high, low, result_high, result_low; rc_halfword_t count; } cases[] = {
        {0, 0, 0, 0, 0x800}, {0, 1, 0x400000, 0, 0xfd2},
        {0xffffff, 0xffffff, 0x800000, 0, 0xfd1},
        {0x400000, 0, 0x400000, 0, 0}, {0x800000, 0, 0x800000, 0, 0}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        for (rc_addr_t address = 80; address <= 81; ++address) {
            machine.cpu.instruction_counter = 32;
            machine.cpu.w[1] = cases[i].high; machine.cpu.w[2] = cases[i].low;
            CHECK(rc_machine_write_word(&machine, 80, UINT32_C(0xabcabc)));
            put_instruction(&machine, 32, 35, 2, false, false, 0, (int32_t)address);
            CHECK(rc_machine_step(&machine) == RC_STEP_OK);
            CHECK(machine.cpu.w[1] == cases[i].result_high && machine.cpu.w[2] == cases[i].result_low);
            CHECK(rc_machine_read_halfword(&machine, address, &halfword) && halfword == cases[i].count);
            CHECK(rc_machine_read_halfword(&machine, address ^ 1U, &halfword) && halfword == 0xabc);
            CHECK(machine.cpu.exception_21 == 1 && machine.cpu.exception_22 == 1 && machine.cpu.exception_23 == 1);
        }
    }
    rc_machine_destroy(&machine);
}

static void test_db_indirect_address_faults(void)
{
    rc_machine_t machine;
    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.interrupt_mask = 0;
    /* Both an absent indirect-pointer word and an absent final M operand
       must fault without indexing outside native storage. Include a high
       unsigned address, the case fixed by DB's unsigned address comparisons. */
    const struct { uint8_t opcode; bool indirect; int32_t displacement; rc_word_t index, pointer; } cases[] = {
        {11, true, 0, 128, 0}, {11, true, 0, 0xffffff, 0},
        {20, true, 80, 0, 128}, {20, true, 80, 0, 0xffffff},
        {20, false, 0, 0xffffff, 0}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        machine.cpu.instruction_counter = 32;
        machine.cpu.interrupt_requests = 0;
        machine.cpu.w[1] = cases[i].index;
        machine.cpu.w[2] = UINT32_C(0xabcdef);
        CHECK(rc_machine_write_word(&machine, 80, cases[i].pointer));
        put_instruction(&machine, 32, cases[i].opcode, 2, false,
                        cases[i].indirect, 1, cases[i].displacement);
        CHECK(rc_machine_step(&machine) == RC_STEP_ADDRESS_EXCEPTION);
        CHECK(machine.cpu.w[2] == UINT32_C(0xabcdef));
        CHECK((machine.cpu.interrupt_requests & UINT32_C(0x800000)) != 0);
    }
    rc_machine_destroy(&machine);
}

static void test_protection_compatibility(void)
{
    rc_machine_t machine;
    rc_halfword_t halfword = 0;

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.w[1] = RC_WORD_MASK;
    put_instruction(&machine, 32, 22, 1, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == 0);

    put_instruction(&machine, 34, 51, 0, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.instruction_counter == 36);
    CHECK(rc_machine_write_halfword(&machine, 81, UINT16_C(0x0fff)));
    put_instruction(&machine, 36, 29, 0, false, false, 0, 81);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_halfword(&machine, 81, &halfword));
    CHECK(halfword == UINT16_C(0x0080));

    put_instruction(&machine, 38, 21, 0, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.instruction_counter == 42);
    rc_machine_destroy(&machine);
}

static void test_interrupt_entry(void)
{
    rc_machine_t machine;
    rc_word_t word = 0;

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.interrupts_enabled = true;
    machine.cpu.interrupt_mask |= UINT32_C(0x00100000);
    CHECK(rc_machine_write_word(&machine, 12, 60));
    put_instruction(&machine, 60, 11, 2, false, false, 0, 77);
    rc_cpu_request_interrupt(&machine.cpu, 3);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[2] == 77);
    CHECK(machine.cpu.instruction_counter == 62);
    CHECK(!machine.cpu.interrupts_enabled);
    CHECK(machine.cpu.monitor_mode);
    CHECK(machine.interrupts_serviced == 1);
    CHECK(rc_machine_read_word(&machine, 8, &word));
    CHECK(word == 6);
    CHECK(rc_machine_read_word(&machine, 10, &word));
    CHECK(word == 32);

    machine.cpu.instruction_counter = 34;
    CHECK(rc_machine_write_word(&machine, 12, 64));
    put_instruction(&machine, 34, 20, 0, false, false, 0, 200);
    put_instruction(&machine, 64, 11, 0, false, false, 0, 1);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.instruction_counter == 64);
    CHECK(machine.interrupts_serviced == 2);
    CHECK(rc_machine_read_word(&machine, 8, &word));
    CHECK(word == 0);
    CHECK(rc_machine_read_word(&machine, 10, &word));
    CHECK(word == 36);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == 1);
    rc_machine_destroy(&machine);
}

static void test_interrupt_clear_instruction(void)
{
    rc_machine_t machine;

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.interrupt_requests = UINT32_C(0x0070f0f0);
    put_instruction(&machine, 32, 47, 0, false, false, 0, 0x0f0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.interrupt_requests == UINT32_C(0x0070f000));
    rc_machine_destroy(&machine);
}

static void test_interrupt_mask_load_instruction(void)
{
    rc_machine_t machine;

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    CHECK(rc_machine_write_word(&machine, 80, UINT32_C(0x00012345)));
    put_instruction(&machine, 32, 12, 0, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.interrupt_mask == UINT32_C(0x00812345));
    rc_machine_destroy(&machine);
}

static void test_exception_register_transfers(void)
{
    rc_machine_t machine;
    rc_halfword_t halfword = 0;

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    CHECK(rc_machine_write_halfword(&machine, 81, UINT16_C(0x0ffd)));
    put_instruction(&machine, 32, 16, 0, false, false, 0, 81);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.exception_21 == 1);
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 1);

    put_instruction(&machine, 34, 27, 0, false, false, 0, 83);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_halfword(&machine, 83, &halfword));
    CHECK(halfword == 5);
    rc_machine_destroy(&machine);
}

static void test_interrupt_register_stores(void)
{
    rc_machine_t machine;
    rc_word_t word = 0;

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.interrupt_requests = UINT32_C(0x00123456);
    machine.cpu.interrupt_mask = UINT32_C(0x00812345);
    put_instruction(&machine, 32, 31, 0, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_word(&machine, 80, &word));
    CHECK(word == UINT32_C(0x00123456));
    put_instruction(&machine, 34, 30, 0, false, false, 0, 82);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_word(&machine, 82, &word));
    CHECK(word == UINT32_C(0x00812345));
    rc_machine_destroy(&machine);
}

static void test_machine_double_arithmetic(void)
{
    rc_machine_t machine;

    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.w[1] = 1;
    machine.cpu.w[2] = RC_WORD_MASK;
    CHECK(rc_machine_write_word(&machine, 78, 2));
    CHECK(rc_machine_write_word(&machine, 80, 1));
    put_instruction(&machine, 32, 56, 2, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == 4);
    CHECK(machine.cpu.w[2] == 0);
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 0);

    put_instruction(&machine, 34, 57, 2, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == 1);
    CHECK(machine.cpu.w[2] == RC_WORD_MASK);
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 1);

    machine.cpu.w[1] = UINT32_C(0x007fffff);
    machine.cpu.w[2] = RC_WORD_MASK;
    CHECK(rc_machine_write_word(&machine, 78, 0));
    CHECK(rc_machine_write_word(&machine, 80, 1));
    put_instruction(&machine, 36, 56, 2, false, false, 0, 80);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == UINT32_C(0x00800000));
    CHECK(machine.cpu.w[2] == 0);
    CHECK(machine.cpu.exception_22 == 1);
    CHECK(machine.cpu.exception_23 == 0);
    rc_machine_destroy(&machine);
}

static void test_floating_multiply_helpers(void)
{
    const int64_t half = INT64_C(1) << 34;
    const int64_t three_quarters = INT64_C(3) << 33;
    rc_float_t left;
    rc_float_t right;
    rc_float_t result;
    rc_float_t unpacked;
    rc_word_t first;
    rc_word_t second;

    left = (rc_float_t){half, 0};
    right = (rc_float_t){half, 0};
    CHECK(!rc_float_multiply(left, right, &result));
    CHECK(result.fraction == half);
    CHECK(result.exponent == -1);

    left = (rc_float_t){-half, 0};
    CHECK(!rc_float_multiply(left, right, &result));
    CHECK(result.fraction == -(INT64_C(1) << 35));
    CHECK(result.exponent == -2);

    left = (rc_float_t){-(INT64_C(1) << 35), 0};
    right = left;
    CHECK(!rc_float_multiply(left, right, &result));
    CHECK(result.fraction == half);
    CHECK(result.exponent == 1);

    left = (rc_float_t){half + 1, 0};
    right = (rc_float_t){three_quarters, 0};
    CHECK(!rc_float_multiply(left, right, &result));
    CHECK(result.fraction == three_quarters + 2);
    CHECK(result.exponent == -1);

    left = (rc_float_t){-(half + 1), 0};
    CHECK(!rc_float_multiply(left, right, &result));
    CHECK(result.fraction == -three_quarters - 1);
    CHECK(result.exponent == -1);

    left = (rc_float_t){0, 123};
    right = (rc_float_t){three_quarters, -17};
    CHECK(!rc_float_multiply(left, right, &result));
    CHECK(result.fraction == 0);
    CHECK(result.exponent == -2048);

    left = (rc_float_t){three_quarters, 2047};
    right = (rc_float_t){three_quarters, 2047};
    CHECK(rc_float_multiply(left, right, &result));
    CHECK(result.fraction == (INT64_C(9) << 31));
    CHECK(result.exponent == 4094);
    rc_float_pack(result, false, &first, &second);
    unpacked = rc_float_unpack(first, second);
    CHECK(unpacked.fraction == result.fraction);
    CHECK(unpacked.exponent == -2);

    left = (rc_float_t){half, -2048};
    right = left;
    CHECK(rc_float_multiply(left, right, &result));
    CHECK(result.fraction == half);
    CHECK(result.exponent == -4097);
    rc_float_pack(result, false, &first, &second);
    unpacked = rc_float_unpack(first, second);
    CHECK(unpacked.fraction == half);
    CHECK(unpacked.exponent == -1);

    result = (rc_float_t){half + 4, -1};
    rc_float_pack(result, true, &first, &second);
    unpacked = rc_float_unpack(first, second);
    CHECK(unpacked.fraction == half + 7);
    CHECK(unpacked.exponent == -1);
}

static void test_floating_divide_helpers(void)
{
    const int64_t half = INT64_C(1) << 34;
    const int64_t three_quarters = INT64_C(3) << 33;
    rc_float_t left;
    rc_float_t right;
    rc_float_t result;
    bool division_by_zero;

    left = (rc_float_t){half, 0};
    right = (rc_float_t){half, 0};
    CHECK(!rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(!division_by_zero);
    CHECK(result.fraction == half);
    CHECK(result.exponent == 1);

    left = (rc_float_t){three_quarters, 0};
    CHECK(!rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(result.fraction == three_quarters);
    CHECK(result.exponent == 1);

    left = (rc_float_t){-(INT64_C(1) << 35), 0};
    CHECK(!rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(result.fraction == -(INT64_C(1) << 35));
    CHECK(result.exponent == 1);

    left = (rc_float_t){half, 0};
    right = (rc_float_t){three_quarters, 0};
    CHECK(!rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(result.fraction == (INT64_C(1) << 36) / 3);
    CHECK(result.exponent == 0);

    /* Non-normalized divisors must not overflow the host guard quotient. */
    right = (rc_float_t){1, 0};
    left = (rc_float_t){2, 0};
    CHECK(!rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(!division_by_zero);
    CHECK(result.fraction == half);
    CHECK(result.exponent == 2);

    left = (rc_float_t){half, 0};
    CHECK(!rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(result.fraction == half);
    CHECK(result.exponent == 35);

    left = (rc_float_t){-(INT64_C(1) << 35), 0};
    CHECK(!rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(result.fraction == -(INT64_C(1) << 35));
    CHECK(result.exponent == 35);

    left = (rc_float_t){half, 2047};
    CHECK(rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(!division_by_zero);
    CHECK(result.fraction == half);
    CHECK(result.exponent == 2082);

    left = (rc_float_t){0, -2048};
    CHECK(!rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(result.fraction == 0);
    CHECK(result.exponent == -2048);

    left = (rc_float_t){half, 17};
    right = (rc_float_t){0, -2048};
    CHECK(rc_float_divide(left, right, &result, &division_by_zero));
    CHECK(division_by_zero);
    CHECK(result.fraction == left.fraction);
    CHECK(result.exponent == left.exponent);
}

static void test_floating_add_helpers(void)
{
    const int64_t half = INT64_C(1) << 34;
    const int64_t three_quarters = INT64_C(3) << 33;
    rc_float_t left;
    rc_float_t right;
    rc_float_t result;
    rc_word_t first;
    rc_word_t second;

    left = (rc_float_t){half, 0};
    right = (rc_float_t){half, -1};
    CHECK(!rc_float_add(left, right, false, &result));
    CHECK(result.fraction == three_quarters);
    CHECK(result.exponent == 0);

    right = (rc_float_t){-(INT64_C(1) << 35), -1};
    CHECK(!rc_float_add(left, right, false, &result));
    CHECK(result.fraction == 0);
    CHECK(result.exponent == -2048);

    right = (rc_float_t){three_quarters, 0};
    CHECK(!rc_float_add(left, right, true, &result));
    CHECK(result.fraction == -(INT64_C(1) << 35));
    CHECK(result.exponent == -2);

    right = (rc_float_t){half, -35};
    CHECK(!rc_float_add(left, right, false, &result));
    CHECK(result.fraction == half + 1);
    CHECK(result.exponent == 0);

    right = (rc_float_t){three_quarters, -38};
    CHECK(!rc_float_add(left, right, false, &result));
    CHECK(result.fraction == left.fraction);
    CHECK(result.exponent == left.exponent);

    left = (rc_float_t){three_quarters, 2047};
    right = left;
    CHECK(rc_float_add(left, right, false, &result));
    CHECK(result.fraction == three_quarters);
    CHECK(result.exponent == 2048);

    left = (rc_float_t){half + 1, -2048};
    right = (rc_float_t){half, -2048};
    CHECK(rc_float_add(left, right, true, &result));
    CHECK(result.fraction == half);
    CHECK(result.exponent == -2082);

    left = rc_float_unpack(UINT32_C(0x40bc31), UINT32_C(0x9b0002));
    right = rc_float_unpack(UINT32_C(0x4ccccc), UINT32_C(0xccdffe));
    CHECK(!rc_float_add(left, right, true, &result));
    rc_float_pack(result, false, &first, &second);
    CHECK(first == UINT32_C(0x77dec9));
    CHECK(second == UINT32_C(0x9c6001));
}

static void test_floating_conversion_helpers(void)
{
    const int64_t half = INT64_C(1) << 34;
    const int64_t three_quarters = INT64_C(3) << 33;
    rc_float_t value;
    int32_t integer = 99;

    CHECK(!rc_float_from_integer(0, 17, &value));
    CHECK(value.fraction == 0);
    CHECK(value.exponent == -2048);
    CHECK(!rc_float_from_integer(1, 0, &value));
    CHECK(value.fraction == half);
    CHECK(value.exponent == 1);
    CHECK(!rc_float_from_integer(-1, 0, &value));
    CHECK(value.fraction == -(INT64_C(1) << 35));
    CHECK(value.exponent == 0);
    CHECK(!rc_float_from_integer(8388607, 0, &value));
    CHECK(value.fraction == (INT64_C(8388607) << 12));
    CHECK(value.exponent == 23);
    CHECK(!rc_float_from_integer(-8388608, 0, &value));
    CHECK(value.fraction == -(INT64_C(1) << 35));
    CHECK(value.exponent == 23);
    CHECK(rc_float_from_integer(1, 2047, &value));
    CHECK(value.fraction == half);
    CHECK(value.exponent == 2048);

    CHECK(!rc_float_to_integer((rc_float_t){half, 1}, 0, &integer));
    CHECK(integer == 1);
    CHECK(!rc_float_to_integer((rc_float_t){half, 0}, 0, &integer));
    CHECK(integer == 1);
    CHECK(!rc_float_to_integer(
        (rc_float_t){-(INT64_C(1) << 35), -1}, 0, &integer));
    CHECK(integer == 0);
    CHECK(!rc_float_to_integer((rc_float_t){three_quarters, 1}, 0,
                               &integer));
    CHECK(integer == 2);
    CHECK(!rc_float_to_integer((rc_float_t){-three_quarters, 1}, 0,
                               &integer));
    CHECK(integer == -1);
    CHECK(!rc_float_to_integer((rc_float_t){half, 0}, 1, &integer));
    CHECK(integer == 1);
    integer = 1234;
    CHECK(rc_float_to_integer((rc_float_t){half, 25}, 0, &integer));
    CHECK(integer == 1234);
}

static void test_floating_multiply_instruction(void)
{
    const int64_t half = INT64_C(1) << 34;
    const int64_t three_quarters = INT64_C(3) << 33;
    rc_machine_t machine;
    rc_float_t value;
    rc_word_t first;
    rc_word_t second;

    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    rc_float_pack((rc_float_t){three_quarters, 0}, false,
                  &machine.cpu.w[0], &machine.cpu.w[1]);
    rc_float_pack((rc_float_t){half, 0}, false, &first, &second);
    CHECK(rc_machine_write_word(&machine, 98, first));
    CHECK(rc_machine_write_word(&machine, 100, second));
    put_instruction(&machine, 32, 50, 1, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    value = rc_float_unpack(machine.cpu.w[0], machine.cpu.w[1]);
    CHECK(value.fraction == three_quarters);
    CHECK(value.exponent == -1);
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 0);

    rc_float_pack((rc_float_t){three_quarters, 2047}, false,
                  &machine.cpu.w[0], &machine.cpu.w[1]);
    rc_float_pack((rc_float_t){three_quarters, 2047}, false,
                  &first, &second);
    CHECK(rc_machine_write_word(&machine, 98, first));
    CHECK(rc_machine_write_word(&machine, 100, second));
    put_instruction(&machine, 34, 50, 1, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    value = rc_float_unpack(machine.cpu.w[0], machine.cpu.w[1]);
    CHECK(value.fraction == (INT64_C(9) << 31));
    CHECK(value.exponent == -2);
    CHECK(machine.cpu.exception_22 == 1);
    CHECK(machine.cpu.exception_23 == 0);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 21));
    rc_machine_destroy(&machine);
}

static void test_floating_divide_instruction(void)
{
    const int64_t half = INT64_C(1) << 34;
    const int64_t three_quarters = INT64_C(3) << 33;
    rc_machine_t machine;
    rc_float_t value;
    rc_word_t saved_first;
    rc_word_t saved_second;
    rc_word_t first;
    rc_word_t second;

    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    rc_float_pack((rc_float_t){three_quarters, 0}, false,
                  &machine.cpu.w[0], &machine.cpu.w[1]);
    rc_float_pack((rc_float_t){half, 0}, false, &first, &second);
    CHECK(rc_machine_write_word(&machine, 98, first));
    CHECK(rc_machine_write_word(&machine, 100, second));
    put_instruction(&machine, 32, 52, 1, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    value = rc_float_unpack(machine.cpu.w[0], machine.cpu.w[1]);
    CHECK(value.fraction == three_quarters);
    CHECK(value.exponent == 1);
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 0);

    saved_first = machine.cpu.w[0];
    saved_second = machine.cpu.w[1];
    rc_float_pack((rc_float_t){0, -2048}, false, &first, &second);
    CHECK(rc_machine_write_word(&machine, 98, first));
    CHECK(rc_machine_write_word(&machine, 100, second));
    put_instruction(&machine, 34, 52, 1, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == saved_first);
    CHECK(machine.cpu.w[1] == saved_second);
    CHECK(machine.cpu.exception_22 == 1);
    CHECK(machine.cpu.exception_23 == 0);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 21));
    rc_machine_destroy(&machine);
}

static void test_floating_add_and_conversion_instructions(void)
{
    const int64_t half = INT64_C(1) << 34;
    const int64_t three_quarters = INT64_C(3) << 33;
    rc_machine_t machine;
    rc_float_t value;
    rc_word_t first;
    rc_word_t second;
    rc_word_t previous;

    CHECK(rc_machine_init(&machine, 256));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;

    machine.cpu.w[1] = 3;
    put_instruction(&machine, 32, 32, 1, false, false, 0, 0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    value = rc_float_unpack(machine.cpu.w[0], machine.cpu.w[1]);
    CHECK(value.fraction == three_quarters);
    CHECK(value.exponent == 2);

    previous = machine.cpu.w[0];
    put_instruction(&machine, 34, 53, 1, false, false, 0, 0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == previous);
    CHECK(machine.cpu.w[1] == 3);

    rc_float_pack((rc_float_t){half, 0}, false, &first, &second);
    CHECK(rc_machine_write_word(&machine, 98, first));
    CHECK(rc_machine_write_word(&machine, 100, second));
    rc_float_pack((rc_float_t){half, 0}, false,
                  &machine.cpu.w[0], &machine.cpu.w[1]);
    put_instruction(&machine, 36, 48, 1, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    value = rc_float_unpack(machine.cpu.w[0], machine.cpu.w[1]);
    CHECK(value.fraction == half);
    CHECK(value.exponent == 1);

    put_instruction(&machine, 38, 49, 1, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    value = rc_float_unpack(machine.cpu.w[0], machine.cpu.w[1]);
    CHECK(value.fraction == half);
    CHECK(value.exponent == 0);

    machine.cpu.exception_21 = 1;
    rc_float_pack((rc_float_t){half + 4, 0}, false,
                  &machine.cpu.w[0], &machine.cpu.w[1]);
    rc_float_pack((rc_float_t){half, -38}, false, &first, &second);
    CHECK(rc_machine_write_word(&machine, 98, first));
    CHECK(rc_machine_write_word(&machine, 100, second));
    put_instruction(&machine, 40, 48, 1, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    value = rc_float_unpack(machine.cpu.w[0], machine.cpu.w[1]);
    CHECK(value.fraction == half + 7);
    CHECK(value.exponent == 0);
    machine.cpu.exception_21 = 0;

    rc_float_pack((rc_float_t){half, 0}, false,
                  &machine.cpu.w[3], &machine.cpu.w[0]);
    rc_float_pack((rc_float_t){half, 0}, false, &first, &second);
    CHECK(rc_machine_write_word(&machine, 98, first));
    CHECK(rc_machine_write_word(&machine, 100, second));
    put_instruction(&machine, 42, 48, 0, false, false, 0, 100);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    value = rc_float_unpack(machine.cpu.w[3], machine.cpu.w[0]);
    CHECK(value.fraction == half);
    CHECK(value.exponent == 1);

    machine.cpu.w[1] = 1;
    put_instruction(&machine, 44, 32, 1, false, false, 0, 2047);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.exception_22 == 1);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 21));

    machine.cpu.interrupt_requests = 0;
    rc_float_pack((rc_float_t){half, 25}, false,
                  &machine.cpu.w[0], &machine.cpu.w[1]);
    previous = machine.cpu.w[1];
    put_instruction(&machine, 46, 53, 1, false, false, 0, 0);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[1] == previous);
    CHECK(machine.cpu.exception_22 == 1);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 22));
    rc_machine_destroy(&machine);
}

static void test_device_boundary(void)
{
    rc_machine_t machine;
    rc_clock_device_t clock;

    CHECK(rc_machine_init(&machine, 128));
    rc_clock_device_init(&clock, 11);
    rc_clock_device_set_ticks(&clock, 65);
    CHECK(rc_machine_attach_device(&machine, 3, &clock.device));
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 16;
    machine.cpu.w[2] = UINT32_C(0xabcdef);
    put_instruction(&machine, 16, 1, 2, false, false, 0, 3 * 64);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[2] == UINT32_C(0x000100));
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 0);
    CHECK(machine.io_operations == 1);
    CHECK(machine.last_io_device == 3);
    CHECK(machine.last_io_command == RC_IO_SENSE);
    CHECK(machine.last_io_modifier == 0);

    machine.cpu.w[2] = UINT32_C(0x123456);
    put_instruction(&machine, 18, 1, 2, false, false, 0, 4 * 64);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[2] == UINT32_C(0x123456));
    CHECK(machine.cpu.exception_22 == 1);
    CHECK(machine.cpu.exception_23 == 0);
    CHECK(machine.io_operations == 2);
    CHECK(machine.last_io_device == 4);
    rc_machine_destroy(&machine);
}

static uint8_t odd_parity_character(uint8_t value)
{
    uint8_t bits = value;
    unsigned ones = 0;

    while (bits != 0) {
        ones += bits & 1U;
        bits >>= 1;
    }
    return (ones & 1U) == 0 ? (uint8_t)(value | UINT8_C(0x80)) : value;
}

static void test_reader_device(void)
{
    char path[] = "/tmp/sim4000-reader-XXXXXX";
    unsigned char input[] = {
        0, odd_parity_character(1), 3
    };
    unsigned char bootstrap[] = {
        odd_parity_character(0), odd_parity_character(0),
        odd_parity_character(0), odd_parity_character(2)
    };
    rc_reader_device_t reader;
    rc_machine_t machine;
    rc_word_t word = 0;
    char error[256];
    int descriptor = mkstemp(path);
    FILE *file;

    CHECK(descriptor >= 0);
    if (descriptor < 0) {
        return;
    }
    file = fdopen(descriptor, "wb");
    CHECK(file != NULL);
    if (file == NULL) {
        (void)close(descriptor);
        (void)unlink(path);
        return;
    }
    CHECK(fwrite(input, 1, sizeof(input), file) == sizeof(input));
    CHECK(fclose(file) == 0);

    rc_reader_device_init(&reader, 9);
    CHECK(reader.device.io(reader.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == (UINT32_C(1) << 18));
    CHECK(rc_reader_device_mount(&reader, path, error, sizeof(error)) == 0);
    CHECK(reader.tape_present);
    CHECK(reader.device.poll(reader.device.context) ==
          (UINT32_C(1) << (23U - 9U)));
    CHECK(reader.device.io(reader.device.context, RC_IO_READ, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(reader.device.io(reader.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == 1);
    CHECK(reader.null_bytes_skipped == 1);
    CHECK(reader.device.io(reader.device.context, RC_IO_READ, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(reader.device.io(reader.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == ((UINT32_C(1) << 22) | 3U));
    CHECK(reader.parity_errors == 1);
    CHECK(rc_reader_device_rewind(&reader, error, sizeof(error)) == 0);
    CHECK(reader.device.io(reader.device.context, RC_IO_READ, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(reader.device.io(reader.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == 1);
    CHECK(rc_reader_device_rewind(&reader, error, sizeof(error)) == 0);
    CHECK(reader.device.io(reader.device.context, RC_IO_READ, 1, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(reader.device.io(reader.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == ((UINT32_C(1) << 22) | 1U));
    CHECK(reader.device.io(reader.device.context, RC_IO_READ, 2, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(reader.device.io(reader.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == 3);
    CHECK(reader.device.io(reader.device.context, RC_IO_READ, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(reader.device.io(reader.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == (UINT32_C(1) << 18));
    CHECK(!reader.tape_present);
    CHECK(rc_reader_device_rewind(&reader, error, sizeof(error)) != 0);

    file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fwrite(bootstrap, 1, sizeof(bootstrap), file) ==
              sizeof(bootstrap));
        CHECK(fclose(file) == 0);
    }
    CHECK(rc_reader_device_mount(&reader, path, error, sizeof(error)) == 0);
    CHECK(rc_machine_init(&machine, 128));
    CHECK(rc_machine_attach_device(&machine, 0, &reader.device));
    CHECK(rc_machine_control_autoload(&machine) == RC_CONTROL_OK);
    CHECK(machine.cpu.w[0] == 2);
    CHECK(machine.cpu.instruction_counter == 0);
    CHECK(!machine.cpu.reset_mode);
    rc_machine_destroy(&machine);
    rc_reader_device_destroy(&reader);
    CHECK(unlink(path) == 0);
}

static void test_punch_device(void)
{
    char path[] = "/tmp/sim4000-punch-XXXXXX";
    const unsigned char expected[] = {0x80, 0x81, 0xa5};
    unsigned char output[sizeof(expected)] = {0};
    rc_punch_device_t punch;
    rc_word_t word = 0;
    char error[256];
    int descriptor = mkstemp(path);
    FILE *file;

    CHECK(descriptor >= 0);
    if (descriptor < 0) {
        return;
    }
    CHECK(close(descriptor) == 0);
    CHECK(unlink(path) == 0);

    rc_punch_device_init(&punch, 12);
    CHECK(punch.device.io(punch.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == (UINT32_C(1) << 21));
    CHECK(rc_punch_device_mount(&punch, path, false,
                                error, sizeof(error)) == 0);
    word = 0;
    CHECK(punch.device.io(punch.device.context, RC_IO_WRITE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    word = 1;
    CHECK(punch.device.io(punch.device.context, RC_IO_WRITE, 1, &word) ==
          RC_DEVICE_ACCEPTED);
    word = UINT32_C(0xa5);
    CHECK(punch.device.io(punch.device.context, RC_IO_WRITE, 2, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(punch.device.poll(punch.device.context) ==
          (UINT32_C(1) << (23U - 12U)));
    CHECK(rc_punch_device_flush(&punch, error, sizeof(error)) == 0);
    rc_punch_device_unmount(&punch);

    file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fread(output, 1, sizeof(output), file) == sizeof(output));
        CHECK(fgetc(file) == EOF);
        CHECK(fclose(file) == 0);
        CHECK(memcmp(output, expected, sizeof(expected)) == 0);
    }
    CHECK(rc_punch_device_mount(&punch, path, false,
                                error, sizeof(error)) != 0);
    CHECK(rc_punch_device_mount(&punch, path, true,
                                error, sizeof(error)) == 0);
    word = UINT32_C(0x5a);
    CHECK(punch.device.io(punch.device.context, RC_IO_WRITE, 2, &word) ==
          RC_DEVICE_ACCEPTED);
    rc_punch_device_destroy(&punch);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fgetc(file) == 0x5a);
        CHECK(fgetc(file) == EOF);
        CHECK(fclose(file) == 0);
    }
    word = 0;
    CHECK(punch.device.io(punch.device.context, RC_IO_WRITE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(punch.device.io(punch.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == (UINT32_C(1) << 21));
    CHECK(unlink(path) == 0);
}

typedef struct {
    rc_device_t device;
    rc_io_command_t commands[24];
    rc_device_result_t results[24];
    rc_word_t words[24];
    size_t count;
    size_t position;
} scripted_device_t;

static rc_device_result_t scripted_device_io(void *context,
                                             rc_io_command_t command,
                                             uint8_t modifier,
                                             rc_word_t *word)
{
    scripted_device_t *device = context;

    (void)command;
    (void)modifier;
    if (device == NULL || word == NULL || device->position >= device->count) {
        return RC_DEVICE_DISCONNECTED;
    }
    CHECK(command == device->commands[device->position]);
    *word = device->words[device->position];
    return device->results[device->position++];
}

static void scripted_device_init(scripted_device_t *device)
{
    memset(device, 0, sizeof(*device));
    device->device.context = device;
    device->device.io = scripted_device_io;
}

static void scripted_device_add(scripted_device_t *device,
                                rc_io_command_t command,
                                rc_device_result_t result, rc_word_t word)
{
    CHECK(device->count < sizeof(device->results) /
                          sizeof(device->results[0]));
    if (device->count < sizeof(device->results) /
                        sizeof(device->results[0])) {
        device->commands[device->count] = command;
        device->results[device->count] = result;
        device->words[device->count++] = word;
    }
}

static void scripted_autoload_character(scripted_device_t *device,
                                        rc_word_t status)
{
    scripted_device_add(device, RC_IO_READ, RC_DEVICE_ACCEPTED, 0);
    scripted_device_add(device, RC_IO_SENSE, RC_DEVICE_ACCEPTED, status);
}

typedef struct {
    FILE *stream;
    uint64_t call;
    bool okay;
} trace_capture_t;

static void capture_trace_write(void *context, rc_addr_t address,
                                rc_word_t previous, rc_word_t value)
{
    trace_capture_t *capture = context;

    if (capture != NULL && capture->okay) {
        capture->okay = rc_trace_write_core_write(
            capture->stream, capture->call, address, previous, value);
    }
}

static void test_architectural_trace_vector(void)
{
    static const char expected_path[] =
        SIM4000_SOURCE_DIR "/tests/fixtures/trace_integer_device_v1.trace";
    rc_machine_t machine;
    scripted_device_t device;
    trace_capture_t capture;
    FILE *actual;
    FILE *expected;
    unsigned char actual_data[8192];
    unsigned char expected_data[8192];
    size_t actual_length;
    size_t expected_length;
    uint64_t call;

    CHECK(rc_machine_init(&machine, 256));
    scripted_device_init(&device);
    scripted_device_add(&device, RC_IO_SENSE, RC_DEVICE_ACCEPTED,
                        UINT32_C(0x654321));
    CHECK(rc_machine_attach_device(&machine, 5, &device.device));
    machine.cpu.instruction_counter = 32;
    machine.cpu.reset_mode = false;
    machine.cpu.stopped = false;
    CHECK(rc_core_write_word(&machine.core, 128, 3));
    CHECK(rc_core_write_word(&machine.core, 130, 0));
    put_instruction(&machine, 32, 11, 1, false, false, 0, 128);
    put_instruction(&machine, 34, 20, 0, false, false, 1, 0);
    put_instruction(&machine, 36, 7, 0, false, false, 0, 128);
    put_instruction(&machine, 38, 23, 0, false, false, 0, 130);
    put_instruction(&machine, 40, 9, 0, false, false, 0, 4);
    put_instruction(&machine, 42, 11, 2, false, false, 0, 10);
    put_instruction(&machine, 44, 1, 3, false, false, 0, 320);

    actual = tmpfile();
    CHECK(actual != NULL);
    if (actual == NULL) {
        rc_machine_destroy(&machine);
        return;
    }
    capture.stream = actual;
    capture.call = 0;
    capture.okay = rc_trace_write_header(actual);
    rc_machine_set_write_observer(&machine, capture_trace_write, &capture);
    for (call = 1; call <= 6; ++call) {
        uint64_t instructions_before = machine.instructions_executed;
        uint64_t io_before = machine.io_operations;
        uint64_t interrupts_before = machine.interrupts_serviced;
        rc_step_result_t result;

        capture.call = call;
        result = rc_machine_step(&machine);
        CHECK(result == RC_STEP_OK);
        capture.okay = capture.okay && rc_trace_write_step(
            actual, call, &machine, result, instructions_before, io_before,
            interrupts_before);
    }
    CHECK(capture.okay);
    CHECK(fflush(actual) == 0);
    CHECK(fseek(actual, 0, SEEK_SET) == 0);
    actual_length = fread(actual_data, 1, sizeof(actual_data), actual);
    CHECK(actual_length < sizeof(actual_data));
    CHECK(ferror(actual) == 0);

    expected = fopen(expected_path, "rb");
    CHECK(expected != NULL);
    if (expected != NULL) {
        expected_length = fread(expected_data, 1, sizeof(expected_data),
                                expected);
        CHECK(expected_length < sizeof(expected_data));
        CHECK(ferror(expected) == 0);
        CHECK(actual_length == expected_length);
        if (actual_length == expected_length) {
            CHECK(memcmp(actual_data, expected_data, actual_length) == 0);
        }
        CHECK(fclose(expected) == 0);
    }
    CHECK(fclose(actual) == 0);
    rc_machine_destroy(&machine);
}

static void test_autoload_word(void)
{
    rc_machine_t machine;
    scripted_device_t reader;
    rc_word_t word = 0;

    CHECK(rc_machine_init(&machine, 128));
    scripted_device_init(&reader);
    CHECK(rc_machine_attach_device(&machine, 0, &reader.device));
    machine.cpu.stopped = false;
    machine.cpu.reset_mode = false;
    machine.cpu.instruction_counter = 16;
    put_instruction(&machine, 16, 0, 3, false, false, 0, 40);

    scripted_device_add(&reader, RC_IO_READ, RC_DEVICE_BUSY, 0);
    scripted_device_add(&reader, RC_IO_READ, RC_DEVICE_ACCEPTED, 0);
    scripted_device_add(&reader, RC_IO_SENSE, RC_DEVICE_BUSY, 0);
    scripted_device_add(&reader, RC_IO_SENSE, RC_DEVICE_ACCEPTED,
                        UINT32_C(0x800000));
    scripted_autoload_character(&reader, 1);
    scripted_autoload_character(&reader, 2);
    scripted_autoload_character(&reader, 3);
    scripted_autoload_character(&reader, 63);

    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(rc_machine_read_word(&machine, 40, &word));
    CHECK(word == UINT32_C(0x000420ff));
    CHECK(machine.cpu.instruction_counter == 18);
    CHECK(machine.io_operations == 12);
    CHECK(machine.last_io_device == 0);
    CHECK(machine.last_io_command == RC_IO_SENSE);
    CHECK(machine.cpu.exception_22 == 0);
    CHECK(machine.cpu.exception_23 == 0);
    rc_machine_destroy(&machine);

    CHECK(rc_machine_init(&machine, 128));
    scripted_device_init(&reader);
    CHECK(rc_machine_attach_device(&machine, 0, &reader.device));
    machine.cpu.stopped = false;
    machine.cpu.reset_mode = false;
    machine.cpu.instruction_counter = 16;
    put_instruction(&machine, 16, 0, 0, false, false, 0, 40);
    scripted_autoload_character(&reader, UINT32_C(0x001000));
    CHECK(rc_machine_step(&machine) == RC_STEP_STOPPED);
    CHECK(machine.cpu.stopped);
    CHECK(machine.cpu.reset_mode);
    CHECK(machine.cpu.instruction_counter == 8);
    rc_machine_destroy(&machine);

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.stopped = false;
    machine.cpu.reset_mode = false;
    machine.cpu.instruction_counter = 16;
    put_instruction(&machine, 16, 0, 0, false, false, 0, 40);
    CHECK(rc_machine_step(&machine) == RC_STEP_STOPPED);
    CHECK(machine.cpu.stopped);
    CHECK(machine.cpu.reset_mode);
    rc_machine_destroy(&machine);
}

static void test_machine_control_panel(void)
{
    rc_machine_t machine;
    scripted_device_t reader;
    rc_word_t saved_ic = 0;
    rc_word_t bootstrap = UINT32_C(0x123456);

    CHECK(rc_machine_init(&machine, 128));
    machine.cpu.reset_mode = false;
    machine.cpu.stopped = false;
    machine.cpu.monitor_mode = false;
    machine.cpu.interrupts_enabled = true;
    machine.cpu.instruction_counter = 42;
    machine.cpu.w[2] = UINT32_C(0xabcdef);
    CHECK(rc_machine_control_reset(&machine) == RC_CONTROL_OK);
    CHECK(rc_machine_read_word(&machine, 10, &saved_ic));
    CHECK(saved_ic == 42);
    CHECK(machine.cpu.reset_mode);
    CHECK(machine.cpu.stopped);
    CHECK(machine.cpu.monitor_mode);
    CHECK(!machine.cpu.interrupts_enabled);
    CHECK(machine.cpu.w[2] == UINT32_C(0xabcdef));

    machine.cpu.instruction_counter = 44;
    CHECK(rc_machine_control_reset(&machine) == RC_CONTROL_OK);
    CHECK(rc_machine_read_word(&machine, 10, &saved_ic));
    CHECK(saved_ic == 42);
    CHECK(rc_machine_write_word(&machine, 14, UINT32_C(0x800021)));
    CHECK(rc_machine_control_start(&machine) == RC_CONTROL_OK);
    CHECK(machine.cpu.instruction_counter == 32);
    CHECK(!machine.cpu.reset_mode);
    CHECK(!machine.cpu.stopped);
    CHECK(rc_machine_control_start(&machine) == RC_CONTROL_NOT_IN_RESET);

    rc_machine_control_interrupts(&machine, true);
    CHECK(machine.cpu.interrupts_enabled);
    rc_machine_control_interrupts(&machine, false);
    CHECK(!machine.cpu.interrupts_enabled);
    CHECK(rc_machine_set_control_register(
        &machine, RC_CONTROL_INTERRUPT_MASK, UINT32_C(0x123456)));
    CHECK(machine.cpu.interrupt_mask == UINT32_C(0x123456));
    CHECK(rc_machine_set_control_register(
        &machine, RC_CONTROL_INTERRUPT_REQUESTS, UINT32_C(0xabcdef)));
    CHECK(machine.cpu.interrupt_requests == UINT32_C(0xabcdef));
    CHECK(rc_machine_set_control_register(
        &machine, RC_CONTROL_EXCEPTION_21, 4));
    CHECK(machine.cpu.exception_21 == 4);
    CHECK(!rc_machine_set_control_register(
        &machine, RC_CONTROL_EXCEPTION_21, 5));
    CHECK(rc_machine_set_control_register(
        &machine, RC_CONTROL_EXCEPTION_22, 1));
    CHECK(!rc_machine_set_control_register(
        &machine, RC_CONTROL_EXCEPTION_22, 2));
    CHECK(rc_machine_set_control_register(
        &machine, RC_CONTROL_EXCEPTION_23, 1));
    CHECK(machine.cpu.exception_22 == 1);
    CHECK(machine.cpu.exception_23 == 1);
    rc_machine_destroy(&machine);

    CHECK(rc_machine_init(&machine, 128));
    scripted_device_init(&reader);
    CHECK(rc_machine_attach_device(&machine, 0, &reader.device));
    scripted_autoload_character(&reader, (bootstrap >> 18) & 63U);
    scripted_autoload_character(&reader, (bootstrap >> 12) & 63U);
    scripted_autoload_character(&reader, (bootstrap >> 6) & 63U);
    scripted_autoload_character(&reader, bootstrap & 63U);
    CHECK(rc_machine_control_autoload(&machine) == RC_CONTROL_OK);
    CHECK(machine.cpu.w[0] == bootstrap);
    CHECK(machine.cpu.instruction_counter == 0);
    CHECK(!machine.cpu.reset_mode);
    CHECK(!machine.cpu.stopped);
    CHECK(rc_machine_control_autoload(&machine) ==
          RC_CONTROL_NOT_IN_RESET);
    rc_machine_destroy(&machine);

    CHECK(rc_machine_init(&machine, 128));
    CHECK(rc_machine_control_autoload(&machine) ==
          RC_CONTROL_AUTOLOAD_ERROR);
    CHECK(machine.cpu.reset_mode);
    CHECK(machine.cpu.stopped);
    rc_machine_destroy(&machine);
}

typedef struct {
    uint8_t bytes[8192];
    size_t count;
} output_capture_t;

static void capture_console_output(void *context, uint8_t character)
{
    output_capture_t *capture = context;

    if (capture->count < sizeof(capture->bytes)) {
        capture->bytes[capture->count++] = character;
    }
}

static bool output_contains_after(const output_capture_t *capture,
                                  size_t first, const char *text)
{
    size_t length = strlen(text);
    size_t index;

    if (length == 0 || first > capture->count ||
        length > capture->count - first) {
        return false;
    }
    for (index = first; index + length <= capture->count; ++index) {
        if (memcmp(capture->bytes + index, text, length) == 0) {
            return true;
        }
    }
    return false;
}

static bool submit_console_text(rc_console_device_t *console,
                                const char *text)
{
    while (*text != '\0') {
        if (!rc_console_device_submit(console, (uint8_t)*text++)) {
            return false;
        }
    }
    return true;
}

static void test_scheduled_clock(void)
{
    rc_machine_t machine;
    rc_clock_device_t clock;

    CHECK(rc_machine_init(&machine, 128));
    rc_clock_device_init(&clock, 11);
    CHECK(rc_machine_attach_device(&machine, 3, &clock.device));
    rc_machine_advance_time(&machine, 2);
    CHECK(machine.virtual_time_ticks == 2);
    CHECK(clock.ticks == 2);
    CHECK(machine.cpu.interrupt_requests == 0);
    rc_machine_poll_devices(&machine);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 12));
    rc_machine_destroy(&machine);
}

static void test_console_device(void)
{
    rc_machine_t machine;
    rc_console_device_t console;
    output_capture_t output = {{0}, 0};
    rc_word_t word;
    size_t index;

    CHECK(rc_machine_init(&machine, 128));
    rc_console_device_init(&console, 6, 3);
    rc_console_device_set_output(&console, capture_console_output, &output);
    CHECK(rc_machine_attach_device(&machine, 2, &console.device));

    word = 'A';
    CHECK(console.device.io(console.device.context, RC_IO_WRITE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(output.count == 1 && output.bytes[0] == 'A');
    rc_machine_poll_devices(&machine);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 17));
    machine.cpu.interrupt_requests = 0;

    word = 0;
    CHECK(console.device.io(console.device.context, RC_IO_READ, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(console.busy);
    CHECK(console.device.io(console.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_BUSY);
    CHECK(rc_console_device_submit(&console, 'b'));
    CHECK(!console.busy);
    rc_machine_poll_devices(&machine);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 17));
    CHECK(console.device.io(console.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == 'b');
    CHECK(output.count == 2 && output.bytes[1] == 'b');

    machine.cpu.interrupt_requests = 0;
    CHECK(rc_console_device_submit(&console, 'c'));
    CHECK(rc_console_device_input_count(&console) == 1);
    CHECK(console.device.io(console.device.context, RC_IO_READ, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(rc_console_device_input_count(&console) == 0);
    CHECK(console.device.io(console.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == 'c');
    rc_machine_poll_devices(&machine);

    machine.cpu.interrupt_requests = 0;
    CHECK(rc_console_device_submit(&console, 27));
    rc_machine_poll_devices(&machine);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 20));

    machine.cpu.interrupt_requests = 0;
    CHECK(rc_console_device_submit(&console, 'q'));
    CHECK(rc_console_device_input_count(&console) == 1);
    rc_console_device_attention(&console);
    CHECK(rc_console_device_input_count(&console) == 0);
    rc_machine_poll_devices(&machine);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 20));

    machine.cpu.interrupt_requests = 0;
    CHECK(console.device.io(console.device.context, RC_IO_READ, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    rc_machine_advance_time(&machine, RC_CONSOLE_TIMEOUT_TICKS - 1U);
    CHECK(console.busy);
    rc_machine_advance_time(&machine, 1);
    CHECK(!console.busy);
    rc_machine_poll_devices(&machine);
    CHECK(machine.cpu.interrupt_requests == (UINT32_C(1) << 17));
    CHECK(console.device.io(console.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == (UINT32_C(1) << 21));

    for (index = 0; index < RC_CONSOLE_QUEUE_SIZE - 1U; ++index) {
        CHECK(rc_console_device_submit(&console, (uint8_t)('a' + index % 26U)));
    }
    CHECK(!rc_console_device_submit(&console, 'x'));
    CHECK(rc_console_device_discard_input(&console) == RC_CONSOLE_QUEUE_SIZE - 1U);
    CHECK(rc_console_device_input_count(&console) == 0);
    CHECK(rc_console_device_discard_input(&console) == 0);
    CHECK(rc_console_device_discard_input(NULL) == 0);

    console.pending_interrupts = 0;
    word = 0;
    CHECK(console.device.io(console.device.context, RC_IO_READ, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(console.busy);
    CHECK(rc_console_device_discard_input(&console) == 0);
    CHECK(console.busy && console.timeout_ticks == RC_CONSOLE_TIMEOUT_TICKS);
    CHECK(console.pending_interrupts == 0);
    CHECK(rc_console_device_submit(&console, 'D'));
    CHECK(rc_console_device_submit(&console, 'E'));
    CHECK(rc_console_device_discard_input(&console) == 1);
    CHECK(!console.busy && console.echo_on_sense && console.status == 'D');
    CHECK(console.pending_interrupts == (UINT32_C(1) << 17));
    CHECK(console.device.io(console.device.context, RC_IO_SENSE, 0, &word) ==
          RC_DEVICE_ACCEPTED);
    CHECK(word == 'D');
    rc_machine_destroy(&machine);
}

static bool write_test_disk_image(const char *path)
{
    FILE *file = fopen(path, "wb");
    unsigned sector;

    if (file == NULL) {
        return false;
    }
    for (sector = 0; sector < 2; ++sector) {
        unsigned word_index;
        for (word_index = 0; word_index < 256; ++word_index) {
            rc_word_t value = ((rc_word_t)(sector + 1U) << 20) |
                              (rc_word_t)word_index;
            uint8_t bytes[3] = {
                (uint8_t)value,
                (uint8_t)(value >> 8),
                (uint8_t)(value >> 16)
            };
            if (fwrite(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) {
                (void)fclose(file);
                (void)remove(path);
                return false;
            }
        }
    }
    return fclose(file) == 0;
}

static void test_catalog_rejects_non_catalog_image(void)
{
    static const char path[] = "sim4000-test-not-catalog.img";
    rc_catalog_t catalog;
    char error[256];

    CHECK(write_test_disk_image(path));
    CHECK(rc_catalog_read(path, &catalog, error, sizeof(error)) != 0);
    CHECK(catalog.entries == NULL);
    CHECK(rc_catalog_read_main(path, &catalog, error, sizeof(error)) != 0);
    CHECK(catalog.entries == NULL);
    CHECK(remove(path) == 0);
}

static void put_rc433x_message(rc_machine_t *machine, rc_word_t operation,
                               rc_addr_t core_address, rc_word_t first_sector,
                               rc_word_t sector_count)
{
    CHECK(rc_machine_write_word(machine, 32, operation << 12));
    CHECK(rc_machine_write_word(machine, 34, core_address));
    CHECK(rc_machine_write_word(machine, 36, 0));
    CHECK(rc_machine_write_word(machine, 38, first_sector));
    CHECK(rc_machine_write_word(machine, 40, 0));
    CHECK(rc_machine_write_word(machine, 42, 0));
    CHECK(rc_machine_write_word(machine, 44, sector_count << 12));
    CHECK(rc_machine_write_word(machine, 46, 0));
}

static void test_rc433x_persistent_overlay(void)
{
    static const char base_path[] = "sim4000-test-persistent.img";
    static const char other_path[] = "sim4000-test-persistent-other.img";
    static const char overlay_path[] = "sim4000-test-persistent.overlay";
    rc_machine_t machine;
    rc_rc433x_device_t disk;
    rc_image_info_t before;
    rc_image_info_t after;
    rc_word_t message_address;
    rc_word_t value;
    FILE *file;
    unsigned word_index;
    char error[256];

    (void)remove(overlay_path);
    CHECK(write_test_disk_image(base_path));
    CHECK(rc_image_inspect(base_path, &before, error, sizeof(error)) == 0);
    CHECK(rc_machine_init(&machine, 4096));
    CHECK(rc_rc433x_device_init_persistent(
              &disk, &machine, base_path, overlay_path,
              error, sizeof(error)) == 0);
    CHECK(disk.persistent_overlay);
    CHECK(disk.overlay_sectors == 0);

    for (word_index = 0; word_index < 256; ++word_index) {
        CHECK(rc_machine_write_word(&machine, 512U + word_index * 2U,
                                    UINT32_C(0x710000) | word_index));
    }
    put_rc433x_message(&machine, 5, 512, 1, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(message_address == 0);
    CHECK(disk.persistence_failures == 0);
    CHECK(disk.overlay_sectors == 1);
    CHECK(rc_rc433x_device_flush(&disk, error, sizeof(error)) == 0);
    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);

    CHECK(rc_machine_init(&machine, 4096));
    CHECK(rc_rc433x_device_init_persistent(
              &disk, &machine, base_path, overlay_path,
              error, sizeof(error)) == 0);
    CHECK(disk.overlay_sectors == 1);
    put_rc433x_message(&machine, 3, 1536, 1, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(message_address == 0);
    CHECK(rc_machine_read_word(&machine, 1536, &value));
    CHECK(value == UINT32_C(0x710000));
    CHECK(rc_machine_read_word(&machine, 1536 + 255U * 2U, &value));
    CHECK(value == UINT32_C(0x7100ff));
    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);

    CHECK(write_test_disk_image(other_path));
    file = fopen(other_path, "r+b");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fputc(0x5a, file) == 0x5a);
        CHECK(fclose(file) == 0);
    }
    CHECK(rc_machine_init(&machine, 4096));
    CHECK(rc_rc433x_device_init_persistent(
              &disk, &machine, other_path, overlay_path,
              error, sizeof(error)) != 0);
    rc_machine_destroy(&machine);

    file = fopen(overlay_path, "r+b");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fseek(file, 48 + 8, SEEK_SET) == 0);
        value = (rc_word_t)fgetc(file);
        CHECK(value != (rc_word_t)EOF);
        CHECK(fseek(file, -1, SEEK_CUR) == 0);
        CHECK(fputc((int)(value ^ UINT32_C(0x01)), file) != EOF);
        CHECK(fclose(file) == 0);
    }
    CHECK(rc_machine_init(&machine, 4096));
    CHECK(rc_rc433x_device_init_persistent(
              &disk, &machine, base_path, overlay_path,
              error, sizeof(error)) != 0);
    rc_machine_destroy(&machine);

    file = fopen(overlay_path, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fwrite("short", 1, 5, file) == 5);
        CHECK(fclose(file) == 0);
    }
    CHECK(rc_machine_init(&machine, 4096));
    CHECK(rc_rc433x_device_init_persistent(
              &disk, &machine, base_path, overlay_path,
              error, sizeof(error)) != 0);
    rc_machine_destroy(&machine);

    CHECK(rc_machine_init(&machine, 4096));
    CHECK(rc_rc433x_device_init_persistent(
              &disk, &machine, base_path, base_path,
              error, sizeof(error)) != 0);
    rc_machine_destroy(&machine);

    CHECK(rc_image_inspect(base_path, &after, error, sizeof(error)) == 0);
    CHECK(after.byte_size == before.byte_size);
    CHECK(after.checksum_fnv1a == before.checksum_fnv1a);
    CHECK(remove(overlay_path) == 0);
    CHECK(remove(other_path) == 0);
    CHECK(remove(base_path) == 0);
}

static void test_rc433x_readonly_device(void)
{
    static const char path[] = "sim4000-test-rc433x.img";
    rc_machine_t machine;
    rc_rc433x_device_t disk;
    rc_image_info_t before;
    rc_image_info_t after;
    rc_word_t value = 0;
    char error[256];

    CHECK(write_test_disk_image(path));
    CHECK(rc_image_inspect(path, &before, error, sizeof(error)) == 0);
    CHECK(before.complete_sectors == 2);
    CHECK(rc_machine_init(&machine, 2048));
    CHECK(rc_rc433x_device_init_readonly(&disk, &machine, path, error,
                                         sizeof(error)) == 0);
    CHECK(disk.image.sector_count == 2);
    CHECK(rc_machine_attach_device(&machine, 15, &disk.device));

    CHECK(rc_machine_write_word(&machine, 32, UINT32_C(3) << 12));
    CHECK(rc_machine_write_word(&machine, 34, 128));
    CHECK(rc_machine_write_word(&machine, 36, 0));
    CHECK(rc_machine_write_word(&machine, 38, 1));
    CHECK(rc_machine_write_word(&machine, 40, 0));
    CHECK(rc_machine_write_word(&machine, 42, 0));
    CHECK(rc_machine_write_word(&machine, 44, UINT32_C(1) << 12));
    CHECK(rc_machine_write_word(&machine, 46, 0));
    machine.cpu.w[0] = 32;
    machine.cpu.instruction_counter = 16;
    machine.cpu.stopped = false;
    put_instruction(&machine, 16, 1, 0, false, false, 0, 15 * 64 + 1);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == 0);
    CHECK(disk.read_operations == 1);
    CHECK(disk.sectors_read == 1);
    CHECK(rc_machine_read_word(&machine, 128, &value));
    CHECK(value == UINT32_C(0x200000));
    CHECK(rc_machine_read_word(&machine, 128 + 255 * 2, &value));
    CHECK(value == UINT32_C(0x2000ff));

    CHECK(rc_machine_write_word(&machine, 32, UINT32_C(5) << 12));
    machine.cpu.w[0] = 32;
    put_instruction(&machine, 18, 1, 0, false, false, 0, 15 * 64 + 1);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == (UINT32_C(1) << 22));
    CHECK(disk.rejected_writes == 1);
    put_instruction(&machine, 20, 1, 0, false, false, 0, 15 * 64);
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.w[0] == (UINT32_C(1) << 22));

    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);
    CHECK(rc_image_inspect(path, &after, error, sizeof(error)) == 0);
    CHECK(after.byte_size == before.byte_size);
    CHECK(after.checksum_fnv1a == before.checksum_fnv1a);
    CHECK(remove(path) == 0);
}

static void test_rc433x_cow_device(void)
{
    static const char path[] = "sim4000-test-rc433x-cow.img";
    rc_machine_t machine;
    rc_rc433x_device_t disk;
    rc_image_info_t before;
    rc_image_info_t after;
    rc_word_t message_address;
    rc_word_t value;
    unsigned word_index;
    char error[256];

    CHECK(write_test_disk_image(path));
    CHECK(rc_image_inspect(path, &before, error, sizeof(error)) == 0);
    CHECK(rc_machine_init(&machine, 4096));
    CHECK(rc_rc433x_device_init_cow(&disk, &machine, path, error,
                                    sizeof(error)) == 0);

    for (word_index = 0; word_index < 256; ++word_index) {
        CHECK(rc_machine_write_word(&machine, 512U + word_index * 2U,
                                    UINT32_C(0x5a0000) | word_index));
    }
    put_rc433x_message(&machine, 5, 512, 1, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(message_address == 0);
    CHECK(disk.write_operations == 1);
    CHECK(disk.sectors_written == 1);
    CHECK(disk.overlay_sectors == 1);
    CHECK(disk.rejected_writes == 0);

    put_rc433x_message(&machine, 3, 1536, 1, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(message_address == 0);
    CHECK(rc_machine_read_word(&machine, 1536, &value));
    CHECK(value == UINT32_C(0x5a0000));
    CHECK(rc_machine_read_word(&machine, 1536 + 255 * 2, &value));
    CHECK(value == UINT32_C(0x5a00ff));

    for (word_index = 0; word_index < 256; ++word_index) {
        CHECK(rc_machine_write_word(&machine, 512U + word_index * 2U,
                                    UINT32_C(0x6b0000) | word_index));
    }
    put_rc433x_message(&machine, 5, 512, 1, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(disk.write_operations == 2);
    CHECK(disk.sectors_written == 2);
    CHECK(disk.overlay_sectors == 1);

    put_rc433x_message(&machine, 5, 512, 5, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(disk.overlay_sectors == 2);
    put_rc433x_message(&machine, 3, 2048, 5, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(rc_machine_read_word(&machine, 2048, &value));
    CHECK(value == UINT32_C(0x6b0000));
    CHECK(rc_machine_read_word(&machine, 2048 + 255 * 2, &value));
    CHECK(value == UINT32_C(0x6b00ff));
    CHECK(disk.read_operations == 2);
    CHECK(disk.sectors_read == 2);

    CHECK(rc_image_inspect(path, &after, error, sizeof(error)) == 0);
    CHECK(after.byte_size == before.byte_size);
    CHECK(after.checksum_fnv1a == before.checksum_fnv1a);
    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);
    CHECK(remove(path) == 0);
}

static void test_rc433x_writable_device(void)
{
    static const char path[] = "sim4000-test-rc433x-writable.img";
    rc_machine_t machine;
    rc_rc433x_device_t disk;
    rc_image_info_t before;
    rc_image_info_t after;
    rc_word_t message_address;
    rc_word_t value;
    unsigned word_index;
    char error[256];

    CHECK(write_test_disk_image(path));
    CHECK(rc_image_inspect(path, &before, error, sizeof(error)) == 0);
    CHECK(rc_machine_init(&machine, 4096));
    CHECK(rc_rc433x_device_init_writable(&disk, &machine, path, error,
                                         sizeof(error)) == 0);
    CHECK(disk.write_through);
    CHECK(disk.image.writable);
    CHECK(!disk.readonly);
    CHECK(!disk.persistent_overlay);

    for (word_index = 0; word_index < 256; ++word_index) {
        CHECK(rc_machine_write_word(&machine, 512U + word_index * 2U,
                                    UINT32_C(0x7c0000) | word_index));
    }
    put_rc433x_message(&machine, 5, 512, 1, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(message_address == 0);
    put_rc433x_message(&machine, 5, 512, 5, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(message_address == 0);
    CHECK(disk.write_operations == 2);
    CHECK(disk.sectors_written == 2);
    CHECK(disk.overlay_sectors == 0);
    CHECK(disk.image.sector_count == 6);
    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);

    CHECK(rc_image_inspect(path, &after, error, sizeof(error)) == 0);
    CHECK(after.complete_sectors == 6);
    CHECK(after.checksum_fnv1a != before.checksum_fnv1a);

    CHECK(rc_machine_init(&machine, 4096));
    CHECK(rc_rc433x_device_init_readonly(&disk, &machine, path, error,
                                         sizeof(error)) == 0);
    put_rc433x_message(&machine, 3, 1536, 1, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(rc_machine_read_word(&machine, 1536, &value));
    CHECK(value == UINT32_C(0x7c0000));
    CHECK(rc_machine_read_word(&machine, 1536 + 255U * 2U, &value));
    CHECK(value == UINT32_C(0x7c00ff));
    put_rc433x_message(&machine, 3, 2048, 5, 1);
    message_address = 32;
    CHECK(disk.device.io(disk.device.context, RC_IO_CONTROL, 0,
                         &message_address) == RC_DEVICE_ACCEPTED);
    CHECK(rc_machine_read_word(&machine, 2048, &value));
    CHECK(value == UINT32_C(0x7c0000));
    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);
    CHECK(remove(path) == 0);
}

static void test_monitor_first_instruction(void)
{
    static const char monitor_path[] =
        SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    rc_machine_t machine;
    rc_addr_t load_address = 0;
    rc_addr_t entry_address = 0;
    char error[256];

    CHECK(rc_machine_init(&machine, 1048576));
    CHECK(rc_monitor_load(monitor_path, &machine.core, true, &load_address,
                          &entry_address, error, sizeof(error)) == 0);
    machine.cpu.instruction_counter = entry_address;
    machine.cpu.stopped = false;
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.last_instruction.opcode == 11);
    CHECK(machine.cpu.instruction_counter == entry_address + 2U);
    rc_machine_destroy(&machine);
}

static void test_monitor_cpu_initialization_checkpoint(void)
{
    static const char monitor_path[] =
        SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    rc_machine_t machine;
    rc_clock_device_t clock;
    rc_addr_t load_address = 0;
    rc_addr_t entry_address = 0;
    rc_step_result_t result = RC_STEP_OK;
    uint64_t calls;
    char error[256];

    CHECK(rc_machine_init(&machine, 1048576));
    CHECK(rc_monitor_load(monitor_path, &machine.core, true, &load_address,
                          &entry_address, error, sizeof(error)) == 0);
    rc_clock_device_init(&clock, 11);
    CHECK(rc_machine_attach_device(&machine, 3, &clock.device));
    machine.cpu.instruction_counter = entry_address;
    machine.cpu.stopped = false;
    for (calls = 0; calls < UINT64_C(150000) && result == RC_STEP_OK;
         ++calls) {
        result = rc_machine_step(&machine);
    }
    CHECK(result == RC_STEP_OK);
    CHECK(machine.last_instruction.opcode == 15);
    /* Printer-capable MONITOR dff97596: fixed-image regression checkpoint. */
    CHECK(machine.last_instruction_address == UINT32_C(0x001c5e));
    CHECK(machine.last_effective_address == UINT32_C(0x001c5e));
    CHECK(machine.cpu.instruction_counter == UINT32_C(0x001c5e));
    CHECK(machine.interrupts_serviced == 17);
    CHECK(machine.io_operations == 275);
    CHECK(machine.instructions_executed == UINT64_C(150151));
    rc_machine_destroy(&machine);
}

static void test_monitor_device_time_checkpoint(void)
{
    static const char monitor_path[] =
        SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    static const uint8_t expected_output[] =
        "\r\n\x7f" "from s\r\n\x7f" "on 015  status  "
        ".........111..........1.\r\n\x7f" "area unknown \r\n\x7f";
    rc_machine_t machine;
    rc_clock_device_t clock;
    rc_console_device_t console2;
    rc_console_device_t console4;
    output_capture_t output = {{0}, 0};
    rc_addr_t load_address = 0;
    rc_addr_t entry_address = 0;
    rc_step_result_t result = RC_STEP_OK;
    uint64_t calls;
    char error[256];

    CHECK(rc_machine_init(&machine, 1048576));
    CHECK(rc_monitor_load(monitor_path, &machine.core, true, &load_address,
                          &entry_address, error, sizeof(error)) == 0);
    rc_clock_device_init(&clock, 11);
    rc_console_device_init(&console2, 6, 3);
    rc_console_device_init(&console4, 20, 19);
    rc_console_device_set_output(&console2, capture_console_output, &output);
    rc_console_device_set_output(&console4, capture_console_output, &output);
    CHECK(rc_machine_attach_device(&machine, 2, &console2.device));
    CHECK(rc_machine_attach_device(&machine, 3, &clock.device));
    CHECK(rc_machine_attach_device(&machine, 4, &console4.device));
    machine.cpu.instruction_counter = entry_address;
    machine.cpu.stopped = false;
    for (calls = 0; calls < UINT64_C(1000000) && result == RC_STEP_OK;
         ++calls) {
        if (calls != 0 && calls % UINT64_C(10000) == 0) {
            rc_machine_advance_time(&machine, 1);
        }
        result = rc_machine_step(&machine);
    }
    CHECK(result == RC_STEP_OK);
    CHECK(machine.last_instruction_address == UINT32_C(0x001c5e));
    CHECK(machine.cpu.instruction_counter == UINT32_C(0x001c5e));
    CHECK(machine.virtual_time_ticks == 99);
    CHECK(machine.interrupts_serviced == 174);
    CHECK(machine.io_operations == 503);
    CHECK(machine.instructions_executed == UINT64_C(1000410));
    CHECK(output.count == sizeof(expected_output) - 1U);
    CHECK(memcmp(output.bytes, expected_output,
                 sizeof(expected_output) - 1U) == 0);
    rc_machine_destroy(&machine);
}

static void test_monitor_readonly_disk_checkpoint(void)
{
    static const char monitor_path[] =
        SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    static const char image_path[] =
        SIM4000_SOURCE_DIR "/src/disks/UNIT0.original";
    static const uint8_t expected_output[] =
        "\r\n\x7f" "from s\r\n\x7f" "disc         mounted on 015 "
        "\r\n\x7f" "connect main catalog  catalog  result  2"
        "\r\n\x7f" "no main catalog connected "
        "\r\n\x7f" "catalog error \r\n\x7f";
    rc_machine_t machine;
    rc_clock_device_t clock;
    rc_console_device_t console2;
    rc_console_device_t console4;
    rc_rc433x_device_t disk;
    rc_image_info_t before;
    rc_image_info_t after;
    output_capture_t output = {{0}, 0};
    rc_addr_t load_address = 0;
    rc_addr_t entry_address = 0;
    rc_step_result_t result = RC_STEP_OK;
    uint64_t calls;
    char error[256];

    CHECK(rc_image_inspect(image_path, &before, error, sizeof(error)) == 0);
    CHECK(rc_machine_init(&machine, 1048576));
    CHECK(rc_monitor_load(monitor_path, &machine.core, true, &load_address,
                          &entry_address, error, sizeof(error)) == 0);
    rc_clock_device_init(&clock, 11);
    rc_console_device_init(&console2, 6, 3);
    rc_console_device_init(&console4, 20, 19);
    rc_console_device_set_output(&console2, capture_console_output, &output);
    rc_console_device_set_output(&console4, capture_console_output, &output);
    CHECK(rc_rc433x_device_init_readonly(&disk, &machine, image_path, error,
                                         sizeof(error)) == 0);
    CHECK(rc_machine_attach_device(&machine, 2, &console2.device));
    CHECK(rc_machine_attach_device(&machine, 3, &clock.device));
    CHECK(rc_machine_attach_device(&machine, 4, &console4.device));
    CHECK(rc_machine_attach_device(&machine, 15, &disk.device));
    machine.cpu.instruction_counter = entry_address;
    machine.cpu.stopped = false;
    for (calls = 0; calls < UINT64_C(1000000) && result == RC_STEP_OK;
         ++calls) {
        if (calls != 0 && calls % UINT64_C(10000) == 0) {
            rc_machine_advance_time(&machine, 1);
        }
        result = rc_machine_step(&machine);
    }
    CHECK(result == RC_STEP_OK);
    CHECK(machine.last_instruction_address == UINT32_C(0x001c5e));
    CHECK(machine.cpu.instruction_counter == UINT32_C(0x001c5e));
    CHECK(machine.virtual_time_ticks == 99);
    CHECK(machine.interrupts_serviced == 259);
    CHECK(machine.io_operations == 649);
    CHECK(machine.instructions_executed == UINT64_C(1000949));
    CHECK(disk.read_operations == 4);
    CHECK(disk.sectors_read == 8);
    CHECK(disk.rejected_writes == 3);
    CHECK(output.count == sizeof(expected_output) - 1U);
    CHECK(memcmp(output.bytes, expected_output,
                 sizeof(expected_output) - 1U) == 0);
    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);
    CHECK(rc_image_inspect(image_path, &after, error, sizeof(error)) == 0);
    CHECK(after.byte_size == before.byte_size);
    CHECK(after.checksum_fnv1a == before.checksum_fnv1a);
}

static void test_monitor_cow_disk_checkpoint(void)
{
    static const char monitor_path[] =
        SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    static const char image_path[] =
        SIM4000_SOURCE_DIR "/src/disks/UNIT0.original";
    static const uint8_t expected_boot_output[] =
        "\r\n\x7f" "from s\r\n\x7f" "disc         mounted on 015 "
        "\r\n\x7f";
    static const uint8_t expected_prompt_output[] =
        "\r\n\x7f" "from s\r\n\x7f" "disc         mounted on 015 "
        "\r\n\x7f\r\n\x7f" "att \x07" "s\r\n"
        "ready  **date not initialized  \r\n\x7f\x07";
    rc_machine_t machine;
    rc_clock_device_t clock;
    rc_console_device_t console2;
    rc_console_device_t console4;
    rc_rc433x_device_t disk;
    rc_image_info_t before;
    rc_image_info_t after;
    output_capture_t output = {{0}, 0};
    rc_addr_t load_address = 0;
    rc_addr_t entry_address = 0;
    rc_step_result_t result = RC_STEP_OK;
    uint64_t calls;
    char error[256];

    CHECK(rc_image_inspect(image_path, &before, error, sizeof(error)) == 0);
    CHECK(rc_machine_init(&machine, 1048576));
    CHECK(rc_monitor_load(monitor_path, &machine.core, true, &load_address,
                          &entry_address, error, sizeof(error)) == 0);
    rc_clock_device_init(&clock, 11);
    rc_console_device_init(&console2, 6, 3);
    rc_console_device_init(&console4, 20, 19);
    rc_console_device_set_output(&console2, capture_console_output, &output);
    rc_console_device_set_output(&console4, capture_console_output, &output);
    CHECK(rc_rc433x_device_init_cow(&disk, &machine, image_path, error,
                                    sizeof(error)) == 0);
    CHECK(rc_machine_attach_device(&machine, 2, &console2.device));
    CHECK(rc_machine_attach_device(&machine, 3, &clock.device));
    CHECK(rc_machine_attach_device(&machine, 4, &console4.device));
    CHECK(rc_machine_attach_device(&machine, 15, &disk.device));
    machine.cpu.instruction_counter = entry_address;
    machine.cpu.stopped = false;
    for (calls = 0; calls < UINT64_C(1000000) && result == RC_STEP_OK;
         ++calls) {
        if (calls != 0 && calls % UINT64_C(10000) == 0) {
            rc_machine_advance_time(&machine, 1);
        }
        result = rc_machine_step(&machine);
    }
    CHECK(result == RC_STEP_OK);
    /* At one million calls the larger monitor is one transfer earlier.
       Keep exact pinned-image expectations, not inequalities or self-oracles. */
    CHECK(machine.last_instruction_address == UINT32_C(0x00036a));
    CHECK(machine.cpu.instruction_counter == UINT32_C(0x00036e));
    CHECK(machine.virtual_time_ticks == 99);
    CHECK(machine.interrupts_serviced == 1378);
    CHECK(machine.io_operations == 1444);
    CHECK(machine.instructions_executed == UINT64_C(1015439));
    CHECK(disk.read_operations == 47);
    CHECK(disk.sectors_read == 51);
    CHECK(disk.write_operations == 214);
    CHECK(disk.sectors_written == 214);
    CHECK(disk.overlay_sectors == 72);
    CHECK(disk.rejected_writes == 0);
    CHECK(output.count == sizeof(expected_boot_output) - 1U);
    CHECK(memcmp(output.bytes, expected_boot_output,
                 sizeof(expected_boot_output) - 1U) == 0);

    CHECK(rc_console_device_submit(&console2, 27));
    for (; calls < UINT64_C(1200000) && result == RC_STEP_OK; ++calls) {
        if (calls != 0 && calls % UINT64_C(10000) == 0) {
            rc_machine_advance_time(&machine, 1);
        }
        result = rc_machine_step(&machine);
    }
    CHECK(result == RC_STEP_OK);
    CHECK(console2.busy);
    CHECK(rc_console_device_submit(&console2, 's'));
    CHECK(rc_console_device_submit(&console2, '\r'));
    for (; calls < UINT64_C(4000000) && result == RC_STEP_OK; ++calls) {
        if (calls != 0 && calls % UINT64_C(10000) == 0) {
            rc_machine_advance_time(&machine, 1);
        }
        result = rc_machine_step(&machine);
    }
    CHECK(result == RC_STEP_OK);
    CHECK(calls == UINT64_C(4000000));
    CHECK(machine.last_instruction_address == UINT32_C(0x001c5e));
    CHECK(machine.cpu.instruction_counter == UINT32_C(0x001c5e));
    CHECK(machine.last_instruction.opcode == 15U);
    CHECK(machine.virtual_time_ticks == 399);
    CHECK(machine.interrupts_serviced == 3892);
    CHECK(machine.io_operations == 3856);
    CHECK(machine.instructions_executed == UINT64_C(4043669));
    CHECK(disk.read_operations == 175);
    CHECK(disk.sectors_read == 196);
    CHECK(disk.write_operations == 493);
    CHECK(disk.sectors_written == 493);
    CHECK(disk.overlay_sectors == 74);
    CHECK(disk.rejected_writes == 0);
    CHECK(output.count == sizeof(expected_prompt_output) - 1U);
    CHECK(memcmp(output.bytes, expected_prompt_output,
                 sizeof(expected_prompt_output) - 1U) == 0);
    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);
    CHECK(rc_image_inspect(image_path, &after, error, sizeof(error)) == 0);
    CHECK(after.byte_size == before.byte_size);
    CHECK(after.checksum_fnv1a == before.checksum_fnv1a);
}

static void test_monitor_secondary_console_workflow(void)
{
    static const char monitor_path[] = SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    static const char image_path[] = SIM4000_SOURCE_DIR "/src/disks/UNIT0";
    rc_machine_t machine;
    rc_clock_device_t clock;
    rc_console_device_t console2;
    rc_console_device_t console4;
    rc_rc433x_device_t disk;
    rc_image_info_t before;
    rc_image_info_t after;
    output_capture_t primary_output = {{0}, 0};
    output_capture_t secondary_output = {{0}, 0};
    rc_addr_t load_address = 0;
    rc_addr_t entry_address = 0;
    rc_step_result_t result = RC_STEP_OK;
    uint64_t calls;
    unsigned stage = 0;
    size_t primary_output_start = 0;
    size_t secondary_output_start = 0;
    char error[256];

    CHECK(rc_image_inspect(image_path, &before, error, sizeof(error)) == 0);
    CHECK(rc_machine_init(&machine, 1048576));
    CHECK(rc_monitor_load(monitor_path, &machine.core, true, &load_address,
                          &entry_address, error, sizeof(error)) == 0);
    rc_clock_device_init(&clock, 11);
    rc_console_device_init(&console2, 6, 3);
    rc_console_device_init(&console4, 20, 19);
    rc_console_device_set_output(&console2, capture_console_output,
                                 &primary_output);
    rc_console_device_set_output(&console4, capture_console_output,
                                 &secondary_output);
    CHECK(rc_rc433x_device_init_cow(&disk, &machine, image_path, error,
                                    sizeof(error)) == 0);
    CHECK(rc_machine_attach_device(&machine, 2, &console2.device));
    CHECK(rc_machine_attach_device(&machine, 3, &clock.device));
    CHECK(rc_machine_attach_device(&machine, 4, &console4.device));
    CHECK(rc_machine_attach_device(&machine, 15, &disk.device));
    machine.cpu.instruction_counter = entry_address;
    machine.cpu.stopped = false;

    for (calls = 0; calls < UINT64_C(30000000) && result == RC_STEP_OK &&
                    stage < 10U; ++calls) {
        if (calls != 0 && calls % UINT64_C(10000) == 0) {
            rc_machine_advance_time(&machine, 1);
        }
        result = rc_machine_step(&machine);
        if (stage == 0U &&
            output_contains_after(&primary_output, primary_output_start,
                                  "initialize date:")) {
            CHECK(submit_console_text(&console2, "86 09 30 08 00 00\r"));
            primary_output_start = primary_output.count;
            stage = 1;
        } else if (stage == 1U &&
                   output_contains_after(&primary_output,
                                         primary_output_start,
                                         "pause startproc finis fp")) {
            CHECK(rc_console_device_submit(&console2, 27));
            primary_output_start = primary_output.count;
            stage = 2;
        } else if (stage == 2U &&
                   output_contains_after(&primary_output,
                                         primary_output_start,
                                         "att ")) {
            CHECK(submit_console_text(&console2, "s\r"));
            primary_output_start = primary_output.count;
            stage = 3;
        } else if (stage == 3U &&
                   output_contains_after(&primary_output,
                                         primary_output_start,
                                         "s\r\n")) {
            CHECK(submit_console_text(
                &console2, "new telproc size 100000 run\r"));
            primary_output_start = primary_output.count;
            stage = 4;
        } else if (stage == 4U &&
                   output_contains_after(&primary_output,
                                         primary_output_start,
                                         "to telproc")) {
            rc_console_device_attention(&console4);
            secondary_output_start = secondary_output.count;
            stage = 5;
        } else if (stage == 5U &&
                   output_contains_after(&secondary_output,
                                         secondary_output_start,
                                         "att ")) {
            CHECK(submit_console_text(&console4, "s\r"));
            secondary_output_start = secondary_output.count;
            stage = 6;
        } else if (stage == 6U &&
                   output_contains_after(&secondary_output,
                                         secondary_output_start,
                                         "s\r\n")) {
            CHECK(submit_console_text(
                &console4, "new remote size 100000 run\r"));
            secondary_output_start = secondary_output.count;
            stage = 7;
        } else if (stage == 7U &&
                   output_contains_after(&secondary_output,
                                         secondary_output_start,
                                         "to remote")) {
            CHECK(submit_console_text(&console4, "geo\r"));
            secondary_output_start = secondary_output.count;
            stage = 8;
        } else if (stage == 8U &&
                   output_contains_after(&secondary_output,
                                         secondary_output_start,
                                         "dansk/English ?")) {
            CHECK(submit_console_text(&console4, "e\r"));
            secondary_output_start = secondary_output.count;
            stage = 9;
        } else if (stage == 9U &&
                   output_contains_after(&secondary_output,
                                         secondary_output_start,
                                         "ARE YOU READY?")) {
            stage = 10;
        }
    }
    CHECK(result == RC_STEP_OK);
    CHECK(stage == 10U);
    CHECK(calls < UINT64_C(30000000));
    CHECK(!output_contains_after(&primary_output, 0, "key trouble"));
    CHECK(!output_contains_after(&secondary_output, 0, "key trouble"));
    CHECK(disk.read_operations > 0);
    CHECK(disk.write_operations > 0);

    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);
    CHECK(rc_image_inspect(image_path, &after, error, sizeof(error)) == 0);
    CHECK(after.byte_size == before.byte_size);
    CHECK(after.checksum_fnv1a == before.checksum_fnv1a);
}

static void test_monitor_program_checkpoint(bool print_output,
                                            bool host_exchange,
                                            bool assemble_source)
{
    static const char monitor_path[] =
        SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    static const char image_path[] =
        SIM4000_SOURCE_DIR "/src/disks/UNIT0";
    rc_machine_t machine;
    rc_clock_device_t clock;
    rc_console_device_t console2;
    rc_console_device_t console4;
    rc_rc433x_device_t disk;
    rc_hostfiles_device_t host;
    char host_root[] = "/tmp/sim4000-host-checkpoint-XXXXXX";
    char host_path[256];
    static const char host_text[] = "host file exchange\nsecond line\n";
    rc_image_info_t before;
    rc_image_info_t after;
    output_capture_t output = {{0}, 0};
    rc_addr_t load_address = 0;
    rc_addr_t entry_address = 0;
    rc_step_result_t result = RC_STEP_OK;
    uint64_t calls;
    unsigned stage = 0;
    unsigned executed = 0;
    rc_addr_t program_address = 0;
    size_t stage_output_start = 0;
    char error[256];

    CHECK(rc_image_inspect(image_path, &before, error, sizeof(error)) == 0);
    CHECK(rc_machine_init(&machine, 1048576));
    CHECK(rc_monitor_load(monitor_path, &machine.core, true, &load_address,
                          &entry_address, error, sizeof(error)) == 0);
    rc_clock_device_init(&clock, 11);
    rc_console_device_init(&console2, 6, 3);
    rc_console_device_init(&console4, 20, 19);
    rc_console_device_set_output(&console2, capture_console_output, &output);
    rc_console_device_set_output(&console4, capture_console_output, &output);
    CHECK(rc_rc433x_device_init_cow(&disk, &machine, image_path, error,
                                    sizeof(error)) == 0);
    CHECK(rc_machine_attach_device(&machine, 2, &console2.device));
    CHECK(rc_machine_attach_device(&machine, 3, &clock.device));
    CHECK(rc_machine_attach_device(&machine, 4, &console4.device));
    CHECK(rc_machine_attach_device(&machine, 15, &disk.device));
    if (host_exchange) {
        FILE *file;
        CHECK(mkdtemp(host_root) != NULL);
        (void)snprintf(host_path, sizeof(host_path), "%s/hfinput", host_root);
        file = fopen(host_path, "wb");
        CHECK(file != NULL);
        if (assemble_source) {
            FILE *source = fopen(SIM4000_SOURCE_DIR
                                  "/tests/fixtures/slang_integer.sl", "rb");
            unsigned char bytes[512];
            size_t count;
            CHECK(source != NULL);
            if (source != NULL && file != NULL) {
                while ((count = fread(bytes, 1, sizeof(bytes), source)) != 0) {
                    CHECK(fwrite(bytes, 1, count, file) == count);
                }
                CHECK(!ferror(source));
            }
            if (source != NULL) CHECK(fclose(source) == 0);
        } else if (file != NULL) {
            CHECK(fwrite(host_text, 1, sizeof(host_text) - 1U, file) ==
                  sizeof(host_text) - 1U);
        }
        if (file != NULL) CHECK(fclose(file) == 0);
        CHECK(rc_hostfiles_device_init(&host, &machine, host_root, 21,
                                       error, sizeof(error)) == 0);
        CHECK(rc_machine_attach_device(&machine, 63, &host.device));
    }
    machine.cpu.instruction_counter = entry_address;
    machine.cpu.stopped = false;

    for (calls = 0; calls < UINT64_C(50000000) && result == RC_STEP_OK &&
                    stage < (assemble_source ? 9U : host_exchange ? 7U : 6U);
                    ++calls) {
        if (calls != 0 && calls % UINT64_C(10000) == 0) {
            rc_machine_advance_time(&machine, 1);
        }
        result = rc_machine_step(&machine);
        if (stage == 0U &&
            output_contains_after(&output, stage_output_start,
                                  "initialize date:")) {
            CHECK(submit_console_text(&console2, "86 09 30 08 00 00\r"));
            stage_output_start = output.count;
            stage = 1;
        } else if (stage == 1U &&
                   output_contains_after(&output, stage_output_start,
                                         "pause startproc finis fp")) {
            CHECK(rc_console_device_submit(&console2, 27));
            stage_output_start = output.count;
            stage = 2;
        } else if (stage == 2U &&
                   output_contains_after(&output, stage_output_start,
                                         "att ")) {
            CHECK(submit_console_text(&console2, "s\r"));
            stage_output_start = output.count;
            stage = 3;
        } else if (stage == 3U &&
                   output_contains_after(&output, stage_output_start,
                                         "s\r\n")) {
            CHECK(submit_console_text(
                &console2, "new sltest size 100000 run\r"));
            stage_output_start = output.count;
            stage = 4;
        } else if (stage == 4U &&
                   output_contains_after(&output, stage_output_start,
                                         "to sltest")) {
            CHECK(submit_console_text(&console2, host_exchange
                ? "hfsource=fromhost hfinput\r" : "slang missing\r"));
            stage_output_start = output.count;
            stage = 5;
        } else if (stage == 5U &&
                   output_contains_after(&output, stage_output_start,
                                         host_exchange ? " bytes\r\n" :
                                         "***slang sorry 1 0/0/0")) {
            if (host_exchange) {
                CHECK(submit_console_text(&console2,
                    assemble_source ? "hfprog=slang hfsource\r" :
                                      "hfoutput=tohost hfsource\r"));
                stage_output_start = output.count;
            }
            stage = 6;
        } else if (host_exchange && stage == 6U &&
                   output_contains_after(&output, stage_output_start,
                        assemble_source ? "slang ok" : " bytes\r\n")) {
            if (assemble_source) {
                CHECK(submit_console_text(&console2,
                                          "hfoutput=tohost conv.no hfprog\r"));
                stage_output_start = output.count;
            }
            stage = 7;
        } else if (assemble_source && stage == 7U &&
                   output_contains_after(&output, stage_output_start,
                                         " bytes\r\n")) {
            CHECK(submit_console_text(&console2, "hfprog\r"));
            stage_output_start = output.count;
            stage = 8;
        } else if (assemble_source && stage == 8U) {
            static const rc_word_t expected[] = {
                UINT32_C(0x2c007b), UINT32_C(0x2d0141),
                UINT32_C(0x1c0002), UINT32_C(0x348000)
            };
            if (executed == 0 && machine.last_instruction.raw == expected[0]) {
                size_t i;
                bool match = true;
                for (i = 1; i < sizeof(expected) / sizeof(expected[0]); ++i) {
                    rc_word_t word = 0;
                    if (!rc_machine_read_word(&machine,
                            machine.last_instruction_address + (rc_addr_t)i * 2U,
                            &word) || word != expected[i]) {
                        match = false;
                    }
                }
                if (match) {
                    program_address = machine.last_instruction_address;
                    executed = 1;
                }
            } else if (executed != 0 &&
                       machine.last_instruction_address ==
                           program_address + executed * 2U &&
                       machine.last_instruction.raw == expected[executed]) {
                ++executed;
                if (executed == 4U) {
                    CHECK(machine.cpu.w[0] == 444);
                    CHECK(machine.cpu.w[1] == 321);
                    CHECK(machine.cpu.instruction_counter ==
                          program_address + 6U);
                    stage = 9;
                }
            }
        }
    }
    if (print_output) {
        (void)fwrite(output.bytes, 1, output.count, stdout);
        (void)printf("\n%s-checkpoint: stage=%u calls=%" PRIu64
                     " result=%d reads=%" PRIu64 " writes=%" PRIu64 "\n",
                     assemble_source ? "slang-build" :
                         host_exchange ? "hostfiles" : "slang",
                     stage, calls, (int)result, disk.read_operations,
                     disk.write_operations);
    }
    CHECK(result == RC_STEP_OK);
    CHECK(stage == (assemble_source ? 9U : host_exchange ? 7U : 6U));
    CHECK(calls < UINT64_C(50000000));
    if (assemble_source) {
        CHECK(executed == 4U);
        CHECK(output_contains_after(&output, 0, "slang ok 1/12/1"));
        CHECK(!output_contains_after(&output, 0, "***slang"));
    }
    if (!host_exchange) {
        CHECK(output_contains_after(&output, 0, "slang missing"));
        CHECK(output_contains_after(&output, 0, "source unknown"));
        CHECK(output_contains_after(&output, 0, "***slang sorry 1 0/0/0"));
    } else {
        FILE *file;
        char text[128] = {0};
        rc_hostfiles_device_destroy(&host);
        (void)snprintf(host_path, sizeof(host_path), "%s/hfoutput", host_root);
        file = fopen(host_path, "rb");
        CHECK(file != NULL);
        if (file != NULL) {
            size_t count = fread(text, 1, sizeof(text), file);
            if (assemble_source) {
                /* Independent bytes: two reserved words followed by AL W0,
                 * AL W1, WA W0 through the W1/core alias, and relative JL. */
                static const unsigned char expected[] = {
                    0, 0, 0, 0, 0, 0,
                    0x2c, 0x00, 0x7b, 0x2d, 0x01, 0x41,
                    0x1c, 0x00, 0x02, 0x34, 0x80, 0x00
                };
                if (print_output) {
                    size_t i;
                    (void)printf("assembled program prefix:");
                    for (i = 0; i < count && i < sizeof(expected); ++i) {
                        (void)printf(" %02x", (unsigned char)text[i]);
                    }
                    (void)putchar('\n');
                }
                CHECK(count >= sizeof(expected));
                CHECK(memcmp(text, expected, sizeof(expected)) == 0);
                CHECK(fseek(file, 0, SEEK_END) == 0);
                CHECK(ftell(file) == 768);
            } else {
                CHECK(count == sizeof(host_text) - 1U);
                CHECK(memcmp(text, host_text, sizeof(host_text) - 1U) == 0);
            }
            CHECK(fclose(file) == 0);
            CHECK(unlink(host_path) == 0);
        }
        (void)snprintf(host_path, sizeof(host_path), "%s/hfinput", host_root);
        CHECK(unlink(host_path) == 0);
        CHECK(rmdir(host_root) == 0);
    }
    CHECK(!output_contains_after(&output, 0, "key trouble"));
    CHECK(disk.read_operations > 0);
    CHECK(disk.write_operations > 0);

    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);
    CHECK(rc_image_inspect(image_path, &after, error, sizeof(error)) == 0);
    CHECK(after.byte_size == before.byte_size);
    CHECK(after.checksum_fnv1a == before.checksum_fnv1a);
}

static void test_monitor_slang_checkpoint(bool print_output)
{
    test_monitor_program_checkpoint(print_output, false, false);
}

typedef struct {
    uint64_t sequence;
    rc_addr_t instruction_address;
    rc_addr_t operand_address;
    uint8_t opcode;
    uint8_t result_register;
    rc_word_t left_first;
    rc_word_t left_second;
    rc_word_t right_first;
    rc_word_t right_second;
    rc_word_t result_first;
    rc_word_t result_second;
} floating_trace_entry_t;

static int64_t reference_shift_right_one(int64_t value)
{
    if (value >= 0 || value % 2 == 0) {
        return value / 2;
    }
    return value / 2 - 1;
}

static int64_t reference_clear_guard_bits(int64_t value)
{
    int64_t remainder = value % 4;

    if (remainder < 0) {
        remainder += 4;
    }
    return value - remainder;
}

static void reference_normalize_and_round(int64_t *fraction,
                                          int32_t *exponent)
{
again:
    if (*fraction >= (INT64_C(1) << 37) ||
        *fraction < -(INT64_C(1) << 37)) {
        *fraction = reference_shift_right_one(*fraction);
        ++*exponent;
    } else if (*fraction == 0) {
        *exponent = -2048;
    } else {
        while ((*fraction > 0 && *fraction < (INT64_C(1) << 36)) ||
               (*fraction < 0 && *fraction >= -(INT64_C(1) << 36))) {
            *fraction *= 2;
            --*exponent;
        }
    }
    if (((uint64_t)*fraction & UINT64_C(2)) != 0) {
        *fraction = reference_clear_guard_bits(*fraction + 4);
        goto again;
    }
    *fraction = reference_clear_guard_bits(*fraction);
}

static uint64_t reference_shift_right_bits(uint64_t value, unsigned count)
{
    if (count == 0U) {
        return value;
    }
    if ((value & (UINT64_C(1) << 63)) != 0) {
        return (value >> count) | (~UINT64_C(0) << (64U - count));
    }
    return value >> count;
}

static int64_t reference_guard_from_bits(uint64_t value)
{
    const uint64_t mask = (UINT64_C(1) << 38) - 1U;
    uint64_t guard = (value >> 26) & mask;

    if ((guard & (UINT64_C(1) << 37)) != 0) {
        return -(int64_t)(((mask + 1U) - guard) & mask);
    }
    return (int64_t)guard;
}

static rc_float_t reference_historical_add(rc_float_t left,
                                           rc_float_t right,
                                           bool subtract)
{
    const uint64_t sign = UINT64_C(1) << 63;
    int32_t difference = left.exponent - right.exponent;
    int32_t exponent;
    uint64_t left_bits;
    uint64_t right_bits;
    uint64_t result_bits;
    bool overflow;
    int64_t fraction;

    if (difference >= 38) {
        return left;
    }
    if (difference <= -38 && !subtract) {
        return right;
    }
    left_bits = (uint64_t)left.fraction << 28;
    right_bits = (uint64_t)right.fraction << 28;
    if (difference <= -38) {
        left_bits = 0;
        exponent = right.exponent;
    } else if (difference > 0) {
        right_bits = reference_shift_right_bits(right_bits,
                                                (unsigned)difference);
        exponent = left.exponent;
    } else if (difference < 0) {
        left_bits = reference_shift_right_bits(left_bits,
                                               (unsigned)(-difference));
        exponent = right.exponent;
    } else {
        exponent = left.exponent;
    }
    result_bits = subtract ? left_bits - right_bits
                           : left_bits + right_bits;
    if (subtract) {
        overflow = ((left_bits ^ right_bits) &
                    (left_bits ^ result_bits) & sign) != 0;
    } else {
        overflow = (~(left_bits ^ right_bits) &
                    (left_bits ^ result_bits) & sign) != 0;
    }
    if (overflow) {
        bool negative = (left_bits & sign) != 0;

        result_bits >>= 1;
        if (negative) {
            result_bits |= sign;
        }
        ++exponent;
    }
    fraction = reference_guard_from_bits(result_bits);
    reference_normalize_and_round(&fraction, &exponent);
    return (rc_float_t){fraction / 4, exponent};
}

static void test_monitor_regnetest_checkpoint(bool print_trace)
{
    static const char monitor_path[] = SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    static const char image_path[] = SIM4000_SOURCE_DIR "/src/disks/UNIT0";
    rc_machine_t machine;
    rc_clock_device_t clock;
    rc_console_device_t console2;
    rc_console_device_t console4;
    rc_rc433x_device_t disk;
    rc_image_info_t before;
    rc_image_info_t after;
    output_capture_t output = {{0}, 0};
    rc_addr_t load_address = 0;
    rc_addr_t entry_address = 0;
    rc_step_result_t result = RC_STEP_OK;
    uint64_t floating_operations = 0;
    uint64_t low_precision_operations = 0;
    uint64_t historical_differences = 0;
    floating_trace_entry_t first_difference = {0};
    rc_word_t first_difference_reference[2] = {0, 0};
    floating_trace_entry_t historical_difference_entries[64];
    rc_word_t historical_difference_references[64][2];
    floating_trace_entry_t trace[4096];
    size_t trace_count = 0;
    size_t trace_next = 0;
    uint64_t calls;
    unsigned stage = 0;
    size_t stage_output_start = 0;
    char error[256];

    CHECK(rc_image_inspect(image_path, &before, error, sizeof(error)) == 0);
    CHECK(rc_machine_init(&machine, 1048576));
    CHECK(rc_monitor_load(monitor_path, &machine.core, true, &load_address,
                          &entry_address, error, sizeof(error)) == 0);
    rc_clock_device_init(&clock, 11);
    rc_console_device_init(&console2, 6, 3);
    rc_console_device_init(&console4, 20, 19);
    rc_console_device_set_output(&console2, capture_console_output, &output);
    rc_console_device_set_output(&console4, capture_console_output, &output);
    CHECK(rc_rc433x_device_init_cow(&disk, &machine, image_path, error,
                                    sizeof(error)) == 0);
    CHECK(rc_machine_attach_device(&machine, 2, &console2.device));
    CHECK(rc_machine_attach_device(&machine, 3, &clock.device));
    CHECK(rc_machine_attach_device(&machine, 4, &console4.device));
    CHECK(rc_machine_attach_device(&machine, 15, &disk.device));
    machine.cpu.instruction_counter = entry_address;
    machine.cpu.stopped = false;

    for (calls = 0; calls < UINT64_C(500000000) && result == RC_STEP_OK &&
                    stage < 6U; ++calls) {
        uint64_t instructions_before = machine.instructions_executed;
        rc_word_t registers_before[4];

        if (print_trace) {
            memcpy(registers_before, machine.cpu.w,
                   sizeof(registers_before));
        }

        if (calls != 0 && calls % UINT64_C(10000) == 0) {
            rc_machine_advance_time(&machine, 1);
        }
        result = rc_machine_step(&machine);
        if (machine.instructions_executed != instructions_before &&
            (machine.last_instruction.opcode == 48U ||
             machine.last_instruction.opcode == 49U ||
             machine.last_instruction.opcode == 50U ||
             machine.last_instruction.opcode == 52U)) {
            ++floating_operations;
            if (machine.cpu.exception_21 != 0) {
                ++low_precision_operations;
            }
            if (print_trace) {
                floating_trace_entry_t *entry = &trace[trace_next];
                unsigned current = machine.last_instruction.result_register;
                unsigned previous = current == 0U ? 3U : current - 1U;
                rc_addr_t operand = machine.last_effective_address &
                                    ~UINT32_C(1);
                rc_addr_t operand_previous = operand == 0U
                    ? 6U : operand - 2U;

                entry->sequence = floating_operations;
                entry->instruction_address = machine.last_instruction_address;
                entry->operand_address = operand;
                entry->opcode = machine.last_instruction.opcode;
                entry->result_register = (uint8_t)current;
                entry->left_first = registers_before[previous];
                entry->left_second = registers_before[current];
                if (operand <= 6U) {
                    entry->right_first =
                        registers_before[(operand_previous / 2U) & 3U];
                    entry->right_second =
                        registers_before[(operand / 2U) & 3U];
                } else {
                    CHECK(rc_machine_read_word(&machine, operand_previous,
                                               &entry->right_first));
                    CHECK(rc_machine_read_word(&machine, operand,
                                               &entry->right_second));
                }
                entry->result_first = machine.cpu.w[previous];
                entry->result_second = machine.cpu.w[current];
                if (entry->opcode == 48U || entry->opcode == 49U) {
                    rc_float_t left = rc_float_unpack(entry->left_first,
                                                      entry->left_second);
                    rc_float_t right = rc_float_unpack(entry->right_first,
                                                       entry->right_second);
                    rc_float_t reference;
                    rc_word_t reference_first;
                    rc_word_t reference_second;

                    reference = reference_historical_add(
                        left, right, entry->opcode == 49U);
                    rc_float_pack(reference,
                                  machine.cpu.exception_21 != 0,
                                  &reference_first, &reference_second);
                    if (reference_first != entry->result_first ||
                        reference_second != entry->result_second) {
                        if (historical_differences == 0) {
                            first_difference = *entry;
                            first_difference_reference[0] = reference_first;
                            first_difference_reference[1] = reference_second;
                        }
                        if (historical_differences < 64U) {
                            size_t difference_index =
                                (size_t)historical_differences;

                            historical_difference_entries[difference_index] =
                                *entry;
                            historical_difference_references
                                [difference_index][0] = reference_first;
                            historical_difference_references
                                [difference_index][1] = reference_second;
                        }
                        ++historical_differences;
                    }
                }
                trace_next = (trace_next + 1U) %
                             (sizeof(trace) / sizeof(trace[0]));
                if (trace_count < sizeof(trace) / sizeof(trace[0])) {
                    ++trace_count;
                }
            }
        }
        if (stage == 0U &&
            output_contains_after(&output, stage_output_start,
                                  "initialize date:")) {
            CHECK(submit_console_text(&console2, "86 09 30 08 00 00\r"));
            stage_output_start = output.count;
            stage = 1;
        } else if (stage == 1U &&
                   output_contains_after(&output, stage_output_start,
                                         "pause startproc finis fp")) {
            CHECK(rc_console_device_submit(&console2, 27));
            stage_output_start = output.count;
            stage = 2;
        } else if (stage == 2U &&
                   output_contains_after(&output, stage_output_start,
                                         "att ")) {
            CHECK(submit_console_text(&console2, "s\r"));
            stage_output_start = output.count;
            stage = 3;
        } else if (stage == 3U &&
                   output_contains_after(&output, stage_output_start,
                                         "s\r\n")) {
            CHECK(submit_console_text(
                &console2, "new test size 100000 run\r"));
            stage_output_start = output.count;
            stage = 4;
        } else if (stage == 4U &&
                   output_contains_after(&output, stage_output_start,
                                         "to test")) {
            CHECK(submit_console_text(&console2, "i regnetest\r"));
            stage_output_start = output.count;
            stage = 5;
        } else if (stage == 5U &&
                   output_contains_after(&output, stage_output_start,
                                         "antal fejlruns")) {
            stage = 6;
        }
    }
    CHECK(result == RC_STEP_OK);
    CHECK(stage == 6U);
    CHECK(calls < UINT64_C(500000000));
    CHECK(output_contains_after(&output, 0,
                                "sum af tal= 360.700643456"));
    CHECK(output_contains_after(&output, 0,
                                "sum af tal=   1.85598980864'3 test ok"));
    CHECK(floating_operations > UINT64_C(9000000));
    CHECK(low_precision_operations == 0);

    if (print_trace) {
        size_t index;
        size_t first = (trace_next + sizeof(trace) / sizeof(trace[0]) -
                        trace_count) % (sizeof(trace) / sizeof(trace[0]));

        (void)printf("historical-fa-fs-differences=%" PRIu64,
                     historical_differences);
        if (historical_differences != 0) {
            (void)printf(",first-ic=%06" PRIx32
                         ",opcode=%u,left=%06" PRIx32 "%06" PRIx32
                         ",right=%06" PRIx32 "%06" PRIx32
                         ",actual=%06" PRIx32 "%06" PRIx32
                         ",reference=%06" PRIx32 "%06" PRIx32,
                         first_difference.instruction_address,
                         (unsigned)first_difference.opcode,
                         first_difference.left_first,
                         first_difference.left_second,
                         first_difference.right_first,
                         first_difference.right_second,
                         first_difference.result_first,
                         first_difference.result_second,
                         first_difference_reference[0],
                         first_difference_reference[1]);
        }
        (void)printf("\n");
        for (index = 0; index < historical_differences && index < 64U;
             ++index) {
            const floating_trace_entry_t *entry =
                &historical_difference_entries[index];

            (void)printf("historical-difference,%" PRIu64 ",%06" PRIx32
                         ",%u,%06" PRIx32 "%06" PRIx32
                         ",%06" PRIx32 "%06" PRIx32
                         ",%06" PRIx32 "%06" PRIx32
                         ",%06" PRIx32 "%06" PRIx32 "\n",
                         entry->sequence, entry->instruction_address,
                         (unsigned)entry->opcode,
                         entry->left_first, entry->left_second,
                         entry->right_first, entry->right_second,
                         entry->result_first, entry->result_second,
                         historical_difference_references[index][0],
                         historical_difference_references[index][1]);
        }
        (void)printf("sequence,ic,opcode,w,operand,left,right,result\n");
        for (index = 0; index < trace_count; ++index) {
            const floating_trace_entry_t *entry = &trace[
                (first + index) % (sizeof(trace) / sizeof(trace[0]))];

            (void)printf("%" PRIu64 ",%06" PRIx32 ",%u,%u,%06" PRIx32
                         ",%06" PRIx32 "%06" PRIx32
                         ",%06" PRIx32 "%06" PRIx32
                         ",%06" PRIx32 "%06" PRIx32 "\n",
                         entry->sequence, entry->instruction_address,
                         (unsigned)entry->opcode,
                         (unsigned)entry->result_register,
                         entry->operand_address,
                         entry->left_first, entry->left_second,
                         entry->right_first, entry->right_second,
                         entry->result_first, entry->result_second);
        }
    }

    rc_rc433x_device_destroy(&disk);
    rc_machine_destroy(&machine);
    CHECK(rc_image_inspect(image_path, &after, error, sizeof(error)) == 0);
    CHECK(after.byte_size == before.byte_size);
    CHECK(after.checksum_fnv1a == before.checksum_fnv1a);
}

static void test_throttle(void)
{
    rc_throttle_config_t config = {RC_THROTTLE_OFF, 0};
    rc_throttle_t throttle;
    rc_config_t parsed;
    rc_config_error_t error;
    char text[80];
    static const char *const invalid[] = {
        "", "0%", "101%", "0ips", "1000000001ips", "-1ips", "50",
        "1.5%", "999999999999999999999ips", "originalx", "off junk"
    };
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        CHECK(!rc_throttle_parse(invalid[index], &config));
    }
    CHECK(config.mode == RC_THROTTLE_OFF);
    CHECK(rc_throttle_parse("50%", &config));
    CHECK(config.mode == RC_THROTTLE_PERCENT && config.value == 50);
    rc_throttle_begin(&throttle, config, 0, 0, 0);
    CHECK(rc_throttle_delay(&throttle, 1000000, 1000000, 100) == 1000000);
    CHECK(rc_throttle_delay(&throttle, 2000000, 1000000, 100) == 0);
    CHECK(rc_throttle_parse("100%", &config));
    rc_throttle_begin(&throttle, config, 0, 0, 0);
    CHECK(rc_throttle_delay(&throttle, 1000000, 1000000, 100) == 0);
    CHECK(rc_throttle_parse("1000IPS", &config));
    CHECK(rc_throttle_quantum(config) == 5);
    rc_throttle_begin(&throttle, config, 0, 0, 0);
    CHECK(rc_throttle_delay(&throttle, 1000000, 1000000, 5) == 4000000);
    CHECK(rc_throttle_delay(&throttle, 5000000, 1000000, 5) == 0);
    CHECK(rc_throttle_delay(&throttle, 1000000000, 1000000, 5) == 0);
    CHECK(throttle.instructions_start == 5); /* Drop host-stall credit. */
    CHECK(rc_throttle_delay(&throttle, 1000000000, 1000000, 6) == 1000000);
    CHECK(rc_throttle_parse("original", &config));
    CHECK(config.value == 250000);
    CHECK(rc_throttle_quantum(config) == 1250);
    rc_throttle_begin(&throttle, config, 0, 0, 0);
    CHECK(rc_throttle_delay(&throttle, 0, 0, 250) == 1000000);
    rc_throttle_format(config, text, sizeof(text));
    CHECK(strstr(text, "approx.") != NULL);
    CHECK(rc_throttle_parse("1ips", &config));
    CHECK(rc_throttle_quantum(config) == 1);
    CHECK(rc_throttle_parse("1000000000ips", &config));
    CHECK(rc_throttle_quantum(config) == 10000);
    CHECK(rc_throttle_parse("OFF", &config));
    CHECK(rc_throttle_quantum(config) == 10000);
    rc_throttle_begin(&throttle, config, 0, 0, 0);
    CHECK(rc_throttle_delay(&throttle, 0, 1000000000, 1000000) == 0);
    CHECK(rc_config_parse_text("THROTTLE VALUE=original\n", &parsed, &error) == 0);
    CHECK(parsed.throttle.mode == RC_THROTTLE_ORIGINAL);
    CHECK(rc_config_parse_text("THROTTLE VALUE=10%\n", &parsed, &error) == 0);
    CHECK(parsed.throttle.value == 10);
    CHECK(rc_config_parse_text("THROTTLE VALUE=OFF\nTHROTTLE VALUE=50%\n",
                               &parsed, &error) != 0);
    CHECK(error.line == 2);
    CHECK(rc_config_parse_text("THROTTLE VALUE=101%\n", &parsed, &error) != 0);
    CHECK(rc_config_parse_text("THROTTLE RATE=50%\n", &parsed, &error) != 0);
    CHECK(rc_config_parse_text("", &parsed, &error) == 0);
    CHECK(parsed.throttle.mode == RC_THROTTLE_OFF);
}

static void test_config_text(void)
{
    static const char input[] =
        "; comment\n"
        "CORESIZE=0,1048576\n"
        "MONITOR FILE=MONITOR\n"
        "OPERATOR ESCAPE=CTRL-E\n"
        "OPERATOR DATE=ALT-D\n"
        "INPUT DEL=CTRL-H\n"
        "DATE YEAROFFSET=-40\n"
        "DEFINE READER Devno=0 Intno=9 File=bootstrap.pt\n"
        "DEFINE CONSOLE Devno=2 Intno=6,3\n"
        "TELNET DEVNO=2 PORT=4002 ATTENTION=CTRL-A\n"
        "DEFINE RC433X Devno=15 File=UNIT0 Overlay=UNIT0.overlay\n";
    rc_config_error_t error;
    rc_config_t config;

    CHECK(rc_config_parse_text(input, &config, &error) == 0);
    CHECK(config.core_min_halfwords == 16384);
    CHECK(config.core_max_halfwords == 1048576);
    CHECK(config.operator_escape == UINT8_C(0x05));
    CHECK(config.operator_date_key == (uint8_t)'D');
    CHECK(config.console_delete_mapping == UINT8_C(0x08));
    CHECK(config.date_year_offset == -40);
    CHECK(config.device_count == 3);
    CHECK(config.telnet_endpoint_count == 1);
    CHECK(config.telnet_endpoints[0].device_number == 2);
    CHECK(config.telnet_endpoints[0].port == 4002);
    CHECK(strcmp(config.telnet_endpoints[0].address, "127.0.0.1") == 0);
    CHECK(config.telnet_endpoints[0].attention_mode ==
          RC_TELNET_ATTENTION_CHARACTER);
    CHECK(config.telnet_endpoints[0].attention_character == UINT8_C(0x01));
    CHECK(strcmp(config.monitor_path, "MONITOR") == 0);
    CHECK(strcmp(config.devices[0].path, "bootstrap.pt") == 0);
    CHECK(config.devices[1].interrupt_count == 2);
    CHECK(config.devices[1].interrupts[1] == 3);
    CHECK(strcmp(config.devices[2].path, "UNIT0") == 0);
    CHECK(strcmp(config.devices[2].overlay_path, "UNIT0.overlay") == 0);
    CHECK(!config.devices[2].writable);

    CHECK(rc_config_parse_text(
              "DEFINE RC433X DEVNO=15 FILE=UNIT0.work WRITABLE=YES\n",
              &config, &error) == 0);
    CHECK(config.devices[0].writable);
    CHECK(rc_config_parse_text(
              "DEFINE RC433X DEVNO=15 FILE=UNIT0 WRITABLE=MAYBE\n",
              &config, &error) != 0);
    CHECK(rc_config_parse_text(
              "DEFINE RC433X DEVNO=15 FILE=UNIT0 WRITABLE=YES "
              "OVERLAY=UNIT0.overlay\n",
              &config, &error) != 0);

    CHECK(rc_config_parse_text("OPERATOR ESCAPE=CTRL-_\n", &config,
                               &error) == 0);
    CHECK(config.operator_escape == UINT8_C(0x1f));
    CHECK(rc_config_parse_text("OPERATOR ESCAPE=NONE\n", &config,
                               &error) == 0);
    CHECK(config.operator_escape == RC_CONFIG_OPERATOR_ESCAPE_DISABLED);
    CHECK(rc_config_parse_text("OPERATOR DATE=ALT-X\n", &config,
                               &error) == 0);
    CHECK(config.operator_date_key == (uint8_t)'X');
    CHECK(rc_config_parse_text("OPERATOR DATE=NONE\n", &config,
                               &error) == 0);
    CHECK(config.operator_date_key == RC_CONFIG_SHORTCUT_DISABLED);
    CHECK(rc_config_parse_text("INPUT DEL=NONE\n", &config, &error) == 0);
    CHECK(config.console_delete_mapping == RC_CONFIG_SHORTCUT_DISABLED);
    CHECK(rc_config_parse_text(
              "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
              "TELNET DEVNO=4 PORT=4004 ATTENTION=CONNECT\n",
              &config, &error) == 0);
    CHECK(config.telnet_endpoints[0].attention_mode ==
          RC_TELNET_ATTENTION_CONNECT);
    CHECK(rc_config_parse_text(
              "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
              "TELNET DEVNO=4 PORT=4004 ATTENTION=NONE\n",
              &config, &error) == 0);
    CHECK(config.telnet_endpoints[0].attention_mode ==
          RC_TELNET_ATTENTION_NONE);
    CHECK(rc_config_parse_text(
              "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
              "TELNET DEVNO=4 PORT=4004\n",
              &config, &error) == 0);
    CHECK(config.telnet_endpoints[0].attention_mode ==
          RC_TELNET_ATTENTION_CHARACTER);
    CHECK(config.telnet_endpoints[0].attention_character == UINT8_C(0x01));
}

static void test_config_errors(void)
{
    static const char duplicate[] =
        "DEFINE READER Devno=0 Intno=9\n"
        "DEFINE PUNCH Devno=0 Intno=12\n";
    rc_config_error_t error;
    rc_config_t config;

    CHECK(rc_config_parse_text(duplicate, &config, &error) != 0);
    CHECK(error.line == 2);
    CHECK(rc_config_parse_text("OPERATOR ESCAPE=ALT-E\n", &config,
                               &error) != 0);
    CHECK(error.line == 1);
    CHECK(rc_config_parse_text("OPERATOR DATE=CTRL-D\n", &config,
                               &error) != 0);
    CHECK(error.line == 1);
    CHECK(rc_config_parse_text("INPUT DEL=ALT-H\n", &config,
                               &error) != 0);
    CHECK(error.line == 1);
    CHECK(rc_config_parse_text("DATE YEAROFFSET=forty\n", &config,
                               &error) != 0);
    CHECK(error.line == 1);
    CHECK(rc_config_parse_text("TELNET DEVNO=4 PORT=4004\n", &config,
                               &error) != 0);
    CHECK(error.line == 1);
    CHECK(rc_config_parse_text(
              "DEFINE CLOCK DEVNO=3 INTNO=11\n"
              "TELNET DEVNO=3 PORT=4003\n",
              &config, &error) != 0);
    CHECK(error.line == 2);
    CHECK(rc_config_parse_text(
              "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
              "TELNET DEVNO=4 PORT=70000\n",
              &config, &error) != 0);
    CHECK(error.line == 2);
    CHECK(rc_config_parse_text(
              "DEFINE CONSOLE DEVNO=4 INTNO=20,19\n"
              "TELNET DEVNO=4 PORT=4004 ATTENTION=ALT-A\n",
              &config, &error) != 0);
    CHECK(error.line == 2);
    CHECK(rc_config_parse_text(
              "DEFINE CLOCK DEVNO=3 INTNO=11 OVERLAY=clock.overlay\n",
              &config, &error) != 0);
    CHECK(error.line == 1);
}

static void test_telnet_terminal_translation(void)
{
    static const uint8_t input[] = {
        'a', 0x1b, '[', 'A', 0x1b, '[', '3', '~', '\r', '\n'
    };
    rc_telnet_endpoint_t endpoint;
    rc_console_device_t console;

    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.client_fd = 1;
    endpoint.terminal_kind = RC_TELNET_TERMINAL_CONSOLE;
    rc_telnet_endpoint_output(&endpoint, '\r');
    rc_telnet_endpoint_output(&endpoint, '\n');
    CHECK(endpoint.output_count == 2);
    CHECK(endpoint.output[0] == '\r');
    CHECK(endpoint.output[1] == '\n');

    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.client_fd = 1;
    endpoint.terminal_kind = RC_TELNET_TERMINAL_RC851;
    endpoint.rc851_row = 1;
    endpoint.rc851_column = 1;
    rc_telnet_endpoint_output(&endpoint, 6);
    rc_telnet_endpoint_output(&endpoint, 96);
    rc_telnet_endpoint_output(&endpoint, 96);
    CHECK(endpoint.output_count == 6);
    CHECK(memcmp(endpoint.output, "\033[1;1H", 6) == 0);

    endpoint.output_head = 0;
    endpoint.output_count = 0;
    rc_telnet_endpoint_output(&endpoint, 12);
    CHECK(endpoint.output_count == 7);
    CHECK(memcmp(endpoint.output, "\033[2J\033[H", 7) == 0);
    CHECK(endpoint.rc851_row == 1);
    CHECK(endpoint.rc851_column == 1);

    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.delete_mapping = UINT8_C(0x08);
    rc_telnet_endpoint_receive_bytes(&endpoint, input, sizeof(input));
    CHECK(endpoint.input_count == 3);
    CHECK(endpoint.input[0] == (uint8_t)'a');
    CHECK(endpoint.input[1] == UINT8_C(0x08));
    CHECK(endpoint.input[2] == UINT8_C('\r'));

    rc_console_device_init(&console, 20, 19);
    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.console = &console;
    endpoint.attention_mode = RC_TELNET_ATTENTION_CHARACTER;
    endpoint.attention_character = UINT8_C(0x01);
    {
        static const uint8_t control_attention[] = {1, 'x'};

        rc_telnet_endpoint_receive_bytes(&endpoint, control_attention,
                                         sizeof(control_attention));
    }
    CHECK(console.pending_interrupts != 0);
    CHECK(endpoint.attention_requests == 1);
    CHECK(endpoint.input_count == 1);
    CHECK(endpoint.input[0] == (uint8_t)'x');
    console.pending_interrupts = 0;
    {
        static const uint8_t telnet_break[] = {255, 243};

        rc_telnet_endpoint_receive_bytes(&endpoint, telnet_break,
                                         sizeof(telnet_break));
    }
    CHECK(console.pending_interrupts != 0);
    CHECK(endpoint.attention_requests == 2);

    rc_console_device_init(&console, 20, 19);
    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.console = &console;
    endpoint.attention_mode = RC_TELNET_ATTENTION_NONE;
    {
        static const uint8_t disabled_attention[] = {1};

        rc_telnet_endpoint_receive_bytes(&endpoint, disabled_attention,
                                         sizeof(disabled_attention));
    }
    CHECK(console.pending_interrupts == 0);
    CHECK(endpoint.input_count == 1);
    CHECK(endpoint.input[0] == UINT8_C(0x01));
}

static void test_telnet_connect_attention(void)
{
    rc_telnet_endpoint_t endpoint;
    rc_console_device_t console;

    rc_console_device_init(&console, 20, 19);
    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.console = &console;
    endpoint.attention_mode = RC_TELNET_ATTENTION_CONNECT;
    endpoint.telnet_state = 99;
    endpoint.ansi_input_state = 99;
    endpoint.input_pending_cr = true;
    endpoint.output_pending_cr = true;
    rc_telnet_endpoint_begin_session(&endpoint);
    CHECK(endpoint.telnet_state == 0);
    CHECK(endpoint.ansi_input_state == 0);
    CHECK(!endpoint.input_pending_cr);
    CHECK(!endpoint.output_pending_cr);
    CHECK(endpoint.attention_requests == 1);
    CHECK(console.pending_interrupts != 0);
}

static void test_shipped_files(void)
{
    static const char config_path[] =
        SIM4000_SOURCE_DIR "/src/config/RC4000.cfg";
    static const char monitor_path[] =
        SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    static const char image_path[] =
        SIM4000_SOURCE_DIR "/src/disks/UNIT0";
    rc_config_error_t config_error;
    char error[256];
    rc_config_t config;
    rc_monitor_info_t monitor;
    rc_image_info_t image;
    FILE *config_file;
    int character;

    CHECK(rc_config_parse_file(config_path, &config, &config_error) == 0);
    CHECK(config.device_count == 8);
    CHECK(config.devices[7].kind == RC_DEVICE_HOSTFILES);
    CHECK(config.devices[7].device_number == 63);
    CHECK(config.devices[7].interrupts[0] == 21);
    CHECK(strcmp(config.devices[7].path, "exchange") == 0);
    CHECK(config.devices[3].kind == RC_DEVICE_RC610);
    CHECK(config.devices[3].device_number == 7);
    CHECK(config.devices[3].interrupts[0] == 10);
    CHECK(config.devices[3].append);
    CHECK(strcmp(config.devices[3].path, "printer.txt") == 0);
    CHECK(config.devices[5].kind == RC_DEVICE_READER);
    CHECK(config.devices[5].device_number == 0);
    CHECK(config.devices[5].interrupt_count == 1);
    CHECK(config.devices[5].interrupts[0] == 9);
    CHECK(config.devices[6].kind == RC_DEVICE_PUNCH);
    CHECK(config.devices[6].device_number == 1);
    CHECK(config.devices[6].interrupt_count == 1);
    CHECK(config.devices[6].interrupts[0] == 12);
    CHECK(strcmp(config.monitor_path, "MONITOR") == 0);
    CHECK(config.core_max_halfwords == 1048576);
    CHECK(config.operator_escape == UINT8_C(0x05));
    CHECK(config.operator_date_key == (uint8_t)'D');
    CHECK(config.console_delete_mapping == UINT8_C(0x08));
    CHECK(config.date_year_offset == -40);
    config_file = fopen(config_path, "rb");
    CHECK(config_file != NULL);
    if (config_file != NULL) {
        while ((character = fgetc(config_file)) != EOF) {
            CHECK(character != '\r');
        }
        CHECK(ferror(config_file) == 0);
        CHECK(fclose(config_file) == 0);
    }
    CHECK(rc_monitor_inspect(monitor_path, &monitor, error, sizeof(error)) == 0);
    CHECK(monitor.byte_size == UINT64_C(38400));
    CHECK(monitor.word_count == UINT64_C(12800));
    CHECK(monitor.trailing_bytes == 0);
    CHECK(rc_image_inspect(image_path, &image, error, sizeof(error)) == 0);
    CHECK(image.byte_size == UINT64_C(4660224));
    CHECK(image.complete_sectors == UINT64_C(6068));
    CHECK(image.trailing_bytes == 0);
}

static void test_shipped_disk_catalog(void)
{
    static const char image_path[] = SIM4000_SOURCE_DIR "/src/disks/UNIT0.original";
    const rc_catalog_entry_t *entry;
    rc_catalog_t catalog;
    char error[256];

    CHECK(rc_catalog_read(image_path, &catalog, error, sizeof(error)) == 0);
    CHECK(strcmp(catalog.name, "catdisc") == 0);
    CHECK(strcmp(catalog.document, "disc") == 0);
    CHECK(catalog.segment_count == 36);
    CHECK(catalog.slice_length == 3);
    CHECK(catalog.first_slice == 2);
    CHECK(catalog.last_slice == 2029);
    CHECK(catalog.entry_count == 415);
    CHECK(rc_catalog_find(&catalog, "catalog") != NULL);
    CHECK(rc_catalog_find(&catalog, "startarea") != NULL);
    entry = rc_catalog_find(&catalog, "slang");
    CHECK(entry != NULL);
    if (entry != NULL) {
        CHECK(entry->catalog_segment == 26);
        CHECK(entry->slot == 7);
        CHECK(entry->first_slice == 818);
        CHECK(entry->size_segments == 14);
        CHECK(strcmp(entry->document, "wrk000011") == 0);
        CHECK(entry->contents == 2);
        CHECK(entry->entry == 4);
    }
    CHECK(rc_catalog_find(&catalog, "slangxref") != NULL);
    rc_catalog_destroy(&catalog);
    CHECK(rc_catalog_read(SIM4000_SOURCE_DIR "/src/disks/UNIT0",
                           &catalog, error, sizeof(error)) == 0);
    entry = rc_catalog_find(&catalog, "fromhost");
    CHECK(entry != NULL);
    if (entry != NULL) {
        CHECK(entry->size_segments == 55);
        CHECK(entry->permission_key == 3);
        CHECK(entry->contents == 2);
    }
    entry = rc_catalog_find(&catalog, "tohost");
    CHECK(entry != NULL);
    if (entry != NULL) {
        CHECK(entry->size_segments == 47);
        CHECK(entry->permission_key == 3);
        CHECK(entry->contents == 2);
    }
    CHECK(rc_catalog_find(&catalog, "fromdos") == NULL);
    CHECK(rc_catalog_find(&catalog, "todos") == NULL);
    rc_catalog_destroy(&catalog);
    CHECK(rc_catalog_read_main(SIM4000_SOURCE_DIR "/src/disks/UNIT0",
                               &catalog, error, sizeof(error)) == 0);
    CHECK(strcmp(catalog.name, "catalog") == 0);
    CHECK(catalog.segment_count == 72);
    CHECK(catalog.entry_count == 418);
    CHECK(catalog.first_slice == 964);
    CHECK(rc_catalog_find(&catalog, "catdisc") != NULL);
    CHECK(rc_catalog_find(&catalog, "c") != NULL);
    CHECK(rc_catalog_find(&catalog, "v") != NULL);
    entry = rc_catalog_find(&catalog, "slang");
    CHECK(entry != NULL);
    if (entry != NULL) {
        CHECK(strcmp(entry->document, "disc") == 0);
        CHECK(entry->first_slice == 818);
    }
    CHECK(rc_catalog_find(&catalog, "fromhost") != NULL);
    CHECK(rc_catalog_find(&catalog, "tohost") != NULL);
    CHECK(rc_catalog_find(&catalog, "fromdos") == NULL);
    rc_catalog_destroy(&catalog);
}

static void test_monitor_load(void)
{
    static const char monitor_path[] =
        SIM4000_SOURCE_DIR "/src/monitor/MONITOR";
    rc_core_t core = {0};
    rc_addr_t load_address = 0;
    rc_addr_t entry_address = 0;
    rc_word_t first_word = 0;
    rc_word_t init_word = 0;
    rc_word_t start_word = 0;
    char error[256];

    CHECK(rc_core_init(&core, 1048576));
    CHECK(rc_monitor_load(monitor_path, &core, true, &load_address,
                          &entry_address, error, sizeof(error)) == 0);
    CHECK(load_address == 524288);
    CHECK(entry_address == 524294);
    CHECK(rc_core_read_word(&core, load_address, &first_word));
    CHECK(first_word == UINT32_C(0x000022));
    CHECK(rc_core_read_word(&core, load_address + 2, &init_word));
    CHECK(init_word == UINT32_C(2));
    CHECK(rc_core_read_word(&core, 14, &start_word));
    CHECK(start_word == entry_address);
    rc_core_destroy(&core);
}

int main(int argc, char **argv)
{
    int quick = 0;
    int regnetest = 0;
    int regnetest_trace = 0;
    int slangtest = 0;
    int hosttest = 0;
    int slangbuild = 0;

    if (argc == 2 && strcmp(argv[1], "--quick") == 0) {
        quick = 1;
    } else if (argc == 2 && strcmp(argv[1], "--regnetest") == 0) {
        regnetest = 1;
    } else if (argc == 2 && strcmp(argv[1], "--regnetest-trace") == 0) {
        regnetest = 1;
        regnetest_trace = 1;
    } else if (argc == 2 && strcmp(argv[1], "--slangtest") == 0) {
        slangtest = 1;
    } else if (argc == 2 && strcmp(argv[1], "--hosttest") == 0) {
        hosttest = 1;
    } else if (argc == 2 && strcmp(argv[1], "--slangbuild") == 0) {
        hosttest = 1;
        slangbuild = 1;
    } else if (argc != 1) {
        (void)fprintf(stderr,
                      "Usage: sim4000_tests "
                      "[--quick|--regnetest|--regnetest-trace|--slangtest|--hosttest|--slangbuild]\n");
        return EXIT_FAILURE;
    }
    if (regnetest) {
        test_monitor_regnetest_checkpoint(regnetest_trace != 0);
        if (failures != 0) {
            (void)fprintf(stderr, "%u test check(s) failed\n", failures);
            return EXIT_FAILURE;
        }
        (void)printf("all checks passed\n");
        return EXIT_SUCCESS;
    }
    if (slangtest || hosttest) {
        test_monitor_program_checkpoint(true, hosttest != 0, slangbuild != 0);
        if (failures != 0) {
            (void)fprintf(stderr, "%u test check(s) failed\n", failures);
            return EXIT_FAILURE;
        }
        (void)printf("all checks passed\n");
        return EXIT_SUCCESS;
    }
    test_types();
    test_core();
    test_architectural_trace();
    test_core_image();
    test_cpu_state();
    test_decode();
    test_machine_write_observer();
    test_machine_addressing();
    test_all_address_modes();
    test_maximum_architectural_address();
    test_missing_storage_boundary();
    test_config_core_limits();
    test_machine_register_aliases();
    test_machine_transfers_and_exceptions();
    test_machine_arithmetic_and_flow();
    test_machine_byte_arithmetic();
    test_machine_double_transfers();
    test_machine_multiply_and_divide();
    test_machine_double_arithmetic();
    test_floating_multiply_helpers();
    test_floating_divide_helpers();
    test_floating_add_helpers();
    test_floating_conversion_helpers();
    test_floating_multiply_instruction();
    test_floating_divide_instruction();
    test_floating_add_and_conversion_instructions();
    test_machine_shifts();
    test_machine_normalization();
    test_db_sx_and_nd_regressions();
    test_db_indirect_address_faults();
    test_protection_compatibility();
    test_interrupt_entry();
    test_interrupt_clear_instruction();
    test_interrupt_mask_load_instruction();
    test_exception_register_transfers();
    test_interrupt_register_stores();
    test_device_boundary();
    test_reader_device();
    test_punch_device();
    test_architectural_trace_vector();
    test_autoload_word();
    test_machine_control_panel();
    test_scheduled_clock();
    test_console_device();
    test_rc433x_readonly_device();
    test_rc433x_cow_device();
    test_rc433x_writable_device();
    test_rc433x_persistent_overlay();
    test_catalog_rejects_non_catalog_image();
    test_config_text();
    test_throttle();
    test_config_errors();
    test_telnet_terminal_translation();
    test_telnet_connect_attention();
    if (!quick) {
        test_shipped_files();
        test_shipped_disk_catalog();
        test_monitor_load();
        test_monitor_first_instruction();
        test_monitor_cpu_initialization_checkpoint();
        test_monitor_device_time_checkpoint();
        test_monitor_readonly_disk_checkpoint();
        test_monitor_cow_disk_checkpoint();
        test_monitor_secondary_console_workflow();
        test_monitor_slang_checkpoint(false);
        test_monitor_program_checkpoint(false, true, false);
        test_monitor_program_checkpoint(false, true, true);
    }

    if (failures != 0) {
        (void)fprintf(stderr, "%u test check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    (void)printf("all checks passed\n");
    return EXIT_SUCCESS;
}
