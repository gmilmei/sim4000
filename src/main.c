#define _POSIX_C_SOURCE 200809L

#include "sim4000/catalog.h"
#include "sim4000/config.h"
#include "sim4000/core.h"
#include "sim4000/core_image.h"
#include "sim4000/dfc403.h"
#include "sim4000/image.h"
#include "sim4000/machine.h"
#include "sim4000/rc610.h"
#include "sim4000/monitor.h"
#include "sim4000/rc749.h"
#include "sim4000/trace.h"
#include "telnet.h"
#include "operator.h"

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* Keep emulator text narrow without altering guest output via fputc/write. */
#define printf rc_operator_printf
#define puts rc_operator_puts
#define putchar rc_operator_putchar

enum {
    RC_RUN_INSTRUCTIONS_PER_DEVICE_TICK = 10000,
    RC_HOST_PATH_SIZE = 512,
    RC_MAX_EXECUTION_BREAKPOINTS = 64,
    RC_MAX_WRITE_BREAKPOINTS = 64
};

typedef struct {
    int input_fd;
    bool eof;
    uint8_t date_shortcut_key;
    uint8_t delete_mapping;
    int date_year_offset;
    bool have_pending_character;
    uint8_t pending_character;
    uint64_t characters_submitted;
    uint8_t *buffer;
    size_t buffer_capacity;
    size_t buffer_head;
    size_t buffer_count;
    bool buffer_failed;
    unsigned ansi_state;
    unsigned ansi_parameter;
    bool ansi_simple_parameter;
    uint64_t rejected_non_ascii;
    uint64_t reported_non_ascii;
} rc_host_console_input_t;

typedef struct {
    rc_config_t config;
    rc_host_console_input_t host_input;
    rc_machine_t machine;
    rc_clock_device_t clocks[RC_CONFIG_MAX_DEVICES];
    rc_console_device_t consoles[RC_CONFIG_MAX_DEVICES];
    unsigned console_numbers[RC_CONFIG_MAX_DEVICES];
    rc_device_kind_t console_kinds[RC_CONFIG_MAX_DEVICES];
    rc_reader_device_t readers[RC_CONFIG_MAX_DEVICES];
    unsigned reader_numbers[RC_CONFIG_MAX_DEVICES];
    rc_rc749_device_t tapes[RC_CONFIG_MAX_DEVICES];
    unsigned tape_numbers[RC_CONFIG_MAX_DEVICES];
    size_t tape_count;
    rc_punch_device_t punches[RC_CONFIG_MAX_DEVICES];
    rc610_device_t printers[RC_CONFIG_MAX_DEVICES];
    unsigned printer_numbers[RC_CONFIG_MAX_DEVICES];
    size_t printer_count;
    unsigned punch_numbers[RC_CONFIG_MAX_DEVICES];
    rc_rc433x_device_t disks[RC_CONFIG_MAX_DEVICES];
    unsigned disk_numbers[RC_CONFIG_MAX_DEVICES];
    bool disk_attached[RC_CONFIG_MAX_DEVICES];
    size_t clock_count;
    size_t console_count;
    size_t reader_count;
    size_t punch_count;
    rc_hostfiles_device_t hostfiles;
    bool hostfiles_initialized;
    rc_telnet_endpoint_t telnet_endpoints[RC_CONFIG_MAX_TELNET_ENDPOINTS];
    size_t telnet_endpoint_count;
    size_t disk_count;
    rc_dfc403_device_t controllers[RC_CONFIG_MAX_DEVICES];
    unsigned controller_numbers[RC_CONFIG_MAX_DEVICES];
    size_t controller_count;
    rc_addr_t load_address;
    rc_addr_t entry_address;
    uint64_t interpreter_calls;
    rc_addr_t execution_breakpoints[RC_MAX_EXECUTION_BREAKPOINTS];
    size_t execution_breakpoint_count;
    rc_addr_t resume_breakpoint_address;
    bool resume_breakpoint;
    rc_addr_t write_breakpoints[RC_MAX_WRITE_BREAKPOINTS];
    size_t write_breakpoint_count;
    rc_addr_t write_breakpoint_address;
    rc_word_t write_breakpoint_previous;
    rc_word_t write_breakpoint_value;
    bool write_breakpoint_active;
    bool write_breakpoint_hit;
    FILE *trace_stream;
    uint64_t trace_call;
    bool trace_failed;
    bool machine_initialized;
} rc_runtime_t;

static struct termios saved_terminal_attributes;
static bool terminal_attributes_saved;
static bool terminal_restore_registered;
static volatile sig_atomic_t host_stop_signal;
static struct sigaction saved_signal_actions[3];
static bool host_signal_handlers_installed;
static const int host_signals[] = {SIGINT, SIGTERM, SIGHUP};

static const char *disk_mode_name(const rc_rc433x_device_t *disk);

static void runtime_observe_write(void *context, rc_addr_t address,
                                  rc_word_t previous, rc_word_t value)
{
    rc_runtime_t *runtime = context;
    size_t index;

    if (runtime == NULL) {
        return;
    }
    if (runtime->trace_stream != NULL &&
        !rc_trace_write_core_write(runtime->trace_stream, runtime->trace_call,
                                   address, previous, value)) {
        runtime->trace_failed = true;
    }
    if (!runtime->write_breakpoint_active ||
        runtime->write_breakpoint_hit) {
        return;
    }
    for (index = 0; index < runtime->write_breakpoint_count; ++index) {
        if (runtime->write_breakpoints[index] == address) {
            runtime->write_breakpoint_address = address;
            runtime->write_breakpoint_previous = previous;
            runtime->write_breakpoint_value = value;
            runtime->write_breakpoint_hit = true;
            return;
        }
    }
}

static void restore_host_terminal(void)
{
    if (terminal_attributes_saved) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &saved_terminal_attributes);
        terminal_attributes_saved = false;
    }
}

static void request_host_stop(int signal_number)
{
    int saved_errno = errno;

    if (host_stop_signal == 0) {
        host_stop_signal = signal_number;
    }
    errno = saved_errno;
}

static int install_host_signal_handlers(void)
{
    struct sigaction action;
    size_t index;

    if (host_signal_handlers_installed) {
        return 0;
    }
    memset(&action, 0, sizeof(action));
    action.sa_handler = request_host_stop;
    (void)sigemptyset(&action.sa_mask);
    for (index = 0; index < sizeof(host_signals) / sizeof(host_signals[0]);
         ++index) {
        if (sigaction(host_signals[index], &action,
                      &saved_signal_actions[index]) != 0) {
            while (index != 0) {
                --index;
                (void)sigaction(host_signals[index],
                                &saved_signal_actions[index], NULL);
            }
            return -1;
        }
    }
    host_signal_handlers_installed = true;
    return 0;
}

static void restore_host_signal_handlers(void)
{
    size_t index;

    if (!host_signal_handlers_installed) {
        return;
    }
    for (index = 0; index < sizeof(host_signals) / sizeof(host_signals[0]);
         ++index) {
        (void)sigaction(host_signals[index], &saved_signal_actions[index],
                        NULL);
    }
    host_signal_handlers_installed = false;
}

static void start_host_console_input(rc_host_console_input_t *input)
{
    struct termios attributes;

    input->input_fd = STDIN_FILENO;
    if (isatty(input->input_fd) != 0 &&
        tcgetattr(input->input_fd, &saved_terminal_attributes) == 0) {
        attributes = saved_terminal_attributes;
        attributes.c_iflag &= (tcflag_t)~(ICRNL | IXON);
        attributes.c_lflag &= (tcflag_t)~(ECHO | ICANON);
        attributes.c_cc[VMIN] = 0;
        attributes.c_cc[VTIME] = 0;
        if (tcsetattr(input->input_fd, TCSANOW, &attributes) == 0) {
            terminal_attributes_saved = true;
            if (!terminal_restore_registered) {
                (void)atexit(restore_host_terminal);
                terminal_restore_registered = true;
            }
        }
    }
}

static void stop_host_console_input(void)
{
    restore_host_terminal();
}

static bool valid_day(int year, int month, int day)
{
    static const int days_per_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int days;

    if (month < 1 || month > 12 || day < 1) {
        return false;
    }
    days = days_per_month[month - 1];
    if (month == 2 &&
        (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))) {
        ++days;
    }
    return day <= days;
}

static int format_host_date(char *text, size_t capacity, int year_offset)
{
    time_t now = time(NULL);
    struct tm local;
    int year;
    int length;

    if (now == (time_t)-1 || localtime_r(&now, &local) == NULL) {
        (void)fprintf(stderr, "\ncannot read the host date\n");
        return -1;
    }
    year = local.tm_year + 1900 + year_offset;
    if (year < 1982 || year > 1999 ||
        !valid_day(year, local.tm_mon + 1, local.tm_mday)) {
        (void)fprintf(stderr,
                      "\nhost date %04d-%02d-%02d with year offset %+d "
                      "is outside the monitor date range\n",
                      local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                      year_offset);
        return -1;
    }
    length = snprintf(text, capacity, "%02d %02d %02d %02d %02d %02d\r",
                      year % 100, local.tm_mon + 1, local.tm_mday,
                      local.tm_hour, local.tm_min, local.tm_sec);
    if (length < 0 || (size_t)length >= capacity) {
        return -1;
    }
    return length;
}

static bool submit_host_date(rc_console_device_t *console, int year_offset)
{
    char text[32];
    int length = format_host_date(text, sizeof(text), year_offset);
    int index;

    if (length < 0) return false;
    for (index = 0; index < length; ++index) {
        if (!rc_console_device_submit(console, (uint8_t)text[index])) {
            (void)fprintf(stderr, "\nconsole input queue is full\n");
            return false;
        }
    }
    return true;
}

static bool reserve_host_input(rc_host_console_input_t *input, size_t extra)
{
    size_t capacity;
    uint8_t *buffer;

    if (extra <= input->buffer_capacity - input->buffer_head - input->buffer_count)
        return true;
    if (input->buffer_head != 0) {
        memmove(input->buffer, input->buffer + input->buffer_head,
                input->buffer_count);
        input->buffer_head = 0;
    }
    if (extra <= input->buffer_capacity - input->buffer_count) return true;
    capacity = input->buffer_capacity == 0 ? 256 : input->buffer_capacity;
    while (capacity - input->buffer_count < extra) {
        if (capacity > SIZE_MAX / 2) goto failed;
        capacity *= 2;
    }
    buffer = realloc(input->buffer, capacity);
    if (buffer == NULL) goto failed;
    input->buffer = buffer;
    input->buffer_capacity = capacity;
    return true;
failed:
    input->buffer_failed = true;
    (void)fprintf(stderr, "\ncannot buffer console input; execution suspended\n");
    return false;
}

static void drain_host_input(rc_host_console_input_t *input,
                             rc_console_device_t *console)
{
    while (input->buffer_count != 0 &&
           rc_console_device_submit(console, input->buffer[input->buffer_head])) {
        ++input->buffer_head;
        --input->buffer_count;
        ++input->characters_submitted;
    }
    if (input->buffer_count == 0) input->buffer_head = 0;
}

static bool queue_operator_input(rc_runtime_t *runtime,
                                  const uint8_t *bytes, size_t count)
{
    rc_host_console_input_t *input = &runtime->host_input;

    if (input->have_pending_character) {
        (void)puts("console has unprocessed input; continue or discard-input first");
        return false;
    }
    if (!reserve_host_input(input, count)) return false;
    memcpy(input->buffer + input->buffer_head + input->buffer_count, bytes, count);
    input->buffer_count += count;
    drain_host_input(input, &runtime->consoles[0]);
    (void)printf("queued input: %zu byte%s\n", count, count == 1 ? "" : "s");
    return true;
}

/* Local terminal framing is host syntax, never RC attention. A bare Escape
 * is handled separately using bounded lookahead; sequence bodies may arrive
 * over multiple service calls. Explicit operator send bypasses this filter. */
static bool filter_local_sequence(rc_host_console_input_t *input, uint8_t *byte)
{
    enum { DATA, ESCAPE, CSI, SS3, STRING, STRING_ESCAPE, ESCAPE_INTERMEDIATE };
    uint8_t character = *byte;

    if (character >= UINT8_C(0x80)) {
        ++input->rejected_non_ascii;
        return false;
    }
    switch (input->ansi_state) {
    case DATA:
        return true;
    case ESCAPE:
        input->ansi_parameter = 0;
        input->ansi_simple_parameter = true;
        if (character == '[') input->ansi_state = CSI;
        else if (character == 'O') input->ansi_state = SS3;
        else if (character == ']' || character == 'P' ||
                 character == '^' || character == '_') input->ansi_state = STRING;
        else if (character >= 0x20 && character <= 0x2f)
            input->ansi_state = ESCAPE_INTERMEDIATE;
        else input->ansi_state = DATA;
        return false;
    case CSI:
        if (character >= '0' && character <= '9') {
            if (input->ansi_parameter < 1000)
                input->ansi_parameter = input->ansi_parameter * 10 + character - '0';
        } else if (character >= 0x40 && character <= 0x7e) {
            input->ansi_state = DATA;
            if (character == '~' && input->ansi_simple_parameter &&
                input->ansi_parameter == 3) {
                *byte = UINT8_C(0x7f);
                return true;
            }
        } else if (character >= 0x20 && character <= 0x3f) {
            input->ansi_simple_parameter = false;
        } else {
            input->ansi_state = character == 0x1b ? ESCAPE : DATA;
        }
        return false;
    case SS3:
        if (character >= 0x40 && character <= 0x7e)
            input->ansi_state = DATA;
        else if (character < 0x20)
            input->ansi_state = character == 0x1b ? ESCAPE : DATA;
        return false;
    case ESCAPE_INTERMEDIATE:
        if (character < 0x20 || character >= 0x30)
            input->ansi_state = character == 0x1b ? ESCAPE : DATA;
        return false;
    case STRING:
        if (character == 7) input->ansi_state = DATA;
        else if (character == 0x1b) input->ansi_state = STRING_ESCAPE;
        return false;
    case STRING_ESCAPE:
        input->ansi_state = character == '\\' ? DATA : STRING;
        return false;
    default:
        input->ansi_state = DATA;
        return false;
    }
}

static void pump_interactive_input(rc_host_console_input_t *input,
                                   rc_console_device_t *console,
                                   uint8_t operator_escape,
                                   bool *operator_requested)
{
    size_t scanned;

    drain_host_input(input, console);
    /* Bound each service call so continuous paste cannot starve CPU, Telnet,
     * clocks or signals. Keep scanning control input even if the guest is full. */
    for (scanned = 0; scanned < 4096 && !input->buffer_failed &&
         host_stop_signal == 0; ++scanned) {
        uint8_t character;
        struct pollfd descriptor = {input->input_fd, POLLIN, 0};
        ssize_t count;

        if (input->have_pending_character) {
            character = input->pending_character;
            input->have_pending_character = false;
        } else {
            if (input->eof || poll(&descriptor, 1, 0) <= 0 ||
                (descriptor.revents & (POLLIN | POLLHUP)) == 0) return;
            count = read(input->input_fd, &character, 1);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) {
                input->eof = true;
                return;
            }
        }
        /* The shortcut is a host control byte, checked before guest mapping. */
        if (operator_escape != RC_CONFIG_OPERATOR_ESCAPE_DISABLED &&
            character == operator_escape) {
            input->ansi_state = 0;
            *operator_requested = true;
            return;
        }
        /* Reserve before mapping or Alt lookahead, preserving this raw byte
         * on failure. This also makes an inserted date an atomic enqueue. */
        if (!reserve_host_input(input, 32)) {
            input->pending_character = character;
            input->have_pending_character = true;
            return;
        }
        if (!filter_local_sequence(input, &character)) continue;
        if (input->ansi_state == 0 &&
            character == UINT8_C(0x1b) && poll(&descriptor, 1, 30) > 0 &&
            (descriptor.revents & POLLIN) != 0 &&
            read(input->input_fd, &input->pending_character, 1) == 1) {
            if (input->date_shortcut_key != RC_CONFIG_SHORTCUT_DISABLED &&
                (uint8_t)toupper((unsigned char)input->pending_character) ==
                input->date_shortcut_key) {
                char text[32];
                int length = format_host_date(text, sizeof(text), input->date_year_offset);
                int index;
                for (index = 0; index < length; ++index)
                    input->buffer[input->buffer_head + input->buffer_count++] =
                        (uint8_t)text[index];
                drain_host_input(input, console);
                continue;
            }
            input->ansi_state = 1;
            input->have_pending_character = true;
            continue;
        }
        if (character == '\n') character = '\r';
        else if (character == UINT8_C(0x7f) &&
                 input->delete_mapping != RC_CONFIG_SHORTCUT_DISABLED)
            character = input->delete_mapping;
        input->buffer[input->buffer_head + input->buffer_count++] = character;
        drain_host_input(input, console);
    }
}

