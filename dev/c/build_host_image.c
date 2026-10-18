#define _POSIX_C_SOURCE 200809L

#include "sim4000/catalog.h"
#include "sim4000/dfc403.h"
#include "sim4000/machine.h"
#include "sim4000/monitor.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

enum { OUTPUT_SIZE = 65536 };
typedef struct {
    rc_word_t pc, raw, saved_ic, effective, w[4];
    uint8_t pr, key;
    bool monitor;
} protection_fault_t;
typedef struct {
    rc_machine_t machine;
    rc_clock_device_t clock;
    rc_console_device_t console;
    rc_console_device_t secondary;
    rc_console_device_t *input_console;
    const char *host_root;
    rc_rc433x_device_t disk;
    rc_dfc403_device_t controller;
    bool hardware;
    bool manual;
    bool monitor_build;
    bool runtime_probe;
    bool probe_catstat;
    rc_addr_t lookup_return;
    rc_addr_t lookup_tail;
    unsigned catalog_lookups;
    rc_word_t lookup_result;
    rc_word_t catalog_size;
    rc_word_t catalog_contents;
    bool unassigned_seen;
    rc_word_t unassigned_word;
    rc_word_t index_extension;
    rc_word_t index_value;
    rc_word_t index_descriptor[3];
    rc_word_t fault_interrupt;
    bool fault_entered;
    rc_hostfiles_device_t host;
    char output[OUTPUT_SIZE];
    size_t count;
    uint64_t calls;
    uint64_t protected_task_calls;
    uint64_t runtime_gg_probe_steps;
    protection_fault_t protection_faults[32];
    size_t protection_fault_count;
    bool task_pl_reported;
    bool isolation_armed;
    rc_addr_t isolation_first, isolation_top;
    unsigned isolation_opcode, isolation_fault_count;
    protection_fault_t isolation_fault;
    rc_addr_t nested_first, nested_top;
    uint64_t nested_child_steps;
    bool nested_bad_execution;
} session_t;

static void output(void *context, uint8_t c)
{
    session_t *session = context;
    if (c != 0 && session->count + 1U < sizeof(session->output)) {
        session->output[session->count++] = (char)c;
        session->output[session->count] = '\0';
    }
    if (c != 127) {
        (void)fputc(c, stdout);
        if (c == '\n') (void)fflush(stdout);
    }
}

static bool step(session_t *session)
{
    if (++session->calls % UINT64_C(10000) == 0) {
        rc_machine_advance_time(&session->machine, 1);
    }
    /* Observe only catstat's catalog lookup in the dedicated checkpoint.
     * Monitor call 42 uses w1 for the tail and w3 for the name, and returns
     * its result in w0 (monsrc/montxt11, p21). Never alter guest state. */
    if (session->probe_catstat && session->lookup_return != 0 &&
        session->machine.cpu.instruction_counter == session->lookup_return) {
        session->lookup_result = session->machine.cpu.w[0];
        if (!rc_core_read_word(&session->machine.core, session->lookup_tail,
                               &session->catalog_size) ||
            !rc_core_read_word(&session->machine.core, session->lookup_tail + 16U,
                               &session->catalog_contents))
            return false;
        ++session->catalog_lookups;
        (void)fprintf(stderr,
            "catstat catalog lookup: result=%u segments=%u contents=%u entry=%u\n",
            session->lookup_result, session->catalog_size,
            session->catalog_contents >> 12, session->catalog_contents & 4095U);
        session->lookup_return = 0;
    }
    /* Capture the first unassigned instruction after the successful lookup,
     * not the many unrelated instructions used for paging and FP recovery. */
    bool capture_fault = false;
    rc_addr_t fault_pc = session->machine.cpu.instruction_counter;
    protection_fault_t snapshot = {0};
    if (session->machine.protection_enabled || session->runtime_probe) {
        snapshot.pc = fault_pc;
        snapshot.pr = session->machine.cpu.protection_register;
        snapshot.monitor = session->machine.cpu.monitor_mode;
        (void)rc_core_read_key(&session->machine.core, fault_pc, &snapshot.key);
        memcpy(snapshot.w, session->machine.cpu.w, sizeof(snapshot.w));
        (void)rc_machine_read_word(&session->machine, fault_pc, &snapshot.raw);
    }
    uint64_t interrupts_before = session->machine.interrupts_serviced;
    uint64_t instructions_before = session->machine.instructions_executed;
    if (session->probe_catstat && session->catalog_lookups != 0 &&
        !session->unassigned_seen) {
        rc_word_t raw;
        if (rc_core_read_word(&session->machine.core, fault_pc, &raw) &&
            !rc_opcode_is_assigned(rc_instruction_decode(raw).opcode)) {
            session->unassigned_word = raw;
            session->index_value = session->machine.cpu.w[1];
            if (!rc_core_read_word(&session->machine.core, fault_pc + 2U,
                                   &session->index_extension)) return false;
            capture_fault = true;
        }
    }
    rc_step_result_t result = rc_machine_step(&session->machine);
    if (session->nested_top > session->nested_first &&
        session->machine.instructions_executed > instructions_before &&
        session->machine.last_instruction_address >= session->nested_first &&
        session->machine.last_instruction_address < session->nested_top) {
        ++session->nested_child_steps;
        if (session->machine.last_instruction.raw != UINT32_C(0x348000) ||
            session->machine.cpu.monitor_mode || session->machine.cpu.protection_register != UINT8_C(0xbf))
            session->nested_bad_execution = true;
    }
    if ((session->machine.protection_enabled || session->runtime_probe) &&
        session->machine.instructions_executed > instructions_before &&
        session->machine.last_instruction_address == snapshot.pc &&
        session->machine.last_instruction.raw == UINT32_C(0x730022))
        ++session->runtime_gg_probe_steps;
    if ((session->machine.protection_enabled || session->runtime_probe) &&
        session->machine.interrupts_serviced > interrupts_before) {
        rc_word_t vector;
        if (rc_machine_read_word(&session->machine, 8, &vector) && vector == 0) {
            (void)rc_machine_read_word(&session->machine, 10, &snapshot.saved_ic);
            snapshot.effective = session->machine.last_effective_address;
            session->protection_faults[session->protection_fault_count++ % 32] = snapshot;
            if (session->isolation_armed && !snapshot.monitor &&
                snapshot.pc >= session->isolation_first && snapshot.pc < session->isolation_top &&
                (snapshot.raw >> 18) == session->isolation_opcode) {
                session->isolation_fault = snapshot;
                ++session->isolation_fault_count;
            }
            if (!session->task_pl_reported && !snapshot.monitor &&
                (snapshot.raw >> 18) == 28 &&
                snapshot.saved_ic == rc_word(snapshot.pc + 2U)) {
                session->task_pl_reported = true;
                (void)fprintf(stderr, "First task PL context (data may resemble code):\n");
                for (int delta = -16; delta <= 16; delta += 2) {
                    rc_addr_t address = rc_word(snapshot.pc + (rc_word_t)delta);
                    rc_word_t raw;
                    char text[96];
                    if (rc_machine_read_word(&session->machine, address, &raw) &&
                        rc_instruction_format(raw, text, sizeof(text)) >= 0)
                        (void)fprintf(stderr, "%06x: %06x %s\n", address, raw, text);
                }
            }
        }
    }
    if (session->machine.protection_enabled && result == RC_STEP_OK &&
        !session->machine.cpu.monitor_mode)
        ++session->protected_task_calls;
    /* A pending clock/device interrupt may run before this instruction.
     * Capture only once it really executes, not at that earlier boundary. */
    if (capture_fault && session->machine.instructions_executed > instructions_before &&
        session->machine.last_instruction_address == fault_pc &&
        session->machine.last_instruction.raw == session->unassigned_word) {
        rc_word_t saved_pc;
        rc_addr_t base = session->machine.last_effective_address & ~UINT32_C(1);
        session->unassigned_seen = true;
        if (!rc_core_read_word(&session->machine.core, 8, &session->fault_interrupt) ||
            !rc_core_read_word(&session->machine.core, 10, &saved_pc)) return false;
        session->fault_entered = result == RC_STEP_OK &&
            session->machine.interrupts_serviced == interrupts_before + 1U &&
            saved_pc == rc_word(fault_pc + 2U) &&
            session->machine.last_instruction_address == fault_pc;
        for (unsigned i = 0; i < 3; ++i)
            if (!rc_core_read_word(&session->machine.core, base + i * 2U,
                                   &session->index_descriptor[i])) return false;
        (void)fprintf(stderr,
            "catstat first unassigned: opcode=%u word=%06x extension=%06x interrupt=%u\n",
            session->unassigned_word >> 18, session->unassigned_word,
            session->index_extension, session->fault_interrupt);
        (void)fprintf(stderr,
            "catstat index descriptor: index=%u lower=%u upper=%u target-is-tail=%u\n",
            session->index_value, session->index_descriptor[2],
            session->index_descriptor[1],
            (unsigned)(rc_word(session->index_descriptor[0] + 2U) == session->lookup_tail));
    }
    if (session->probe_catstat && result == RC_STEP_OK &&
        session->machine.last_instruction.opcode == 14 &&
        session->machine.last_instruction.result_register == 0 &&
        session->machine.last_effective_address == UINT32_C(0xfff82a)) {
        static const rc_word_t name[4] = {
            UINT32_C(0x636174), UINT32_C(0x616c6f), UINT32_C(0x670000), 0
        };
        bool matches = true;
        for (unsigned i = 0; i < 4; ++i) {
            rc_word_t value;
            if (!rc_core_read_word(&session->machine.core,
                rc_word(session->machine.cpu.w[3] + i * 2U), &value) ||
                value != name[i])
                matches = false;
        }
        if (matches) {
            session->lookup_return = session->machine.last_instruction_address + 2;
            session->lookup_tail = session->machine.cpu.w[1];
        }
    }
    return result == RC_STEP_OK;
}

