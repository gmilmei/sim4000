#define _POSIX_C_SOURCE 200809L

#include "sim4000/core.h"
#include "sim4000/core_image.h"
#include "sim4000/config.h"
#include "sim4000/cpu.h"
#include "sim4000/machine.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static unsigned failures;

#define CHECK(condition) do {                                                \
    if (!(condition)) {                                                      \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                      __FILE__, __LINE__, #condition);                       \
        ++failures;                                                          \
    }                                                                        \
} while (0)

static void core_keys(void)
{
    rc_core_t core = {0};
    uint8_t key = 99;
    rc_word_t word = 0;

    if (!rc_core_init(&core, 17)) {
        CHECK(false);
        return;
    }
    for (rc_addr_t address = 0; address < 17; ++address) {
        CHECK(rc_core_read_key(&core, address, &key));
        CHECK(key == 0);
    }
    /* Both RC bytes share a key; adjacent words do not. Include W0--W3 slots. */
    for (rc_addr_t address = 0; address < 16; address += 2) {
        uint8_t expected = (uint8_t)(address / 2);
        CHECK(rc_core_write_key(&core, address + 1, expected));
        CHECK(rc_core_read_key(&core, address, &key));
        CHECK(key == expected);
    }
    for (rc_addr_t address = 0; address < 16; address += 2) {
        CHECK(rc_core_write_word(&core, address, UINT32_C(0xabcdef)));
        CHECK(rc_core_write_halfword(&core, address + 1, UINT16_C(0x123)));
        CHECK(rc_core_read_key(&core, address + 1, &key));
        CHECK(key == address / 2);
        CHECK(rc_core_read_word(&core, address, &word));
        CHECK(word == UINT32_C(0xabc123));
    }
    /* The final single halfword has a valid containing key, but no full word. */
    CHECK(rc_core_write_key(&core, 16, 7));
    CHECK(rc_core_write_halfword(&core, 16, 123));
    CHECK(rc_core_read_key(&core, 16, &key));
    CHECK(key == 7);
    CHECK(!rc_core_read_word(&core, 16, &word));
    CHECK(!rc_core_write_key(&core, 17, 1));
    CHECK(!rc_core_write_key(&core, UINT32_MAX, 1));
    CHECK(!rc_core_write_key(&core, 16, 8));
    CHECK(!rc_core_write_key(&core, 16, UINT8_MAX));
    key = 99;
    CHECK(!rc_core_read_key(&core, 17, &key));
    CHECK(key == 99);
    CHECK(rc_core_read_key(&core, 16, &key));
    CHECK(key == 7);
    CHECK(!rc_core_read_key(&core, 0, NULL));
    rc_core_reset(&core);
    for (rc_addr_t address = 0; address < 17; ++address) {
        CHECK(rc_core_read_key(&core, address, &key));
        CHECK(key == 0);
    }
    rc_core_destroy(&core);
    CHECK(core.data == NULL && core.protection_keys == NULL);
    CHECK(core.halfword_count == 0);
    CHECK(!rc_core_read_key(&core, 0, &key));
    CHECK(!rc_core_write_key(&core, 0, 0));
    rc_core_destroy(&core);
    rc_core_reset(&core);
    CHECK(!rc_core_init(&core, 0));
    CHECK(!rc_core_init(&core, SIZE_MAX));
    CHECK(rc_core_init(&core, 1));
    CHECK(rc_core_write_key(&core, 0, 3));
    CHECK(rc_core_read_key(&core, 0, &key));
    CHECK(key == 3);
    CHECK(!rc_core_read_key(&core, 1, &key));
    rc_core_destroy(&core);
    CHECK(!rc_core_init(NULL, 16));
    CHECK(!rc_core_read_key(NULL, 0, &key));
    CHECK(!rc_core_write_key(NULL, 0, 0));
    rc_core_reset(NULL);
    rc_core_destroy(NULL);
}

static void protection_register(void)
{
    rc_cpu_t cpu;
    rc_cpu_reset(&cpu);
    CHECK(cpu.protection_register == UINT8_C(0x80));
    /* All 256 byte inputs and eight keys, using independently listed bit masks. */
    static const uint8_t bit[] = {0x80, 0x40, 0x20, 0x10, 8, 4, 2, 1};
    for (unsigned value = 0; value < 256; ++value) {
        rc_cpu_set_protection_register(&cpu, (uint8_t)value);
        CHECK(cpu.protection_register == (value | 0x80U));
        for (unsigned key = 0; key < 8; ++key) {
            bool expected = key == 0 || (value & bit[key]) != 0;
            CHECK(rc_cpu_key_is_protected(&cpu, key) == expected);
        }
    }
    CHECK(rc_cpu_key_is_protected(&cpu, 8));
    CHECK(rc_cpu_key_is_protected(&cpu, UINT_MAX));
    CHECK(rc_cpu_key_is_protected(NULL, 0));
    rc_cpu_set_protection_register(NULL, 0);
    rc_cpu_reset(&cpu);
    CHECK(cpu.protection_register == UINT8_C(0x80));
}

