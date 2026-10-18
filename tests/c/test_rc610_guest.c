/* Guest-s transport and guest Algol source-print acceptance, not full PRIMO.
   Uses a volatile disk overlay; never opens the base image for writing. */
#include "sim4000/machine.h"
#include "sim4000/dfc403.h"
#include "sim4000/monitor.h"
#include "sim4000/rc610.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    rc_machine_t machine;
    rc_console_device_t console;
    rc_clock_device_t clock;
    rc_rc433x_device_t disk;
    rc_dfc403_device_t controller;
    rc610_device_t printer;
    rc_hostfiles_device_t host;
    char output[65536];
    size_t count;
    uint64_t calls;
    uint64_t protected_task_calls;
    uint64_t keyed_task_calls;
} session_t;

static void output(void *context, uint8_t c)
{
    session_t *s = context;
    if (c != 0 && s->count + 1 < sizeof(s->output)) {
        s->output[s->count++] = (char)c;
        s->output[s->count] = '\0';
    }
    if (c != 0 && c != 127) (void)fputc(c, stdout);
}
static bool step(session_t *s)
{
    if (++s->calls % UINT64_C(10000) == 0)
        rc_machine_advance_time(&s->machine, 1);
    bool task = s->machine.protection_enabled && !s->machine.cpu.monitor_mode;
    rc_addr_t pc = s->machine.cpu.instruction_counter;
    uint64_t executed = s->machine.instructions_executed;
    rc_step_result_t result = rc_machine_step(&s->machine);
    if (result == RC_STEP_OK && task && s->machine.instructions_executed > executed &&
        s->machine.last_instruction_address == pc) {
        uint8_t key;
        ++s->protected_task_calls;
        if (rc_core_read_key(&s->machine.core, pc, &key) && key != 0)
            ++s->keyed_task_calls;
    }
    return result == RC_STEP_OK;
}
static bool until(session_t *s, size_t start, const char *text)
{
    uint64_t limit = s->calls + UINT64_C(10000000);
    while (s->calls < limit) {
        if (strstr(s->output + start, text) != NULL) return true;
        if (!step(s)) break;
    }
    (void)fprintf(stderr, "missing guest response: %s\n", text);
    return false;
}
static bool command(session_t *s, const char *text)
{
    size_t start = s->count, last = start;
    uint64_t quiet = 0, limit = s->calls + UINT64_C(30000000);
    while (s->calls < limit) {
        if (*text != '\0' && rc_console_device_submit(&s->console, (uint8_t)*text)) ++text;
        if (!step(s)) break;
        quiet = s->count == last ? quiet + 1 : 0;
        last = s->count;
        if (*text == '\0' && s->console.input_count == 0 &&
            (s->console.busy || strstr(s->output + start, "ready") != NULL) && quiet >= 100000) {
            if (strstr(s->output + start, "***") == NULL &&
                strstr(s->output + start, "syntax error:") == NULL &&
                strstr(s->output + start, " break fp") == NULL) return true;
            break;
        }
    }
    (void)fprintf(stderr, "guest command failed\n");
    return false;
}
static bool select_s(session_t *s)
{
    size_t start = s->count;
    rc_console_device_attention(&s->console);
    return until(s, start, "att ") && command(s, "s\r");
}
static bool date(session_t *s)
{
    const char *text = "86 09 30 08 00 00\r";
    size_t start = s->count;
    while (*text != '\0')
        if (!rc_console_device_submit(&s->console, (uint8_t)*text++)) return false;
    return until(s, start, "pause startproc finis fp");
}
static bool contains_file(const char *path, const char *expected)
{
    char bytes[4096];
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    size_t n = fread(bytes, 1, sizeof(bytes) - 1, file);
    bool ok = !ferror(file);
    bytes[n] = '\0';
    if (fclose(file) != 0) ok = false;
    return ok && strstr(bytes, expected) != NULL;
}
static bool same_file(const char *left, const char *right)
{
    FILE *a = fopen(left, "rb"), *b = fopen(right, "rb");
    bool equal = a != NULL && b != NULL;
    if (equal) {
        int x, y;
        do { x = fgetc(a); y = fgetc(b); if (x != y) equal = false; }
        while (equal && x != EOF);
        if (ferror(a) || ferror(b)) equal = false;
    }
    if (a != NULL && fclose(a) != 0) equal = false;
    if (b != NULL && fclose(b) != 0) equal = false;
    return equal;
}
static bool normal_end(const char *text)
{
    /* The monitor's raw console stream may use LF-CR and RUBOUT padding.
       Require a whole normal FP end line, not a substring in "abnormal end". */
    while (*text != '\0') {
        while (*text == '\r' || *text == '\n' || *text == ' ' || *text == '\t' ||
               (unsigned char)*text == 127) ++text;
        if (strncmp(text, "end ", 4) == 0) {
            const char *number = text + 4;
            while (*number == ' ') ++number;
            const char *end = number;
            while (*end >= '0' && *end <= '9') ++end;
            if (end != number) {
                while (*end == ' ' || (unsigned char)*end == 127) ++end;
                if (*end == '\r' || *end == '\n' || *end == '\0') return true;
            }
        }
        text += strcspn(text, "\r\n");
    }
    return false;
}
int main(int argc, char **argv)
{
    session_t *s;
    rc_addr_t load, entry;
    char error[256] = "guest acceptance failed";
    bool disk_open = false, printer_open = false, host_open = false;
    bool protected_source = argc == 6 && strcmp(argv[1], "--source-protected") == 0;
    bool protected_mode = protected_source || (argc == 5 && strcmp(argv[1], "--protected") == 0);
    bool hardware = argc == 5 && strcmp(argv[1], "--dfc403") == 0;
    bool source_print = protected_source || (argc == 6 && strcmp(argv[1], "--source") == 0);
    int first = protected_mode || hardware || source_print ? 2 : 1;
    int result = EXIT_FAILURE;
    uint64_t source_keyed_calls = 0;
    if (argc != (source_print ? 6 : protected_mode || hardware ? 5 : 4)) {
        (void)fprintf(stderr, "Usage: test_rc610_guest [--protected|--dfc403] MONITOR BASE_DISK NEW_OUTPUT\n"
                             "       test_rc610_guest --source|--source-protected MONITOR BASE_DISK NEW_OUTPUT HOST_ROOT\n");
        return EXIT_FAILURE;
    }
    s = calloc(1, sizeof(*s));
    if (s == NULL) return EXIT_FAILURE;
    if (!rc_machine_init(&s->machine, 1048576)) { free(s); return EXIT_FAILURE; }
    s->machine.ix_enabled = true;
    s->machine.protection_enabled = protected_mode;
    if (rc_monitor_load(argv[first], &s->machine.core, true, &load, &entry, error, sizeof(error)) != 0)
        goto done;
    rc_dfc403_init(&s->controller, &s->machine, 23, 22);
    if ((hardware ? rc_dfc403_mount(&s->controller, 0, argv[first + 1], error, sizeof(error)) :
        rc_rc433x_device_init_cow(&s->disk, &s->machine, argv[first + 1], error, sizeof(error))) != 0)
        goto done;
    disk_open = true;
    if (!rc610_init(&s->printer, 10) || rc610_open(&s->printer, argv[first + 2], error, sizeof(error)) != 0)
        goto done;
    printer_open = true;
    if (source_print) {
        if (rc_hostfiles_device_init(&s->host, &s->machine, argv[5], 21, error, sizeof(error)) != 0)
            goto done;
        host_open = true;
        if (!rc_machine_attach_device(&s->machine, 63, &s->host.device)) goto done;
    }
    rc_clock_device_init(&s->clock, 11);
    rc_console_device_init(&s->console, 6, 3);
    rc_console_device_set_output(&s->console, output, s);
    if (!rc_machine_attach_device(&s->machine, 2, &s->console.device) ||
        !rc_machine_attach_device(&s->machine, 3, &s->clock.device) ||
        !rc_machine_attach_device(&s->machine, 7, &s->printer.device) ||
        !rc_machine_attach_device(&s->machine, 15,
            hardware ? &s->controller.device : &s->disk.device)) goto done;
    s->machine.cpu.instruction_counter = entry;
    s->machine.cpu.stopped = false;
    if (!until(s, 0, "initialize date:") || !date(s)) goto done;
    if (!source_print) {
        if (!select_s(s) || !command(s, "print 16 16\r")) goto done;
        if (!contains_file(argv[first + 2], "16")) {
            (void)snprintf(error, sizeof(error), "Guest-s output not visible before printer close");
            goto done;
        }
        /* Address 16 is the IRQ-0 vector, not a transient register or an
           arbitrary user allocation. This smoke check does not certify s's
           numeric formatting; independent formatted-value vectors remain open. */
        uint64_t first_bytes = s->printer.output_bytes;
        if (first_bytes == 0 || s->printer.completed_prints != 1 ||
            !select_s(s) || !command(s, "print 16 16\r") ||
            s->printer.output_bytes != 2 * first_bytes ||
            !select_s(s) || !command(s, "list\r")) goto done;
        if (s->printer.printing || s->printer.failed_prints != 0 ||
            s->printer.completed_prints < 2) goto done;
    }
    if (protected_mode && (s->protected_task_calls == 0 || s->keyed_task_calls == 0)) {
        (void)snprintf(error, sizeof(error), "no protected execution in keyed task memory observed");
        goto done;
    }
    if (source_print) {
        if (!select_s(s) || !command(s, "new ptest size 100000 temp disc 200 10 run\r") ||
            !command(s, protected_mode ? "psrc=rcin \"print_source.alg\"\r" :
                                        "psrc=fromhost \"print_source.alg\"\r") ||
            !command(s, "ptext=algol psrc connect.no\r")) goto done;
        uint64_t keyed_before = s->keyed_task_calls;
        size_t print_start = s->count;
        if (!command(s, "ptext\r") || !normal_end(s->output + print_start))
            goto done;
        source_keyed_calls = s->keyed_task_calls - keyed_before;
        if (s->printer.completed_prints != 630 || s->printer.output_bytes != 17309 ||
            !contains_file(argv[first + 2], "MPROFILE")) {
            (void)snprintf(error, sizeof(error), "source text not printed");
            goto done;
        }
        if (protected_mode && (!s->machine.protection_enabled || source_keyed_calls == 0)) {
            (void)snprintf(error, sizeof(error), "source printing did not execute protected keyed tasks");
            goto done;
        }
        if (!command(s, protected_mode ? "reference=rcout cpuregtest\r" :
                                        "reference=tohost cpuregtest\r")) goto done;
        char reference[RC610_PATH_SIZE];
        int length = snprintf(reference, sizeof(reference), "%s/reference", argv[5]);
        if (length < 0 || (size_t)length >= sizeof(reference) ||
            !same_file(argv[first + 2], reference)) {
            (void)snprintf(error, sizeof(error), "printed source differs from independent guest export");
            goto done;
        }
        if (s->printer.printing || s->printer.failed_prints != 0 || s->printer.output_errors != 0)
            goto done;
        size_t finish_start = s->count;
        const char *finish = "finis\r";
        while (*finish != '\0')
            if (!rc_console_device_submit(&s->console, (uint8_t)*finish++)) goto done;
        if (!until(s, finish_start, "pause ptest finis fp") ||
            !select_s(s) || !command(s, "list\r")) goto done;
    }
    result = EXIT_SUCCESS;
done:
    if (host_open) rc_hostfiles_device_destroy(&s->host);
    if (printer_open && rc610_close(&s->printer, error, sizeof(error)) != 0) result = EXIT_FAILURE;
    if (disk_open) {
        if (hardware) rc_dfc403_destroy(&s->controller);
        else rc_rc433x_device_destroy(&s->disk);
    }
    rc_machine_destroy(&s->machine);
    (void)fprintf(stderr, "RC610 guest: result=%d calls=%" PRIu64 " prints=%" PRIu64 " errors=%" PRIu64 " %s\n",
        result, s->calls, s->printer.completed_prints, s->printer.output_errors,
        result == EXIT_SUCCESS ? "passed" : error);
    (void)fprintf(stderr, "protection=%s task-observations=%" PRIu64 " keyed-task-observations=%" PRIu64 "\n",
        protected_mode ? "rc4000" : "off", s->protected_task_calls, s->keyed_task_calls);
    if (source_print)
        (void)fprintf(stderr, "source-print keyed-task-observations=%" PRIu64 " bytes=%" PRIu64 "\n",
            source_keyed_calls, s->printer.output_bytes);
    free(s);
    return result;
}