static void report_protection_faults(const session_t *session)
{
    size_t count = session->protection_fault_count;
    size_t first = count > 32 ? count - 32 : 0;
    for (size_t i = first; i < count; ++i) {
        const protection_fault_t *f = &session->protection_faults[i % 32];
        (void)fprintf(stderr, "IRQ0 pc=%06x raw=%06x saved=%06x ea=%06x PR=%02x key=%u monitor=%u "
                       "W=%06x,%06x,%06x,%06x\n", f->pc, f->raw, f->saved_ic,
                       f->effective, (unsigned)f->pr, (unsigned)f->key,
                       (unsigned)f->monitor,
                       f->w[0], f->w[1], f->w[2], f->w[3]);
    }
}

static bool wait_text(session_t *session, size_t start, const char *text)
{
    uint64_t limit = session->calls + UINT64_C(10000000);
    while (session->calls < limit) {
        if (strstr(session->output + start, text) != NULL) {
            return true;
        }
        if (!step(session)) {
            break;
        }
        /* An explicitly expected program break may begin with an FP error
         * banner. Wait for its complete pause notice, never accept the banner
         * itself as completion. Other waits retain strict error handling. */
        if (strstr(session->output + start, "syntax error:") != NULL ||
            (strstr(text, " break fp") == NULL &&
             strstr(session->output + start, "***") != NULL)) {
            break;
        }
    }
    (void)fprintf(stderr, "session did not reach '%s'\n", text);
    if (session->machine.protection_enabled) {
        (void)fprintf(stderr, "protected state: IC=%06x PR=%02x monitor=%u task-calls=%" PRIu64 "\n",
                       session->machine.cpu.instruction_counter,
                       (unsigned)session->machine.cpu.protection_register,
                       (unsigned)session->machine.cpu.monitor_mode,
                       session->protected_task_calls);
    }
    return false;
}

/* Feed long lines incrementally, then wait for quiet output and an input
 * read (or a system reply in manual mode). Do not depend on absolute idle
 * addresses: maintained monitor variants legitimately change layout. */
static bool command_checked(session_t *session, const char *line,
                            const char *expected_diagnostic)
{
    uint64_t limit = session->calls + (session->monitor_build
        ? UINT64_C(500000000) : session->manual
        ? UINT64_C(30000000) : UINT64_C(300000000));
    uint64_t quiet = 0;
    size_t start = session->count;
    size_t last_output = start;
    const char *cursor = line;
    bool rejected = false;
    while (session->calls < limit) {
        if (*cursor != '\0' &&
            rc_console_device_submit(session->input_console, (uint8_t)*cursor)) {
            ++cursor;
        }
        if (!step(session)) {
            break;
        }
        if (strstr(session->output + start, "syntax error:") != NULL ||
            strstr(session->output + start, "***") != NULL) {
            rejected = true;
        }
        if (session->monitor_build &&
            (strstr(session->output + start, "pause monbuild") != NULL ||
             strstr(session->output + start, "bs claims exceeded") != NULL)) {
            (void)fprintf(stderr, "monitor build process ended or its resource claim was rejected\n");
            return false;
        }
        if ((session->machine.protection_enabled || session->runtime_probe) &&
            strstr(session->output + start, " break fp") != NULL) {
            (void)fprintf(stderr, "guest checkpoint program broke: %s\n", line);
            report_protection_faults(session);
            return false;
        }
        if (session->count == last_output) {
            ++quiet;
        } else {
            last_output = session->count;
            quiet = 0;
        }
        if (*cursor == '\0' && session->input_console->input_count == 0 &&
            (session->input_console->busy ||
             (session->manual && strstr(session->output + start, "ready") != NULL)) &&
            quiet >= UINT64_C(100000)) {
            (void)fflush(stdout);
            /* Expected failures still require command completion and a new
             * diagnostic from this command, never just an error somewhere in
             * the accumulated session transcript. Other calls stay strict. */
            if (expected_diagnostic != NULL) {
                if (strstr(session->output + start, expected_diagnostic) != NULL)
                    return true;
                (void)fprintf(stderr, "command did not report '%s': %s\n",
                              expected_diagnostic, line);
                return false;
            }
            if (rejected)
                (void)fprintf(stderr, "guest rejected command: %s\n", line);
            return !rejected;
        }
    }
    (void)fprintf(stderr, "command failed or did not finish: %s\n", line);
    return false;
}

static bool command(session_t *session, const char *line)
{
    return command_checked(session, line, NULL);
}

static bool manual_command(session_t *session, const char *line,
                           const char *expected)
{
    size_t start = session->count;
    if (!command(session, line)) return false;
    if (expected != NULL && !wait_text(session, start, expected)) return false;
    return true;
}

static bool select_system(session_t *session)
{
    size_t start = session->count;
    /* This is an operator request, not a character for a pending guest read.
     * Raw Escape delivery depends on whether console I/O is already busy. */
    rc_console_device_attention(session->input_console);
    if (!wait_text(session, start, "att ")) return false;
    return command(session, "s\r");
}

static bool pause_command(session_t *session, const char *line, const char *notice)
{
    size_t start = session->count;
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        if (!rc_console_device_submit(session->input_console, (uint8_t)*cursor)) return false;
    }
    /* A paused process cannot satisfy the normal next-console-read boundary. */
    return wait_text(session, start, notice);
}

static bool protected_upsoscat_probe(session_t *session)
{
    if (!manual_command(session, "catspec=rcin input\r", "bytes") ||
        !command(session, "upbinary=rcout conv.bin upsoscat\r") ||
        !command(session, "uplook=lookup upsoscat\r") ||
        !command(session, "upmetadata=rcout uplook\r")) return false;
    size_t first = session->protection_fault_count;
    size_t start = session->count;
    if (!pause_command(session, "soscat=upsoscat catspec\r", "pause hostcheck break fp")) return false;
    report_protection_faults(session);
    if (session->protection_fault_count > 32 && first < session->protection_fault_count - 32)
        first = session->protection_fault_count - 32;
    size_t matched = 0;
    for (size_t i = first; i < session->protection_fault_count; ++i) {
        const protection_fault_t *f = &session->protection_faults[i % 32];
        if (!f->monitor && f->raw == UINT32_C(0x730022) && f->effective == 34 &&
            f->saved_ic == rc_word(f->pc + 2U) && f->pr == UINT8_C(0xbf) && f->key == 1)
            ++matched;
    }
    if (matched != 1 || strstr(session->output + start, "tofrom1") == NULL) return false;
    (void)fprintf(stderr, "UPSOSCAT probe: task PL address=34 IRQ0 saved-next=yes PR=bf key=1\n");
    if (!select_system(session) ||
        !command_checked(session, "proc hostcheck start\r", "***fp reinitialized") ||
        !command(session, "inputafter=rcout catspec\r") ||
        !manual_command(session, "lookup upsoscat\r", "; system")) return false;
    (void)fprintf(stderr, "UPSOSCAT known incompatibility captured; FP recovered, not a catalog build\n");
    return true;
}

static bool protected_cmp_probe(session_t *session)
{
    if (!manual_command(session, "msrc=rcin input\r", "bytes") ||
        !command(session, "mcopy=copy msrc\r") ||
        !command(session, "cmpbinary=rcout conv.bin cmp\r") ||
        !command(session, "cmplook=lookup cmp\r") ||
        !command(session, "cmpmetadata=rcout cmplook\r")) return false;
    size_t first = session->protection_fault_count;
    size_t start = session->count;
    if (!pause_command(session, "cmp msrc mcopy\r", "pause hostcheck break fp")) return false;
    report_protection_faults(session);
    if (session->protection_fault_count > 32 && first < session->protection_fault_count - 32)
        first = session->protection_fault_count - 32;
    bool matched = false;
    for (size_t i = first; i < session->protection_fault_count; ++i) {
        const protection_fault_t *f = &session->protection_faults[i % 32];
        if (!f->monitor && f->raw == UINT32_C(0x730022) &&
            f->effective == 34 && f->saved_ic == rc_word(f->pc + 2U) &&
            f->pr == UINT8_C(0xbf) && f->key == 1) {
            matched = true;
            (void)fprintf(stderr, "CMP probe: task PL address=34 IRQ0 saved-next=yes PR=bf key=1\n");
        }
    }
    if (!matched || strstr(session->output + start, "tofrom1") == NULL ||
        strstr(session->output + start, "files are identical") != NULL) return false;
    if (!select_system(session) ||
        !command_checked(session, "proc hostcheck start\r", "***fp reinitialized") ||
        !command(session, "origafter=rcout msrc\r") ||
        !command(session, "copyafter=rcout mcopy\r")) return false;
    (void)fprintf(stderr, "CMP known incompatibility localized; FP recovered, not a passing comparison\n");
    return true;
}