static void compatibility_stubs(void)
{
    rc_machine_t machine;
    rc_halfword_t halfword = 0;
    uint8_t key = 0;
    if (!rc_machine_init(&machine, 128)) {
        CHECK(false);
        return;
    }
    /* Nonzero stored keys and PR must not enable protection implicitly. */
    static const struct { uint8_t opcode, reg; int16_t address; } code[] = {
        {22, 2, 96}, /* kl: still returns zero. */
        {29, 0, 99}, /* ps: still stores 0x080, not the new PR state. */
        {51, 3, 96}, /* ks: still leaves stored key unchanged. */
        {28, 0, 96}, /* pl: still leaves PR unchanged. */
        {21, 0, 96}  /* sp: still skips despite protected mask/key. */
    };
    for (size_t i = 0; i < sizeof(code) / sizeof(code[0]); ++i) {
        CHECK(rc_machine_write_word(&machine, 64 + (rc_addr_t)(2 * i),
            rc_instruction_encode(code[i].opcode, code[i].reg,
                                  false, false, 0, code[i].address)));
    }
    CHECK(rc_core_write_key(&machine.core, 96, 7));
    CHECK(rc_machine_write_word(&machine, 96, UINT32_C(0x123456)));
    rc_cpu_set_protection_register(&machine.cpu, UINT8_MAX);
    machine.cpu.w[2] = 123;
    machine.cpu.w[3] = 5;
    machine.cpu.instruction_counter = 64;
    machine.cpu.stopped = false;
    machine.cpu.reset_mode = false;
    for (unsigned i = 0; i < 5; ++i) {
        CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    }
    CHECK(machine.cpu.w[2] == 0 && machine.cpu.w[3] == 5);
    CHECK(rc_machine_read_halfword(&machine, 99, &halfword));
    CHECK(halfword == UINT16_C(0x080));
    CHECK(rc_core_read_key(&machine.core, 96, &key) && key == 7);
    CHECK(machine.cpu.protection_register == UINT8_MAX);
    CHECK(machine.cpu.instruction_counter == 76);
    CHECK(machine.interrupts_serviced == 0);
    rc_machine_destroy(&machine);
}

static void machine_lifecycle(void)
{
    rc_machine_t machine;
    uint8_t key = 99;
    rc_word_t word = 0;
    if (!rc_machine_init(&machine, 64)) {
        CHECK(false);
        return;
    }
    /* Key slots 0--3 describe register aliases, even though data lives in W. */
    for (rc_addr_t address = 0; address < 8; address += 2) {
        CHECK(rc_core_write_key(&machine.core, address, (uint8_t)(address / 2 + 1)));
        CHECK(rc_machine_write_word(&machine, address, UINT32_C(0x123456)));
        CHECK(rc_core_read_key(&machine.core, address + 1, &key));
        CHECK(key == address / 2 + 1);
    }
    rc_cpu_set_protection_register(&machine.cpu, UINT8_C(0xaa));
    CHECK(rc_core_write_key(&machine.core, 32, 7));
    CHECK(rc_machine_write_word(&machine, 32, UINT32_C(0xabcdef)));
    CHECK(rc_machine_write_word(&machine, 14, 16));
    machine.cpu.stopped = false;
    machine.cpu.reset_mode = false;
    machine.cpu.instruction_counter = 18;
    CHECK(rc_machine_control_reset(&machine) == RC_CONTROL_OK);
    CHECK(machine.cpu.protection_register == UINT8_C(0xaa));
    CHECK(rc_core_read_key(&machine.core, 32, &key) && key == 7);
    CHECK(rc_machine_read_word(&machine, 32, &word) && word == UINT32_C(0xabcdef));
    CHECK(rc_machine_control_start(&machine) == RC_CONTROL_OK);
    CHECK(machine.cpu.protection_register == UINT8_C(0xaa));
    CHECK(rc_core_read_key(&machine.core, 32, &key) && key == 7);
    rc_machine_reset(&machine);
    CHECK(machine.cpu.protection_register == UINT8_C(0x80));
    CHECK(rc_core_read_key(&machine.core, 32, &key) && key == 0);
    CHECK(rc_machine_read_word(&machine, 32, &word) && word == 0);
    for (rc_addr_t address = 0; address < 8; address += 2) {
        CHECK(rc_core_read_key(&machine.core, address, &key) && key == 0);
        CHECK(rc_machine_read_word(&machine, address, &word) && word == 0);
    }
    rc_machine_destroy(&machine);
}

