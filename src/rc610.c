#define _POSIX_C_SOURCE 200809L
#include "sim4000/rc610.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(char *error, size_t size, const char *message)
{
    if (error != NULL && size != 0)
        (void)snprintf(error, size, "%s", message);
    return -1;
}

rc_word_t rc610_status(const rc610_device_t *p)
{
    if (p == NULL) return RC610_PAPER_OUT;
    return p->latched_status |
        ((!p->paper_present || !p->connected || p->output_failed) ? RC610_PAPER_OUT : 0U);
}

static void output_error(rc610_device_t *p)
{
    p->output_failed = true;
    ++p->output_errors;
}

static bool graphic(uint8_t c)
{
    /* 88 glyphs plus space, RCSL 55-D54 p.2. National codes stay seven-bit. */
    return c >= 32 && c <= 125 && c != 35 && c != 36 &&
           c != 64 && c != 94 && c != 96;
}

static rc_device_result_t printer_io(void *context, rc_io_command_t command,
                                     uint8_t modifier, rc_word_t *word)
{
    rc610_device_t *p = context;
    uint8_t c;
    (void)modifier;
    if (p == NULL || word == NULL) return RC_DEVICE_DISCONNECTED;
    /* These commands have no effect, even when the printer is busy/offline. */
    if (command == RC_IO_READ || command == RC_IO_CONTROL) return RC_DEVICE_ACCEPTED;
    if (command != RC_IO_SENSE && command != RC_IO_WRITE) return RC_DEVICE_DISCONNECTED;
    if (p->printing) return RC_DEVICE_BUSY;
    if (command == RC_IO_SENSE) {
        *word = rc610_status(p);
        return RC_DEVICE_ACCEPTED;
    }
    p->latched_status &= ~(RC610_INTERVENTION | RC610_PARITY);
    ++p->write_operations;
    c = (uint8_t)(*word & UINT32_C(0x7f));
    if (c >= 10 && c <= 13) {
        p->printing = true;
        p->print_control = c;
        p->printing_started = p->remote;
        p->print_with_paper = p->paper_present;
    } else if (!graphic(c)) {
        ++p->ignored_characters;
    } else if (p->line_length == RC610_COLUMNS) {
        ++p->overflow_characters;
    } else {
        p->line[p->line_length++] = c;
    }
    return RC_DEVICE_ACCEPTED;
}

static void printer_advance(void *context, uint64_t ticks)
{
    rc610_device_t *p = context;
    uint8_t bytes[RC610_COLUMNS + 1];
    bool failed = false;
    if (p == NULL || ticks == 0 || !p->printing) return;
    if (!p->printing_started) {
        if (!p->remote) return;
        p->printing_started = true;
        p->print_with_paper = p->paper_present;
    }
    /* One positive advance completes one line; no physical timing claim. */
    if (p->connected && !p->output_failed && p->print_with_paper &&
        (p->latched_status & (RC610_PARITY | RC610_TIMER)) == 0) {
        size_t count = p->line_length + 1;
        size_t written;
        memcpy(bytes, p->line, p->line_length);
        bytes[p->line_length] = p->print_control;
        written = p->sink.write(p->sink_context, bytes, count);
        p->output_bytes += written <= count ? written : count;
        /* Flush every print, including CR/VT/FF and a short write's prefix.
           Completion must not leave successful output in a stdio buffer. */
        bool flush_failed = p->sink.flush != NULL && p->sink.flush(p->sink_context) != 0;
        if (written != count || flush_failed) {
            output_error(p);
            failed = true;
        }
    } else {
        failed = true;
    }
    if (failed) ++p->failed_prints;
    ++p->completed_prints;
    p->line_length = 0;
    p->printing = false;
    p->printing_started = false;
    p->pending_interrupts |= UINT32_C(1) << (23U - p->interrupt_level);
    if (p->local_pending) {
        p->local_pending = false;
        p->remote = false;
        p->latched_status |= RC610_INTERVENTION;
    }
}

static uint32_t printer_poll(void *context)
{
    rc610_device_t *p = context;
    uint32_t result;
    if (p == NULL) return 0;
    result = p->pending_interrupts;
    p->pending_interrupts = 0;
    return result;
}

bool rc610_init(rc610_device_t *p, unsigned level)
{
    if (p == NULL) return false;
    *p = (rc610_device_t){0};
    if (level >= 24) return false;
    p->device = (rc_device_t){p, printer_io, printer_advance, printer_poll};
    p->interrupt_level = level;
    p->remote = true;
    p->paper_present = true;
    return true;
}