static bool manual_session(session_t *session)
{
    if (!manual_command(session, "list\r", "ready") ||
        !select_system(session) ||
        !manual_command(session, "max\r", "max") ||
        !select_system(session) ||
        !manual_command(session, "date 86 09 30 08 00 00\r", "ready") ||
        !select_system(session) ||
        !manual_command(session,
            "new muser size 100000 temp disc 200 10 run\r", "to muser") ||
        !manual_command(session, "lookup fromhost tohost slang algol\r", "fromhost") ||
        !manual_command(session, "catsort\r", "2 entr.") ||
        !manual_command(session, "catsort scope.system\r", "414 entr.") ||
        !manual_command(session, "claim\r", "disc: 3 segm/slice") ||
        !manual_command(session, "claim\r", "disc: 3 segm/slice") ||
        !manual_command(session, "show\r", "free disc space:") ||
        !manual_command(session, "o sortreport\r", NULL) ||
        !manual_command(session, "catsort scope.system\r", NULL) ||
        !manual_command(session, "o c\r", NULL) ||
        !manual_command(session, "sortout=tohost sortreport\r", "bytes") ||
        !pause_command(session, "catstat sortreport\r", "pause muser break fp") ||
        !select_system(session) ||
        !command_checked(session, "proc muser start\r", "***fp reinitialized") ||
        !manual_command(session, "sortagain=tohost sortreport\r", "bytes") ||
        !manual_command(session, "lookup sortreport\r", "sortreport") ||
        !manual_command(session, "clear temp sortreport\r", NULL) ||
        !command_checked(session, "lookup sortreport\r", "***lookup sortreport unknown") ||
        !manual_command(session, "show\r", "free disc space:") ||
        !manual_command(session, "spacetmp=fromhost input\r", "bytes") ||
        !manual_command(session, "lookup spacetmp\r", "spacetmp") ||
        !manual_command(session, "claim\r", "disc: 3 segm/slice") ||
        !manual_command(session, "show\r", "free disc space:") ||
        !manual_command(session, "clear temp spacetmp\r", NULL) ||
        !command_checked(session, "lookup spacetmp\r", "***lookup spacetmp unknown") ||
        !manual_command(session, "claim\r", "disc: 3 segm/slice") ||
        !manual_command(session, "show\r", "free disc space:") ||
        !manual_command(session, "msrc=fromhost input\r", "bytes") ||
        !manual_command(session, "lookup msrc\r", "msrc") ||
        !manual_command(session, "c=copy msrc\r", "Monitor manual sample.") ||
        !manual_command(session, "mcopy=copy msrc\r", NULL) ||
        !manual_command(session, "rename mcopy.moved\r", NULL) ||
        !manual_command(session, "lookup moved\r", "moved") ||
        !manual_command(session, "c=copy msrc moved\r", "Second line.") ||
        !manual_command(session, "output=tohost moved\r", "bytes") ||
        !manual_command(session, "medit=edit msrc\r", NULL) ||
        !manual_command(session, "l./Monitor/,r/Monitor/Edited/,f\r", NULL) ||
        !manual_command(session, "edited=tohost medit\r", "bytes") ||
        !manual_command(session, "cmp msrc moved\r", "files are identical") ||
        !manual_command(session, "cmp msrc medit\r", "1 discrepancy found") ||
        !manual_command(session, "o cmpreport\r", NULL) ||
        !manual_command(session, "cmp msrc medit\r", NULL) ||
        !manual_command(session, "o c\r", NULL) ||
        !manual_command(session, "cmpout=tohost cmpreport\r", "bytes") ||
        !manual_command(session, "cmporig=tohost msrc\r", "bytes") ||
        !manual_command(session, "cmpcopy=tohost moved\r", "bytes") ||
        !manual_command(session, "cmpedit=tohost medit\r", "bytes") ||
        !manual_command(session, "clear temp cmpreport\r", NULL) ||
        !manual_command(session, "mlines=fromhost edinput\r", "bytes") ||
        !manual_command(session, "mpatch=edit mlines\r", NULL) ||
        !manual_command(session,
            "l1,l1,d0,i!\rInserted line.\rLiteral comma, slash / preserved.\r!\rf\r", NULL) ||
        !manual_command(session, "edops=tohost mpatch\r", "bytes") ||
        !manual_command(session, "mcut=edit mlines\r", NULL) ||
        !manual_command(session, "l2,d1,f\r", NULL) ||
        !manual_command(session, "edcut=tohost mcut\r", "bytes") ||
        !manual_command(session, "edorig=tohost mlines\r", "bytes") ||
        !manual_command(session, "clear temp mlines mpatch mcut\r", NULL) ||
        !manual_command(session, "sosstart=tohost startsos\r", "bytes") ||
        !manual_command(session, "asrc=fromhost alginput\r", "bytes") ||
        !manual_command(session, "mprog=algol asrc connect.no\r", "algol end") ||
        !manual_command(session, "mprog\r", "Hello from Algol") ||
        !manual_command(session, "clear temp msrc moved medit asrc mprog\r", NULL) ||
        !manual_command(session, "slsrc=fromhost slinput\r", "bytes") ||
        !manual_command(session, "slcmd=fromhost cmdinput\r", "bytes") ||
        !manual_command(session, "i slcmd\r", "bytes") ||
        !manual_command(session, "lookup slprog\r", "slprog") ||
        !manual_command(session, "clear temp slsrc slcmd slist slprog\r", NULL) ||
        !manual_command(session, "outercmd=fromhost outerinput\r", "bytes") ||
        !manual_command(session, "innercmd=fromhost innerinput\r", "bytes") ||
        !manual_command(session, "i outercmd\r", "bytes") ||
        !manual_command(session, "lookup slang\r", "slang") ||
        !manual_command(session, "clear temp outercmd innercmd nestlog\r", NULL) ||
        !command_checked(session, "lookup outercmd innercmd nestlog\r",
                         "***lookup outercmd unknown") ||
        !manual_command(session, "errcmd=fromhost errinput\r", "bytes") ||
        !command_checked(session, "i errcmd\r", "***fp name nosuchprog") ||
        !manual_command(session, "lookup slang\r", "slang") ||
        !manual_command(session, "errtop=fromhost errtopin\r", "bytes") ||
        !command_checked(session, "i errtop\r", "***fp name nosuchprog") ||
        !manual_command(session, "lookup slang\r", "slang") ||
        !manual_command(session, "clear temp errcmd errtop\r", NULL) ||
        !command_checked(session, "lookup errcmd errtop\r", "***lookup errcmd unknown") ||
        !manual_command(session, "batchsrc=fromhost badinput\r", "bytes") ||
        !manual_command(session, "batchcmd=fromhost asmerrin\r", "bytes") ||
        !command_checked(session, "i batchcmd\r", "***slang sorry") ||
        !command_checked(session, "lookup batchbad\r", "***lookup batchbad unknown") ||
        !manual_command(session, "lookup slang\r", "slang") ||
        !manual_command(session, "clear temp batchsrc batchcmd\r", NULL) ||
        !command_checked(session, "lookup batchsrc batchcmd\r",
                         "***lookup batchsrc unknown") ||
        !manual_command(session, "condcmd=fromhost condinput\r", "bytes") ||
        !command_checked(session, "i condcmd\r", "***lookup absentone unknown") ||
        !manual_command(session, "lookup slang\r", "slang") ||
        !manual_command(session, "clear temp condcmd\r", NULL) ||
        !command_checked(session, "lookup condcmd\r", "***lookup condcmd unknown") ||
        !manual_command(session, "checksrc=fromhost badinput\r", "bytes") ||
        !manual_command(session, "checkvalid=fromhost slinput\r", "bytes") ||
        !manual_command(session, "checkcmd=fromhost statusin\r", "bytes") ||
        !command_checked(session, "i checkcmd\r", "***slang sorry") ||
        !command_checked(session, "lookup checkbad checkgood\r", "***lookup checkbad unknown") ||
        !manual_command(session, "lookup slang\r", "slang") ||
        !manual_command(session, "clear temp checksrc checkvalid checkcmd\r", NULL) ||
        !command_checked(session, "lookup checksrc checkvalid checkcmd\r",
                         "***lookup checksrc unknown") ||
        !command_checked(session, "nosuchprog\r", "***fp name nosuchprog") ||
        !command_checked(session, "lookup nosuchprog\r", "***lookup nosuchprog unknown") ||
        !manual_command(session, "lookup slang\r", "slang") ||
        !manual_command(session, "badsrc=fromhost badinput\r", "bytes") ||
        !command_checked(session, "badprog=slang badsrc\r", "***slang sorry") ||
        !manual_command(session, "lookup badprog\r", "2.4 4") ||
        !manual_command(session, "clear temp badprog\r", NULL) ||
        !command_checked(session, "lookup badprog\r", "***lookup badprog unknown") ||
        !manual_command(session, "fixsrc=fromhost slinput\r", "bytes") ||
        !manual_command(session, "fixprog=slang fixsrc\r", "slang ok") ||
        !manual_command(session, "recovered=tohost conv.bin fixprog\r", "bytes") ||
        !manual_command(session, "clear temp badsrc fixsrc fixprog\r", NULL) ||
        !select_system(session) ||
        !manual_command(session, "proc muser stop\r", "ready") ||
        !select_system(session) ||
        !manual_command(session, "list\r", "muser") ||
        !select_system(session) ||
        !manual_command(session, "proc muser start\r", "to muser")) return false;
    {
        size_t start = session->count;
        const char *line = "finis\r";
        while (*line != '\0')
            if (!rc_console_device_submit(&session->console, (uint8_t)*line++))
                return false;
        if (!wait_text(session, start, "pause muser finis fp")) return false;
    }
    return select_system(session) &&
           manual_command(session, "list\r", "ready") &&
           select_system(session) &&
           manual_command(session, "new mremove size 100000 run\r", "to mremove") &&
           select_system(session) &&
           manual_command(session, "proc mremove remove\r", "ready") &&
           select_system(session) &&
           manual_command(session, "list\r", "ready");
}

static bool finish_process(session_t *session, const char *expected)
{
    size_t start = session->count;
    return rc_console_device_submit(&session->console, 'f') &&
        rc_console_device_submit(&session->console, 'i') &&
        rc_console_device_submit(&session->console, 'n') &&
        rc_console_device_submit(&session->console, 'i') &&
        rc_console_device_submit(&session->console, 's') &&
        rc_console_device_submit(&session->console, '\r') &&
        wait_text(session, start, expected);
}

typedef struct {
    rc_addr_t first, top;
    uint8_t key, before, after;
} process_keys_t;

static bool check_process_keys(session_t *session, const process_keys_t *area,
                               uint8_t expected, const char *stage)
{
    uint8_t key;
    for (rc_addr_t address = area->first; address < area->top; address += 2) {
        if (!rc_core_read_key(&session->machine.core, address, &key) || key != expected) {
            (void)fprintf(stderr, "%s: wrong/unavailable key at %06x, expected %u\n",
                           stage, address, (unsigned)expected);
            return false;
        }
    }
    if (!rc_core_read_key(&session->machine.core, area->first - 2, &key) ||
        key != area->before ||
        !rc_core_read_key(&session->machine.core, area->top, &key) || key != area->after) {
        (void)fprintf(stderr, "%s: adjacent word keys changed\n", stage);
        return false;
    }
    (void)fprintf(stderr, "Process keys %s: [%u,%u) words=%u key=%u boundaries=unchanged\n",
                   stage, area->first, area->top, (area->top - area->first) / 2,
                   (unsigned)expected);
    return true;
}

