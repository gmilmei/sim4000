#include "sim4000/machine.h"
#include "sim4000/config.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while (0)

static void setup(rc_machine_t *m, unsigned reg, rc_word_t index,
                  int type, rc_word_t lower, rc_word_t upper, rc_word_t origin)
{
    CHECK(rc_machine_init(m, 512));
    m->ix_enabled = true;
    m->cpu.stopped = false;
    m->cpu.instruction_counter = 64;
    m->cpu.w[reg] = index;
    m->cpu.exception_21 = m->cpu.exception_22 = m->cpu.exception_23 = 1;
    CHECK(rc_core_write_word(&m->core, 64, rc_instruction_encode(61, (uint8_t)reg, false, false, 0, 128)));
    CHECK(rc_core_write_word(&m->core, 66, 4U << 12 | ((unsigned)type & 4095U)));
    CHECK(rc_core_write_word(&m->core, 128, origin));
    CHECK(rc_core_write_word(&m->core, 130, upper));
    CHECK(rc_core_write_word(&m->core, 132, lower));
    CHECK(rc_core_write_word(&m->core, 12, 96));
    CHECK(rc_core_write_word(&m->core, 96, rc_instruction_encode(11, 0, false, false, 0, 0)));
}

static void vectors(void)
{
    /* Literal answers, including the DOS count-modulo-32 convention and
     * 24-bit truncation of its left-aligned 32-bit shift. */
    static const struct { rc_word_t index; int type; rc_word_t lower, upper, origin, expected; } v[] = {
        {1, 1, 0, 40, 100, 102}, {3, 2, 0, 20, 100, 112},
        {3, 0, 0, 20, 100, 103}, {3, -1, 0, 20, 100, 103},
        {3, -2048, 0, 20, 100, 103},
        {0xffffff, 0, 0xfffffe, 0, 100, 99},
        {0xffffff, 1, 0xfffffd, 0, 100, 98},
        {2, 1, 0, 10, 0xfffffe, 2},
        {1, 22, 0, 0x7fffff, 0, 0x400000},
        {1, 24, 0xffffff, 1, 100, 100},
        {1, 31, 0xffffff, 1, 100, 100},
        {1, 32, 0, 10, 100, 101}, {1, 33, 0, 10, 100, 102},
        {1, 2047, 0xffffff, 1, 100, 100}
    };
    for (unsigned ex21 = 0; ex21 < 2; ++ex21)
    for (unsigned r = 0; r < 4; ++r) for (size_t i = 0; i < sizeof(v)/sizeof(v[0]); ++i) {
        rc_machine_t m;
        setup(&m, r, v[i].index, v[i].type, v[i].lower, v[i].upper, v[i].origin);
        m.cpu.exception_21 = (uint8_t)ex21;
        rc_word_t before[4]; memcpy(before, m.cpu.w, sizeof(before));
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        CHECK(m.cpu.w[r] == v[i].expected);
        for (unsigned j = 0; j < 4; ++j) if (j != r) CHECK(m.cpu.w[j] == before[j]);
        CHECK(m.cpu.instruction_counter == 68);
        CHECK(m.cpu.exception_21 == ex21 && m.cpu.exception_22 == 0 && m.cpu.exception_23 == 0);
        CHECK(m.cpu.interrupt_requests == 0 && m.instructions_executed == 1);
        rc_machine_destroy(&m);
    }
}