int rc610_connect(rc610_device_t *p, rc610_sink_t sink, void *context,
                  char *error, size_t size)
{
    if (p == NULL || p->device.io != printer_io || sink.write == NULL)
        return fail(error, size, "invalid RC610 output sink");
    if (p->connected || p->printing || p->line_length != 0)
        return fail(error, size, "RC610 is connected or has pending output");
    p->sink = sink;
    p->sink_context = context;
    p->connected = true;
    p->output_failed = false;
    return 0;
}

static size_t file_write(void *context, const uint8_t *bytes, size_t count)
{
    return fwrite(bytes, 1, count, context);
}
static int file_flush(void *context) { return fflush(context); }
static int file_close(void *context) { return fclose(context); }

static int open_output(rc610_device_t *p, const char *path, bool append,
                        char *error, size_t size)
{
    int descriptor;
    FILE *file;
    rc610_sink_t sink = {file_write, file_flush, file_close};
    if (p == NULL || p->device.io != printer_io || path == NULL || path[0] == '\0' ||
        strlen(path) >= sizeof(p->path))
        return fail(error, size, "invalid RC610 output path");
    if (p->connected || p->printing || p->line_length != 0)
        return fail(error, size, "RC610 is connected or has pending output");
    descriptor = open(path, O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK |
                      (append ? O_APPEND : O_EXCL), 0600);
    if (descriptor < 0) {
        if (error != NULL && size != 0)
            (void)snprintf(error, size, "cannot create RC610 output: %s", strerror(errno));
        return -1;
    }
    struct stat info;
    if (fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode)) {
        (void)close(descriptor);
        return fail(error, size, "RC610 output must be a regular file");
    }
    file = fdopen(descriptor, append ? "ab" : "wb");
    if (file == NULL) {
        (void)close(descriptor);
        /* Keep the exclusively created empty file; never unlink a raced path. */
        return fail(error, size, "cannot initialize RC610 output stream");
    }
    if (rc610_connect(p, sink, file, error, size) != 0) {
        (void)fclose(file);
        return -1;
    }
    (void)strcpy(p->path, path);
    return 0;
}

int rc610_open(rc610_device_t *p, const char *path, char *error, size_t size)
{
    return open_output(p, path, false, error, size);
}

int rc610_open_append(rc610_device_t *p, const char *path, char *error, size_t size)
{
    return open_output(p, path, true, error, size);
}

int rc610_close(rc610_device_t *p, char *error, size_t size)
{
    int result = 0;
    if (p == NULL) return fail(error, size, "invalid RC610 device");
    if (p->printing) return fail(error, size, "RC610 print operation is still active");
    if (p->connected) {
        if (p->sink.flush != NULL && p->sink.flush(p->sink_context) != 0) result = -1;
        /* Always close, even after a flush error. Ownership ends here. */
        if (p->sink.close != NULL && p->sink.close(p->sink_context) != 0) result = -1;
        if (result != 0) output_error(p);
        if (p->output_failed) result = -1;
    }
    p->incomplete_characters += p->line_length;
    p->line_length = 0;
    p->sink = (rc610_sink_t){0};
    p->sink_context = NULL;
    p->connected = false;
    p->path[0] = '\0';
    if (result != 0) return fail(error, size, "RC610 output failed; file may contain a partial print");
    return 0;
}

void rc610_set_remote(rc610_device_t *p, bool remote)
{
    if (p == NULL) return;
    if (remote) {
        p->local_pending = false;
        p->remote = true;
    } else if (p->remote && p->printing_started) {
        p->local_pending = true;
    } else if (p->remote) {
        p->remote = false;
        p->latched_status |= RC610_INTERVENTION;
    }
}

void rc610_set_paper(rc610_device_t *p, bool present)
{
    if (p != NULL) p->paper_present = present;
}

bool rc610_set_fault(rc610_device_t *p, rc_word_t mask)
{
    if (p == NULL || (mask & ~(RC610_INTERVENTION | RC610_PARITY | RC610_TIMER)) != 0)
        return false;
    p->latched_status |= mask;
    return true;
}

void rc610_master_clear(rc610_device_t *p)
{
    if (p != NULL) p->latched_status &= ~RC610_TIMER;
}