static bool inspect_process_keys(session_t *session, unsigned expected_size,
                                 process_keys_t *area)
{
    size_t start = session->count;
    unsigned size, first;
    if (!manual_command(session, "claim\r", "first core")) return false;
    const char *report = strstr(session->output + start, "size ");
    if (report == NULL || sscanf(report, "size %u first core %u", &size, &first) != 2 ||
        size != expected_size || first < 2 || (first & 1U) || (size & 1U) ||
        first >= session->machine.core.halfword_count ||
        size >= session->machine.core.halfword_count - first) {
        (void)fprintf(stderr, "invalid/unexpected process claim bounds\n");
        return false;
    }
    area->first = first;
    area->top = first + size;
    if (!rc_core_read_key(&session->machine.core, first, &area->key) ||
        area->key == 0 ||
        !rc_core_read_key(&session->machine.core, first - 2, &area->before) ||
        !rc_core_read_key(&session->machine.core, area->top, &area->after)) return false;
    return check_process_keys(session, area, area->key, "running");
}

static bool protected_lifecycle_session(session_t *session)
{
    process_keys_t original, recreated;
    if (!inspect_process_keys(session, 100000, &original) ||
        !select_system(session) ||
        !manual_command(session, "proc hostcheck stop\r", "ready") ||
        !check_process_keys(session, &original, 0, "stopped") ||
        !select_system(session) ||
        !manual_command(session, "proc hostcheck start\r", "to hostcheck") ||
        !check_process_keys(session, &original, original.key, "resumed") ||
        !manual_command(session, "resumed=rcout hftest\r", "bytes") ||
        !manual_command(session, "binresume=rcout conv.bin hfbin\r", "bytes") ||
        /* Direct system-level removal does not run FP file cleanup. Do that
         * explicitly rather than claiming catalog reclamation from key checks. */
        !manual_command(session, "clear temp hftest hfbin\r", NULL) ||
        !command_checked(session, "lookup hftest\r", "***lookup hftest unknown") ||
        !command_checked(session, "lookup hfbin\r", "***lookup hfbin unknown") ||
        !select_system(session) ||
        !manual_command(session, "proc hostcheck remove\r", "ready") ||
        !check_process_keys(session, &original, 0, "removed") ||
        !select_system(session)) return false;
    size_t start = session->count;
    if (!manual_command(session, "list\r", "ready") ||
        strstr(session->output + start, "hostcheck") != NULL) {
        (void)fprintf(stderr, "removed process remains in system list\n");
        return false;
    }
    if (!select_system(session) ||
        !manual_command(session, "new hostcheck size 100006 run\r", "to hostcheck") ||
        !inspect_process_keys(session, 100006, &recreated) ||
        !command_checked(session, "lookup hftest\r", "***lookup hftest unknown") ||
        !command_checked(session, "lookup hfbin\r", "***lookup hfbin unknown") ||
        !manual_command(session, "renewed=rcin input\r", "bytes") ||
        !manual_command(session, "recreated=rcout renewed\r", "bytes") ||
        !manual_command(session, "clear temp renewed\r", NULL) ||
        !select_system(session) ||
        !manual_command(session, "proc hostcheck remove\r", "ready") ||
        !check_process_keys(session, &recreated, 0, "recreated-removed")) return false;
    (void)fprintf(stderr, "Protected process lifecycle checkpoint passed\n");
    return true;
}

static bool write_isolation_source(const session_t *session, unsigned test,
                                    rc_addr_t target, rc_word_t original)
{
    static const char *operations[] = {"rs w2 x1", "jl w3 x1", "pl w0 0", "ks w2 x1"};
    size_t length = strlen(session->host_root) + 32;
    char *path = malloc(length);
    if (path == NULL || test >= 4) { free(path); return false; }
    (void)snprintf(path, length, "%s/atk%usrc", session->host_root, test);
    FILE *file = fopen(path, "wx");
    free(path);
    if (file == NULL) return false;
    /* All probes first perform a legitimate cross-key data read. Only the
     * subsequent write/jump/privileged operation is expected to fault. */
    int written = fprintf(file,
        "; Generated guest isolation probe; assembled by guest SLANG.\n"
        "s. a1 w.\n0,0\nrl. w1 a0.\nrl w0 x1\nal w2 %u\n%s\njl. 0\na0: %u\ne.\n",
        test == 3 ? 0U : original == 1 ? 2U : 1U, operations[test], target);
    bool ok = written > 0;
    if (fclose(file) != 0) ok = false;
    return ok;
}

static bool protected_isolation_session(session_t *session)
{
    static const unsigned opcodes[] = {23, 13, 28, 51}; /* RS, JL, PL, KS */
    process_keys_t victim, attacker;
    rc_word_t *saved = NULL;
    bool ok = false;
    if (!inspect_process_keys(session, 100000, &victim)) return false;
    session->input_console = &session->secondary;
    if (!select_system(session) ||
        !manual_command(session, "new attacker size 100000 run\r", "to attacker") ||
        !inspect_process_keys(session, 100000, &attacker) ||
        victim.key == attacker.key ||
        !(victim.top <= attacker.first || attacker.top <= victim.first)) {
        (void)fprintf(stderr, "independent keyed sibling setup failed\n");
        return false;
    }
    /* A newly allocated neighbor may legitimately change a boundary key.
     * Capture the stable two-process layout before testing fault effects. */
    if (!rc_core_read_key(&session->machine.core, victim.first - 2, &victim.before) ||
        !rc_core_read_key(&session->machine.core, victim.top, &victim.after)) return false;
    size_t words = (victim.top - victim.first) / 2;
    saved = malloc(words * sizeof(*saved));
    if (saved == NULL) return false;
    for (unsigned test = 0; test < 4; ++test) {
        char line[96];
        rc_addr_t target = victim.top - 2;
        rc_word_t original;
        if (!rc_machine_read_word(&session->machine, target, &original) ||
            !write_isolation_source(session, test, target, original)) goto done;
        (void)snprintf(line, sizeof(line), "src%u=rcin atk%usrc\r", test, test);
        if (!manual_command(session, line, "bytes")) goto done;
        (void)snprintf(line, sizeof(line), "bad%u=slang src%u\r", test, test);
        if (!manual_command(session, line, "slang ok")) goto done;
        for (size_t i = 0; i < words; ++i)
            if (!rc_machine_read_word(&session->machine, victim.first + (rc_addr_t)(i * 2),
                                       &saved[i])) goto done;
        session->isolation_first = attacker.first;
        session->isolation_top = attacker.top;
        session->isolation_opcode = opcodes[test];
        session->isolation_fault_count = 0;
        session->isolation_armed = true;
        (void)snprintf(line, sizeof(line), "bad%u\r", test);
        bool paused = pause_command(session, line, "pause attacker break fp");
        session->isolation_armed = false;
        protection_fault_t *f = &session->isolation_fault;
        if (!paused || session->isolation_fault_count != 1 ||
            f->saved_ic != rc_word(f->pc + 2) || f->key != attacker.key ||
            (f->pr & (UINT8_C(0x80) >> victim.key)) == 0 ||
            (f->pr & (UINT8_C(0x80) >> attacker.key)) != 0 ||
            f->w[0] != original ||
            (test == 0 && f->w[2] != (original == 1 ? 2U : 1U)) ||
            (test == 3 && f->w[2] != 0) ||
            f->effective != (test == 2 ? 0 : target)) {
            (void)fprintf(stderr, "isolation probe %u did not reach its expected task fault\n", test);
            report_protection_faults(session);
            goto done;
        }
        for (size_t i = 0; i < words; ++i) {
            rc_word_t value;
            if (!rc_machine_read_word(&session->machine, victim.first + (rc_addr_t)(i * 2),
                                       &value) || value != saved[i]) {
                (void)fprintf(stderr, "isolation probe %u changed victim word %zu\n", test, i);
                goto done;
            }
        }
        if (!check_process_keys(session, &attacker, 0, "fault-stopped-attacker")) goto done;
        /* The monitor stops the faulting sibling and returns its area to
         * the system parent's key. A contiguous victim boundary follows
         * that legitimate transition; the victim's own keys must not change. */
        if (victim.top == attacker.first) victim.after = 0;
        if (attacker.top == victim.first) victim.before = 0;
        if (!check_process_keys(session, &victim, victim.key, "unaffected-sibling")) goto done;
        (void)fprintf(stderr, "Isolation probe %u: opcode=%u pc=%06x ea=%06x PR=%02x "
                       "read=allowed fault=IRQ0 sibling-data=unchanged\n",
                       test, opcodes[test], f->pc, f->effective, (unsigned)f->pr);
        session->input_console = &session->console;
        (void)snprintf(line, sizeof(line), "after%u=rcout hftest\r", test);
        if (!manual_command(session, line, "bytes")) goto done;
        session->input_console = &session->secondary;
        if (!select_system(session)) goto done;
        if (!command_checked(session, "proc attacker start\r", "***fp reinitialized") ||
            !check_process_keys(session, &attacker, attacker.key, "recovered-attacker")) goto done;
        if (victim.top == attacker.first) victim.after = attacker.key;
        if (attacker.top == victim.first) victim.before = attacker.key;
    }
    (void)fprintf(stderr, "Protected independent-process isolation checkpoint passed\n");
    ok = true;
done:
    session->isolation_armed = false;
    free(saved);
    return ok;
}

static bool compile_protected_transfer(session_t *session, const char *line,
                                        const char *summary, bool system_binding)
{
    size_t start = session->count;
    if (!manual_command(session, line, summary)) return false;
    bool bases = strstr(session->output + start, "oldalgollib bases") != NULL;
    (void)fprintf(stderr, "Algol transfer binding=%s base-diagnostic=%s\n",
                   system_binding ? "system" : "temporary", bases ? "present" : "absent");
    if (system_binding && bases) {
        (void)fprintf(stderr, "system-scoped binding still produces a library-base diagnostic\n");
        return false;
    }
    return true;
}