static void invalid(void)
{
    static const rc_word_t values[] = {0, 10, 0xffffff, 11};
    for (unsigned ex21 = 0; ex21 < 2; ++ex21)
    for (unsigned active = 0; active < 2; ++active)
    for (unsigned enabled = 0; enabled < 2; ++enabled)
    for (unsigned r = 0; r < 4; ++r)
    for (size_t i = 0; i < sizeof(values)/sizeof(values[0]); ++i) {
        rc_machine_t m;
        setup(&m, r, values[i], 0, 0, 10, 100);
        m.cpu.exception_21 = (uint8_t)ex21;
        m.cpu.interrupt_mask |= active ? 0x200000U : 0;
        m.cpu.interrupts_enabled = enabled != 0;
        m.cpu.interrupt_requests = 0x100000; /* Unrelated masked request. */
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        CHECK(m.cpu.w[r] == (active ? values[i] : 110));
        CHECK(m.cpu.instruction_counter == 68);
        CHECK(m.cpu.exception_21 == ex21 && m.cpu.exception_22 == 1 && m.cpu.exception_23 == 1);
        CHECK(m.cpu.interrupt_requests == (active ? 0x300000U : 0x100000U));
        CHECK(m.interrupts_serviced == 0);
        m.cpu.interrupt_mask |= 0x200000U;
        m.cpu.interrupts_enabled = true;
        if (active) {
            rc_word_t word;
            CHECK(rc_machine_step(&m) == RC_STEP_OK);
            CHECK(m.interrupts_serviced == 1);
            CHECK(rc_core_read_word(&m.core, 8, &word) && word == 4);
            CHECK(rc_core_read_word(&m.core, 10, &word) && word == 68);
            CHECK(m.cpu.interrupt_requests == 0x100000);
        } else {
            /* A masked IX failure creates no deferred floating request. */
            CHECK(rc_core_write_word(&m.core, 68, rc_instruction_encode(11, 0, false, false, 0, 0)));
            CHECK(rc_machine_step(&m) == RC_STEP_OK);
            CHECK(m.interrupts_serviced == 0 && m.cpu.interrupt_requests == 0x100000U);
        }
        rc_machine_destroy(&m);
    }
}

static void addressing(void)
{
    for (unsigned mode = 0; mode < 8; ++mode) {
        rc_machine_t m;
        setup(&m, 1, 1, 1, 0, 40, 100);
        bool relative = (mode & 4U) != 0, indirect = (mode & 2U) != 0;
        unsigned x = (mode & 1U) ? 2 : 0;
        m.cpu.w[2] = 16;
        int d = (indirect ? 160 : 129) - (relative ? 64 : 0) - (x ? 16 : 0);
        CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode(61, 1, relative, indirect, (uint8_t)x, d)));
        CHECK(rc_core_write_word(&m.core, 160, 129));
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        CHECK(m.cpu.w[1] == 102 && m.cpu.w[2] == 16 && m.cpu.instruction_counter == 68);
        rc_machine_destroy(&m);
    }
    rc_machine_t m;
    setup(&m, 1, 1, 1, 0, 40, 100);
    /* Signed negative displacement; round only after adding to odd base. */
    CHECK(rc_core_write_word(&m.core, 66, 0xffe001));
    CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode(61, 1, false, false, 0, 129)));
    CHECK(rc_core_write_word(&m.core, 126, 0));
    CHECK(rc_core_write_word(&m.core, 124, 40));
    CHECK(rc_machine_step(&m) == RC_STEP_OK && m.cpu.w[1] == 102);
    rc_machine_destroy(&m);
    setup(&m, 1, 1, 1, 0, 40, 100);
    m.cpu.instruction_counter = 62;
    CHECK(rc_core_write_word(&m.core, 62, rc_instruction_encode(9, 0, false, false, 0, 4)));
    CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode(61, 1, false, false, 0, 124)));
    CHECK(rc_machine_step(&m) == RC_STEP_OK);
    CHECK(m.cpu.w[1] == 102 && m.cpu.instruction_counter == 68 && m.instructions_executed == 2);
    rc_machine_destroy(&m);
    setup(&m, 3, 1, 1, 0, 40, 100);
    m.cpu.w[0] = 100; m.cpu.w[1] = 40; m.cpu.w[2] = 0;
    CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode(61, 3, false, false, 0, 0)));
    CHECK(rc_machine_step(&m) == RC_STEP_OK && m.cpu.w[3] == 102);
    CHECK(m.cpu.w[0] == 100 && m.cpu.w[1] == 40 && m.cpu.w[2] == 0);
    rc_machine_reset(&m); CHECK(m.ix_enabled);
    rc_machine_destroy(&m);
    setup(&m, 1, 1, 1, 0, 40, 100);
    CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode(61, 1, false, false, 1, 127)));
    CHECK(rc_machine_step(&m) == RC_STEP_OK && m.cpu.w[1] == 102);
    rc_machine_destroy(&m);
    /* Destination may alias any descriptor word; read all before writing. */
    static const rc_word_t answers[] = {3, 101, 102, 102};
    for (unsigned r = 0; r < 4; ++r) {
        setup(&m, r, 1, 1, 0, 40, 100);
        m.cpu.w[0] = 100; m.cpu.w[1] = 40;
        m.cpu.w[2] = 0; m.cpu.w[3] = 1;
        m.cpu.w[r] = 1;
        rc_word_t before[4];
        memcpy(before, m.cpu.w, sizeof(before));
        CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode(61, (uint8_t)r, false, false, 0, 0)));
        CHECK(rc_machine_step(&m) == RC_STEP_OK && m.cpu.w[r] == answers[r]);
        for (unsigned other = 0; other < 4; ++other)
            if (other != r) CHECK(m.cpu.w[other] == before[other]);
        CHECK(m.cpu.exception_21 == 1 && m.cpu.exception_22 == (r == 1) && m.cpu.exception_23 == (r == 1));
        rc_machine_destroy(&m);
    }
}