static bool execution_setup(rc_machine_t *m, unsigned opcode, unsigned reg,
                             int16_t address, bool monitor)
{
    if (!rc_machine_init(m, 1024)) {
        CHECK(false);
        return false;
    }
    m->protection_enabled = true;
    m->cpu.instruction_counter = 0x100;
    m->cpu.stopped = m->cpu.reset_mode = false;
    m->cpu.monitor_mode = monitor;
    CHECK(rc_machine_write_word(m, 0x100,
        rc_instruction_encode((uint8_t)opcode, (uint8_t)reg,
                              false, false, 0, address)));
    CHECK(rc_core_write_key(&m->core, 0x100, monitor ? 0 : 1));
    CHECK(rc_machine_write_word(m, 12, 0x300));
    CHECK(rc_machine_write_word(m, 0x180, UINT32_C(0xabcdef)));
    return true;
}

static rc_word_t read_word(rc_machine_t *m, rc_addr_t address)
{
    rc_word_t value = 0;
    CHECK(rc_machine_read_word(m, address, &value));
    return value;
}

static void fault_state(rc_machine_t *m, rc_addr_t saved_ic)
{
    CHECK(m->interrupts_serviced == 1);
    CHECK(read_word(m, 8) == 0);
    CHECK(read_word(m, 10) == saved_ic);
    CHECK(m->cpu.instruction_counter == 0x300);
    CHECK(m->cpu.monitor_mode && !m->cpu.interrupts_enabled);
}

static void fetch_and_reads(void)
{
    rc_machine_t m;
    if (!execution_setup(&m, 20, 2, 0x180, false)) return;
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(m.cpu.w[2] == UINT32_C(0xabcdef)); /* Protected data is readable. */
    CHECK(!m.cpu.monitor_mode && m.interrupts_serviced == 0);
    CHECK(rc_machine_write_word(&m, 0x102,
        rc_instruction_encode(11, 2, false, false, 0, 7)));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x104);
    CHECK(m.cpu.w[2] == UINT32_C(0xabcdef));
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 20, 2, 0x180, false)) return;
    CHECK(rc_machine_write_word(&m, 0x100,
        rc_instruction_encode(20, 2, false, true, 0, 0x182)));
    CHECK(rc_machine_write_word(&m, 0x182, 0x180));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(m.cpu.w[2] == UINT32_C(0xabcdef));
    CHECK(m.interrupts_serviced == 0 && !m.cpu.monitor_mode);
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 9, 0, 0, false)) return; /* am continuation */
    CHECK(rc_machine_write_word(&m, 0x102,
        rc_instruction_encode(11, 2, false, false, 0, 7)));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x104);
    CHECK(m.cpu.w[2] == 0);
    rc_machine_destroy(&m);
}

static void scalar_stores(void)
{
    static const unsigned opcodes[] = {23, 26, 27, 30, 31, 29};
    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); ++i) {
        for (unsigned odd = 0; odd < 2; ++odd) {
            rc_machine_t m;
            if (!execution_setup(&m, opcodes[i], 2, (int16_t)(0x180 + odd), false))
                return;
            m.cpu.w[2] = UINT32_C(0x123456);
            m.cpu.exception_22 = m.cpu.exception_23 = 1;
            CHECK(rc_machine_step(&m) == RC_STEP_OK);
            fault_state(&m, 0x102);
            CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
            CHECK(m.cpu.exception_22 == 1 && m.cpu.exception_23 == 1);
            rc_machine_destroy(&m);
        }
    }
    /* Matching allowed cases ensure checks do not simply reject every store. */
    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); ++i) {
        for (unsigned odd = 0; odd < 2; ++odd) {
            rc_machine_t m;
            if (!execution_setup(&m, opcodes[i], 2, (int16_t)(0x180 + odd), false))
                return;
            CHECK(rc_core_write_key(&m.core, 0x180, 1));
            m.cpu.w[2] = UINT32_C(0x123456);
            m.cpu.exception_22 = m.cpu.exception_23 = 1;
            static const rc_word_t expected[6][2] = {
                {0x123456, 0x123456}, /* rs */
                {0x456def, 0xabc456}, /* hs */
                {0x003def, 0xabc003}, /* xs */
                {0x800000, 0x800000}, /* ms */
                {0, 0},             /* is */
                {0x080def, 0xabc080} /* ps */
            };
            CHECK(rc_machine_step(&m) == RC_STEP_OK);
            CHECK(m.interrupts_serviced == 0);
            CHECK(read_word(&m, 0x180) == expected[i][odd]);
            uint8_t key = 99;
            CHECK(rc_core_read_key(&m.core, 0x180, &key) && key == 1);
            rc_machine_destroy(&m);
        }
    }
    for (unsigned key = 0; key < 8; ++key) {
        for (unsigned denied = 0; denied < 2; ++denied) {
            rc_machine_t m;
            if (!execution_setup(&m, 23, 2, 0x180, false)) return;
            unsigned code_key = key == 1 ? 2 : 1;
            CHECK(rc_core_write_key(&m.core, 0x100, (uint8_t)code_key));
            CHECK(rc_core_write_key(&m.core, 0x180, (uint8_t)key));
            rc_cpu_set_protection_register(&m.cpu,
                (uint8_t)(0x80U | (denied ? 0x80U >> key : 0U)));
            m.cpu.w[2] = UINT32_C(0x123456);
            CHECK(rc_machine_step(&m) == RC_STEP_OK);
            if (denied || key == 0) {
                fault_state(&m, 0x102);
                CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
            } else {
                CHECK(m.interrupts_serviced == 0);
                CHECK(read_word(&m, 0x180) == UINT32_C(0x123456));
            }
            uint8_t stored_key = 99;
            CHECK(rc_core_read_key(&m.core, 0x180, &stored_key));
            CHECK(stored_key == key);
            rc_machine_destroy(&m);
        }
    }
}