static bool protected_algol_session(session_t *session, bool system_binding)
{
    if (!manual_command(session, "asrc=rcin alginput\r", "bytes") ||
        !manual_command(session, "hello=algol asrc connect.no\r", "algol end") ||
        !manual_command(session, "hello\r", "Hello from Algol") ||
        !manual_command(session, "arithsrc=rcin arithinput\r", "bytes") ||
        !manual_command(session, "arith=algol arithsrc connect.no\r", "algol end") ||
        !manual_command(session, "arith\r", "Protected arithmetic ok") ||
        !manual_command(session, "hellobin=rcout conv.bin hello\r", "bytes") ||
        !manual_command(session, "arithbin=rcout conv.bin arith\r", "bytes") ||
        !manual_command(session, "phsrc=rcin frominput\r", "bytes") ||
        !manual_command(session, "ptsrc=rcin toinput\r", "bytes") ||
        (!system_binding &&
         !manual_command(session, "tofrom=set bs oldalgollib 2048.22 83.2665 0 46.14 1.0\r", NULL)) ||
        !manual_command(session, "lookup tofrom oldalgollib\r", "oldalgollib") ||
        !compile_protected_transfer(session, "pcin=algol phsrc connect.no\r", "algol end 54", system_binding) ||
        !compile_protected_transfer(session, "pcout=algol ptsrc connect.no\r", "algol end 46", system_binding) ||
        (!system_binding &&
         !manual_command(session, "tofrom=set bs algollib 2048.24 83.2665 0 49.16 2.0\r", NULL)) ||
        !manual_command(session, "lookup tofrom\r", system_binding
            ? "=set bs oldalgollib 2048.22 83.2665 0 46.14 1.0"
            : "=set bs algollib 2048.24 83.2665 0 49.16 2.0") ||
        !manual_command(session, "ptext=pcin input\r", "bytes") ||
        !manual_command(session, "algtext=pcout ptext\r", "bytes") ||
        !manual_command(session, "pbin=pcin conv.bin binary\r", "bytes") ||
        !manual_command(session, "algbinary=pcout conv.bin pbin\r", "bytes") ||
        !manual_command(session, "pcinbin=pcout conv.bin pcin\r", "bytes") ||
        !manual_command(session, "pcoutbin=pcout conv.bin pcout\r", "bytes")) return false;
    (void)fprintf(stderr, "Protected guest Algol compilation/execution checkpoint passed\n");
    return true;
}

static bool validate_system_binding(const char *image)
{
    for (unsigned main_catalog = 0; main_catalog < 2; ++main_catalog) {
        rc_catalog_t catalog;
        char error[256];
        if ((main_catalog ? rc_catalog_read_main(image, &catalog, error, sizeof(error))
                          : rc_catalog_read(image, &catalog, error, sizeof(error))) != 0) {
            (void)fprintf(stderr, "cannot validate derived binding: %s\n", error);
            return false;
        }
        const rc_catalog_entry_t *binding = rc_catalog_find(&catalog, "tofrom");
        const rc_catalog_entry_t *library = rc_catalog_find(&catalog, "oldalgollib");
        bool ok = binding != NULL && library != NULL &&
            binding->lower_base == UINT32_C(0x800001) &&
            binding->upper_base == UINT32_C(0x7ffffd) &&
            binding->permission_key == 3 && library->permission_key == 3 &&
            binding->lower_base == library->lower_base &&
            binding->upper_base == library->upper_base &&
            strcmp(binding->document, "oldalgollib") == 0;
        if (ok)
            (void)fprintf(stderr, "Derived %s catalog binding: matched system bases=%06x,%06x key=3\n",
                           main_catalog ? "main" : "device", binding->lower_base, binding->upper_base);
        else
            (void)fprintf(stderr, "derived binding/library system bases do not match\n");
        rc_catalog_destroy(&catalog);
        if (!ok) return false;
    }
    return true;
}

static bool find_internal_process(session_t *session, const char *name,
                                  rc_addr_t first, rc_addr_t top, rc_addr_t *descriptor)
{
    unsigned found = 0;
    for (rc_addr_t address = 8; address + 82 < session->machine.core.halfword_count; address += 2) {
        rc_word_t kind, low, high;
        if (!rc_machine_read_word(&session->machine, address, &kind) || kind != 0 ||
            !rc_machine_read_word(&session->machine, address + 22, &low) || low != first ||
            !rc_machine_read_word(&session->machine, address + 24, &high) || high != top) continue;
        char stored[13] = {0};
        for (unsigned i = 0; i < 4; ++i) {
            rc_word_t word;
            if (!rc_machine_read_word(&session->machine, address + 2 + i * 2, &word)) return false;
            for (unsigned j = 0; j < 3; ++j) stored[i * 3 + j] = (char)(word >> (16 - j * 8));
        }
        if (strcmp(stored, name) == 0) { *descriptor = address; ++found; }
    }
    if (found != 1) (void)fprintf(stderr, "expected one internal %s descriptor, found %u\n", name, found);
    return found == 1;
}

static bool write_nested_source(session_t *session, unsigned test, rc_addr_t parent,
                                 rc_addr_t child, unsigned size, rc_addr_t stage)
{
    rc_word_t base[4];
    for (unsigned i = 0; i < 4; ++i)
        /* a44/a45 name the final words of their double-word intervals. */
        if (!rc_machine_read_word(&session->machine, parent + 72 + i * 2, &base[i])) return false;
    size_t length = strlen(session->host_root) + 32;
    char *path = malloc(length);
    if (path == NULL) return false;
    (void)snprintf(path, length, "%s/nest%usrc", session->host_root, test);
    FILE *file = fopen(path, "wx");
    free(path);
    if (file == NULL) return false;
    int written = fprintf(file,
        "; Guest parent creates, starts and stops its own key-1 child.\n"
        "s. a20 w.\n0,0\n"
        "rl. w1 a10.\nal w0 0\nrs w0 x1\n"
        "rl. w1 a11.\nrl. w0 a12.\nrs w0 x1\n"
        "al. w1 a13.\nal. w3 a14.\njd 1<11+56\n"
        "se w0 1\njl. a9.\nrl. w1 a10.\nrs w0 x1-4\n"
        "rl. w0 a17.\nrs. w0 a18.\n"
        "al. w1 a13.\nal. w3 a14.\njd 1<11+56\n"
        "se w0 0\njl. a9.\n"
        "al. w3 a14.\njd 1<11+58\nse w0 0\njl. a9.\n"
        "rl. w1 a10.\nal w0 1\nrs w0 x1\nrl. w2 a15.\n"
        "a1: al w2 x2-1\nsn w2 0\njl. a2.\njl. a1.\n"
        "a2: al. w3 a14.\njd 1<11+60\nse w0 0\njl. a9.\n"
        "al. w1 a16.\njd 1<11+18\nse w0 1\njl. a9.\n"
        "rl. w1 a10.\nal w0 2\nrs w0 x1\na3: jl. a3.\n"
        "a9: rl. w1 a10.\nrs w0 x1-2\nal w0 9\nrs w0 x1\njl. 0\n"
        "a10: %u\na11: %u\na12: jl. 0\n"
        "a13: %u,%u,0,0\na18: 128<12+1,%u,%u,%u,%u\n"
        "a14: <:nested:>,0,0\na15: 500000\na16: 0,r.8\na17: 191<12+1\ne.\n",
        stage, child, child, child + size, base[0], base[1], base[2], base[3]);
    bool ok = written > 0;
    if (fclose(file) != 0) ok = false;
    return ok;
}

static bool nested_stage(session_t *session, rc_addr_t address, rc_word_t expected,
                          const char *line)
{
    uint64_t limit = session->calls + UINT64_C(10000000);
    const char *cursor = line == NULL ? "" : line;
    while (session->calls < limit) {
        if (*cursor && rc_console_device_submit(session->input_console, (uint8_t)*cursor)) ++cursor;
        if (!step(session)) break;
        rc_word_t stage;
        if (*cursor == '\0' && rc_machine_read_word(&session->machine, address, &stage)) {
            if (stage == expected) return true;
            if (stage == 9) {
                rc_word_t error;
                if (rc_machine_read_word(&session->machine, address - 2, &error))
                    (void)fprintf(stderr, "nested guest monitor result=%u\n", error);
                break;
            }
        }
    }
    (void)fprintf(stderr, "nested guest did not reach stage %u\n", expected);
    report_protection_faults(session);
    return false;
}

static bool nested_region_keys(session_t *session, const process_keys_t *area,
                                rc_addr_t child, unsigned size, uint8_t parent_key,
                                uint8_t child_key, const char *label)
{
    for (rc_addr_t address = area->first; address < area->top; address += 2) {
        uint8_t actual, expected = address >= child && address < child + size ? child_key : parent_key;
        if (!rc_core_read_key(&session->machine.core, address, &actual) || actual != expected) {
            (void)fprintf(stderr, "nested %s key mismatch at %u, expected %u\n", label, address, expected);
            return false;
        }
    }
    uint8_t boundary;
    if (!rc_core_read_key(&session->machine.core, area->first - 2, &boundary) ||
        boundary != area->before ||
        !rc_core_read_key(&session->machine.core, area->top, &boundary) || boundary != area->after) {
        (void)fprintf(stderr, "nested %s changed an outer boundary key\n", label);
        return false;
    }
    (void)fprintf(stderr, "Nested keys %s: parent=%u child=%u size=%u full-area=checked\n",
                   label, parent_key, child_key, size);
    return true;
}