static void pump_host_console_input(rc_host_console_input_t *input,
                                    rc_console_device_t *console,
                                    uint8_t operator_escape,
                                    bool *operator_requested)
{
    struct pollfd descriptor;

    if (input == NULL || console == NULL) {
        return;
    }
    if (operator_requested != NULL) {
        pump_interactive_input(input, console, operator_escape, operator_requested);
        return;
    }
    if (input->eof) return;
    for (;;) {
        uint8_t character;
        ssize_t count;
        int poll_result;

        if (input->have_pending_character) {
            character = input->pending_character;
        } else {
            descriptor.fd = input->input_fd;
            descriptor.events = POLLIN;
            descriptor.revents = 0;
            poll_result = poll(&descriptor, 1, 0);
            if (poll_result <= 0 ||
                (descriptor.revents & (POLLIN | POLLHUP)) == 0) {
                return;
            }
            count = read(input->input_fd, &character, 1);
            if (count == 0) {
                input->eof = true;
                return;
            }
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                input->eof = true;
                return;
            }
        }
        if (character == (uint8_t)'\n') {
            character = (uint8_t)'\r';
        } else if (character == UINT8_C(0x7f) &&
                   input->delete_mapping != RC_CONFIG_SHORTCUT_DISABLED) {
            character = input->delete_mapping;
        }
        if (input->date_shortcut_key != RC_CONFIG_SHORTCUT_DISABLED &&
            character == UINT8_C(0x1b)) {
            uint8_t next_character;

            descriptor.fd = input->input_fd;
            descriptor.events = POLLIN;
            descriptor.revents = 0;
            poll_result = poll(&descriptor, 1, 30);
            if (poll_result > 0 && (descriptor.revents & POLLIN) != 0 &&
                read(input->input_fd, &next_character, 1) == 1) {
                if ((uint8_t)toupper((unsigned char)next_character) ==
                    input->date_shortcut_key) {
                    (void)submit_host_date(console, input->date_year_offset);
                    continue;
                }
                input->pending_character = next_character;
                input->have_pending_character = true;
            }
        }
        if (operator_requested != NULL &&
            operator_escape != RC_CONFIG_OPERATOR_ESCAPE_DISABLED &&
            character == operator_escape) {
            *operator_requested = true;
            input->have_pending_character = false;
            return;
        }
        if (!rc_console_device_submit(console, character)) {
            input->pending_character = character;
            input->have_pending_character = true;
            return;
        }
        input->have_pending_character = false;
        ++input->characters_submitted;
    }
}

static int readable_file(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return 0;
    }
    (void)fclose(file);
    return 1;
}

static int resolve_config_path(const char *config_path,
                               const char *configured_path,
                               char *resolved, size_t resolved_size)
{
    char directory[RC_HOST_PATH_SIZE];
    const char *slash;
    size_t directory_length;
    int length;

    if (config_path == NULL || configured_path == NULL || resolved == NULL ||
        resolved_size == 0) {
        return -1;
    }
    if (configured_path[0] == '/') {
        length = snprintf(resolved, resolved_size, "%s", configured_path);
        return length >= 0 && (size_t)length < resolved_size ? 0 : -1;
    }

    slash = strrchr(config_path, '/');
    directory_length = slash == NULL ? 1U : (size_t)(slash - config_path);
    if (directory_length >= sizeof(directory)) {
        return -1;
    }
    if (slash == NULL) {
        directory[0] = '.';
    } else {
        memcpy(directory, config_path, directory_length);
    }
    directory[directory_length] = '\0';

    length = snprintf(resolved, resolved_size, "%s/%s", directory,
                      configured_path);
    if (length < 0 || (size_t)length >= resolved_size) {
        return -1;
    }
    return 0;
}

static int resolve_config_file(const char *config_path,
                               const char *configured_path,
                               char *resolved, size_t resolved_size)
{
    return resolve_config_path(config_path, configured_path, resolved,
                               resolved_size) == 0 && readable_file(resolved)
               ? 0 : -1;
}

static void usage(FILE *stream)
{
    (void)fprintf(stream,
        "Usage:\n"
        "  sim4000 -i|--interactive [OPTIONS]\n"
        "  sim4000 -a [OPTIONS]\n"
        "  sim4000 --run [--config FILE] [--monitor FILE] [--disk FILE]\n"
        "                [--disk-overlay FILE|--disk-write-through]\n"
        "                [--steps COUNT] [--trace FILE]\n"
        "                [--set-disc-load-flag]\n"
        "  sim4000 --check-config FILE\n"
        "  sim4000 --mkdisc FILE\n"
        "  sim4000 --inspect-monitor FILE\n"
        "  sim4000 --inspect-image FILE\n"
        "  sim4000 --list-catalog FILE\n"
        "  sim4000 --list-main-catalog FILE\n"
        "\n"
        "Options:\n"
        "  -i, --interactive         start the interactive operator interface\n"
        "  -a                        autostart: interactive, disc load, run\n"
        "  -c, --config FILE         use FILE instead of ./RC4000.cfg\n"
        "      --monitor FILE        override the configured monitor file\n"
        "      --disk FILE           override the first configured disk image\n"
        "      --disk-overlay FILE   persist first-disk writes in FILE\n"
        "      --disk-write-through  modify the explicit --disk image\n"
        "      --set-disc-load-flag  request monitor disc autostart\n"
        "      --ix / --no-ix        override optional opcode-61 capability\n"
        "      --steps COUNT         bounded --run instruction-call limit\n"
        "      --trace FILE          write a deterministic --run trace\n"
        "      --mkdisc FILE         create a new unlabelled disk (never overwrite)\n"
        "  -h, --help                display this help\n");
}

static int make_disc(const char *path, bool operator_command)
{
    char error[512];
    if (rc_image_create_blank(path, error, sizeof(error)) != 0) {
        if (operator_command) {
            (void)printf("%s\n", error);
        } else {
            (void)fprintf(stderr, "%s\n", error);
        }
        return EXIT_FAILURE;
    }
    (void)printf("created %s: %u zero-filled sectors (%u bytes); not labelled\n",
                 path, (unsigned)RC433X_BLANK_SECTORS,
                 (unsigned)(RC433X_BLANK_SECTORS * RC433X_SECTOR_BYTES));
    return EXIT_SUCCESS;
}

static const char *telnet_attention_name(rc_telnet_attention_t mode,
                                         uint8_t character,
                                         char text[16])
{
    if (mode == RC_TELNET_ATTENTION_CONNECT) {
        return "connect";
    }
    if (mode == RC_TELNET_ATTENTION_NONE) {
        return "none";
    }
    (void)snprintf(text, 16, "ctrl-%c", (int)(character + '@'));
    return text;
}

static int check_config(const char *path)
{
    rc_config_error_t error;
    rc_config_t config;
    size_t i;

    if (rc_config_parse_file(path, &config, &error) != 0) {
        (void)fprintf(stderr, "%s:%zu: %s\n", path, error.line, error.message);
        return EXIT_FAILURE;
    }
    (void)printf("core: min=%" PRIu32 " max=%" PRIu32 " halfwords\n",
                 config.core_min_halfwords, config.core_max_halfwords);
    (void)printf("cpu: RC4000 ix=%s\n", config.ix_enabled ? "on" : "off");
    (void)printf("protection: %s\n", config.protection_enabled ? "rc4000" : "off");
    {
        char text[80];
        rc_throttle_format(config.throttle, text, sizeof(text));
        (void)printf("throttle: %s (interactive run only)\n", text);
    }
    if (config.monitor_path[0] != '\0') {
        (void)printf("monitor: file=%s\n", config.monitor_path);
    }
    if (config.operator_escape == RC_CONFIG_OPERATOR_ESCAPE_DISABLED) {
        (void)printf("operator: escape=none\n");
    } else {
        (void)printf("operator: escape=ctrl-%c\n",
                     (int)(config.operator_escape + '@'));
    }
    if (config.operator_date_key == RC_CONFIG_SHORTCUT_DISABLED) {
        (void)printf("operator: date=none\n");
    } else {
        (void)printf("operator: date=alt-%c\n",
                     (int)config.operator_date_key);
    }
    if (config.console_delete_mapping == RC_CONFIG_SHORTCUT_DISABLED) {
        (void)printf("input: del=none\n");
    } else {
        (void)printf("input: del=ctrl-%c\n",
                     (int)(config.console_delete_mapping + '@'));
    }
    (void)printf("date: year-offset=%+d\n", config.date_year_offset);
    for (i = 0; i < config.device_count; ++i) {
        const rc_device_config_t *device = &config.devices[i];
        size_t interrupt;
        (void)printf("device: kind=%s", rc_device_kind_name(device->kind));
        if (device->device_number >= 0) {
            (void)printf(" devno=%d", device->device_number);
        }
        if (device->unit_number >= 0) {
            (void)printf(" unit=%d", device->unit_number);
        }
        if (device->controller_number >= 0) {
            (void)printf(" dfc=%d", device->controller_number);
        }
        for (interrupt = 0; interrupt < device->interrupt_count; ++interrupt) {
            (void)printf(" int%zu=%d", interrupt, device->interrupts[interrupt]);
        }
        if (device->path[0] != '\0') {
            (void)printf(" %s=%s",
                         device->kind == RC_DEVICE_HOSTFILES ? "root" : "file",
                         device->path);
        }
        if (device->root_path[0] != '\0') {
            (void)printf(" root=%s", device->root_path);
        }
        if (device->terminal_protocol == RC_TERMINAL_V2) (void)printf(" protocol=v2");
        if (device->overlay_path[0] != '\0') {
            (void)printf(" overlay=%s", device->overlay_path);
        }
        if (device->writable) {
            (void)printf(" writable=yes");
        }
        if (device->cylinders != 0) {
            (void)printf(" cylinders=%" PRIu32, device->cylinders);
        }
        if (device->segments != 0) {
            (void)printf(" segments=%" PRIu32, device->segments);
        }
        (void)putchar('\n');
    }
    for (i = 0; i < config.telnet_endpoint_count; ++i) {
        const rc_telnet_config_t *endpoint = &config.telnet_endpoints[i];
        char attention[16];

        (void)printf("telnet: devno=%d address=%s port=%u attention=%s\n",
                     endpoint->device_number, endpoint->address,
                     (unsigned)endpoint->port,
                     telnet_attention_name(endpoint->attention_mode,
                                           endpoint->attention_character,
                                           attention));
    }
    (void)printf("configuration valid: %zu device declaration(s), "
                 "%zu TELNET endpoint(s)\n",
                 config.device_count, config.telnet_endpoint_count);
    return EXIT_SUCCESS;
}