static void partial_effects(void)
{
    rc_machine_t m;
    if (!execution_setup(&m, 25, 2, 0x180, false)) return;
    m.cpu.w[2] = UINT32_C(0x123456);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(m.cpu.w[2] == UINT32_C(0xabcdef));
    CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 55, 3, 0x182, false)) return;
    CHECK(rc_core_write_key(&m.core, 0x182, 1));
    m.cpu.w[2] = UINT32_C(0x111111);
    m.cpu.w[3] = UINT32_C(0x222222);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(read_word(&m, 0x182) == UINT32_C(0x222222));
    CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 34, 2, 0x180, false)) return;
    m.cpu.w[2] = UINT32_C(0x100000);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(m.cpu.w[2] == UINT32_C(0x400000));
    CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
    rc_machine_destroy(&m);
}

static void branches_and_privilege(void)
{
    static const unsigned privileged[] = {0, 1, 12, 14, 15, 28, 47, 51};
    for (size_t i = 0; i < sizeof(privileged) / sizeof(privileged[0]); ++i) {
        rc_machine_t m;
        if (!execution_setup(&m, privileged[i], 2, 0x180, false)) return;
        m.cpu.w[2] = UINT32_C(0x123456);
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        fault_state(&m, 0x102);
        CHECK(m.cpu.w[2] == UINT32_C(0x123456));
        CHECK(m.cpu.protection_register == 0x80);
        CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
        CHECK(m.io_operations == 0);
        rc_machine_destroy(&m);
    }
    rc_machine_t m;
    if (!execution_setup(&m, 13, 2, 0x180, false)) return;
    m.cpu.w[2] = 123;
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(m.cpu.w[2] == 123);
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 15, 2, 0x180, true)) return;
    CHECK(rc_core_write_key(&m.core, 0x180, 1));
    CHECK(rc_machine_write_word(&m, 0x180,
        rc_instruction_encode(11, 3, false, false, 0, 7)));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(m.cpu.monitor_mode && m.cpu.interrupts_enabled);
    CHECK(m.cpu.w[2] == 0x102 && m.cpu.instruction_counter == 0x180);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(!m.cpu.monitor_mode && m.cpu.w[3] == 7);
    rc_machine_destroy(&m);
}

static void protection_instructions(void)
{
    rc_machine_t m;
    uint8_t key = 99;
    if (!execution_setup(&m, 51, 2, 0x180, true)) return;
    m.cpu.w[2] = UINT32_C(0xabcdef);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(rc_core_read_key(&m.core, 0x180, &key) && key == 7);
    CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
    CHECK(m.cpu.w[2] == UINT32_C(0xabcdef));
    rc_machine_destroy(&m);

    for (unsigned k = 0; k < 8; ++k) {
        if (!execution_setup(&m, 22, 2, 0x181, false)) return;
        CHECK(rc_core_write_key(&m.core, 0x180, (uint8_t)k));
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        CHECK(m.cpu.w[2] == k && m.interrupts_serviced == 0);
        rc_machine_destroy(&m);
        for (unsigned monitor = 0; monitor < 2; ++monitor) {
            if (!execution_setup(&m, 21, 0, 0x180, monitor != 0)) return;
            CHECK(rc_core_write_key(&m.core, 0x180, (uint8_t)k));
            CHECK(rc_machine_step(&m) == RC_STEP_OK);
            CHECK(m.cpu.instruction_counter == (k == 0 ? 0x102U : 0x104U));
            rc_machine_destroy(&m);
        }
    }
    for (unsigned all = 0; all < 2; ++all) {
        if (!execution_setup(&m, 28, 0, 0x181, true)) return;
        CHECK(rc_machine_write_halfword(&m, 0x181, all ? 0x07f : 0));
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        CHECK(m.cpu.protection_register == (all ? 0xff : 0x80));
        rc_machine_destroy(&m);
    }

    if (!execution_setup(&m, 29, 0, 0x181, false)) return;
    CHECK(rc_core_write_key(&m.core, 0x180, 1));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(read_word(&m, 0x180) == UINT32_C(0xabc080));
    CHECK(m.interrupts_serviced == 0);
    rc_machine_destroy(&m);
}