static bool protected_nested_session(session_t *session)
{
    static const unsigned sizes[] = {2, 20, 26};
    for (unsigned test = 0; test < 3; ++test) {
        process_keys_t area;
        rc_addr_t parent = 0, child_descriptor = 0;
        if (!inspect_process_keys(session, 100000, &area) || area.key != 2 ||
            !find_internal_process(session, "hostcheck", area.first, area.top, &parent)) return false;
        rc_word_t parent_pr_key;
        if (!rc_machine_read_word(&session->machine, parent + 32, &parent_pr_key) ||
            parent_pr_key != (UINT32_C(0x9f) << 12 | 2)) return false;
        rc_addr_t child = area.top - 26, stage = child - 2;
        if (!write_nested_source(session, test, parent, child, sizes[test], stage)) return false;
        char line[96];
        (void)snprintf(line, sizeof(line), "nsrc%u=rcin nest%usrc\r", test, test);
        if (!manual_command(session, line, "bytes")) return false;
        (void)snprintf(line, sizeof(line), "nprog%u=slang nsrc%u\r", test, test);
        if (!manual_command(session, line, "slang ok")) return false;
        session->nested_first = child;
        session->nested_top = child + sizes[test];
        session->nested_child_steps = 0;
        session->nested_bad_execution = false;
        (void)snprintf(line, sizeof(line), "nprog%u\r", test);
        if (!nested_stage(session, stage, 1, line) ||
            !find_internal_process(session, "nested", child, child + sizes[test], &child_descriptor)) return false;
        rc_word_t actual_parent, pr_key, denial, parent_claims;
        if (!rc_machine_read_word(&session->machine, child_descriptor + 50, &actual_parent) ||
            actual_parent != parent ||
            !rc_machine_read_word(&session->machine, child_descriptor + 32, &pr_key) ||
            pr_key != (UINT32_C(0xbf) << 12 | 1) ||
            !rc_machine_read_word(&session->machine, stage - 4, &denial) || denial != 1 ||
            !rc_machine_read_word(&session->machine, parent + 26, &parent_claims) ||
            !nested_region_keys(session, &area, child, sizes[test], 2, 1, "running")) return false;
        /* Let the one-word child execute too; parent stage 1 has a bounded
         * guest countdown, not a host-written rendezvous flag. */
        for (unsigned i = 0; i < 10000; ++i) if (!step(session)) return false;
        if (session->nested_child_steps == 0 || !select_system(session) ||
            !manual_command(session, "proc hostcheck stop\r", "ready") ||
            !nested_region_keys(session, &area, child, sizes[test], 0, 0, "tree-stopped")) return false;
        uint64_t stopped_steps = session->nested_child_steps;
        for (unsigned i = 0; i < 10000; ++i) if (!step(session)) return false;
        if (session->nested_child_steps != stopped_steps || !select_system(session) ||
            !manual_command(session, "proc hostcheck start\r", "ready") ||
            !nested_region_keys(session, &area, child, sizes[test], 2, 1, "tree-resumed") ||
            !nested_stage(session, stage, 2, NULL) ||
            session->nested_child_steps <= stopped_steps ||
            !nested_region_keys(session, &area, child, sizes[test], 2, 2, "child-stopped-by-parent")) return false;
        rc_word_t returned_claims;
        if (!rc_machine_read_word(&session->machine, parent + 26, &returned_claims) ||
            returned_claims != parent_claims || session->nested_bad_execution) return false;
        stopped_steps = session->nested_child_steps;
        for (unsigned i = 0; i < 10000; ++i) if (!step(session)) return false;
        if (session->nested_child_steps != stopped_steps || !select_system(session) ||
            !manual_command(session, "proc hostcheck remove\r", "ready") ||
            !nested_region_keys(session, &area, child, sizes[test], 0, 0, "tree-removed")) return false;
        rc_word_t name;
        if (!rc_machine_read_word(&session->machine, parent + 2, &name) || name != 0 ||
            !rc_machine_read_word(&session->machine, child_descriptor + 2, &name) || name != 0) return false;
        (void)fprintf(stderr, "Nested guest case size=%u passed; privilege-widening=denied "
                       "stop-answer=released child-steps=%" PRIu64 "\n",
                       sizes[test], stopped_steps);
        session->nested_first = session->nested_top = 0;
        if (test + 1 < 3 && (!select_system(session) ||
            !manual_command(session, "new hostcheck size 100000 key 2 internal 2 run\r", "to hostcheck"))) return false;
    }
    (void)fprintf(stderr, "Protected nested-process checkpoint passed\n");
    return true;
}

static bool catalog_session(session_t *session, bool read_only, bool cleanup)
{
    if (!manual_command(session,
        "new keeper size 100000 temp disc 200 10 perm disc 200 10 run\r", "to keeper"))
        return false;
    if (cleanup)
        return manual_command(session, "scope temp saved\r", NULL) &&
               manual_command(session, "lookup saved\r", "; temp") &&
               finish_process(session, "pause keeper finis fp");
    if (read_only)
        return manual_command(session, "lookup saved\r", "; user") &&
               manual_command(session, "rebooted=tohost saved\r", "bytes");
    return manual_command(session, "saved=fromhost input\r", "bytes") &&
        manual_command(session, "gone=fromhost input\r", "bytes") &&
        manual_command(session, "lookup saved\r", "; temp") &&
        manual_command(session, "catsort scope.temp\r", "4 entr.") &&
        manual_command(session, "scope user saved\r", NULL) &&
        manual_command(session, "lookup saved\r", "; user") &&
        manual_command(session, "catsort\r", "4 entr.") &&
        manual_command(session, "catsort scope.temp\r", "3 entr.") &&
        manual_command(session, "catsort scope.user\r", "1 entr.") &&
        manual_command(session, "search user\r", "; user") &&
        manual_command(session, "templist=search temp\r", NULL) &&
        manual_command(session, "userlist=search user\r", NULL) &&
        manual_command(session, "syslist=search system\r", NULL) &&
        manual_command(session, "rootlist=lookup catalog\r", NULL) &&
        manual_command(session, "listing=copy templist userlist syslist rootlist\r", NULL) &&
        manual_command(session, "listout=tohost listing\r", "bytes") &&
        manual_command(session, "clear temp templist userlist syslist rootlist listing\r", NULL) &&
        manual_command(session, "cata\r", "indgange ialt:") &&
        manual_command(session, "catraw=tohost conv.bin catalog\r", "bytes") &&
        manual_command(session, "details=lookup saved algol slang\r", NULL) &&
        manual_command(session, "detailsout=tohost details\r", "bytes") &&
        finish_process(session, "pause keeper finis fp") &&
        select_system(session) &&
        manual_command(session,
            "new reader size 100000 temp disc 200 10 perm disc 200 10 run\r", "to reader") &&
        manual_command(session, "lookup saved\r", "; user") &&
        manual_command(session, "afterproc=tohost saved\r", "bytes") &&
        finish_process(session, "pause reader finis fp");
}

static bool monitor_build_session(session_t *session)
{
    char line[128];
    if (!manual_command(session,
        "new monbuild size 250000 temp disc 2000 100 run\r", "to monbuild"))
        return false;
    for (unsigned i = 1; i <= 11; ++i) {
        (void)snprintf(line, sizeof(line), "montxt%u=fromhost montxt%u\r", i, i);
        if (!manual_command(session, line, "bytes")) return false;
    }
    if (!manual_command(session, "mon8s=fromhost mon8s\r", "bytes") ||
        !manual_command(session, "mon8initcat=fromhost mon8init\r", "bytes") ||
        !manual_command(session, "mbuild=fromhost mbuild\r", "bytes")) return false;
    /* Protected entry calls the shared generation file at the second (last)
       FP input level. A fresh private host root has mcommon only in this mode. */
    char common_path[4096];
    bool protected_entry = false;
    int length = snprintf(common_path, sizeof(common_path), "%s/mcommon", session->host_root);
    if (length < 0 || (size_t)length >= sizeof(common_path)) return false;
    FILE *common = fopen(common_path, "rb");
    if (common != NULL) {
        protected_entry = true;
        if (fclose(common) != 0 ||
            !manual_command(session, "mcommon=fromhost mcommon\r", "bytes")) return false;
    }
    /* Keep the existing bounded claim. The protected guest wrapper compacts
       padded imports to make room for its extra source file and editor log. */
    if (!manual_command(session, "show\r", "free disc space:")) return false;
    bool built = command(session, "i mbuild\r");
    if (!built && strstr(session->output, "pause monbuild") != NULL)
        return false; /* No live FP remains from which to export temporary logs. */
    /* Preserve the assembler listing even when generation was rejected. */
    bool listed = manual_command(session, "buildlist=tohost mongenout\r", "bytes");
    if (protected_entry) {
        /* Preserve editor evidence even when the top-level build failed. */
        bool edited = manual_command(session, "pkeditlog=tohost pkeditlog\r", "bytes");
        bool source = manual_command(session, "pkedited=tohost ptxt10\r", "bytes");
        bool restored = manual_command(session, "pkrestored=tohost montxt10\r", "bytes");
        if (!edited || !source || !restored) return false;
    }
    if (!built || !listed) return false;
    return manual_command(session, "candidate=tohost conv.bin binmon\r", "bytes");
}

static bool catstat_session(session_t *session)
{
    if (!manual_command(session,
        "new muser size 100000 temp disc 200 10 run\r", "to muser") ||
        !command_checked(session, "lookup catalog catstat\r", "; ***") ||
        !manual_command(session, "catraw=tohost conv.bin catalog\r", "bytes"))
        return false;
    if (session->machine.ix_enabled)
        return manual_command(session, "catstat\r", "segment  71") &&
            manual_command(session, "again=tohost conv.bin catalog\r", "bytes");
    session->probe_catstat = true;
    bool paused = pause_command(session, "catstat\r", "pause muser break fp");
    session->probe_catstat = false;
    return paused && session->catalog_lookups == 1 && session->lookup_return == 0 &&
        session->lookup_result == 0 && session->catalog_size == 72 &&
        session->catalog_contents == RC_WORD_MASK && session->unassigned_seen &&
        session->unassigned_word == UINT32_C(0xf52fed) &&
        session->index_extension == UINT32_C(0x004001) &&
        session->index_value == 1 && session->index_descriptor[1] == 40 &&
        session->index_descriptor[2] == 0 &&
        rc_word(session->index_descriptor[0] + 2U) == session->lookup_tail &&
        session->fault_entered && session->fault_interrupt == 0 && select_system(session) &&
        command_checked(session, "proc muser start\r", "***fp reinitialized") &&
        manual_command(session, "again=tohost conv.bin catalog\r", "bytes");
}