static int inspect_monitor(const char *path)
{
    char error[256];
    rc_monitor_info_t info;

    if (rc_monitor_inspect(path, &info, error, sizeof(error)) != 0) {
        (void)fprintf(stderr, "%s\n", error);
        return EXIT_FAILURE;
    }
    (void)printf("monitor: bytes=%" PRIu64 " words=%" PRIu64
                 " trailing=%" PRIu32 " fnv1a64=%016" PRIx64 "\n",
                 info.byte_size, info.word_count, info.trailing_bytes,
                 info.checksum_fnv1a);
    return info.trailing_bytes == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int inspect_image(const char *path)
{
    char error[256];
    rc_image_info_t info;

    if (rc_image_inspect(path, &info, error, sizeof(error)) != 0) {
        (void)fprintf(stderr, "%s\n", error);
        return EXIT_FAILURE;
    }
    (void)printf("rc433x image: bytes=%" PRIu64 " sectors=%" PRIu64
                 " trailing=%" PRIu32 " fnv1a64=%016" PRIx64 "\n",
                 info.byte_size, info.complete_sectors, info.trailing_bytes,
                 info.checksum_fnv1a);
    return info.trailing_bytes == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static const char *catalog_entry_kind(const rc_catalog_entry_t *entry)
{
    return entry->size_segments > 0 ? "area" : "non-area";
}

static int list_catalog(const char *path, bool main_catalog)
{
    rc_catalog_t catalog;
    char error[256];
    size_t index;

    if ((main_catalog ? rc_catalog_read_main(path, &catalog, error, sizeof(error))
                      : rc_catalog_read(path, &catalog, error, sizeof(error))) != 0) {
        (void)fprintf(stderr, "%s\n", error);
        return EXIT_FAILURE;
    }
    (void)printf(
        "catalog: name=%s document=%s segments=%" PRIu32
        " entries=%zu slice-length=%" PRIu32
        " first-slice=%u last-slice=%u chain-state=0x%03x\n",
        catalog.name, catalog.document, catalog.segment_count,
        catalog.entry_count, catalog.slice_length,
        (unsigned)catalog.first_slice, (unsigned)catalog.last_slice,
        (unsigned)catalog.chain_state);
    (void)puts(
        "SEG SLOT NAME         KIND       SIZE FIRST KEY PERM "
        "LOWER  UPPER  DOCUMENT     SHORTCL FILE   BLOCK  CONT ENTRY TAIL");
    for (index = 0; index < catalog.entry_count; ++index) {
        const rc_catalog_entry_t *entry = &catalog.entries[index];

        (void)printf(
            "%3" PRIu32 " %4u %-12s %-8s %7" PRId32
            " %5u %3u %4u %06" PRIx32 " %06" PRIx32
            " %-12s %06" PRIx32 " %06" PRIx32 " %06" PRIx32
            " %4u %5u %06" PRIx32 "\n",
            entry->catalog_segment, (unsigned)entry->slot, entry->name,
            catalog_entry_kind(entry), entry->size_segments,
            (unsigned)entry->first_slice, (unsigned)entry->name_key,
            (unsigned)entry->permission_key, entry->lower_base,
            entry->upper_base, entry->document, entry->short_clock,
            entry->file, entry->block, (unsigned)entry->contents,
            (unsigned)entry->entry, entry->final_tail_word);
    }
    rc_catalog_destroy(&catalog);
    return EXIT_SUCCESS;
}

static int load_monitor(const char *config_path, const char *monitor_path,
                        int set_disc_load_flag)
{
    rc_config_error_t config_error;
    char error[256];
    rc_config_t config;
    rc_core_t core = {0};
    rc_addr_t load_address;
    rc_addr_t entry_address;

    if (rc_config_parse_file(config_path, &config, &config_error) != 0) {
        (void)fprintf(stderr, "%s:%zu: %s\n", config_path, config_error.line,
                      config_error.message);
        return EXIT_FAILURE;
    }
    if (!rc_core_init(&core, config.core_max_halfwords)) {
        (void)fprintf(stderr, "cannot allocate core store\n");
        return EXIT_FAILURE;
    }
    if (rc_monitor_load(monitor_path, &core, set_disc_load_flag != 0,
                        &load_address, &entry_address, error,
                        sizeof(error)) != 0) {
        (void)fprintf(stderr, "%s\n", error);
        rc_core_destroy(&core);
        return EXIT_FAILURE;
    }
    (void)printf("monitor loaded: core=%zu load=%" PRIu32
                 " entry=%" PRIu32 " disc-load-flag=%s\n",
                 core.halfword_count, load_address, entry_address,
                 set_disc_load_flag != 0 ? "on" : "off");
    rc_core_destroy(&core);
    return EXIT_SUCCESS;
}

static const char *step_result_name(rc_step_result_t result)
{
    switch (result) {
    case RC_STEP_OK:
        return "ok";
    case RC_STEP_STOPPED:
        return "stopped";
    case RC_STEP_ADDRESS_EXCEPTION:
        return "address-exception";
    case RC_STEP_ILLEGAL_INSTRUCTION:
        return "illegal-instruction";
    case RC_STEP_UNIMPLEMENTED:
        return "unimplemented";
    }
    return "unknown";
}

static int parse_step_limit(const char *text, uint64_t *value)
{
    char *end;
    unsigned long long parsed;

    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0) {
        return -1;
    }
    *value = (uint64_t)parsed;
    return 0;
}

static void print_machine_stop(const rc_machine_t *machine,
                               const char *reason, uint64_t calls)
{
    const rc_instruction_t *instruction = &machine->last_instruction;
    const char *name = rc_opcode_name_extended(instruction->opcode, machine->ix_enabled);

    (void)printf("run stopped: reason=%s calls=%" PRIu64
                 " instructions=%" PRIu64 " interrupts=%" PRIu64 "\n",
                 reason, calls, machine->instructions_executed,
                 machine->interrupts_serviced);
    (void)printf("last: address=%06" PRIx32 " raw=%06" PRIx32
                 " opcode=%u(%s) w=%u r=%u i=%u x=%u d=%" PRId32
                 " effective=%06" PRIx32 " next-ic=%06" PRIx32 "\n",
                 machine->last_instruction_address,
                 instruction->raw, instruction->opcode,
                 name != NULL ? name : "unassigned",
                 instruction->result_register,
                 instruction->relative ? 1U : 0U,
                 instruction->indirect ? 1U : 0U,
                 instruction->index_register, instruction->displacement,
                 machine->last_effective_address,
                 machine->cpu.instruction_counter);
    (void)printf("registers: w0=%06" PRIx32 " w1=%06" PRIx32
                 " w2=%06" PRIx32 " w3=%06" PRIx32
                 " ex=%u%u%u ir=%06" PRIx32 " im=%06" PRIx32 "\n",
                 machine->cpu.w[0], machine->cpu.w[1], machine->cpu.w[2],
                 machine->cpu.w[3], machine->cpu.exception_21,
                 machine->cpu.exception_22, machine->cpu.exception_23,
                 machine->cpu.interrupt_requests,
                 machine->cpu.interrupt_mask);
    (void)printf("io: operations=%" PRIu64
                 " last-device=%u last-command=%u last-modifier=%u"
                 " device-ticks=%" PRIu64 "\n",
                 machine->io_operations, machine->last_io_device,
                 machine->last_io_command, machine->last_io_modifier,
                 machine->virtual_time_ticks);
}

static void terminal_console_output(void *context, uint8_t character)
{
    FILE *stream = context;

    if (character == UINT8_C(0x7f)) {
        return;
    }
    (void)fputc((int)character, stream);
    (void)fflush(stream);
}

static int runtime_destroy(rc_runtime_t *runtime)
{
    size_t index;
    int result = 0;

    if (runtime == NULL) {
        return 0;
    }
    for (index = 0; index < runtime->telnet_endpoint_count; ++index) {
        rc_telnet_endpoint_destroy(&runtime->telnet_endpoints[index]);
    }
    for (index = 0; index < runtime->reader_count; ++index) {
        rc_reader_device_destroy(&runtime->readers[index]);
    }
    for (index = 0; index < runtime->tape_count; ++index)
        rc_rc749_destroy(&runtime->tapes[index]);
    for (index = 0; index < runtime->punch_count; ++index) {
        rc_punch_device_destroy(&runtime->punches[index]);
    }
    for (index = 0; index < runtime->printer_count; ++index) {
        rc610_device_t *printer = &runtime->printers[index];
        char error[256];
        /* Finish a hardware print already accepted before shutdown. Do not
           print an incomplete line or invent a guest flush/job boundary. */
        printer->device.advance(printer, 1);
        if (rc610_close(printer, error, sizeof(error)) != 0) {
            (void)fprintf(stderr, "RC610 device %u: %s\n",
                          runtime->printer_numbers[index], error);
            result = -1;
        }
    }
    for (index = 0; index < runtime->disk_count; ++index) {
        if (runtime->disk_attached[index]) {
            rc_rc433x_device_destroy(&runtime->disks[index]);
        }
    }
    for (index = 0; index < runtime->controller_count; ++index)
        rc_dfc403_destroy(&runtime->controllers[index]);
    if (runtime->hostfiles_initialized) {
        rc_hostfiles_device_destroy(&runtime->hostfiles);
    }
    if (runtime->machine_initialized) {
        rc_machine_destroy(&runtime->machine);
    }
    free(runtime->host_input.buffer);
    memset(runtime, 0, sizeof(*runtime));
    return result;
}

static int runtime_init(rc_runtime_t *runtime, const char *config_path,
                        const char *monitor_override,
                        const char *disk_override,
                        const char *disk_overlay_override,
                        bool disk_write_through_override,
                        int set_disc_load_flag, int ix_override)
{
    rc_config_error_t config_error;
    char error[256];
    char monitor_path[RC_HOST_PATH_SIZE];
    size_t device_index;
    int disk_was_overridden = 0;
    int disk_overlay_was_overridden = 0;
    bool unsupported_devices = false;

    *runtime = (rc_runtime_t){0};
    if (rc_config_parse_file(config_path, &runtime->config,
                             &config_error) != 0) {
        (void)fprintf(stderr, "%s:%zu: %s\n", config_path,
                      config_error.line, config_error.message);
        return -1;
    }
    /* Check drive-only entries too, before loading the monitor or media. */
    for (device_index = 0; device_index < runtime->config.device_count;
         ++device_index) {
        const rc_device_config_t *device =
            &runtime->config.devices[device_index];
        switch (device->kind) {
        case RC_DEVICE_READER:
        case RC_DEVICE_RC749:
        case RC_DEVICE_PUNCH:
        case RC_DEVICE_RC610:
        case RC_DEVICE_CONSOLE:
        case RC_DEVICE_RC851:
        case RC_DEVICE_CLOCK:
        case RC_DEVICE_RC433X:
        case RC_DEVICE_DFC403:
        case RC_DEVICE_RC4819:
        case RC_DEVICE_RC4819_RAM:
        case RC_DEVICE_HOSTFILES:
            break;
        default:
            (void)fprintf(stderr,
                          "%s: device type %s is recognized but not implemented\n",
                          config_path, rc_device_kind_name(device->kind));
            unsupported_devices = true;
            break;
        }
    }
    if (unsupported_devices) {
        return -1;
    }
    if (monitor_override != NULL) {
        if (strlen(monitor_override) >= sizeof(monitor_path)) {
            (void)fprintf(stderr, "monitor path is too long\n");
            return -1;
        }
        (void)strcpy(monitor_path, monitor_override);
    } else if (runtime->config.monitor_path[0] == '\0' ||
               resolve_config_file(config_path, runtime->config.monitor_path,
                                   monitor_path, sizeof(monitor_path)) != 0) {
        (void)fprintf(stderr, "cannot resolve monitor file from %s\n",
                      config_path);
        return -1;
    }
    if (!rc_machine_init(&runtime->machine,
                         runtime->config.core_max_halfwords)) {
        (void)fprintf(stderr, "cannot allocate machine core store\n");
        return -1;
    }
    runtime->machine_initialized = true;
    if (ix_override >= 0) runtime->config.ix_enabled = ix_override != 0;
    runtime->machine.ix_enabled = runtime->config.ix_enabled;
    runtime->machine.protection_enabled = runtime->config.protection_enabled;
    if (rc_monitor_load(monitor_path, &runtime->machine.core,
                        set_disc_load_flag != 0, &runtime->load_address,
                        &runtime->entry_address, error, sizeof(error)) != 0) {
        (void)fprintf(stderr, "%s\n", error);
        runtime_destroy(runtime);
        return -1;
    }

    for (device_index = 0; device_index < runtime->config.device_count;
         ++device_index) {
        const rc_device_config_t *device =
            &runtime->config.devices[device_index];
        unsigned interrupt_level = 0;

        if (device->kind == RC_DEVICE_RC4819 ||
            device->kind == RC_DEVICE_RC4819_RAM) {
            size_t controller_index;
            for (controller_index = 0;
                 controller_index < runtime->controller_count;
                 ++controller_index) {
                if (runtime->controller_numbers[controller_index] ==
                    (unsigned)device->controller_number) break;
            }
            if (controller_index == runtime->controller_count) {
                (void)fprintf(stderr, "RC4819 controller is not attached\n");
                runtime_destroy(runtime);
                return -1;
            }
            rc_dfc403_device_t *controller = &runtime->controllers[controller_index];
            int mount_result;
            if (device->kind == RC_DEVICE_RC4819_RAM) {
                mount_result = rc_dfc403_mount_ram(controller,
                    (unsigned)device->unit_number, device->cylinders,
                    error, sizeof(error));
            } else {
                char image_path[RC_HOST_PATH_SIZE];
                int path_result;
                if (disk_override != NULL && !disk_was_overridden) {
                    int length = snprintf(image_path, sizeof(image_path), "%s",
                                          disk_override);
                    path_result = length < 0 || (size_t)length >= sizeof(image_path)
                        ? -1 : 0;
                    disk_was_overridden = 1;
                } else path_result = resolve_config_file(config_path, device->path,
                                                          image_path, sizeof(image_path));
                if (disk_overlay_override != NULL || disk_write_through_override) {
                    (void)fprintf(stderr, "RC4819 currently supports only volatile copy-on-write; disk persistence options require RC433X\n");
                    runtime_destroy(runtime);
                    return -1;
                }
                error[0] = '\0';
                if (path_result != 0)
                    (void)snprintf(error, sizeof(error), "cannot resolve RC4819 image path");
                mount_result = path_result != 0 ? -1 :
                    rc_dfc403_mount(controller, (unsigned)device->unit_number,
                                    image_path, error, sizeof(error));
            }
            if (mount_result != 0) {
                (void)fprintf(stderr, "cannot attach %s DFC=%d UNITNO=%d: %s\n",
                    rc_device_kind_name(device->kind), device->controller_number,
                    device->unit_number, error);
                runtime_destroy(runtime);
                return -1;
            }
            continue;
        }
        if (device->device_number < 0) {
            continue;
        }
        if (device->kind == RC_DEVICE_DFC403) {
            size_t index = runtime->controller_count++;
            rc_dfc403_init(&runtime->controllers[index], &runtime->machine,
                          (unsigned)device->interrupts[0],
                          (unsigned)device->interrupts[1]);
            runtime->controller_numbers[index] = (unsigned)device->device_number;
            (void)rc_machine_attach_device(&runtime->machine,
                (unsigned)device->device_number, &runtime->controllers[index].device);
        } else if (device->kind == RC_DEVICE_HOSTFILES) {
            char root[RC_HOST_PATH_SIZE];
            (void)snprintf(error, sizeof(error), "HOSTFILES root path is too long");
            if (resolve_config_path(config_path, device->path,
                                     root, sizeof(root)) != 0 ||
                rc_hostfiles_device_init(
                    &runtime->hostfiles, &runtime->machine, root,
                    (unsigned)device->interrupts[0], error, sizeof(error)) != 0) {
                (void)fprintf(stderr, "cannot initialize HOSTFILES: %s\n",
                              error);
                runtime_destroy(runtime);
                return -1;
            }
            runtime->hostfiles_initialized = true;
            (void)rc_machine_attach_device(&runtime->machine, 63,
                                            &runtime->hostfiles.device);
        } else if (device->kind == RC_DEVICE_CLOCK) {
            if (device->interrupt_count != 0) {
                interrupt_level = (unsigned)device->interrupts[0];
            }
            rc_clock_device_init(&runtime->clocks[runtime->clock_count],
                                 interrupt_level);
            (void)rc_machine_attach_device(
                &runtime->machine, (unsigned)device->device_number,
                &runtime->clocks[runtime->clock_count].device);
            ++runtime->clock_count;
        } else if ((device->kind == RC_DEVICE_CONSOLE ||
                    device->kind == RC_DEVICE_RC851) &&
                   device->interrupt_count == 2) {
            rc_console_device_init(
                &runtime->consoles[runtime->console_count],
                (unsigned)device->interrupts[0],
                (unsigned)device->interrupts[1]);
            if (device->terminal_protocol == RC_TERMINAL_V2)
                (void)rc_console_device_set_protocol(
                    &runtime->consoles[runtime->console_count], &runtime->machine,
                    RC_TERMINAL_V2);
            rc_console_device_set_output(
                &runtime->consoles[runtime->console_count],
                runtime->console_count == 0 ? terminal_console_output : NULL,
                runtime->console_count == 0 ? stdout : NULL);
            (void)rc_machine_attach_device(
                &runtime->machine, (unsigned)device->device_number,
                &runtime->consoles[runtime->console_count].device);
            runtime->console_numbers[runtime->console_count] =
                (unsigned)device->device_number;
            runtime->console_kinds[runtime->console_count] = device->kind;
            ++runtime->console_count;
        } else if (device->kind == RC_DEVICE_READER &&
                   device->interrupt_count == 1) {
            char tape_path[RC_HOST_PATH_SIZE];

            error[0] = '\0';
            rc_reader_device_init(
                &runtime->readers[runtime->reader_count],
                (unsigned)device->interrupts[0]);
            if (device->path[0] != '\0') {
                if (resolve_config_file(config_path, device->path, tape_path,
                                        sizeof(tape_path)) != 0 ||
                    rc_reader_device_mount(
                        &runtime->readers[runtime->reader_count], tape_path,
                        error, sizeof(error)) != 0) {
                    (void)fprintf(stderr,
                                  "cannot mount READER device %d: %s\n",
                                  device->device_number,
                                  error[0] != '\0' ? error : device->path);
                    runtime_destroy(runtime);
                    return -1;
                }
            }
            (void)rc_machine_attach_device(
                &runtime->machine, (unsigned)device->device_number,
                &runtime->readers[runtime->reader_count].device);
            runtime->reader_numbers[runtime->reader_count] =
                (unsigned)device->device_number;
            ++runtime->reader_count;
        } else if (device->kind == RC_DEVICE_RC749) {
            size_t tape_index = runtime->tape_count++;
            char path[RC_HOST_PATH_SIZE];
            rc_rc749_device_t *tape = &runtime->tapes[tape_index];
            rc_rc749_init(tape, &runtime->machine, (unsigned)device->interrupts[0]);
            runtime->tape_numbers[tape_index] = (unsigned)device->device_number;
            error[0] = '\0';
            if ((device->root_path[0] != '\0' &&
                 (resolve_config_file(config_path, device->root_path, path, sizeof(path)) != 0 ||
                  rc_rc749_set_root(tape, path, error, sizeof(error)) != 0)) ||
                (device->path[0] != '\0' &&
                 (resolve_config_file(config_path, device->path, path, sizeof(path)) != 0 ||
                  rc_rc749_mount(tape, path, error, sizeof(error)) != 0))) {
                (void)fprintf(stderr, "cannot initialize RC749 device %d: %s\n",
                              device->device_number, error);
                runtime_destroy(runtime);
                return -1;
            }
            (void)rc_machine_attach_device(&runtime->machine,
                (unsigned)device->device_number, &tape->device);
        } else if (device->kind == RC_DEVICE_PUNCH &&
                   device->interrupt_count == 1) {
            char tape_path[RC_HOST_PATH_SIZE];

            rc_punch_device_init(
                &runtime->punches[runtime->punch_count],
                (unsigned)device->interrupts[0]);
            if (device->path[0] != '\0') {
                error[0] = '\0';
                if (resolve_config_path(config_path, device->path, tape_path,
                                        sizeof(tape_path)) != 0 ||
                    rc_punch_device_mount(
                        &runtime->punches[runtime->punch_count], tape_path,
                        false, error, sizeof(error)) != 0) {
                    (void)fprintf(stderr,
                                  "cannot mount PUNCH device %d: %s\n",
                                  device->device_number,
                                  error[0] != '\0' ? error : device->path);
                    runtime_destroy(runtime);
                    return -1;
                }
            }
            (void)rc_machine_attach_device(
                &runtime->machine, (unsigned)device->device_number,
                &runtime->punches[runtime->punch_count].device);
            runtime->punch_numbers[runtime->punch_count] =
                (unsigned)device->device_number;
            ++runtime->punch_count;
        } else if (device->kind == RC_DEVICE_RC610) {
            size_t printer_index = runtime->printer_count++;
            rc610_device_t *printer = &runtime->printers[printer_index];
            char path[RC_HOST_PATH_SIZE];
            runtime->printer_numbers[printer_index] = (unsigned)device->device_number;
            (void)rc610_init(printer, (unsigned)device->interrupts[0]);
            error[0] = '\0';
            if (device->path[0] != '\0' &&
                (resolve_config_path(config_path, device->path, path, sizeof(path)) != 0 ||
                 (device->append ? rc610_open_append(printer, path, error, sizeof(error)) :
                                   rc610_open(printer, path, error, sizeof(error))) != 0)) {
                (void)fprintf(stderr, "cannot open RC610 device %d output: %s\n",
                              device->device_number, error[0] != '\0' ? error : "invalid output path");
                runtime_destroy(runtime);
                return -1;
            }
            (void)rc_machine_attach_device(&runtime->machine,
                (unsigned)device->device_number, &printer->device);
        } else if (device->kind == RC_DEVICE_RC433X) {
            char image_path[RC_HOST_PATH_SIZE];
            char overlay_path[RC_HOST_PATH_SIZE];
            bool persistent = false;
            bool write_through = device->writable;
            int path_result;

            error[0] = '\0';
            if (disk_override != NULL && !disk_was_overridden) {
                int length = snprintf(image_path, sizeof(image_path), "%s",
                                      disk_override);
                path_result = length < 0 ||
                              (size_t)length >= sizeof(image_path) ? -1 : 0;
                disk_was_overridden = 1;
                if (disk_write_through_override) {
                    write_through = true;
                }
            } else {
                path_result = resolve_config_file(
                    config_path, device->path, image_path,
                    sizeof(image_path));
            }
            if (write_through && disk_overlay_override != NULL &&
                !disk_overlay_was_overridden) {
                (void)fprintf(stderr,
                              "RC433X write-through and overlay modes conflict\n");
                runtime_destroy(runtime);
                return -1;
            }
            if (write_through) {
                persistent = false;
            } else if (disk_overlay_override != NULL &&
                !disk_overlay_was_overridden) {
                int length = snprintf(overlay_path, sizeof(overlay_path),
                                      "%s", disk_overlay_override);
                if (length < 0 || (size_t)length >= sizeof(overlay_path)) {
                    path_result = -1;
                } else {
                    persistent = true;
                }
                disk_overlay_was_overridden = 1;
            } else if (device->overlay_path[0] != '\0') {
                if (resolve_config_path(config_path, device->overlay_path,
                                        overlay_path,
                                        sizeof(overlay_path)) != 0) {
                    path_result = -1;
                } else {
                    persistent = true;
                }
            }
            if (path_result != 0 ||
                (write_through
                     ? rc_rc433x_device_init_writable(
                           &runtime->disks[runtime->disk_count],
                           &runtime->machine, image_path,
                           error, sizeof(error))
                 : persistent
                     ? rc_rc433x_device_init_persistent(
                           &runtime->disks[runtime->disk_count],
                           &runtime->machine, image_path, overlay_path,
                           error, sizeof(error))
                     : rc_rc433x_device_init_cow(
                    &runtime->disks[runtime->disk_count], &runtime->machine,
                           image_path, error, sizeof(error))) != 0) {
                (void)fprintf(stderr, "cannot attach RC433X device %d: %s\n",
                              device->device_number,
                              error[0] != '\0' ? error : device->path);
                runtime_destroy(runtime);
                return -1;
            }
            (void)rc_machine_attach_device(
                &runtime->machine, (unsigned)device->device_number,
                &runtime->disks[runtime->disk_count].device);
            runtime->disk_numbers[runtime->disk_count] =
                (unsigned)device->device_number;
            runtime->disk_attached[runtime->disk_count] = true;
            ++runtime->disk_count;
        }
    }
    for (device_index = 0;
         device_index < runtime->config.telnet_endpoint_count;
         ++device_index) {
        const rc_telnet_config_t *config =
            &runtime->config.telnet_endpoints[device_index];
        size_t console_index;

        for (console_index = 0; console_index < runtime->console_count;
             ++console_index) {
            if (runtime->console_numbers[console_index] ==
                (unsigned)config->device_number) {
                break;
            }
        }
        if (console_index == runtime->console_count) {
            (void)fprintf(stderr,
                          "TELNET device %d is not an implemented console\n",
                          config->device_number);
            runtime_destroy(runtime);
            return -1;
        }
        if (console_index == 0) {
            (void)fprintf(stderr,
                          "TELNET device %d is the primary local console\n",
                          config->device_number);
            runtime_destroy(runtime);
            return -1;
        }
        if (rc_telnet_endpoint_init(
                &runtime->telnet_endpoints[runtime->telnet_endpoint_count],
                config->address, config->port, config->device_number,
                &runtime->consoles[console_index],
                runtime->console_kinds[console_index] == RC_DEVICE_RC851
                    ? RC_TELNET_TERMINAL_RC851
                    : RC_TELNET_TERMINAL_CONSOLE,
                runtime->config.console_delete_mapping,
                config->attention_mode, config->attention_character,
                error, sizeof(error)) != 0) {
            (void)fprintf(stderr, "%s\n", error);
            runtime_destroy(runtime);
            return -1;
        }
        rc_console_device_set_output(
            &runtime->consoles[console_index], rc_telnet_endpoint_output,
            &runtime->telnet_endpoints[runtime->telnet_endpoint_count]);
        {
            char attention[16];

            (void)printf("telnet: device=%d listening=%s:%u attention=%s\n",
                         config->device_number, config->address,
                         (unsigned)config->port,
                         telnet_attention_name(config->attention_mode,
                                               config->attention_character,
                                               attention));
        }
        ++runtime->telnet_endpoint_count;
    }
    runtime->machine.cpu.instruction_counter = runtime->entry_address;
    runtime->machine.cpu.stopped = true;
    return 0;
}

static rc_step_result_t runtime_step(rc_runtime_t *runtime)
{
    rc_step_result_t result;

    /* Deliver buffered bytes during step/trace as well as continuous runs.
     * The bounded runner never uses this FIFO, preserving its trace timing. */
    if (runtime->host_input.buffer_count != 0 && runtime->console_count != 0)
        drain_host_input(&runtime->host_input, &runtime->consoles[0]);
    result = rc_machine_step(&runtime->machine);
    ++runtime->interpreter_calls;
    return result;
}

static void pump_telnet_endpoints(rc_runtime_t *runtime)
{
    size_t index;

    for (index = 0; index < runtime->telnet_endpoint_count; ++index) {
        rc_telnet_endpoint_pump(&runtime->telnet_endpoints[index]);
    }
}

static void print_telnet_statistics(const rc_runtime_t *runtime)
{
    size_t index;

    for (index = 0; index < runtime->telnet_endpoint_count; ++index) {
        const rc_telnet_endpoint_t *endpoint =
            &runtime->telnet_endpoints[index];

        (void)printf("telnet: device=%d port=%u accepted=%" PRIu64
                     " rejected=%" PRIu64 " dropped-input=%" PRIu64
                     " dropped-output=%" PRIu64 "\n",
                     endpoint->device_number, (unsigned)endpoint->port,
                     endpoint->accepted_connections,
                     endpoint->rejected_connections,
                     endpoint->dropped_input, endpoint->dropped_output);
    }
}

static void interactive_show_telnet(const rc_runtime_t *runtime)
{
    size_t index;

    if (runtime->telnet_endpoint_count == 0) {
        (void)puts("no Telnet endpoints configured");
        return;
    }
    for (index = 0; index < runtime->telnet_endpoint_count; ++index) {
        const rc_telnet_endpoint_t *endpoint =
            &runtime->telnet_endpoints[index];
        char attention[16];

        (void)printf(
            "device=%d type=%s listen=%s:%u state=%s"
            " input-buffer=%zu output-buffer=%zu"
            " attention=%s attention-requests=%" PRIu64
            " accepted=%" PRIu64 " rejected=%" PRIu64
            " dropped-input=%" PRIu64 " dropped-output=%" PRIu64 "\n",
            endpoint->device_number,
            endpoint->terminal_kind == RC_TELNET_TERMINAL_RC851
                ? "RC851" : "CONSOLE",
            endpoint->address, (unsigned)endpoint->port,
            endpoint->client_fd >= 0 ? "connected" : "listening",
            endpoint->input_count, endpoint->output_count,
            telnet_attention_name(endpoint->attention_mode,
                                  endpoint->attention_character, attention),
            endpoint->attention_requests,
            endpoint->accepted_connections, endpoint->rejected_connections,
            endpoint->dropped_input, endpoint->dropped_output);
    }
}

static size_t runtime_disk_index(const rc_runtime_t *runtime,
                                 unsigned device_number)
{
    size_t index;

    for (index = 0; index < runtime->disk_count; ++index) {
        if (runtime->disk_numbers[index] == device_number) {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t runtime_reader_index(const rc_runtime_t *runtime,
                                   unsigned device_number)
{
    size_t index;

    for (index = 0; index < runtime->reader_count; ++index) {
        if (runtime->reader_numbers[index] == device_number) {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t runtime_tape_index(const rc_runtime_t *runtime, unsigned device_number)
{
    for (size_t index = 0; index < runtime->tape_count; ++index)
        if (runtime->tape_numbers[index] == device_number) return index;
    return SIZE_MAX;
}

static size_t runtime_punch_index(const rc_runtime_t *runtime,
                                  unsigned device_number)
{
    size_t index;

    for (index = 0; index < runtime->punch_count; ++index) {
        if (runtime->punch_numbers[index] == device_number) {
            return index;
        }
    }
    return SIZE_MAX;
}

static void interactive_show_devices(const rc_runtime_t *runtime)
{
    size_t index;

    for (index = 0; index < runtime->config.device_count; ++index) {
        const rc_device_config_t *config = &runtime->config.devices[index];
        size_t interrupt;

        const char *state = "not-implemented";

        if (config->kind == RC_DEVICE_RC4819 ||
            config->kind == RC_DEVICE_RC4819_RAM) state = "attached";

        if (config->device_number >= 0 &&
            runtime->machine.devices[config->device_number] != NULL) {
            state = "attached";
        } else if (config->kind == RC_DEVICE_RC433X &&
                   config->device_number >= 0 &&
                   runtime_disk_index(runtime,
                                      (unsigned)config->device_number) !=
                       SIZE_MAX) {
            state = "detached";
        }
        (void)printf("device=%d type=%s state=%s", config->device_number,
                     rc_device_kind_name(config->kind), state);
        if (config->terminal_protocol == RC_TERMINAL_V2) (void)printf(" protocol=v2");
        for (interrupt = 0; interrupt < config->interrupt_count; ++interrupt) {
            (void)printf(" int%zu=%d", interrupt,
                         config->interrupts[interrupt]);
        }
        if (config->kind == RC_DEVICE_HOSTFILES) {
            (void)printf(" root=%s file=%s", config->path,
                         runtime->hostfiles.file == NULL ? "closed" : "open");
        }
        if (config->kind == RC_DEVICE_RC4819 ||
            config->kind == RC_DEVICE_RC4819_RAM)
            (void)printf(" dfc=%d unit=%d", config->controller_number,
                         config->unit_number);
        (void)putchar('\n');
    }
}

static size_t runtime_printer_index(const rc_runtime_t *runtime, unsigned number)
{
    for (size_t i = 0; i < runtime->printer_count; ++i)
        if (runtime->printer_numbers[i] == number) return i;
    return SIZE_MAX;
}

static void print_printer_statistics(const rc_runtime_t *runtime)
{
    for (size_t i = 0; i < runtime->printer_count; ++i) {
        const rc610_device_t *p = &runtime->printers[i];
        (void)printf("device=%u type=RC610 output=%s status=%06x\n",
                     runtime->printer_numbers[i], p->connected ? "open" : "closed",
                     (unsigned)rc610_status(p));
        (void)printf("  prints=%" PRIu64 " failed=%" PRIu64 "\n",
                     p->completed_prints, p->failed_prints);
        (void)printf("  bytes=%" PRIu64 " errors=%" PRIu64 "\n",
                     p->output_bytes, p->output_errors);
        (void)printf("  buffered=%zu ignored=%" PRIu64 " overflow=%" PRIu64 "\n",
                     p->line_length, p->ignored_characters, p->overflow_characters);
        if (p->connected) {
            for (size_t offset = 0; offset < strlen(p->path); offset += 72)
                (void)printf("  %.*s\n", 72, p->path + offset);
        }
    }
}

static void interactive_show_media(const rc_runtime_t *runtime)
{
    size_t index;

    if (runtime->reader_count == 0 && runtime->punch_count == 0 &&
        runtime->disk_count == 0 && runtime->controller_count == 0 && runtime->tape_count == 0 &&
        runtime->printer_count == 0) {
        (void)puts("no removable media devices configured");
        return;
    }
    print_printer_statistics(runtime);
    for (index = 0; index < runtime->controller_count; ++index) {
        const rc_dfc403_device_t *controller = &runtime->controllers[index];
        for (unsigned unit = 0; unit < RC4819_UNITS; ++unit) {
            const rc_rc4819_drive_t *drive = &controller->drives[unit];
            if (!drive->present) continue;
            (void)printf("dfc=%u unit=%u type=%s state=attached",
                runtime->controller_numbers[index], unit,
                drive->memory ? "RC4819RAM" : "RC4819");
            if (drive->memory) (void)printf(" cylinders=%u", drive->ram_cylinders);
            else (void)printf(" base=%s mode=volatile-overlay overlay-sectors=%" PRIu64,
                drive->storage.base_image_path, drive->storage.overlay_sectors);
            (void)printf(" cylinder=%d head=%u sector=%u\n",
                         drive->cylinder, drive->head, drive->sector);
        }
    }
    for (index = 0; index < runtime->reader_count; ++index) {
        const rc_reader_device_t *reader = &runtime->readers[index];

        if (!reader->tape_present) {
            (void)printf("device=%u type=READER state=empty\n",
                         runtime->reader_numbers[index]);
        } else {
            (void)printf("device=%u type=READER state=mounted tape=%s"
                         " position=%ld characters=%" PRIu64 "\n",
                         runtime->reader_numbers[index], reader->path,
                         ftell(reader->file), reader->characters_read);
        }
    }
    for (index = 0; index < runtime->tape_count; ++index) {
        const rc_rc749_device_t *tape = &runtime->tapes[index];
        (void)printf("device=%u type=RC749 mode=read-only state=%s\n",
                     runtime->tape_numbers[index], tape->file != NULL ? "mounted" : "empty");
        if (tape->file != NULL) {
            (void)printf("  tape=%s\n", tape->path);
            (void)printf("  position=%zu records=%zu status=%06x\n",
                         tape->position, tape->record_count, tape->status);
            (void)printf("  reads=%" PRIu64 " bytes=%" PRIu64 " rejected-writes=%" PRIu64 "\n",
                         tape->reads, tape->bytes_read, tape->rejected_writes);
        }
    }
    for (index = 0; index < runtime->punch_count; ++index) {
        const rc_punch_device_t *punch = &runtime->punches[index];

        if (!punch->tape_present) {
            (void)printf("device=%u type=PUNCH state=empty\n",
                         runtime->punch_numbers[index]);
        } else {
            (void)printf("device=%u type=PUNCH state=mounted tape=%s"
                         " characters=%" PRIu64 "\n",
                         runtime->punch_numbers[index], punch->path,
                         punch->characters_written);
        }
    }
    for (index = 0; index < runtime->disk_count; ++index) {
        const rc_rc433x_device_t *disk = &runtime->disks[index];

        if (!runtime->disk_attached[index]) {
            (void)printf("device=%u type=RC433X state=detached\n",
                         runtime->disk_numbers[index]);
            continue;
        }
        (void)printf("device=%u type=RC433X state=attached base=%s mode=%s"
                     " overlay-sectors=%" PRIu64,
                     runtime->disk_numbers[index], disk->base_image_path,
                     disk_mode_name(disk),
                     disk->overlay_sectors);
        if (disk->persistent_overlay) {
            (void)printf(" overlay=%s", disk->overlay_path);
        }
        (void)putchar('\n');
    }
}

static void print_disk_statistics(const rc_runtime_t *runtime)
{
    size_t index;

    for (index = 0; index < runtime->controller_count; ++index) {
        const rc_dfc403_device_t *controller = &runtime->controllers[index];
        (void)printf("dfc403: device=%u transfers=%" PRIu64 " seeks=%" PRIu64
                     " selected-unit=%u\n", runtime->controller_numbers[index],
                     controller->data_operations, controller->seek_operations,
                     controller->unit);
    }

    for (index = 0; index < runtime->disk_count; ++index) {
        const rc_rc433x_device_t *disk = &runtime->disks[index];
        if (!runtime->disk_attached[index]) {
            continue;
        }
        (void)printf("rc433x: device=%u reads=%" PRIu64
                     " sectors-read=%" PRIu64 " writes=%" PRIu64
                     " sectors-written=%" PRIu64
                     " overlay-sectors=%" PRIu64
                     " rejected-writes=%" PRIu64
                     " persistence-failures=%" PRIu64 " mode=%s",
                     runtime->disk_numbers[index], disk->read_operations,
                     disk->sectors_read, disk->write_operations,
                     disk->sectors_written, disk->overlay_sectors,
                     disk->rejected_writes, disk->persistence_failures,
                     disk_mode_name(disk));
        if (disk->persistent_overlay) {
            (void)printf(" overlay=%s", disk->overlay_path);
        }
        (void)putchar('\n');
    }
}

static void print_reader_statistics(const rc_runtime_t *runtime)
{
    size_t index;

    for (index = 0; index < runtime->reader_count; ++index) {
        const rc_reader_device_t *reader = &runtime->readers[index];

        (void)printf("reader: device=%u tape=%s reads=%" PRIu64
                     " characters=%" PRIu64 " nulls-skipped=%" PRIu64
                     " parity-errors=%" PRIu64 "\n",
                     runtime->reader_numbers[index],
                     reader->tape_present ? reader->path : "none",
                     reader->read_operations, reader->characters_read,
                     reader->null_bytes_skipped, reader->parity_errors);
    }
}

static void print_punch_statistics(const rc_runtime_t *runtime)
{
    size_t index;

    for (index = 0; index < runtime->punch_count; ++index) {
        const rc_punch_device_t *punch = &runtime->punches[index];

        (void)printf("punch: device=%u tape=%s writes=%" PRIu64
                     " characters=%" PRIu64 " errors=%" PRIu64 "\n",
                     runtime->punch_numbers[index],
                     punch->tape_present ? punch->path : "none",
                     punch->write_operations, punch->characters_written,
                     punch->write_errors);
    }
}

static const char *disk_mode_name(const rc_rc433x_device_t *disk)
{
    if (disk->readonly) {
        return "read-only";
    }
    if (disk->write_through) {
        return "write-through";
    }
    return disk->persistent_overlay ? "persistent-overlay"
                                    : "volatile-overlay";
}

static int run_monitor(const char *config_path, const char *monitor_override,
                       const char *disk_override,
                       const char *disk_overlay_override,
                       bool disk_write_through_override,
                       const char *step_text, const char *trace_path,
                       int set_disc_load_flag, int ix_override)
{
    rc_runtime_t runtime;
    rc_host_console_input_t host_console_input = {0};
    uint64_t step_limit;
    uint64_t calls;
    rc_step_result_t result = RC_STEP_OK;
    bool host_console_started = false;
    bool trace_failed = false;
    int received_signal;

    if (parse_step_limit(step_text, &step_limit) != 0) {
        (void)fprintf(stderr, "invalid positive step count: %s\n", step_text);
        return EXIT_FAILURE;
    }
    if (runtime_init(&runtime, config_path, monitor_override, disk_override,
                     disk_overlay_override, disk_write_through_override,
                     set_disc_load_flag, ix_override) != 0) {
        return EXIT_FAILURE;
    }
    if (trace_path != NULL) {
        runtime.trace_stream = fopen(trace_path, "wx");
        if (runtime.trace_stream == NULL) {
            (void)fprintf(stderr, "cannot create trace file %s: %s\n",
                          trace_path, strerror(errno));
            runtime_destroy(&runtime);
            return EXIT_FAILURE;
        }
        if (!rc_trace_write_header(runtime.trace_stream)) {
            (void)fprintf(stderr, "cannot write trace file %s: %s\n",
                          trace_path, strerror(errno));
            (void)fclose(runtime.trace_stream);
            runtime.trace_stream = NULL;
            runtime_destroy(&runtime);
            return EXIT_FAILURE;
        }
        rc_machine_set_write_observer(&runtime.machine,
                                      runtime_observe_write, &runtime);
    }
    host_stop_signal = 0;
    if (install_host_signal_handlers() != 0) {
        (void)fprintf(stderr, "cannot install host signal handlers: %s\n",
                      strerror(errno));
        if (runtime.trace_stream != NULL) {
            (void)fclose(runtime.trace_stream);
            runtime.trace_stream = NULL;
        }
        runtime_destroy(&runtime);
        return EXIT_FAILURE;
    }
    runtime.machine.cpu.stopped = false;
    if (runtime.console_count != 0) {
        start_host_console_input(&host_console_input);
        host_console_input.date_shortcut_key =
            RC_CONFIG_SHORTCUT_DISABLED;
        host_console_input.delete_mapping =
            runtime.config.console_delete_mapping;
        host_console_started = true;
    }
    for (calls = 0; calls < step_limit && host_stop_signal == 0; ++calls) {
        uint64_t instructions_before;
        uint64_t io_before;
        uint64_t interrupts_before;

        runtime.trace_call = calls + 1U;
        if (calls == 0 && host_console_started) {
            pump_host_console_input(&host_console_input, &runtime.consoles[0],
                                    RC_CONFIG_OPERATOR_ESCAPE_DISABLED, NULL);
        }
        if (calls != 0 &&
            calls % RC_RUN_INSTRUCTIONS_PER_DEVICE_TICK == 0) {
            rc_machine_advance_time(&runtime.machine, 1);
            pump_telnet_endpoints(&runtime);
            if (host_console_started) {
                pump_host_console_input(
                    &host_console_input, &runtime.consoles[0],
                    RC_CONFIG_OPERATOR_ESCAPE_DISABLED, NULL);
            }
        }
        instructions_before = runtime.machine.instructions_executed;
        io_before = runtime.machine.io_operations;
        interrupts_before = runtime.machine.interrupts_serviced;
        result = runtime_step(&runtime);
        if (runtime.trace_stream != NULL &&
            !rc_trace_write_step(runtime.trace_stream, runtime.trace_call,
                                 &runtime.machine, result,
                                 instructions_before, io_before,
                                 interrupts_before)) {
            runtime.trace_failed = true;
        }
        if (runtime.trace_failed) {
            result = RC_STEP_STOPPED;
            ++calls;
            break;
        }
        if (result != RC_STEP_OK) {
            ++calls;
            break;
        }
    }
    if (host_console_started) {
        stop_host_console_input();
    }
    print_machine_stop(&runtime.machine,
                       host_stop_signal != 0 ? "host-signal" :
                       result == RC_STEP_OK ? "step-limit"
                                            : step_result_name(result),
                       calls);
    if (host_console_started) {
        (void)printf("console: device=primary host-input=%" PRIu64
                     " eof=%s\n",
                     host_console_input.characters_submitted,
                     host_console_input.eof ? "yes" : "no");
    }
    print_disk_statistics(&runtime);
    print_reader_statistics(&runtime);
    print_punch_statistics(&runtime);
    print_printer_statistics(&runtime);
    print_telnet_statistics(&runtime);
    if (runtime.trace_stream != NULL) {
        int flush_result;
        int close_result;

        rc_machine_set_write_observer(&runtime.machine, NULL, NULL);
        flush_result = fflush(runtime.trace_stream);
        close_result = fclose(runtime.trace_stream);
        trace_failed = runtime.trace_failed || flush_result != 0 ||
                       close_result != 0;
        runtime.trace_stream = NULL;
        if (trace_failed) {
            (void)fprintf(stderr, "cannot complete trace file %s: %s\n",
                          trace_path, strerror(errno));
        }
    }
    received_signal = (int)host_stop_signal;
    int shutdown_result = runtime_destroy(&runtime);
    restore_host_signal_handlers();
    if (received_signal != 0) {
        return 128 + received_signal;
    }
    if (trace_failed || shutdown_result != 0) {
        return EXIT_FAILURE;
    }
    return result == RC_STEP_ADDRESS_EXCEPTION ||
                   result == RC_STEP_ILLEGAL_INSTRUCTION
        ? EXIT_FAILURE : EXIT_SUCCESS;
}

static void show_cpu(const rc_runtime_t *runtime)
{
    const rc_cpu_t *cpu = &runtime->machine.cpu;

    (void)printf("IC=%06" PRIx32 " W0=%06" PRIx32 " W1=%06" PRIx32
                 " W2=%06" PRIx32 " W3=%06" PRIx32 "\n",
                 cpu->instruction_counter, cpu->w[0], cpu->w[1], cpu->w[2],
                 cpu->w[3]);
    (void)printf("IR=%06" PRIx32 " IM=%06" PRIx32
                 " interrupts=%s monitor=%s\n"
                 "reset=%s stopped=%s"
                 " exceptions=%u%u%u calls=%" PRIu64 "\n",
                 cpu->interrupt_requests, cpu->interrupt_mask,
                 cpu->interrupts_enabled ? "on" : "off",
                 cpu->monitor_mode ? "yes" : "no",
                 cpu->reset_mode ? "yes" : "no",
                 cpu->stopped ? "yes" : "no", cpu->exception_21,
                 cpu->exception_22, cpu->exception_23,
                 runtime->interpreter_calls);
    (void)printf("cpu=RC4000 ix=%s\n", runtime->machine.ix_enabled ? "on" : "off");
    (void)printf("protection=%s PR=%02x\n",
                 runtime->machine.protection_enabled ? "rc4000" : "off",
                 (unsigned)cpu->protection_register);
}

static void show_throttle(const rc_runtime_t *runtime)
{
    char text[80];
    rc_throttle_format(runtime->config.throttle, text, sizeof(text));
    (void)printf("throttle=%s (interactive run only)\n", text);
}

static int parse_unsigned(const char *text, int base, uint64_t *value)
{
    char *end;
    unsigned long long parsed;

    if (text == NULL || *text == '\0' || *text == '-') {
        return -1;
    }
    errno = 0;
    parsed = strtoull(text, &end, base);
    if (errno != 0 || end == text || *end != '\0') {
        return -1;
    }
    *value = (uint64_t)parsed;
    return 0;
}

static void show_keys(const rc_runtime_t *runtime, const char *address_text,
                       const char *count_text, const char *extra_text)
{
    uint64_t address, count = 1;
    if (parse_unsigned(address_text, 0, &address) != 0 || address > RC_WORD_MASK ||
        (count_text != NULL &&
         (parse_unsigned(count_text, 0, &count) != 0 || count == 0 || count > 256)) ||
        extra_text != NULL) {
        (void)puts("usage: show keys ADDRESS [COUNT=1..256] (RC bytes, decimal or 0xHEX)");
        return;
    }
    /* Validate the entire range before printing; a trailing halfword has a key. */
    uint64_t last = address + (count - 1) * 2;
    if (last > RC_WORD_MASK || last >= runtime->machine.core.halfword_count) {
        (void)puts("key address range is outside installed core");
        return;
    }
    address &= ~UINT64_C(1);
    for (uint64_t i = 0; i < count; ++i) {
        uint8_t key;
        rc_addr_t current = (rc_addr_t)(address + i * 2);
        if (!rc_core_read_key(&runtime->machine.core, current, &key)) {
            (void)puts("cannot read core key");
            return;
        }
        (void)printf("%06" PRIx32 ": key=%u PR-protected=%s\n", current,
                     (unsigned)key,
                     rc_cpu_key_is_protected(&runtime->machine.cpu, key) ? "yes" : "no");
    }
}

static bool execution_breakpoint_exists(const rc_runtime_t *runtime,
                                        rc_addr_t address)
{
    size_t index;

    for (index = 0; index < runtime->execution_breakpoint_count; ++index) {
        if (runtime->execution_breakpoints[index] == address) {
            return true;
        }
    }
    return false;
}

static int parse_breakpoint_address(const char *text, rc_addr_t *address)
{
    uint64_t parsed;

    if (parse_unsigned(text, 0, &parsed) != 0 || parsed > RC_WORD_MASK ||
        (parsed & UINT64_C(1)) != 0) {
        (void)puts("breakpoint address must be an even 24-bit byte address");
        return -1;
    }
    *address = (rc_addr_t)parsed;
    return 0;
}

static void interactive_show_breakpoints(const rc_runtime_t *runtime)
{
    size_t index;

    if (runtime->execution_breakpoint_count == 0) {
        (void)puts("no execution breakpoints");
        return;
    }
    (void)puts("execution breakpoints:");
    for (index = 0; index < runtime->execution_breakpoint_count; ++index) {
        (void)printf("  %06" PRIx32 "\n",
                     runtime->execution_breakpoints[index]);
    }
}

static void interactive_set_breakpoint(rc_runtime_t *runtime,
                                       const char *address_text,
                                       const char *extra_text)
{
    rc_addr_t address;

    if (address_text == NULL || extra_text != NULL) {
        (void)puts("usage: break ADDRESS");
        return;
    }
    if (parse_breakpoint_address(address_text, &address) != 0) {
        return;
    }
    if (execution_breakpoint_exists(runtime, address)) {
        (void)printf("execution breakpoint already set at %06" PRIx32 "\n",
                     address);
        return;
    }
    if (runtime->execution_breakpoint_count ==
        RC_MAX_EXECUTION_BREAKPOINTS) {
        (void)printf("execution breakpoint limit reached (%u)\n",
                     (unsigned)RC_MAX_EXECUTION_BREAKPOINTS);
        return;
    }
    runtime->execution_breakpoints[runtime->execution_breakpoint_count++] =
        address;
    (void)printf("execution breakpoint set at %06" PRIx32 "\n", address);
}

static void interactive_remove_breakpoint(rc_runtime_t *runtime,
                                          const char *address_text,
                                          const char *extra_text)
{
    rc_addr_t address;
    size_t index;

    if (address_text == NULL || extra_text != NULL) {
        (void)puts("usage: nobreak ADDRESS|all");
        return;
    }
    if (strcmp(address_text, "all") == 0) {
        runtime->execution_breakpoint_count = 0;
        runtime->resume_breakpoint = false;
        (void)puts("all execution breakpoints removed");
        return;
    }
    if (parse_breakpoint_address(address_text, &address) != 0) {
        return;
    }
    for (index = 0; index < runtime->execution_breakpoint_count; ++index) {
        if (runtime->execution_breakpoints[index] == address) {
            size_t remaining = runtime->execution_breakpoint_count - index - 1;

            if (remaining != 0) {
                memmove(&runtime->execution_breakpoints[index],
                        &runtime->execution_breakpoints[index + 1],
                        remaining * sizeof(runtime->execution_breakpoints[0]));
            }
            --runtime->execution_breakpoint_count;
            if (runtime->resume_breakpoint_address == address) {
                runtime->resume_breakpoint = false;
            }
            (void)printf("execution breakpoint removed at %06" PRIx32 "\n",
                         address);
            return;
        }
    }
    (void)printf("no execution breakpoint at %06" PRIx32 "\n", address);
}

static bool write_breakpoint_exists(const rc_runtime_t *runtime,
                                    rc_addr_t address)
{
    size_t index;

    for (index = 0; index < runtime->write_breakpoint_count; ++index) {
        if (runtime->write_breakpoints[index] == address) {
            return true;
        }
    }
    return false;
}

static void interactive_show_watchpoints(const rc_runtime_t *runtime)
{
    size_t index;

    if (runtime->write_breakpoint_count == 0) {
        (void)puts("no core-write breakpoints");
        return;
    }
    (void)puts("core-write breakpoints:");
    for (index = 0; index < runtime->write_breakpoint_count; ++index) {
        (void)printf("  %06" PRIx32 "\n", runtime->write_breakpoints[index]);
    }
}

static void interactive_set_watchpoint(rc_runtime_t *runtime,
                                       const char *address_text,
                                       const char *extra_text)
{
    rc_addr_t address;

    if (address_text == NULL || extra_text != NULL) {
        (void)puts("usage: watch ADDRESS");
        return;
    }
    if (parse_breakpoint_address(address_text, &address) != 0) {
        return;
    }
    if (address < 8U) {
        (void)puts("core-write breakpoint cannot target a W-register alias");
        return;
    }
    if ((uint64_t)address + 1U >= runtime->machine.core.halfword_count) {
        (void)puts("core-write breakpoint address is outside core store");
        return;
    }
    if (write_breakpoint_exists(runtime, address)) {
        (void)printf("core-write breakpoint already set at %06" PRIx32 "\n",
                     address);
        return;
    }
    if (runtime->write_breakpoint_count == RC_MAX_WRITE_BREAKPOINTS) {
        (void)printf("core-write breakpoint limit reached (%u)\n",
                     (unsigned)RC_MAX_WRITE_BREAKPOINTS);
        return;
    }
    runtime->write_breakpoints[runtime->write_breakpoint_count++] = address;
    if (runtime->write_breakpoint_count == 1) {
        rc_machine_set_write_observer(&runtime->machine,
                                      runtime_observe_write, runtime);
    }
    (void)printf("core-write breakpoint set at %06" PRIx32 "\n", address);
}

static void interactive_remove_watchpoint(rc_runtime_t *runtime,
                                          const char *address_text,
                                          const char *extra_text)
{
    rc_addr_t address;
    size_t index;

    if (address_text == NULL || extra_text != NULL) {
        (void)puts("usage: nowatch ADDRESS|all");
        return;
    }
    if (strcmp(address_text, "all") == 0) {
        runtime->write_breakpoint_count = 0;
        runtime->write_breakpoint_hit = false;
        rc_machine_set_write_observer(&runtime->machine, NULL, NULL);
        (void)puts("all core-write breakpoints removed");
        return;
    }
    if (parse_breakpoint_address(address_text, &address) != 0) {
        return;
    }
    for (index = 0; index < runtime->write_breakpoint_count; ++index) {
        if (runtime->write_breakpoints[index] == address) {
            size_t remaining = runtime->write_breakpoint_count - index - 1U;

            if (remaining != 0) {
                memmove(&runtime->write_breakpoints[index],
                        &runtime->write_breakpoints[index + 1U],
                        remaining * sizeof(runtime->write_breakpoints[0]));
            }
            --runtime->write_breakpoint_count;
            if (runtime->write_breakpoint_count == 0) {
                rc_machine_set_write_observer(&runtime->machine, NULL, NULL);
            }
            if (runtime->write_breakpoint_address == address) {
                runtime->write_breakpoint_hit = false;
            }
            (void)printf("core-write breakpoint removed at %06" PRIx32 "\n",
                         address);
            return;
        }
    }
    (void)printf("no core-write breakpoint at %06" PRIx32 "\n", address);
}

static void report_write_breakpoint(const rc_runtime_t *runtime)
{
    (void)printf("core-write breakpoint hit at %06" PRIx32
                 ": %06" PRIx32 " -> %06" PRIx32 "\n",
                 runtime->write_breakpoint_address,
                 runtime->write_breakpoint_previous,
                 runtime->write_breakpoint_value);
}

static bool host_time_ns(clockid_t clock, uint64_t *value)
{
    struct timespec now;
    if (clock_gettime(clock, &now) != 0) {
        return false;
    }
    *value = (uint64_t)now.tv_sec * UINT64_C(1000000000) +
        (uint64_t)now.tv_nsec;
    return true;
}

static bool interactive_service(rc_runtime_t *runtime,
                                 rc_host_console_input_t *input,
                                 rc_throttle_t *throttle,
                                 uint64_t *last_time,
                                 uint64_t *pending_tick_units,
                                 bool *operator_requested)
{
    while (host_stop_signal == 0 && !*operator_requested) {
        uint64_t wall_ns;
        uint64_t cpu_ns;
        uint64_t ticks;
        uint64_t delay;
        struct pollfd descriptor = {STDIN_FILENO, POLLIN, 0};
        int timeout;
        bool wait_for_input;

        if (!host_time_ns(CLOCK_MONOTONIC, &wall_ns) ||
            !host_time_ns(CLOCK_PROCESS_CPUTIME_ID, &cpu_ns)) {
            (void)puts("cannot read execution pacing clocks");
            return false;
        }
        *pending_tick_units += (wall_ns - *last_time) * RC_DEVICE_TICKS_PER_SECOND;
        *last_time = wall_ns;
        ticks = *pending_tick_units / UINT64_C(1000000000);
        *pending_tick_units %= UINT64_C(1000000000);
        if (ticks != 0) {
            rc_machine_advance_time(&runtime->machine, ticks);
        }
        pump_host_console_input(input, &runtime->consoles[0],
                                runtime->config.operator_escape,
                                operator_requested);
        if (input->buffer_failed) return false;
        pump_telnet_endpoints(runtime);
        if (*operator_requested || host_stop_signal != 0) {
            break;
        }
        delay = rc_throttle_delay(throttle, wall_ns, cpu_ns,
                                  runtime->machine.instructions_executed);
        if (delay == 0) {
            return true;
        }
        /* Short waits preserve attention, Telnet, clock and signal responsiveness. */
        timeout = delay >= UINT64_C(5000000)
            ? 5 : (int)((delay + UINT64_C(999999)) / UINT64_C(1000000));
        /* Buffered paste may leave stdin continuously readable. Service it in
         * bounded batches, but actually sleep to pay down the CPU budget. */
        wait_for_input = !input->eof && input->buffer_count == 0;
        if (poll(wait_for_input ? &descriptor : NULL, wait_for_input ? 1 : 0,
                 timeout) < 0 && errno != EINTR) {
            (void)puts("execution pacing wait failed");
            return false;
        }
    }
    return true;
}

static rc_step_result_t interactive_run(rc_runtime_t *runtime)
{
    rc_host_console_input_t *input = &runtime->host_input;
    rc_step_result_t result = RC_STEP_OK;
    bool operator_requested = false;
    uint64_t last_time;
    uint64_t cpu_time;
    rc_throttle_t throttle;
    uint64_t quantum = rc_throttle_quantum(runtime->config.throttle);
    uint64_t pending_tick_units = 0;
    uint64_t calls_since_service = 0;
    bool ignore_initial_breakpoint;
    bool breakpoint_hit = false;

    if (runtime->console_count == 0) {
        (void)fprintf(stderr, "no console device is configured\n");
        return RC_STEP_STOPPED;
    }
    runtime->machine.cpu.stopped = false;
    runtime->machine.cpu.reset_mode = false;
    ignore_initial_breakpoint =
        runtime->resume_breakpoint &&
        runtime->resume_breakpoint_address ==
            runtime->machine.cpu.instruction_counter;
    runtime->resume_breakpoint = false;
    runtime->write_breakpoint_hit = false;
    runtime->write_breakpoint_active = true;
    start_host_console_input(input);
    input->buffer_failed = false;
    input->date_shortcut_key = runtime->config.operator_date_key;
    input->delete_mapping = runtime->config.console_delete_mapping;
    input->date_year_offset = runtime->config.date_year_offset;
    if (!host_time_ns(CLOCK_MONOTONIC, &last_time) ||
        !host_time_ns(CLOCK_PROCESS_CPUTIME_ID, &cpu_time)) {
        stop_host_console_input();
        runtime->machine.cpu.stopped = true;
        runtime->write_breakpoint_active = false;
        (void)puts("cannot read execution pacing clocks");
        return RC_STEP_STOPPED;
    }
    rc_throttle_begin(&throttle, runtime->config.throttle, last_time, cpu_time,
                      runtime->machine.instructions_executed);
    pump_host_console_input(input, &runtime->consoles[0],
                            runtime->config.operator_escape,
                            &operator_requested);
    while (host_stop_signal == 0 && !operator_requested && !input->buffer_failed) {
        if (calls_since_service == quantum) {
            if (!interactive_service(runtime, input, &throttle, &last_time,
                                      &pending_tick_units, &operator_requested)) {
                result = RC_STEP_STOPPED;
                break;
            }
            calls_since_service = 0;
            if (operator_requested || host_stop_signal != 0) {
                break;
            }
        }
        if (!ignore_initial_breakpoint &&
            execution_breakpoint_exists(
                runtime, runtime->machine.cpu.instruction_counter)) {
            breakpoint_hit = true;
            runtime->resume_breakpoint = true;
            runtime->resume_breakpoint_address =
                runtime->machine.cpu.instruction_counter;
            break;
        }
        ignore_initial_breakpoint = false;
        result = runtime_step(runtime);
        ++calls_since_service;
        if (result != RC_STEP_OK || runtime->write_breakpoint_hit) {
            break;
        }
    }
    runtime->write_breakpoint_active = false;
    stop_host_console_input();
    runtime->machine.cpu.stopped = true;
    if (input->buffer_failed) result = RC_STEP_STOPPED;
    (void)putchar('\n');
    if (input->rejected_non_ascii != input->reported_non_ascii) {
        (void)printf("local input: rejected %" PRIu64 " non-ASCII byte(s); use ASCII\n",
                     input->rejected_non_ascii - input->reported_non_ascii);
        input->reported_non_ascii = input->rejected_non_ascii;
    }
    if (host_stop_signal != 0) {
        (void)printf("execution interrupted by host signal\n");
    } else if (operator_requested) {
        (void)printf("execution suspended by operator escape\n");
    } else if (breakpoint_hit) {
        (void)printf("execution breakpoint hit at %06" PRIx32 "\n",
                     runtime->machine.cpu.instruction_counter);
    } else if (runtime->write_breakpoint_hit) {
        report_write_breakpoint(runtime);
    } else {
        (void)printf("machine stopped: %s\n", step_result_name(result));
    }
    return result;
}

static void interactive_step(rc_runtime_t *runtime, uint64_t count)
{
    uint64_t index;
    rc_step_result_t result = RC_STEP_OK;

    runtime->resume_breakpoint = false;
    runtime->write_breakpoint_hit = false;
    runtime->write_breakpoint_active = true;
    runtime->machine.cpu.stopped = false;
    runtime->machine.cpu.reset_mode = false;
    for (index = 0; index < count; ++index) {
        result = runtime_step(runtime);
        if (result != RC_STEP_OK || runtime->write_breakpoint_hit) {
            break;
        }
    }
    runtime->write_breakpoint_active = false;
    runtime->machine.cpu.stopped = true;
    if (runtime->write_breakpoint_hit) {
        report_write_breakpoint(runtime);
    }
    if (result != RC_STEP_OK) {
        (void)printf("machine stopped: %s\n", step_result_name(result));
    }
    show_cpu(runtime);
}

static const char *trace_address_mode(uint8_t mode)
{
    static const char *const names[] = {
        "direct", "indirect", "relative", "relative-indirect"
    };

    return mode < sizeof(names) / sizeof(names[0]) ? names[mode] : "invalid";
}

static void print_instruction_trace(const rc_machine_t *machine,
                                    uint64_t instructions_in_call)
{
    const rc_instruction_t *instruction = &machine->last_instruction;
    const char *opcode = rc_opcode_name_extended(instruction->opcode, machine->ix_enabled);

    (void)printf("%06" PRIx32 ": %06" PRIx32
                 "  %-2s w%u %s x%u d=%+" PRId32
                 " ea=%06" PRIx32 " -> IC=%06" PRIx32,
                 machine->last_instruction_address, instruction->raw,
                 opcode != NULL ? opcode : "??",
                 (unsigned)instruction->result_register,
                 trace_address_mode(instruction->address_mode),
                 (unsigned)instruction->index_register,
                 instruction->displacement, machine->last_effective_address,
                 machine->cpu.instruction_counter);
    if (instructions_in_call > 1) {
        (void)printf(" am-prefixes=%" PRIu64, instructions_in_call - 1);
    }
    (void)putchar('\n');
}

static void interactive_trace(rc_runtime_t *runtime, uint64_t count)
{
    uint64_t index;
    rc_step_result_t result = RC_STEP_OK;

    runtime->resume_breakpoint = false;
    runtime->write_breakpoint_hit = false;
    runtime->write_breakpoint_active = true;
    runtime->machine.cpu.stopped = false;
    runtime->machine.cpu.reset_mode = false;
    for (index = 0; index < count; ++index) {
        uint64_t instructions_before =
            runtime->machine.instructions_executed;

        result = runtime_step(runtime);
        if (runtime->machine.instructions_executed > instructions_before) {
            print_instruction_trace(
                &runtime->machine,
                runtime->machine.instructions_executed - instructions_before);
        }
        if (result != RC_STEP_OK || runtime->write_breakpoint_hit) {
            break;
        }
    }
    runtime->write_breakpoint_active = false;
    runtime->machine.cpu.stopped = true;
    if (runtime->write_breakpoint_hit) {
        report_write_breakpoint(runtime);
    }
    if (result != RC_STEP_OK) {
        (void)printf("machine stopped: %s\n", step_result_name(result));
    }
    show_cpu(runtime);
}

typedef enum {
    RC_EXAMINE_HEX,
    RC_EXAMINE_CHARACTERS,
    RC_EXAMINE_BINARY,
    RC_EXAMINE_INSTRUCTIONS
} rc_examine_format_t;

static bool parse_examine_format(const char *text,
                                 rc_examine_format_t *format)
{
    if (text == NULL) {
        return false;
    }
    if (strcmp(text, "/hex") == 0 || strcmp(text, "/x") == 0) {
        *format = RC_EXAMINE_HEX;
    } else if (strcmp(text, "/characters") == 0 ||
               strcmp(text, "/char") == 0 || strcmp(text, "/c") == 0) {
        *format = RC_EXAMINE_CHARACTERS;
    } else if (strcmp(text, "/binary") == 0 || strcmp(text, "/b") == 0) {
        *format = RC_EXAMINE_BINARY;
    } else if (strcmp(text, "/instructions") == 0 ||
               strcmp(text, "/instruction") == 0 ||
               strcmp(text, "/i") == 0) {
        *format = RC_EXAMINE_INSTRUCTIONS;
    } else {
        return false;
    }
    return true;
}

static void format_binary_word(rc_word_t value, char text[25])
{
    unsigned bit;

    for (bit = 0; bit < 24U; ++bit) {
        text[bit] = (value & (UINT32_C(1) << (23U - bit))) != 0
            ? '1' : '0';
    }
    text[24] = '\0';
}

static char display_character(rc_word_t value)
{
    unsigned char byte = (unsigned char)(value & UINT32_C(0xff));

    return isprint(byte) != 0 ? (char)byte : '.';
}

static void interactive_examine(const rc_runtime_t *runtime,
                                const char *first_text,
                                const char *second_text,
                                const char *third_text)
{
    rc_examine_format_t format = RC_EXAMINE_HEX;
    const char *address_text = first_text;
    const char *count_text = second_text;
    uint64_t address;
    uint64_t count = 1;
    uint64_t index;

    if (parse_examine_format(first_text, &format)) {
        address_text = second_text;
        count_text = third_text;
    } else if (third_text != NULL) {
        (void)puts("usage: examine [/hex|/char|/binary|/instruction] ADDRESS|IC [COUNT]");
        return;
    }
    if (address_text != NULL &&
        (strcmp(address_text, "ic") == 0 || strcmp(address_text, "IC") == 0)) {
        address = runtime->machine.cpu.instruction_counter;
    } else if (parse_unsigned(address_text, 0, &address) != 0) {
        (void)puts("usage: examine [/hex|/char|/binary|/instruction] ADDRESS|IC [COUNT]");
        return;
    }
    if (address > RC_WORD_MASK ||
        (count_text != NULL &&
         (parse_unsigned(count_text, 0, &count) != 0 || count == 0))) {
        (void)puts("usage: examine [/hex|/char|/binary|/instruction] ADDRESS|IC [COUNT]");
        return;
    }
    address &= ~UINT64_C(1);
    for (index = 0; index < count; ++index) {
        rc_word_t value;
        uint64_t wide_address;
        rc_addr_t current;
        char marker;

        if (index > (RC_WORD_MASK - address) / UINT64_C(2)) {
            (void)puts("address exceeds the 24-bit address space");
            return;
        }
        wide_address = address + index * UINT64_C(2);
        current = (rc_addr_t)wide_address;

        if (!rc_machine_read_word(&runtime->machine, current, &value)) {
            (void)printf("address out of range: %06" PRIx32 "\n", current);
            return;
        }
        marker = current == runtime->machine.cpu.instruction_counter
            ? '>' : ' ';
        if (format == RC_EXAMINE_CHARACTERS) {
            (void)printf("%c%06" PRIx32 ": %06" PRIx32 "  '%c%c%c'\n",
                         marker, current, value,
                         display_character(value >> 16),
                         display_character(value >> 8),
                         display_character(value));
        } else if (format == RC_EXAMINE_BINARY) {
            char binary[25];

            format_binary_word(value, binary);
            (void)printf("%c%06" PRIx32 ": %s\n", marker, current, binary);
        } else if (format == RC_EXAMINE_INSTRUCTIONS) {
            char instruction[80];

            if (rc_instruction_format_extended(value, runtime->machine.ix_enabled, instruction,
                                      sizeof(instruction)) != 0) {
                (void)strcpy(instruction, "??");
            }
            (void)printf("%c%06" PRIx32 ": %06" PRIx32 "  %s\n",
                         marker, current, value, instruction);
        } else {
            (void)printf("%c%06" PRIx32 ": %06" PRIx32 "\n",
                         marker, current, value);
        }
    }
}

static int word_register_number(const char *text, unsigned *number)
{
    if (text == NULL || number == NULL ||
        (text[0] != 'w' && text[0] != 'W') || text[1] < '0' ||
        text[1] >= (char)('0' + RC_REGISTER_COUNT) || text[2] != '\0') {
        return -1;
    }
    *number = (unsigned)(text[1] - '0');
    return 0;
}

static int control_register_number(const char *text,
                                   rc_control_register_t *control)
{
    if (strcmp(text, "im") == 0 || strcmp(text, "IM") == 0) {
        *control = RC_CONTROL_INTERRUPT_MASK;
    } else if (strcmp(text, "ir") == 0 || strcmp(text, "IR") == 0) {
        *control = RC_CONTROL_INTERRUPT_REQUESTS;
    } else if (strcmp(text, "ex21") == 0 || strcmp(text, "EX21") == 0) {
        *control = RC_CONTROL_EXCEPTION_21;
    } else if (strcmp(text, "ex22") == 0 || strcmp(text, "EX22") == 0) {
        *control = RC_CONTROL_EXCEPTION_22;
    } else if (strcmp(text, "ex23") == 0 || strcmp(text, "EX23") == 0) {
        *control = RC_CONTROL_EXCEPTION_23;
    } else {
        return -1;
    }
    return 0;
}

static void interactive_deposit(rc_runtime_t *runtime, const char *target,
                                const char *value_text,
                                const char *extra_text)
{
    uint64_t value;
    unsigned register_number;
    rc_control_register_t control;

    if (target == NULL || value_text == NULL || extra_text != NULL ||
        parse_unsigned(value_text, 0, &value) != 0 || value > RC_WORD_MASK) {
        (void)puts("usage: deposit ADDRESS|W0|W1|W2|W3|IC|IM|IR|EX21|EX22|EX23 VALUE");
        return;
    }
    if (word_register_number(target, &register_number) == 0) {
        runtime->machine.cpu.w[register_number] = (rc_word_t)value;
        (void)printf("W%u=%06" PRIx64 "\n", register_number, value);
        return;
    }
    if (strcmp(target, "ic") == 0 || strcmp(target, "IC") == 0) {
        if ((value & UINT64_C(1)) != 0) {
            (void)puts("instruction counter must be an even byte address");
            return;
        }
        runtime->machine.cpu.instruction_counter = (rc_addr_t)value;
        runtime->resume_breakpoint = false;
        (void)printf("IC=%06" PRIx64 "\n", value);
        return;
    }
    if (control_register_number(target, &control) == 0) {
        if (!rc_machine_set_control_register(&runtime->machine, control,
                                             (rc_word_t)value)) {
            (void)printf("value is invalid for control register %s\n",
                         target);
            return;
        }
        (void)printf("%s=%06" PRIx64 "\n", target, value);
        return;
    }
    {
        uint64_t address;
        rc_addr_t aligned_address;

        if (parse_unsigned(target, 0, &address) != 0 ||
            address > RC_WORD_MASK) {
            (void)puts("usage: deposit ADDRESS|W0|W1|W2|W3|IC|IM|IR|EX21|EX22|EX23 VALUE");
            return;
        }
        aligned_address = (rc_addr_t)address & ~UINT32_C(1);
        if (!rc_machine_write_word(&runtime->machine, (rc_addr_t)address,
                                   (rc_word_t)value)) {
            (void)printf("address out of range: %06" PRIx32 "\n",
                         aligned_address);
            return;
        }
        (void)printf("%06" PRIx32 ": %06" PRIx64 "\n", aligned_address,
                     value);
    }
}

static void interactive_load_core(rc_runtime_t *runtime, const char *path,
                                  const char *address_text,
                                  const char *extra_text)
{
    uint64_t address;
    size_t word_count = 0;
    char error[256];

    if (path == NULL || address_text == NULL || extra_text != NULL ||
        parse_unsigned(address_text, 0, &address) != 0 ||
        address > RC_WORD_MASK || (address & UINT64_C(1)) != 0) {
        (void)puts("usage: load-core FILE ADDRESS");
        return;
    }
    if (rc_core_image_load(path, &runtime->machine, (rc_addr_t)address,
                           &word_count, error, sizeof(error)) != 0) {
        (void)printf("cannot load core data: %s\n", error);
        return;
    }
    (void)printf("loaded %zu words at %06" PRIx64 " from %s\n",
                 word_count, address, path);
}

static void interactive_save_core(const rc_runtime_t *runtime,
                                  const char *path,
                                  const char *address_text,
                                  const char *count_text,
                                  const char *extra_text)
{
    uint64_t address;
    uint64_t count;
    char error[256];

    if (path == NULL || address_text == NULL || count_text == NULL ||
        extra_text != NULL || parse_unsigned(address_text, 0, &address) != 0 ||
        parse_unsigned(count_text, 0, &count) != 0 ||
        address > RC_WORD_MASK || (address & UINT64_C(1)) != 0 || count == 0 ||
        count > SIZE_MAX) {
        (void)puts("usage: save-core FILE ADDRESS COUNT");
        return;
    }
    if (rc_core_image_save(path, &runtime->machine, (rc_addr_t)address,
                           (size_t)count, error, sizeof(error)) != 0) {
        (void)printf("cannot save core data: %s\n", error);
        return;
    }
    (void)printf("saved %" PRIu64 " words at %06" PRIx64 " to %s\n",
                 count, address, path);
}

static int parse_device_number(const char *text, unsigned *device_number)
{
    uint64_t parsed;

    if (parse_unsigned(text, 0, &parsed) != 0 ||
        parsed >= RC_DEVICE_COUNT) {
        return -1;
    }
    *device_number = (unsigned)parsed;
    return 0;
}

static void interactive_attach(rc_runtime_t *runtime,
                               const char *device_text,
                               const char *image_path,
                               const char *mode,
                               const char *overlay_path,
                               const char *extra_text)
{
    unsigned device_number;
    size_t index;
    size_t reader_index;
    size_t punch_index;
    rc_rc433x_device_t *disk;
    char error[256];
    int result;

    if (parse_device_number(device_text, &device_number) != 0 ||
        image_path == NULL || extra_text != NULL) {
        (void)puts(
            "usage: attach DEVICE FILE [readonly|writable|overlay FILE|replace]");
        return;
    }
    error[0] = '\0';
    index = runtime_printer_index(runtime, device_number);
    if (index != SIZE_MAX) {
        rc610_device_t *printer = &runtime->printers[index];
        if ((mode != NULL && strcmp(mode, "append") != 0) || overlay_path != NULL) {
            (void)puts("usage for RC610: attach DEVICE FILE [append]"); return;
        }
        int opened = mode != NULL ? rc610_open_append(printer, image_path, error, sizeof(error)) :
                                    rc610_open(printer, image_path, error, sizeof(error));
        if (opened != 0)
            (void)printf("cannot attach RC610 output: %s\n", error);
        else (void)printf("RC610 device %u output attached\n", device_number);
        return;
    }
    reader_index = runtime_reader_index(runtime, device_number);
    index = runtime_tape_index(runtime, device_number);
    if (index != SIZE_MAX) {
        rc_rc749_device_t *tape = &runtime->tapes[index];
        if ((mode != NULL && strcmp(mode, "readonly") != 0) || overlay_path != NULL) {
            (void)puts("usage for RC749: attach DEVICE TAPE [readonly]"); return;
        }
        if (tape->file != NULL) {
            (void)puts("RC749 tape already mounted; detach it first"); return;
        }
        if (rc_rc749_mount(tape, image_path, error, sizeof(error)) != 0)
            (void)printf("cannot mount RC749 device %u: %s\n", device_number, error);
        else (void)printf("RC749 device %u mounted read-only\n", device_number);
        return;
    }
    if (reader_index != SIZE_MAX) {
        rc_reader_device_t *reader = &runtime->readers[reader_index];

        if (mode != NULL || overlay_path != NULL) {
            (void)puts("usage for READER: attach DEVICE TAPE");
            return;
        }
        if (reader->tape_present) {
            (void)printf("READER device %u already has a paper tape mounted\n",
                         device_number);
            return;
        }
        if (rc_reader_device_mount(reader, image_path, error,
                                   sizeof(error)) != 0) {
            (void)printf("cannot mount READER device %u: %s\n",
                         device_number, error);
            return;
        }
        (void)printf("READER device %u mounted: tape=%s\n", device_number,
                     reader->path);
        return;
    }
    punch_index = runtime_punch_index(runtime, device_number);
    if (punch_index != SIZE_MAX) {
        rc_punch_device_t *punch = &runtime->punches[punch_index];
        bool replace = mode != NULL && strcmp(mode, "replace") == 0;

        if ((mode != NULL && !replace) || overlay_path != NULL) {
            (void)puts("usage for PUNCH: attach DEVICE TAPE [replace]");
            return;
        }
        if (punch->tape_present) {
            (void)printf("PUNCH device %u already has an output tape mounted\n",
                         device_number);
            return;
        }
        if (rc_punch_device_mount(punch, image_path, replace, error,
                                  sizeof(error)) != 0) {
            (void)printf("cannot mount PUNCH device %u: %s\n",
                         device_number, error);
            return;
        }
        (void)printf("PUNCH device %u mounted: tape=%s%s\n", device_number,
                     punch->path, replace ? " (replaced)" : "");
        return;
    }
    index = runtime_disk_index(runtime, device_number);
    if (index == SIZE_MAX) {
        (void)printf("device %u is not a configured media device\n",
                     device_number);
        return;
    }
    if ((mode != NULL && strcmp(mode, "readonly") != 0 &&
         strcmp(mode, "writable") != 0 &&
         strcmp(mode, "overlay") != 0) ||
        (mode != NULL && strcmp(mode, "overlay") == 0 &&
         overlay_path == NULL) ||
        ((mode == NULL || strcmp(mode, "readonly") == 0 ||
          strcmp(mode, "writable") == 0) &&
         overlay_path != NULL)) {
        (void)puts(
            "usage for RC433X: attach DEVICE IMAGE [readonly|writable|overlay FILE]");
        return;
    }
    if (runtime->disk_attached[index]) {
        (void)printf("RC433X device %u is already attached\n", device_number);
        return;
    }
    disk = &runtime->disks[index];
    if (mode != NULL && strcmp(mode, "readonly") == 0) {
        result = rc_rc433x_device_init_readonly(
            disk, &runtime->machine, image_path, error, sizeof(error));
    } else if (mode != NULL && strcmp(mode, "writable") == 0) {
        result = rc_rc433x_device_init_writable(
            disk, &runtime->machine, image_path, error, sizeof(error));
    } else if (mode != NULL) {
        result = rc_rc433x_device_init_persistent(
            disk, &runtime->machine, image_path, overlay_path,
            error, sizeof(error));
    } else {
        result = rc_rc433x_device_init_cow(
            disk, &runtime->machine, image_path, error, sizeof(error));
    }
    if (result != 0) {
        (void)printf("cannot attach RC433X device %u: %s\n", device_number,
                     error[0] != '\0' ? error : "unknown media error");
        return;
    }
    if (!rc_machine_attach_device(&runtime->machine, device_number,
                                  &disk->device)) {
        rc_rc433x_device_destroy(disk);
        (void)printf("cannot attach RC433X device %u to the machine\n",
                     device_number);
        return;
    }
    runtime->disk_attached[index] = true;
    (void)printf("RC433X device %u attached: base=%s mode=%s", device_number,
                 disk->base_image_path,
                 disk_mode_name(disk));
    if (disk->persistent_overlay) {
        (void)printf(" overlay=%s", disk->overlay_path);
    }
    (void)putchar('\n');
}

static void interactive_detach(rc_runtime_t *runtime,
                               const char *device_text,
                               const char *extra_text)
{
    unsigned device_number;
    size_t index;
    size_t reader_index;
    size_t punch_index;
    rc_rc433x_device_t *disk;
    char error[256];

    if (parse_device_number(device_text, &device_number) != 0 ||
        extra_text != NULL) {
        (void)puts("usage: detach DEVICE");
        return;
    }
    index = runtime_printer_index(runtime, device_number);
    if (index != SIZE_MAX) {
        if (rc610_close(&runtime->printers[index], error, sizeof(error)) != 0)
            (void)printf("cannot detach RC610 output: %s\n", error);
        else (void)printf("RC610 device %u output detached\n", device_number);
        return;
    }
    reader_index = runtime_reader_index(runtime, device_number);
    index = runtime_tape_index(runtime, device_number);
    if (index != SIZE_MAX) {
        rc_rc749_unmount(&runtime->tapes[index]);
        (void)printf("RC749 device %u tape detached\n", device_number);
        return;
    }
    if (reader_index != SIZE_MAX) {
        rc_reader_device_t *reader = &runtime->readers[reader_index];

        if (!reader->tape_present) {
            (void)printf("READER device %u has no paper tape mounted\n",
                         device_number);
            return;
        }
        rc_reader_device_unmount(reader);
        (void)printf("READER device %u paper tape detached\n",
                     device_number);
        return;
    }
    punch_index = runtime_punch_index(runtime, device_number);
    if (punch_index != SIZE_MAX) {
        rc_punch_device_t *punch = &runtime->punches[punch_index];

        if (!punch->tape_present) {
            (void)printf("PUNCH device %u has no output tape mounted\n",
                         device_number);
            return;
        }
        if (rc_punch_device_flush(punch, error, sizeof(error)) != 0) {
            (void)printf("cannot detach PUNCH device %u: %s\n",
                         device_number, error);
            return;
        }
        rc_punch_device_unmount(punch);
        (void)printf("PUNCH device %u output tape detached\n",
                     device_number);
        return;
    }
    index = runtime_disk_index(runtime, device_number);
    if (index == SIZE_MAX) {
        (void)printf("device %u is not a configured media device\n",
                     device_number);
        return;
    }
    if (!runtime->disk_attached[index]) {
        (void)printf("RC433X device %u is already detached\n", device_number);
        return;
    }
    disk = &runtime->disks[index];
    if (rc_rc433x_device_flush(disk, error, sizeof(error)) != 0) {
        (void)printf("cannot detach RC433X device %u: %s\n", device_number,
                     error);
        return;
    }
    if (!rc_machine_detach_device(&runtime->machine, device_number,
                                  &disk->device)) {
        (void)printf("cannot detach RC433X device %u from the machine\n",
                     device_number);
        return;
    }
    rc_rc433x_device_destroy(disk);
    runtime->disk_attached[index] = false;
    (void)printf("RC433X device %u detached\n", device_number);
}

static void interactive_rewind(rc_runtime_t *runtime,
                               const char *device_text,
                               const char *extra_text)
{
    unsigned device_number;
    size_t index;
    char error[256];

    if (parse_device_number(device_text, &device_number) != 0 ||
        extra_text != NULL) {
        (void)puts("usage: rewind DEVICE");
        return;
    }
    index = runtime_reader_index(runtime, device_number);
    {
        size_t tape_index = runtime_tape_index(runtime, device_number);
        if (tape_index != SIZE_MAX) {
            if (rc_rc749_rewind(&runtime->tapes[tape_index], error, sizeof(error)) != 0)
                (void)printf("cannot rewind RC749 device %u: %s\n", device_number, error);
            else (void)printf("RC749 device %u rewound\n", device_number);
            return;
        }
    }
    if (index == SIZE_MAX) {
        (void)printf("device %u is not a configured READER or RC749 device\n",
                     device_number);
        return;
    }
    if (rc_reader_device_rewind(&runtime->readers[index], error,
                                sizeof(error)) != 0) {
        (void)printf("cannot rewind READER device %u: %s\n", device_number,
                     error);
        return;
    }
    (void)printf("READER device %u rewound\n", device_number);
}

static void interactive_show_input(const rc_runtime_t *runtime)
{
    const rc_host_console_input_t *input = &runtime->host_input;

    if (runtime->console_count == 0) {
        (void)puts("no console device is configured");
        return;
    }
    (void)printf("input: local device=%u host-buffer=%zu pending-byte=%u\n",
                 runtime->console_numbers[0], input->buffer_count,
                 input->have_pending_character ? 1U : 0U);
    (void)printf("  guest-queue=%zu completed-read=%s eof=%s\n",
                 rc_console_device_input_count(&runtime->consoles[0]),
                 runtime->consoles[0].echo_on_sense &&
                 runtime->consoles[0].status <= UINT32_C(0x7f) ? "yes" : "no",
                 input->eof ? "yes" : "no");
    (void)printf("  terminal-sequence=%s rejected-non-ascii=%" PRIu64 "\n",
                 input->ansi_state != 0 ? "pending" : "none",
                 input->rejected_non_ascii);
}

static void interactive_discard_input(rc_runtime_t *runtime)
{
    rc_host_console_input_t *input = &runtime->host_input;
    size_t host_count = input->buffer_count;
    unsigned pending = input->have_pending_character ? 1U : 0U;
    size_t guest_count;

    if (runtime->console_count == 0) {
        (void)puts("no console device is configured");
        return;
    }
    guest_count = rc_console_device_discard_input(&runtime->consoles[0]);
    free(input->buffer);
    input->buffer = NULL;
    input->buffer_capacity = 0;
    input->buffer_head = 0;
    input->buffer_count = 0;
    input->have_pending_character = false;
    input->buffer_failed = false;
    input->ansi_state = 0;
    (void)printf("discarded input: host=%zu pending=%u guest=%zu\n",
                 host_count, pending, guest_count);
}

static void interactive_send(rc_runtime_t *runtime, char *token)
{
    rc_host_console_input_t staged = {0};

    if (runtime->console_count == 0) {
        (void)printf("no console device is configured\n");
        return;
    }
    if (token == NULL) {
        (void)printf("usage: send BYTE...\n");
        return;
    }
    do {
        uint64_t byte;

        if (parse_unsigned(token, 16, &byte) != 0 || byte > 0x7fU) {
            (void)printf("invalid 7-bit hexadecimal byte: %s\n", token);
            goto done;
        }
        if (!reserve_host_input(&staged, 1)) goto done;
        staged.buffer[staged.buffer_count++] = (uint8_t)byte;
        token = strtok(NULL, " \t\r\n");
    } while (token != NULL);
    (void)queue_operator_input(runtime, staged.buffer, staged.buffer_count);
done:
    free(staged.buffer);
}

static void interactive_help(void)
{
    (void)puts(
        "Commands:\n"
        "  run (r), continue (c)       resume execution; Control-E returns here\n"
        "  stop (st)                   confirm that execution is suspended\n"
        "  step (s) [COUNT]            execute one or more interpreter calls\n"
        "  trace (t) [COUNT]           step with decoded instruction output\n"
        "  show (sh) TARGET            cpu, devices, media, telnet,\n"
        "                              breakpoints, watchpoints, throttle, input\n"
        "  show keys ADDR [COUNT]      inspect word keys and PR bits (up to 256)\n"
        "  set throttle VALUE          off, 1..100%, NUMBERips, or original\n"
        "  examine (x) [FMT] ADDR [N]  hex, char, binary, or instruction view\n"
        "  deposit (d) TARGET VALUE    set core, W0-W3, IC, IM, IR, or EX21-23\n"
        "  load-core (lc) FILE ADDR    load packed 24-bit words into core\n"
        "  save-core (sc) FILE ADDR N  save packed 24-bit words atomically\n"
        "  mkdisc FILE                 create a new unlabelled disk; no overwrite\n"
        "  attach (at) DEV FILE [...]  attach disk, tape, or printer output\n"
        "  detach (det) DEVICE         detach media or close printer output\n"
        "  rewind (rew) DEVICE         rewind a mounted tape\n"
        "  break (b) ADDR              stop before executing at an address\n"
        "  nobreak (nb) ADDR|all       remove execution breakpoints\n"
        "  watch (w) ADDR              stop after a core word is written\n"
        "  nowatch (nw) ADDR|all       remove core-write breakpoints\n"
        "  reset (res)                 enter reset state and save IC at word 10\n"
        "  start (sta)                 leave reset and run from word 14\n"
        "  autoload (al)               bootstrap from device 0 while in reset\n"
        "  interrupts (int) on|off     set interrupt enable while stopped\n"
        "  attention (att)             request the RC operator interrupt\n"
        "  send (se) BYTE...           queue hexadecimal 7-bit console bytes\n"
        "  send-date (sd)              queue the translated current host date\n"
        "  discard-input (di)          discard queued local console input\n"
        "  help (h, ?)                 display this list\n"
        "  quit (q), exit              leave the emulator");
}

static int interactive_command(rc_runtime_t *runtime, char *line)
{
    char *command = strtok(line, " \t\r\n");
    char *argument = strtok(NULL, " \t\r\n");

    if (command == NULL) {
        return 0;
    }
    if (strcmp(command, "quit") == 0 || strcmp(command, "q") == 0 ||
        strcmp(command, "exit") == 0) {
        return 1;
    }
    if (strcmp(command, "help") == 0 || strcmp(command, "h") == 0 ||
        strcmp(command, "?") == 0) {
        interactive_help();
    } else if (strcmp(command, "run") == 0 ||
               strcmp(command, "r") == 0 ||
               strcmp(command, "c") == 0 ||
               strcmp(command, "continue") == 0) {
        (void)interactive_run(runtime);
    } else if (strcmp(command, "stop") == 0 || strcmp(command, "st") == 0) {
        (void)puts("execution is suspended");
    } else if (strcmp(command, "step") == 0 || strcmp(command, "s") == 0) {
        uint64_t count = 1;
        if (argument != NULL &&
            (parse_unsigned(argument, 0, &count) != 0 || count == 0)) {
            (void)puts("usage: step [COUNT]");
        } else {
            interactive_step(runtime, count);
        }
    } else if (strcmp(command, "trace") == 0 || strcmp(command, "t") == 0) {
        uint64_t count = 1;
        if (argument != NULL &&
            (parse_unsigned(argument, 0, &count) != 0 || count == 0)) {
            (void)puts("usage: trace [COUNT]");
        } else {
            interactive_trace(runtime, count);
        }
    } else if (strcmp(command, "set") == 0) {
        char *value = strtok(NULL, " \t\r\n");
        rc_throttle_config_t config;
        if (argument == NULL || strcmp(argument, "throttle") != 0 ||
            value == NULL || strtok(NULL, " \t\r\n") != NULL ||
            !rc_throttle_parse(value, &config)) {
            (void)puts("usage: set throttle off|1..100%|1..1000000000ips|original");
        } else {
            runtime->config.throttle = config;
            show_throttle(runtime);
        }
    } else if (strcmp(command, "show") == 0 || strcmp(command, "sh") == 0) {
        if (argument != NULL && strcmp(argument, "keys") == 0) {
            char *address = strtok(NULL, " \t\r\n");
            char *count = strtok(NULL, " \t\r\n");
            char *extra = strtok(NULL, " \t\r\n");
            show_keys(runtime, address, count, extra);
        } else if (argument != NULL &&
            (strcmp(argument, "cpu") == 0 ||
             strcmp(argument, "reg") == 0 ||
             strcmp(argument, "registers") == 0)) {
            show_cpu(runtime);
        } else if (argument != NULL &&
                   (strcmp(argument, "break") == 0 ||
                    strcmp(argument, "breakpoints") == 0)) {
            interactive_show_breakpoints(runtime);
        } else if (argument != NULL && strcmp(argument, "telnet") == 0) {
            interactive_show_telnet(runtime);
        } else if (argument != NULL && strcmp(argument, "devices") == 0) {
            interactive_show_devices(runtime);
        } else if (argument != NULL && strcmp(argument, "media") == 0) {
            interactive_show_media(runtime);
        } else if (argument != NULL && strcmp(argument, "throttle") == 0) {
            show_throttle(runtime);
        } else if (argument != NULL && strcmp(argument, "input") == 0) {
            if (strtok(NULL, " \t\r\n") != NULL) (void)puts("usage: show input");
            else interactive_show_input(runtime);
        } else if (argument != NULL &&
                   (strcmp(argument, "watch") == 0 ||
                    strcmp(argument, "watchpoints") == 0)) {
            interactive_show_watchpoints(runtime);
        } else {
            (void)puts("usage: show cpu|devices|media|breakpoints|watchpoints|telnet|throttle|input\n"
                       "       show keys ADDRESS [COUNT]");
        }
    } else if (strcmp(command, "examine") == 0 || strcmp(command, "x") == 0) {
        char *second = strtok(NULL, " \t\r\n");
        char *third = strtok(NULL, " \t\r\n");
        char *extra = strtok(NULL, " \t\r\n");

        if (extra != NULL) {
            (void)puts("usage: examine [/hex|/char|/binary|/instruction] ADDRESS|IC [COUNT]");
        } else {
            interactive_examine(runtime, argument, second, third);
        }
    } else if (strcmp(command, "deposit") == 0 || strcmp(command, "d") == 0) {
        char *value = strtok(NULL, " \t\r\n");
        interactive_deposit(runtime, argument, value,
                            strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "load-core") == 0 ||
               strcmp(command, "lc") == 0) {
        char *address = strtok(NULL, " \t\r\n");
        interactive_load_core(runtime, argument, address,
                              strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "save-core") == 0 ||
               strcmp(command, "sc") == 0) {
        char *address = strtok(NULL, " \t\r\n");
        char *count = strtok(NULL, " \t\r\n");
        interactive_save_core(runtime, argument, address, count,
                              strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "mkdisc") == 0) {
        if (argument == NULL || strtok(NULL, " \t\r\n") != NULL) {
            (void)puts("usage: mkdisc FILE");
        } else {
            (void)make_disc(argument, true);
        }
    } else if (strcmp(command, "attach") == 0 ||
               strcmp(command, "at") == 0) {
        char *image = strtok(NULL, " \t\r\n");
        char *mode = strtok(NULL, " \t\r\n");
        char *overlay = strtok(NULL, " \t\r\n");
        interactive_attach(runtime, argument, image, mode, overlay,
                           strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "detach") == 0 ||
               strcmp(command, "det") == 0) {
        interactive_detach(runtime, argument, strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "rewind") == 0 ||
               strcmp(command, "rew") == 0) {
        interactive_rewind(runtime, argument, strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "break") == 0 || strcmp(command, "b") == 0) {
        interactive_set_breakpoint(runtime, argument,
                                   strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "nobreak") == 0 ||
               strcmp(command, "nb") == 0) {
        interactive_remove_breakpoint(runtime, argument,
                                      strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "watch") == 0 || strcmp(command, "w") == 0) {
        interactive_set_watchpoint(runtime, argument,
                                   strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "nowatch") == 0 ||
               strcmp(command, "nw") == 0) {
        interactive_remove_watchpoint(runtime, argument,
                                      strtok(NULL, " \t\r\n"));
    } else if (strcmp(command, "reset") == 0 || strcmp(command, "res") == 0) {
        bool was_in_reset = runtime->machine.cpu.reset_mode;

        if (argument != NULL) {
            (void)puts("usage: reset");
        } else if (rc_machine_control_reset(&runtime->machine) !=
                   RC_CONTROL_OK) {
            (void)puts("reset failed: cannot store the restart address");
        } else {
            runtime->resume_breakpoint = false;
            (void)puts(was_in_reset
                ? "machine remains in reset; core and devices preserved"
                : "machine reset; IC saved at word 10; core and devices preserved");
        }
    } else if (strcmp(command, "start") == 0 || strcmp(command, "sta") == 0) {
        rc_control_result_t result;

        if (argument != NULL) {
            (void)puts("usage: start");
        } else if ((result = rc_machine_control_start(&runtime->machine)) ==
                   RC_CONTROL_NOT_IN_RESET) {
            (void)puts("start ignored: machine is not in reset state");
        } else if (result != RC_CONTROL_OK) {
            (void)puts("start failed: cannot read address word 14");
        } else {
            runtime->resume_breakpoint = false;
            (void)interactive_run(runtime);
        }
    } else if (strcmp(command, "autoload") == 0 ||
               strcmp(command, "al") == 0) {
        rc_control_result_t result;

        if (argument != NULL) {
            (void)puts("usage: autoload");
        } else if ((result = rc_machine_control_autoload(&runtime->machine)) ==
                   RC_CONTROL_NOT_IN_RESET) {
            (void)puts("autoload ignored: machine is not in reset state");
        } else if (result != RC_CONTROL_OK) {
            (void)puts("autoload failed: device 0 is unavailable or reported an error");
        } else {
            runtime->resume_breakpoint = false;
            (void)interactive_run(runtime);
        }
    } else if (strcmp(command, "interrupts") == 0 ||
               strcmp(command, "int") == 0) {
        char *extra = strtok(NULL, " \t\r\n");

        if (argument == NULL || extra != NULL ||
            (strcmp(argument, "on") != 0 && strcmp(argument, "off") != 0)) {
            (void)puts("usage: interrupts on|off");
        } else {
            bool enabled = strcmp(argument, "on") == 0;

            rc_machine_control_interrupts(&runtime->machine, enabled);
            (void)printf("interrupts=%s\n", enabled ? "on" : "off");
        }
    } else if (strcmp(command, "attention") == 0 ||
               strcmp(command, "att") == 0) {
        if (runtime->console_count == 0) {
            (void)puts("no console device is configured");
        } else {
            rc_console_device_attention(&runtime->consoles[0]);
            (void)puts("operator interrupt requested");
        }
    } else if (strcmp(command, "discard-input") == 0 || strcmp(command, "di") == 0) {
        if (argument != NULL) (void)puts("usage: discard-input");
        else interactive_discard_input(runtime);
    } else if (strcmp(command, "send") == 0 || strcmp(command, "se") == 0) {
        interactive_send(runtime, argument);
    } else if (strcmp(command, "send-date") == 0 ||
               strcmp(command, "sd") == 0) {
        if (argument != NULL) {
            (void)puts("usage: send-date");
        } else if (runtime->console_count == 0) {
            (void)puts("no console device is configured");
        } else {
            char text[32];
            int length = format_host_date(text, sizeof(text),
                                           runtime->config.date_year_offset);
            if (length >= 0)
                (void)queue_operator_input(runtime, (const uint8_t *)text,
                                            (size_t)length);
        }
    } else {
        (void)printf("unknown command: %s\n", command);
    }
    return 0;
}

static int interactive_monitor(const char *config_path,
                               const char *monitor_override,
                               const char *disk_override,
                               const char *disk_overlay_override,
                               bool disk_write_through_override,
                               int set_disc_load_flag, bool auto_run, int ix_override)
{
    rc_runtime_t runtime;
    int received_signal;

    if (isatty(STDIN_FILENO) == 0) {
        (void)fprintf(stderr,
                      "interactive mode requires a terminal on standard input\n");
        return EXIT_FAILURE;
    }
    if (runtime_init(&runtime, config_path, monitor_override, disk_override,
                     disk_overlay_override, disk_write_through_override,
                     set_disc_load_flag, ix_override) != 0) {
        return EXIT_FAILURE;
    }
    host_stop_signal = 0;
    if (install_host_signal_handlers() != 0) {
        (void)fprintf(stderr, "cannot install host signal handlers: %s\n",
                      strerror(errno));
        runtime_destroy(&runtime);
        return EXIT_FAILURE;
    }
    rc_operator_output_enable(true);
    (void)printf("Sim4000 interactive session; type help for commands.\n");
    if (auto_run) {
        (void)interactive_run(&runtime);
    }
    while (host_stop_signal == 0) {
        char *line;
        struct pollfd input = {STDIN_FILENO, POLLIN, 0};
        int poll_result;

        rc_operator_input_begin();
        while (!rc_operator_input_ready()) {
            if (host_stop_signal != 0) {
                poll_result = -1;
                break;
            }
            pump_telnet_endpoints(&runtime);
            poll_result = poll(&input, 1, 50);
            if (poll_result > 0) {
                rc_operator_input_read();
                continue;
            }
            if (poll_result < 0) {
                if (errno == EINTR && host_stop_signal == 0) {
                    continue;
                }
                break;
            }
        }
        if (host_stop_signal != 0) {
            break;
        }
        line = rc_operator_input_take();
        if (line == NULL) {
            break;
        }
        {
            int quit = interactive_command(&runtime, line);
            free(line);
            if (quit != 0) {
                break;
            }
        }
    }
    rc_operator_input_end();
    if (host_stop_signal != 0) {
        (void)printf("\nhost signal %d received; shutting down\n",
                     (int)host_stop_signal);
    }
    print_disk_statistics(&runtime);
    print_reader_statistics(&runtime);
    print_punch_statistics(&runtime);
    print_printer_statistics(&runtime);
    print_telnet_statistics(&runtime);
    received_signal = (int)host_stop_signal;
    int shutdown_result = runtime_destroy(&runtime);
    restore_host_signal_handlers();
    rc_operator_output_enable(false);
    return received_signal != 0 ? 128 + received_signal :
           shutdown_result != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

static bool long_option_is(const char *argument, const char *name)
{
    return argument != NULL && strncmp(argument, "--", 2) == 0 &&
           strcmp(argument + 2, name) == 0;
}

typedef enum {
    RC_MODE_NONE,
    RC_MODE_INTERACTIVE,
    RC_MODE_RUN,
    RC_MODE_MKDISC
} rc_program_mode_t;

static int select_mode(rc_program_mode_t *mode, rc_program_mode_t requested)
{
    if (*mode != RC_MODE_NONE && *mode != requested) {
        (void)fprintf(stderr, "conflicting execution modes\n");
        return -1;
    }
    *mode = requested;
    return 0;
}

static int normal_from_options(int argc, char **argv)
{
    enum {
        OPTION_RUN = 256,
        OPTION_MONITOR,
        OPTION_DISK,
        OPTION_DISK_OVERLAY,
        OPTION_DISK_WRITE_THROUGH,
        OPTION_STEPS,
        OPTION_TRACE,
        OPTION_DISC_LOAD_FLAG,
        OPTION_IX,
        OPTION_NO_IX,
        OPTION_MKDISC
    };
    static const struct option options[] = {
        {"interactive", no_argument, NULL, 'i'},
        {"config", required_argument, NULL, 'c'},
        {"help", no_argument, NULL, 'h'},
        {"run", no_argument, NULL, OPTION_RUN},
        {"monitor", required_argument, NULL, OPTION_MONITOR},
        {"disk", required_argument, NULL, OPTION_DISK},
        {"disk-overlay", required_argument, NULL, OPTION_DISK_OVERLAY},
        {"disk-write-through", no_argument, NULL,
         OPTION_DISK_WRITE_THROUGH},
        {"steps", required_argument, NULL, OPTION_STEPS},
        {"trace", required_argument, NULL, OPTION_TRACE},
        {"set-disc-load-flag", no_argument, NULL, OPTION_DISC_LOAD_FLAG},
        {"ix", no_argument, NULL, OPTION_IX},
        {"no-ix", no_argument, NULL, OPTION_NO_IX},
        {"mkdisc", required_argument, NULL, OPTION_MKDISC},
        {NULL, 0, NULL, 0}
    };
    const char *config_path = "RC4000.cfg";
    const char *monitor_override = NULL;
    const char *disk_override = NULL;
    const char *disk_overlay_override = NULL;
    bool disk_write_through = false;
    const char *steps = "1000000";
    const char *trace_path = NULL;
    rc_program_mode_t mode = RC_MODE_NONE;
    int set_disc_load_flag = 0;
    bool auto_run = false;
    int ix_override = -1;
    int option;
    const char *new_disc = NULL;
    bool runtime_options = false;

    opterr = 0;
    optind = 1;
    while ((option = getopt_long(argc, argv, "iac:h", options, NULL)) != -1) {
        switch (option) {
        case 'i':
            if (select_mode(&mode, RC_MODE_INTERACTIVE) != 0) {
                return EXIT_FAILURE;
            }
            break;
        case 'a':
            if (select_mode(&mode, RC_MODE_INTERACTIVE) != 0) {
                return EXIT_FAILURE;
            }
            set_disc_load_flag = 1;
            auto_run = true;
            break;
        case 'c':
            runtime_options = true;
            config_path = optarg;
            break;
        case 'h':
            usage(stdout);
            return EXIT_SUCCESS;
        case OPTION_RUN:
            if (select_mode(&mode, RC_MODE_RUN) != 0) {
                return EXIT_FAILURE;
            }
            break;
        case OPTION_MONITOR:
            runtime_options = true;
            monitor_override = optarg;
            break;
        case OPTION_DISK:
            runtime_options = true;
            disk_override = optarg;
            break;
        case OPTION_DISK_OVERLAY:
            runtime_options = true;
            disk_overlay_override = optarg;
            break;
        case OPTION_DISK_WRITE_THROUGH:
            runtime_options = true;
            disk_write_through = true;
            break;
        case OPTION_STEPS:
            runtime_options = true;
            steps = optarg;
            break;
        case OPTION_TRACE:
            runtime_options = true;
            trace_path = optarg;
            break;
        case OPTION_DISC_LOAD_FLAG:
            runtime_options = true;
            set_disc_load_flag = 1;
            break;
        case OPTION_IX:
            runtime_options = true;
            ix_override = 1;
            break;
        case OPTION_NO_IX:
            runtime_options = true;
            ix_override = 0;
            break;
        case OPTION_MKDISC:
            if (new_disc != NULL || select_mode(&mode, RC_MODE_MKDISC) != 0) {
                (void)fprintf(stderr, "--mkdisc must be used once, without execution options\n");
                return EXIT_FAILURE;
            }
            new_disc = optarg;
            break;
        default:
            if (optopt != 0) {
                (void)fprintf(stderr, "invalid option: -%c\n", optopt);
            } else {
                (void)fprintf(stderr, "invalid option: %s\n",
                              optind > 0 ? argv[optind - 1] : "");
            }
            usage(stderr);
            return EXIT_FAILURE;
        }
    }
    if (optind != argc) {
        (void)fprintf(stderr, "unexpected argument: %s\n", argv[optind]);
        return EXIT_FAILURE;
    }
    if (mode == RC_MODE_MKDISC) {
        if (runtime_options) {
            (void)fprintf(stderr, "--mkdisc cannot be combined with runtime options\n");
            return EXIT_FAILURE;
        }
        return make_disc(new_disc, false);
    }
    if (disk_write_through && disk_override == NULL) {
        (void)fprintf(stderr,
                      "--disk-write-through requires an explicit --disk FILE\n");
        return EXIT_FAILURE;
    }
    if (disk_write_through && disk_overlay_override != NULL) {
        (void)fprintf(stderr,
                      "--disk-write-through conflicts with --disk-overlay\n");
        return EXIT_FAILURE;
    }
    if (mode == RC_MODE_INTERACTIVE) {
        if (trace_path != NULL) {
            (void)fprintf(stderr, "--trace requires --run\n");
            return EXIT_FAILURE;
        }
        return interactive_monitor(config_path, monitor_override,
                                   disk_override, disk_overlay_override,
                                   disk_write_through,
                                   set_disc_load_flag, auto_run, ix_override);
    }
    if (mode == RC_MODE_RUN) {
        return run_monitor(config_path, monitor_override, disk_override,
                           disk_overlay_override, disk_write_through,
                           steps, trace_path,
                           set_disc_load_flag, ix_override);
    }
    usage(argc == 1 ? stdout : stderr);
    return argc == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    if (argc == 3 && long_option_is(argv[1], "check-config")) {
        return check_config(argv[2]);
    }
    if (argc == 3 && long_option_is(argv[1], "inspect-monitor")) {
        return inspect_monitor(argv[2]);
    }
    if (argc == 3 && long_option_is(argv[1], "inspect-image")) {
        return inspect_image(argv[2]);
    }
    if (argc == 3 && long_option_is(argv[1], "list-catalog")) {
        return list_catalog(argv[2], false);
    }
    if (argc == 3 && long_option_is(argv[1], "list-main-catalog")) {
        return list_catalog(argv[2], true);
    }
    if ((argc == 4 || argc == 5) &&
        long_option_is(argv[1], "load-monitor")) {
        int set_disc_load_flag = 0;
        if (argc == 5) {
            if (!long_option_is(argv[4], "set-disc-load-flag")) {
                usage(stderr);
                return EXIT_FAILURE;
            }
            set_disc_load_flag = 1;
        }
        return load_monitor(argv[2], argv[3], set_disc_load_flag);
    }
    if ((argc == 5 || argc == 6) &&
        long_option_is(argv[1], "run-monitor")) {
        int set_disc_load_flag = 0;
        if (argc == 6) {
            if (!long_option_is(argv[5], "set-disc-load-flag")) {
                usage(stderr);
                return EXIT_FAILURE;
            }
            set_disc_load_flag = 1;
        }
        return run_monitor(argv[2], argv[3], NULL, NULL, false,
                           argv[4], NULL,
                           set_disc_load_flag, -1);
    }
    return normal_from_options(argc, argv);
}