static void observe_data_write(void *context, rc_addr_t address,
                               rc_word_t previous, rc_word_t current)
{
    unsigned *count = context;
    (void)previous;
    (void)current;
    if (address == 0x180 || address == 0x182) ++*count;
}

typedef struct { unsigned character; rc_word_t word; } autoload_input_t;

static rc_device_result_t autoload_input(void *context, rc_io_command_t command,
                                        uint8_t modifier, rc_word_t *transfer)
{
    autoload_input_t *input = context;
    CHECK(modifier == 0);
    if (command == RC_IO_SENSE) {
        CHECK(input->character < 4);
        if (input->character >= 4) return RC_DEVICE_DISCONNECTED;
        *transfer = (input->word >> (18U - 6U * input->character)) & 63U;
        ++input->character;
    } else {
        CHECK(command == RC_IO_READ);
        *transfer = 0;
    }
    return RC_DEVICE_ACCEPTED;
}

static void control_and_bypass(void)
{
    rc_machine_t m;
    unsigned count = 0;
    uint8_t key = 99;
    if (!execution_setup(&m, 55, 3, 0x182, false)) return;
    rc_machine_set_write_observer(&m, observe_data_write, &count);
    CHECK(rc_core_write_key(&m.core, 0x182, 1));
    m.cpu.w[2] = 11;
    m.cpu.w[3] = 22;
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(count == 1); /* One successful data write, not two or zero. */
    CHECK(rc_machine_write_word(&m, 0x180, 33)); /* Explicit host/DMA bypass. */
    CHECK(count == 2);
    CHECK(rc_core_read_key(&m.core, 0x180, &key) && key == 0);
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 51, 2, 1024, true)) return;
    CHECK(rc_machine_set_control_register(&m, RC_CONTROL_INTERRUPT_MASK, 0));
    CHECK(m.cpu.interrupt_mask == UINT32_C(0x800000));
    CHECK(rc_machine_step(&m) == RC_STEP_OK); /* Bootstrap-like ks limit probe. */
    fault_state(&m, 0x102);
    m.protection_enabled = true;
    rc_machine_reset(&m);
    CHECK(m.protection_enabled);
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 11, 2, 7, true)) return;
    m.cpu.instruction_counter = 1024;
    m.cpu.interrupt_mask = 0; /* Even a direct host write cannot mask IRQ 0. */
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 1024);
    rc_machine_destroy(&m);

    for (rc_addr_t address = 0; address < 8; ++address) {
        if (!execution_setup(&m, 51, 2, (int16_t)address, true)) return;
        m.cpu.w[2] = 5;
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        CHECK(rc_core_read_key(&m.core, address, &key) && key == 5);
        rc_machine_destroy(&m);
    }
    if (!execution_setup(&m, 23, 2, 0, false)) return;
    m.cpu.w[0] = 11;
    m.cpu.w[2] = 22;
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(m.cpu.w[0] == 11);
    rc_machine_destroy(&m);
    if (!execution_setup(&m, 11, 2, 7, false)) return;
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(m.cpu.w[2] == 7 && m.interrupts_serviced == 0);
    rc_machine_destroy(&m);

    for (unsigned panel = 0; panel < 2; ++panel) {
        if (!execution_setup(&m, 0, 0, 0x180, true)) return;
        autoload_input_t input = {0, UINT32_C(0x123456)};
        rc_device_t device = {0};
        device.context = &input;
        device.io = autoload_input;
        CHECK(rc_machine_attach_device(&m, 0, &device));
        rc_addr_t target = panel ? 0 : 0x180;
        CHECK(rc_core_write_key(&m.core, target, 7));
        CHECK(rc_core_write_key(&m.core, target + 2, 6));
        if (panel) {
            CHECK(rc_machine_control_reset(&m) == RC_CONTROL_OK);
            CHECK(rc_machine_control_autoload(&m) == RC_CONTROL_OK);
        } else {
            CHECK(rc_machine_step(&m) == RC_STEP_OK);
        }
        CHECK(input.character == 4 && m.io_operations == 8);
        CHECK(read_word(&m, target) == UINT32_C(0x123456));
        CHECK(rc_core_read_key(&m.core, target, &key) && key == 0);
        CHECK(rc_core_read_key(&m.core, target + 2, &key) && key == 6);
        rc_machine_destroy(&m);
    }
}