static void wrap_and_pending(void)
{
    rc_machine_t m;
    CHECK(rc_machine_init(&m, UINT32_C(1) << 24));
    CHECK(m.ix_enabled);
    m.cpu.stopped = false; m.cpu.instruction_counter = 64;
    m.cpu.w[1] = 40; m.cpu.w[2] = 0; m.cpu.w[3] = 1;
    CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode(61, 3, false, false, 0, -2)));
    CHECK(rc_core_write_word(&m.core, 66, 0x006001));
    CHECK(rc_core_write_word(&m.core, 0xfffffe, 100));
    CHECK(rc_machine_step(&m) == RC_STEP_OK && m.cpu.w[3] == 102);
    /* Inline word wraps to the W0 alias; next IC must be 2. */
    m.cpu.instruction_counter = 0xfffffe; m.cpu.w[0] = 0x004001; m.cpu.w[3] = 1;
    CHECK(rc_core_write_word(&m.core, 0xfffffe, rc_instruction_encode(61, 3, false, false, 0, 128)));
    CHECK(rc_core_write_word(&m.core, 128, 100));
    CHECK(rc_core_write_word(&m.core, 130, 40));
    CHECK(rc_core_write_word(&m.core, 132, 0));
    CHECK(rc_machine_step(&m) == RC_STEP_OK && m.cpu.w[3] == 102 && m.cpu.instruction_counter == 2);
    rc_machine_destroy(&m);
    setup(&m, 1, 1, 1, 0, 40, 100);
    m.cpu.interrupt_requests = 0x200000;
    CHECK(rc_machine_step(&m) == RC_STEP_OK && m.cpu.w[1] == 102);
    CHECK(m.cpu.interrupt_requests == 0x200000 && m.interrupts_serviced == 0);
    rc_machine_destroy(&m);
    setup(&m, 1, 1, 23, 0x800000, 0, 100);
    CHECK(rc_machine_step(&m) == RC_STEP_OK && m.cpu.w[1] == 100);
    CHECK(m.cpu.exception_22 == 1 && m.cpu.exception_23 == 1);
    rc_machine_destroy(&m);
    setup(&m, 1, 10, 0, 0, 10, 100);
    m.cpu.interrupt_requests = 0x200000U;
    CHECK(rc_machine_step(&m) == RC_STEP_OK && m.cpu.w[1] == 110);
    CHECK(m.cpu.interrupt_requests == 0x200000U && m.interrupts_serviced == 0);
    rc_machine_destroy(&m);
}