int main(int argc, char **argv)
{
    session_t *session;
    rc_addr_t load;
    rc_addr_t entry;
    char error[256];
    int result = EXIT_FAILURE;
    bool disk_open = false;
    bool host_open = false;
    bool inspect_libraries = argc == 5 && strcmp(argv[1], "--inspect-libraries") == 0;
    bool inspect_mips = argc == 5 && strcmp(argv[1], "--inspect-mips") == 0;
    bool build_system_binding = argc == 5 && strcmp(argv[1], "--build-rc4000-profile") == 0;
    bool build_old_runtime = (argc == 5 && strcmp(argv[1], "--build-rc4000-transfers") == 0) ||
                             build_system_binding;
    bool protected_isolation = argc == 5 && strcmp(argv[1], "--verify-protected-isolation") == 0;
    bool protected_nested = argc == 5 && strcmp(argv[1], "--verify-protected-nested") == 0;
    bool verify_system_binding = (argc == 5 &&
        (strcmp(argv[1], "--verify-protected-algol-profile") == 0 ||
         strcmp(argv[1], "--verify-protected-profile") == 0)) || protected_nested;
    bool protected_algol = (argc == 5 && strcmp(argv[1], "--verify-protected-algol") == 0) ||
                           (argc == 5 && strcmp(argv[1], "--verify-protected-algol-profile") == 0);
    bool protected_lifecycle = argc == 5 && strcmp(argv[1], "--verify-protected-lifecycle") == 0;
    bool probe_cmp = argc == 5 && strcmp(argv[1], "--probe-protected-cmp") == 0;
    bool probe_upsoscat = argc == 5 && strcmp(argv[1], "--probe-protected-upsoscat") == 0;
    bool verify_old_runtime = (argc == 5 && strcmp(argv[1], "--verify-rc4000-transfers") == 0) ||
                              protected_lifecycle || protected_isolation || protected_algol ||
                              protected_nested || verify_system_binding;
    bool protected_monitor = (argc == 5 && strcmp(argv[1], "--verify-protected") == 0) ||
                             verify_old_runtime || probe_cmp || probe_upsoscat;
    bool monitor_build = argc == 5 && strcmp(argv[1], "--monitor-build") == 0;
    bool catstat_ix = argc == 5 &&
        (strcmp(argv[1], "--verify-catstat-ix") == 0 ||
         strcmp(argv[1], "--verify-catstat-ix-dfc") == 0);
    bool catstat = argc == 5 &&
        (strcmp(argv[1], "--verify-catstat") == 0 ||
         strcmp(argv[1], "--verify-catstat-dfc") == 0 || catstat_ix);
    bool catalog_write = argc == 5 && strcmp(argv[1], "--catalog-create") == 0;
    bool catalog_cleanup = argc == 5 && strcmp(argv[1], "--catalog-cleanup") == 0;
    bool catalog_read = argc == 5 && strcmp(argv[1], "--verify-catalog-read") == 0;
    bool catalog_mode = argc == 5 &&
        (strcmp(argv[1], "--verify-catalog") == 0 || catalog_write || catalog_read ||
         catalog_cleanup);
    bool manual = argc == 5 &&
        (strcmp(argv[1], "--verify-manual") == 0 ||
         strcmp(argv[1], "--verify-manual-dfc") == 0 || catalog_mode || monitor_build || catstat || inspect_mips);
    bool hardware = argc == 5 && (strcmp(argv[1], "--verify-dfc") == 0 ||
                                 strcmp(argv[1], "--verify-manual-dfc") == 0 ||
                                 strcmp(argv[1], "--verify-catstat-dfc") == 0 ||
                                 strcmp(argv[1], "--verify-catstat-ix-dfc") == 0);
    bool export_standard = argc == 5 && strcmp(argv[1], "--export-standard") == 0;
    bool verify = (argc == 5 && strcmp(argv[1], "--verify") == 0) ||
                  hardware || export_standard || manual || protected_monitor || inspect_libraries ||
                  build_old_runtime;
    const char *monitor;
    const char *image;
    const char *root;

    if (argc != 4 && !verify) {
        (void)fprintf(stderr, "Usage: sim4000_build_host_image [--verify|--verify-protected|--inspect-libraries|--build-rc4000-transfers|--verify-rc4000-transfers|--verify-protected-lifecycle|--verify-protected-isolation|--verify-protected-algol|--export-standard|--verify-dfc|--verify-manual|--verify-manual-dfc|--verify-catstat|--verify-catstat-dfc|--verify-catstat-ix|--verify-catstat-ix-dfc|--verify-catalog|--catalog-create|--verify-catalog-read|--catalog-cleanup|--monitor-build] MONITOR WORK_IMAGE HOST_ROOT\n");
        (void)fprintf(stderr, "Derived-profile modes: --build-rc4000-profile, --verify-protected-algol-profile, --verify-protected-nested\n");
        (void)fprintf(stderr, "Inspection modes: --inspect-mips, --probe-protected-cmp, --probe-protected-upsoscat\n");
        return EXIT_FAILURE;
    }
    monitor = argv[verify ? 2 : 1];
    image = argv[verify ? 3 : 2];
    root = argv[verify ? 4 : 3];
    session = calloc(1, sizeof(*session));
    if (session == NULL || !rc_machine_init(&session->machine, 1048576)) {
        free(session);
        return EXIT_FAILURE;
    }
    session->hardware = hardware;
    session->host_root = root;
    session->runtime_probe = build_old_runtime || verify_old_runtime;
    session->machine.protection_enabled = protected_monitor;
    if (protected_monitor)
        (void)fprintf(stderr, "Candidate verification: RC4000 protection enabled\n");
    /* Historical negative checkpoints deliberately retain IX-off behavior;
     * this test helper does not select the normal frontend's default. */
    session->machine.ix_enabled = catstat_ix || protected_monitor || build_old_runtime || inspect_mips;
    /* A rebuilt monitor need not retain the legacy idle-loop addresses. */
    session->manual = manual || protected_monitor ||
        (argc == 5 && strcmp(argv[1], "--verify") == 0);
    session->monitor_build = monitor_build;
    rc_dfc403_init(&session->controller, &session->machine, 23, 22);
    if (rc_monitor_load(monitor, &session->machine.core, true, &load, &entry,
                         error, sizeof(error)) != 0 ||
        (hardware ? rc_dfc403_mount(&session->controller, 0, image,
                                    error, sizeof(error)) :
         verify && !catalog_write && !catalog_cleanup && !build_old_runtime
                ? rc_rc433x_device_init_cow(&session->disk, &session->machine,
                                            image, error, sizeof(error)) :
                  rc_rc433x_device_init_writable(&session->disk, &session->machine,
                                                 image, error, sizeof(error))) != 0) {
        (void)fprintf(stderr, "%s\n", error);
        goto done;
    }
    disk_open = true;
    if (rc_hostfiles_device_init(&session->host, &session->machine, root,
                                 21, error, sizeof(error)) != 0) {
        (void)fprintf(stderr, "%s\n", error);
        goto done;
    }
    host_open = true;
    rc_clock_device_init(&session->clock, 11);
    rc_console_device_init(&session->console, 6, 3);
    rc_console_device_init(&session->secondary, 20, 19);
    session->input_console = &session->console;
    rc_console_device_set_output(&session->console, output, session);
    if (protected_isolation)
        rc_console_device_set_output(&session->secondary, output, session);
    (void)rc_machine_attach_device(&session->machine, 2, &session->console.device);
    (void)rc_machine_attach_device(&session->machine, 3, &session->clock.device);
    (void)rc_machine_attach_device(&session->machine, 4, &session->secondary.device);
    (void)rc_machine_attach_device(&session->machine, 15,
        hardware ? &session->controller.device : &session->disk.device);
    (void)rc_machine_attach_device(&session->machine, 63, &session->host.device);
    session->machine.cpu.instruction_counter = entry;
    session->machine.cpu.stopped = false;
    if (!wait_text(session, 0, "initialize date:")) {
        goto done;
    }
    {
        const char *date = "86 09 30 08 00 00\r";
        size_t start = session->count;
        while (*date != '\0') {
            if (!rc_console_device_submit(&session->console, (uint8_t)*date++))
                goto done;
        }
        if (!wait_text(session, start, "pause startproc finis fp")) goto done;
    }
    {
        size_t start = session->count;
        (void)rc_console_device_submit(&session->console, 27);
        if (!wait_text(session, start, "att ")) goto done;
    }
    if (!command(session, "s\r")) goto done;
    if (manual && !inspect_mips) {
        if (catstat ? catstat_session(session) :
            monitor_build ? monitor_build_session(session) :
            catalog_mode ? catalog_session(session, catalog_read, catalog_cleanup)
                         : manual_session(session))
            result = EXIT_SUCCESS;
        goto done;
    }
    if (build_old_runtime) {
        /* Experimental guest-only binding on the disposable writable image.
         * lookup emits FP's set syntax. Do not alias nsystem to system:
         * equal parameter descriptions do not establish compatible internals.
         * The single-segment tofrom binding is derived from its relocation
         * header and identical parameter-entry prologue, then tested below.
         * Normally restore the catalog binding after linking. Only the explicit
         * derived-system-profile mode retains it in this private image;
         * retain the original transfer binaries and both library areas. */
        if (!command(session, "all hostbuild size 150000 temp disc 1000 20 perm disc 200 10 run\r") ||
            !command(session, "fhsrc=fromhost \"fromhost.alg\"\r") ||
            !command(session, "thsrc=fromhost \"tohost.alg\"\r") ||
            !command(session, "tofrom=set bs oldalgollib 2048.22 83.2665 0 46.14 1.0\r") ||
            !command(session, "rcin=algol fhsrc connect.no\r") ||
            !command(session, "rcout=algol thsrc connect.no\r") ||
            (!build_system_binding &&
             !command(session, "tofrom=set bs algollib 2048.24 83.2665 0 49.16 2.0\r")) ||
            !command(session, "hftest=rcin input\r") ||
            !command(session, "output=rcout hftest\r") ||
            !command(session, "hfbin=rcin conv.bin binary\r") ||
            !command(session, "binoutput=rcout conv.bin hfbin\r") ||
            !command(session, "rcinbinary=tohost conv.bin rcin\r") ||
            !command(session, "rcoutbinary=tohost conv.bin rcout\r") ||
            !command(session, "scope user rcin rcout\r") ||
            !pause_command(session, "finis\r", "pause hostbuild finis fp")) goto done;
        result = EXIT_SUCCESS;
        goto done;
    }
    if (!verify &&
       (!command(session, "all hostbuild size 150000 temp disc 1000 20 run\r") ||
        !command(session, "fhsrc=fromdos \"fromhost.alg\"\r") ||
        !command(session, "thsrc=fromdos \"tohost.alg\"\r") ||
        !command(session, "rename fromdos.fromhost\r") ||
        !command(session, "rename todos.tohost\r") ||
        !command(session, "fromhost=algol fhsrc connect.no\r") ||
        !command(session, "tohost=algol thsrc connect.no\r"))) {
        goto done;
    }
    if (verify && !command(session, protected_nested
        ? "new hostcheck size 100000 key 2 internal 2 run\r"
        : "new hostcheck size 100000 run\r")) goto done;
    if (inspect_mips) {
        static const char *const texts[] = {
            "startmips", "starttem", "startprimo", "startsos",
            "temtrim", "primotrim", "sostrim", "startsysadm"
        };
        static const char *const binaries[] = {"btem", "bprimo", "bsos", "upsoscat"};
        char line[128];
        for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i) {
            (void)snprintf(line, sizeof(line), "%s=tohost %s\r", texts[i], texts[i]);
            if (!manual_command(session, line, "bytes")) goto done;
        }
        for (size_t i = 0; i < sizeof(binaries) / sizeof(binaries[0]); ++i) {
            (void)snprintf(line, sizeof(line), "%s=tohost conv.bin %s\r", binaries[i], binaries[i]);
            if (!manual_command(session, line, "bytes")) goto done;
        }
        if (!command(session, "mipsmeta=lookup startmips starttem startprimo startsos btem bprimo bsos upsoscat\r") ||
            !command(session, "metadata=tohost mipsmeta\r")) goto done;
        (void)fprintf(stderr, "MIPS inventory exported without executing subsystem startup\n");
        result = EXIT_SUCCESS;
        goto done;
    }
    if (inspect_libraries) {
        if (!command(session, "oldruntime=tohost conv.bin oldalgollib\r") ||
            !command(session, "newruntime=tohost conv.bin algollib\r") ||
            !command(session, "frombinary=tohost conv.bin fromhost\r") ||
            !command(session, "tobinary=tohost conv.bin tohost\r") ||
            !command(session, "liblookup=lookup tofrom system nsystem getzone6\r") ||
            !command(session, "libmetadata=tohost liblookup\r")) goto done;
        result = EXIT_SUCCESS;
        goto done;
    }
    if (protected_monitor && !manual_command(session, "claim\r", "size 100000")) goto done;
    if (probe_cmp) {
        result = protected_cmp_probe(session) ? EXIT_SUCCESS : EXIT_FAILURE;
        goto done;
    }
    if (probe_upsoscat) {
        result = protected_upsoscat_probe(session) ? EXIT_SUCCESS : EXIT_FAILURE;
        goto done;
    }
    if (export_standard) {
        if (!command(session, "stdmon=tohost conv.bin stdmonitor\r")) goto done;
        result = EXIT_SUCCESS;
        goto done;
    }
    if (!verify && (strstr(session->output, "algol end 55") == NULL ||
                    strstr(session->output, "algol end 47") == NULL)) {
        (void)fprintf(stderr, "expected successful Algol compilation summaries\n");
        goto done;
    }
    if (!command(session, verify_old_runtime ? "hftest=rcin input\r" : "hftest=fromhost input\r") ||
        !command(session, verify_old_runtime ? "output=rcout hftest\r" : "output=tohost hftest\r") ||
        !command(session, verify_old_runtime ? "hfbin=rcin conv.bin binary\r" : "hfbin=fromhost conv.bin binary\r") ||
        !command(session, verify_old_runtime ? "binoutput=rcout conv.bin hfbin\r" : "binoutput=tohost conv.bin hfbin\r")) goto done;
    if (hardware && (session->controller.data_operations == 0 ||
                     session->controller.seek_operations == 0 ||
                     session->controller.drives[0].storage.read_operations == 0 ||
                     session->controller.drives[0].storage.write_operations == 0)) {
        (void)fprintf(stderr, "standard monitor did not exercise DFC403 read/write/seek\n");
        goto done;
    }
    if (!verify && !command(session, "clear temp fhsrc thsrc hftest hfbin\r")) goto done;
    if (protected_monitor) {
        size_t keyed_words = 0;
        for (size_t i = 0; i < session->machine.core.halfword_count; i += 2) {
            uint8_t key;
            if (!rc_core_read_key(&session->machine.core, (rc_addr_t)i, &key)) goto done;
            if (key != 0) ++keyed_words;
        }
        (void)fprintf(stderr, "Protected checkpoint: task-calls=%" PRIu64 " keyed-words=%zu\n",
                       session->protected_task_calls, keyed_words);
        if (session->protected_task_calls == 0 || keyed_words == 0) {
            (void)fprintf(stderr, "protected candidate did not exercise keyed task execution\n");
            goto done;
        }
        if (protected_lifecycle && !protected_lifecycle_session(session)) goto done;
        if (protected_isolation && !protected_isolation_session(session)) goto done;
        if (protected_algol && !protected_algol_session(session, verify_system_binding)) goto done;
        if (protected_nested && !protected_nested_session(session)) goto done;
        if (verify_old_runtime) {
            (void)fprintf(stderr, "Remaining GG probe executions=%" PRIu64 "\n",
                           session->runtime_gg_probe_steps);
            if (session->runtime_gg_probe_steps != 0) {
                (void)fprintf(stderr, "transfer checkpoint unexpectedly executed a remaining GG probe\n");
                goto done;
            }
        }
    }
    result = EXIT_SUCCESS;