static void configuration(void)
{
    rc_config_t config;
    rc_config_error_t error;
    CHECK(rc_config_parse_text("", &config, &error) == 0);
    CHECK(!config.protection_enabled && config.ix_enabled);
    CHECK(rc_config_parse_text("PROTECTION MODE=OFF\nCPU IX=NO\n", &config, &error) == 0);
    CHECK(!config.protection_enabled && !config.ix_enabled);
    CHECK(rc_config_parse_text("protection mode=rc4000 ; explicit opt-in\nCPU IX=YES\n",
                               &config, &error) == 0);
    CHECK(config.protection_enabled && config.ix_enabled);
    static const char *const invalid[] = {
        "PROTECTION MODE=YES", "PROTECTION MODE=ON", "PROTECTION MODE=RC8000",
        "PROTECTION MODE=", "PROTECTION MODE", "PROTECTION OTHER=OFF",
        "PROTECTION MODE=OFF EXTRA", "PROTECTION",
        "PROTECTION MODE=OFF\nPROTECTION MODE=RC4000",
        "PROTECTION MODE=RC4000\nPROTECTION MODE=RC4000"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        CHECK(rc_config_parse_text(invalid[i], &config, &error) != 0);
}

static void packed_core_loading(void)
{
    rc_machine_t m;
    if (!execution_setup(&m, 11, 0, 0, false)) return;
    char path[] = "/tmp/sim4000-protection-core-XXXXXX";
    int descriptor = mkstemp(path);
    if (descriptor < 0) {
        CHECK(false);
        rc_machine_destroy(&m);
        return;
    }
    const unsigned char bytes[] = {0x12, 0x34, 0x56, 0xab, 0xcd, 0xef};
    CHECK(write(descriptor, bytes, sizeof(bytes)) == (ssize_t)sizeof(bytes));
    CHECK(close(descriptor) == 0);
    CHECK(rc_core_write_key(&m.core, 0x180, 7));
    CHECK(rc_core_write_key(&m.core, 0x182, 6));
    size_t count = 0;
    char error[256];
    CHECK(rc_core_image_load(path, &m, 0x180, &count, error, sizeof(error)) == 0);
    CHECK(count == 2 && read_word(&m, 0x180) == UINT32_C(0x123456));
    CHECK(read_word(&m, 0x182) == UINT32_C(0xabcdef));
    uint8_t key = 99;
    CHECK(rc_core_read_key(&m.core, 0x180, &key) && key == 7);
    CHECK(rc_core_read_key(&m.core, 0x182, &key) && key == 6);
    CHECK(rc_core_image_load(path, &m, 1022, &count, error, sizeof(error)) != 0);
    CHECK(rc_core_read_key(&m.core, 0x180, &key) && key == 7);
    CHECK(read_word(&m, 0x180) == UINT32_C(0x123456));
    CHECK(truncate(path, 2) == 0);
    CHECK(rc_core_image_load(path, &m, 0x180, &count, error, sizeof(error)) != 0);
    CHECK(rc_core_read_key(&m.core, 0x180, &key) && key == 7);
    CHECK(read_word(&m, 0x180) == UINT32_C(0x123456));
    CHECK(unlink(path) == 0);
    rc_machine_destroy(&m);
}

static rc_device_result_t failed_autoload(void *context, rc_io_command_t command,
                                         uint8_t modifier, rc_word_t *transfer)
{
    bool status_error = *(bool *)context;
    (void)modifier;
    if (!status_error) return RC_DEVICE_DISCONNECTED;
    *transfer = command == RC_IO_SENSE ? UINT32_C(0x1000) : 0;
    return RC_DEVICE_ACCEPTED;
}

static void autoload_failure_lifecycle(void)
{
    for (unsigned panel = 0; panel < 2; ++panel) {
        for (unsigned status = 0; status < 2; ++status) {
            rc_machine_t m;
            if (!execution_setup(&m, 0, 0, 0x180, true)) return;
            bool status_error = status != 0;
            rc_device_t device = {0};
            device.context = &status_error;
            device.io = failed_autoload;
            CHECK(rc_machine_attach_device(&m, 0, &device));
            CHECK(rc_core_write_key(&m.core, 0x180, 7));
            CHECK(rc_core_write_key(&m.core, 0, 6));
            rc_cpu_set_protection_register(&m.cpu, 0xaa);
            if (panel) {
                CHECK(rc_machine_control_reset(&m) == RC_CONTROL_OK);
                CHECK(rc_machine_control_autoload(&m) == RC_CONTROL_AUTOLOAD_ERROR);
            } else {
                CHECK(rc_machine_step(&m) == RC_STEP_STOPPED);
            }
            CHECK(m.cpu.reset_mode && m.cpu.stopped && m.cpu.monitor_mode);
            CHECK(m.cpu.protection_register == 0xaa && m.protection_enabled);
            CHECK(read_word(&m, 10) == (panel ? 0x100U : 0x102U));
            CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
            uint8_t key = 99;
            CHECK(rc_core_read_key(&m.core, 0x180, &key) && key == 7);
            CHECK(rc_core_read_key(&m.core, 0, &key) && key == 6);
            rc_machine_destroy(&m);
        }
    }
}

static void boundary_and_alias_vectors(void)
{
    rc_machine_t m;
    /* Each instruction must reject an address immediately beyond installed core. */
    static const unsigned invalid[] = {20, 23, 26, 25, 28, 29, 21, 22, 51, 55, 13};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        if (!execution_setup(&m, invalid[i], 2, 1024, true)) return;
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        fault_state(&m, 0x102);
        rc_machine_destroy(&m);
    }
    for (unsigned odd = 0; odd < 2; ++odd) {
        if (!execution_setup(&m, 23, 2, (int16_t)(1022 + odd), false)) return;
        CHECK(rc_core_write_key(&m.core, 1022, 1));
        m.cpu.w[2] = UINT32_C(0x765432);
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        CHECK(read_word(&m, 1022) == UINT32_C(0x765432));
        CHECK(m.interrupts_serviced == 0);
        rc_machine_destroy(&m);
    }
    /* Double store must not preflight away the specified alias/partial effects. */
    if (!execution_setup(&m, 55, 3, 4, false)) return;
    m.cpu.w[1] = UINT32_C(0x333333);
    m.cpu.w[2] = UINT32_C(0x111111);
    m.cpu.w[3] = UINT32_C(0x222222);
    CHECK(rc_core_write_key(&m.core, 4, 1));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(m.cpu.w[2] == UINT32_C(0x222222));
    CHECK(m.cpu.w[1] == UINT32_C(0x333333));
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 55, 3, 0, false)) return;
    CHECK(rc_core_write_key(&m.core, 0, 1));
    CHECK(rc_core_write_key(&m.core, 6, 1));
    m.cpu.w[2] = UINT32_C(0x111111);
    m.cpu.w[3] = UINT32_C(0x222222);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(m.cpu.w[0] == UINT32_C(0x222222));
    CHECK(m.cpu.w[3] == UINT32_C(0x111111)); /* Previous-word wrap to 6. */
    CHECK(m.interrupts_serviced == 0);
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 55, 3, 0x182, false)) return;
    CHECK(rc_machine_write_word(&m, 0x182, UINT32_C(0x333333)));
    CHECK(rc_core_write_key(&m.core, 0x180, 1)); /* First destination still denied. */
    m.cpu.w[2] = UINT32_C(0x111111);
    m.cpu.w[3] = UINT32_C(0x222222);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(read_word(&m, 0x182) == UINT32_C(0x333333));
    CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 35, 3, 0x180, false)) return;
    m.cpu.w[2] = UINT32_C(0x100000);
    m.cpu.w[3] = 0;
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(m.cpu.w[2] == UINT32_C(0x400000) && m.cpu.w[3] == 0);
    CHECK(read_word(&m, 0x180) == UINT32_C(0xabcdef));
    rc_machine_destroy(&m);

    /* Instructions stored in working registers use those registers' own keys. */
    for (unsigned denied = 0; denied < 2; ++denied) {
        if (!execution_setup(&m, 11, 2, 0, false)) return;
        m.cpu.instruction_counter = 0;
        m.cpu.w[0] = rc_instruction_encode(11, 2, false, false, 0, 5);
        CHECK(rc_core_write_key(&m.core, 0, denied ? 0 : 1));
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        if (denied) {
            fault_state(&m, 2);
            CHECK(m.cpu.w[2] == 0);
        } else {
            CHECK(m.cpu.w[2] == 5 && !m.cpu.monitor_mode);
            CHECK(m.cpu.instruction_counter == 2);
        }
        rc_machine_destroy(&m);
    }
    /* Indirect addressing can read protected W0, but not write through it. */
    if (!execution_setup(&m, 23, 2, 0, false)) return;
    CHECK(rc_machine_write_word(&m, 0x100,
        rc_instruction_encode(23, 2, false, true, 0, 0)));
    m.cpu.w[0] = 4;
    m.cpu.w[2] = UINT32_C(0x123456);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(m.cpu.w[2] == UINT32_C(0x123456));
    rc_machine_destroy(&m);
}