static void am_exceptions(void)
{
    for (unsigned fault = 0; fault < 2; ++fault)
    for (unsigned ex21 = 0; ex21 < 2; ++ex21) {
        rc_machine_t m;
        setup(&m, 1, 10, 0, 0, 10, 100);
        m.cpu.exception_21 = (uint8_t)ex21;
        m.cpu.exception_22 = m.cpu.exception_23 = 0;
        m.cpu.interrupt_mask |= 0x200000U;
        m.cpu.interrupts_enabled = true;
        m.cpu.instruction_counter = 62;
        CHECK(rc_core_write_word(&m.core, 62, rc_instruction_encode(9, 0, false, false, 0, 4)));
        CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode(61, 1, false, false, 0, fault ? 508 : 124)));
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        CHECK(m.instructions_executed == 2 && m.cpu.w[1] == 10);
        CHECK(m.cpu.exception_21 == ex21);
        rc_word_t saved;
        if (fault) {
            /* AM makes the origin inaccessible: no partial IX flags/result. */
            CHECK(m.interrupts_serviced == 1);
            CHECK(m.cpu.exception_22 == 0 && m.cpu.exception_23 == 0);
            CHECK(rc_core_read_word(&m.core, 8, &saved) && saved == 0);
            CHECK(rc_core_read_word(&m.core, 10, &saved) && saved == 66);
        } else {
            /* Floating interrupt is deferred until after the complete AM/IX. */
            CHECK(m.interrupts_serviced == 0 && m.cpu.instruction_counter == 68);
            CHECK(m.cpu.exception_22 == 1 && m.cpu.exception_23 == 1);
            CHECK(m.cpu.interrupt_requests == 0x200000U);
            CHECK(rc_machine_step(&m) == RC_STEP_OK && m.interrupts_serviced == 1);
            CHECK(rc_core_read_word(&m.core, 8, &saved) && saved == 4);
            CHECK(rc_core_read_word(&m.core, 10, &saved) && saved == 68);
        }
        rc_machine_destroy(&m);
    }
}

static void faults_and_default(void)
{
    for (unsigned which = 0; which < 5; ++which) {
        rc_machine_t m; rc_word_t saved;
        setup(&m, 1, 1, 1, 0, 40, 100);
        if (which == 0) m.ix_enabled = false;
        if (which == 1) { /* Inaccessible inline word. */
            CHECK(rc_core_write_word(&m.core, 510, rc_instruction_encode(61, 1, false, false, 0, 128)));
            m.cpu.instruction_counter = 510;
        }
        if (which == 2) CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode(61, 1, false, false, 0, 512)));
        if (which == 3) CHECK(rc_core_write_word(&m.core, 66, 0x180001));
        if (which == 4) CHECK(rc_core_write_word(&m.core, 66, 0xf80001));
        CHECK(rc_machine_step(&m) == RC_STEP_OK);
        CHECK(m.interrupts_serviced == 1 && m.cpu.w[1] == 1);
        CHECK(m.cpu.exception_21 == 1 && m.cpu.exception_22 == 1 && m.cpu.exception_23 == 1);
        CHECK(rc_core_read_word(&m.core, 8, &saved) && saved == 0);
        CHECK(rc_core_read_word(&m.core, 10, &saved) && saved == (which == 1 ? 512U : 66U));
        rc_machine_destroy(&m);
    }
    for (unsigned op = 58; op < 64; ++op) {
        if (op == 61) continue;
        rc_machine_t m;
        setup(&m, 1, 1, 1, 0, 40, 100);
        CHECK(rc_core_write_word(&m.core, 64, rc_instruction_encode((uint8_t)op, 1, false, false, 0, 128)));
        CHECK(rc_machine_step(&m) == RC_STEP_OK && m.interrupts_serviced == 1);
        CHECK(m.cpu.w[1] == 1);
        rc_machine_destroy(&m);
    }
    CHECK(!rc_opcode_is_assigned(61));
    CHECK(rc_opcode_name(61) == NULL);
    CHECK(strcmp(rc_opcode_name_extended(61, true), "ix") == 0);
    rc_config_t config; rc_config_error_t error;
    CHECK(rc_config_parse_text("CPU IX=YES\n", &config, &error) == 0 && config.ix_enabled);
    CHECK(rc_config_parse_text("cpu ix=no\n", &config, &error) == 0 && !config.ix_enabled);
    CHECK(rc_config_parse_text("", &config, &error) == 0 && config.ix_enabled);
    CHECK(rc_config_parse_text("CPU IX=YES\nCPU IX=NO\n", &config, &error) != 0);
    CHECK(rc_config_parse_text("CPU IX=MAYBE\n", &config, &error) != 0);
    CHECK(rc_config_parse_text("CPU OTHER=YES\n", &config, &error) != 0);
    CHECK(rc_config_parse_text("CPU IX=YES extra\n", &config, &error) != 0);
}

int main(void)
{
    vectors(); invalid(); addressing(); wrap_and_pending(); am_exceptions(); faults_and_default();
    if (failures) return 1;
    puts("IX instruction and configuration checks passed"); return 0;
}