done:
    if (protected_monitor)
        (void)fprintf(stderr, "Protected observation: task-calls=%" PRIu64 "\n",
                       session->protected_task_calls);
    if (hardware) {
        (void)fprintf(stderr, "DFC403: transfers=%" PRIu64 " seeks=%" PRIu64
                       " reads=%" PRIu64 " writes=%" PRIu64 "\n",
            session->controller.data_operations, session->controller.seek_operations,
            session->controller.drives[0].storage.read_operations,
            session->controller.drives[0].storage.write_operations);
    }
    if (host_open) rc_hostfiles_device_destroy(&session->host);
    if (disk_open) {
        if (hardware) rc_dfc403_destroy(&session->controller);
        else rc_rc433x_device_destroy(&session->disk);
    }
    if (result == EXIT_SUCCESS) {
        rc_catalog_t catalog;
        if (rc_catalog_read(image, &catalog, error, sizeof(error)) != 0) {
            (void)fprintf(stderr, "cannot validate native catalog: %s\n", error);
            result = EXIT_FAILURE;
        } else {
            const rc_catalog_entry_t *from = rc_catalog_find(&catalog, "fromhost");
            const rc_catalog_entry_t *to = rc_catalog_find(&catalog, "tohost");
            if (from == NULL || to == NULL ||
                rc_catalog_find(&catalog, "fromdos") != NULL ||
                rc_catalog_find(&catalog, "todos") != NULL ||
                from->size_segments != 55 || to->size_segments != 47 ||
                from->permission_key != 3 || to->permission_key != 3 ||
                from->lower_base != UINT32_C(0x800001) ||
                to->lower_base != UINT32_C(0x800001) ||
                from->upper_base != UINT32_C(0x7ffffd) ||
                to->upper_base != UINT32_C(0x7ffffd) ||
                from->contents != 2 || to->contents != 2) {
                (void)fprintf(stderr, "native catalog entries are missing or invalid\n");
                result = EXIT_FAILURE;
            }
            if (build_old_runtime || verify_old_runtime) {
                const rc_catalog_entry_t *in = rc_catalog_find(&catalog, "rcin");
                const rc_catalog_entry_t *out = rc_catalog_find(&catalog, "rcout");
                const rc_catalog_entry_t *binding = rc_catalog_find(&catalog, "tofrom");
                const rc_catalog_entry_t *system = rc_catalog_find(&catalog, "nsystem");
                if (in == NULL || out == NULL || in->size_segments != 54 ||
                    out->size_segments != 46 || binding == NULL || system == NULL ||
                    strcmp(binding->document, (build_system_binding || verify_system_binding)
                           ? "oldalgollib" : "algollib") != 0 ||
                    binding->short_clock != ((build_system_binding || verify_system_binding)
                           ? UINT32_C(0x800016) : UINT32_C(0x800018)) ||
                    binding->file != UINT32_C(0x053a69) || binding->block != 0 ||
                    binding->contents != ((build_system_binding || verify_system_binding) ? 46 : 49) ||
                    binding->entry != ((build_system_binding || verify_system_binding) ? 14 : 16) ||
                    binding->final_tail_word != ((build_system_binding || verify_system_binding)
                           ? UINT32_C(0x001000) : UINT32_C(0x002000)) ||
                    strcmp(system->document, "algollib") != 0) {
                (void)fprintf(stderr, "older-runtime entries or selected binding invalid\n");
                    result = EXIT_FAILURE;
                }
            }
            rc_catalog_destroy(&catalog);
        }
        if (result == EXIT_SUCCESS && (build_system_binding || verify_system_binding) &&
            !validate_system_binding(image)) result = EXIT_FAILURE;
    }
    rc_machine_destroy(&session->machine);
    (void)fprintf(stderr, "host-image session: calls=%" PRIu64 " result=%d\n",
                   session->calls, result);
    free(session);
    return result;
}