static uint32_t protected_dma_poll(void *context)
{
    rc_machine_t *m = context;
    CHECK(rc_machine_write_word(m, 0x180, UINT32_C(0x123456)));
    return 0;
}

static void interrupt_return_and_device_boundary(void)
{
    rc_machine_t m;
    if (!execution_setup(&m, 23, 2, 0x180, false)) return;
    CHECK(rc_machine_write_word(&m, 0x102,
        rc_instruction_encode(11, 3, false, false, 0, 7)));
    CHECK(rc_core_write_key(&m.core, 0x102, 1));
    CHECK(rc_machine_write_word(&m, 0x300,
        rc_instruction_encode(15, 0, false, true, 0, 10))); /* je (saved IC) */
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x102);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(m.cpu.instruction_counter == 0x102 && m.cpu.monitor_mode);
    CHECK(m.cpu.interrupts_enabled && m.interrupts_serviced == 1);
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(!m.cpu.monitor_mode && m.cpu.w[3] == 7);
    CHECK(m.cpu.instruction_counter == 0x104 && m.interrupts_serviced == 1);
    rc_machine_destroy(&m);

    /* IRQ 0 wins over an enabled external request; that request remains pending. */
    if (!execution_setup(&m, 11, 2, 0, false)) return;
    m.cpu.interrupts_enabled = true;
    m.cpu.interrupt_mask |= UINT32_C(0x100000);
    rc_cpu_request_interrupt(&m.cpu, 0);
    rc_cpu_request_interrupt(&m.cpu, 3);
    CHECK(rc_machine_write_word(&m, 0x300,
        rc_instruction_encode(11, 0, false, false, 0, 11)));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(read_word(&m, 8) == 0 && read_word(&m, 10) == 0x100);
    CHECK(m.cpu.w[0] == 11 && m.cpu.monitor_mode);
    CHECK(m.cpu.interrupt_requests == UINT32_C(0x100000));
    CHECK(m.interrupts_serviced == 1 && !m.cpu.interrupts_enabled);
    rc_machine_destroy(&m);

    if (!execution_setup(&m, 20, 2, 0x180, false)) return;
    rc_device_t device = {0};
    device.context = &m;
    device.poll = protected_dma_poll;
    CHECK(rc_machine_attach_device(&m, 15, &device));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(m.cpu.w[2] == UINT32_C(0x123456));
    CHECK(m.interrupts_serviced == 0 && !m.cpu.monitor_mode);
    uint8_t key = 99;
    CHECK(rc_core_read_key(&m.core, 0x180, &key) && key == 0);
    rc_machine_destroy(&m);

    /* A monitor AM continuation may enter task mode, losing I/O privilege. */
    if (!execution_setup(&m, 9, 0, 0, true)) return;
    CHECK(rc_machine_write_word(&m, 0x102,
        rc_instruction_encode(1, 2, false, false, 0, 0)));
    CHECK(rc_core_write_key(&m.core, 0x102, 1));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    fault_state(&m, 0x104);
    CHECK(m.io_operations == 0);
    rc_machine_destroy(&m);
}

static void runtime_probe_privilege_contract(void)
{
    /* Literal sequence observed in tofrom1. Compatibility PL is a no-op,
     * whereas RC4000 task-mode PL must fault; do not weaken CPU privilege
     * checks to accommodate this software probe. The high W3 value selects
     * the branch following its two range comparisons in compatibility mode. */
    for (unsigned protected = 0; protected < 2; ++protected) {
        rc_machine_t m;
        if (!execution_setup(&m, 28, 3, 34, false)) return;
        m.protection_enabled = protected != 0;
        rc_cpu_set_protection_register(&m.cpu, 0xbf);
        m.cpu.w[3] = UINT32_C(0x02314f);
        CHECK(rc_machine_write_word(&m, 0x102, UINT32_C(0xa3004f)));
        CHECK(rc_machine_write_word(&m, 0x104, UINT32_C(0xa3003b)));
        CHECK(rc_machine_write_word(&m, 0x106, UINT32_C(0x34802e)));
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        if (protected) {
            fault_state(&m, 0x102);
            CHECK(m.cpu.protection_register == 0xbf);
        } else {
            CHECK(rc_machine_step(&m) == RC_STEP_OK);
            CHECK(m.cpu.instruction_counter == 0x106);
            CHECK(rc_machine_step(&m) == RC_STEP_OK);
            CHECK(m.cpu.instruction_counter == 0x134);
            CHECK(m.interrupts_serviced == 0);
        }
        CHECK(m.cpu.w[3] == UINT32_C(0x02314f));
        rc_machine_destroy(&m);
    }
}

int main(void)
{
    core_keys();
    protection_register();
    machine_lifecycle();
    compatibility_stubs();
    fetch_and_reads();
    scalar_stores();
    partial_effects();
    branches_and_privilege();
    protection_instructions();
    control_and_bypass();
    configuration();
    packed_core_loading();
    autoload_failure_lifecycle();
    boundary_and_alias_vectors();
    interrupt_return_and_device_boundary();
    runtime_probe_privilege_contract();
    if (failures != 0) {
        (void)fprintf(stderr, "%u protection state checks failed\n", failures);
        return 1;
    }
    (void)puts("Protection state, CPU access, fault and compatibility checks passed");
    return 0;
}
