#define _POSIX_C_SOURCE 200809L

#include "sim4000/device.h"
#include "sim4000/machine.h"
#include "terminal_v2.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static uint32_t interrupt_bit(unsigned level)
{
    return level < 24U ? UINT32_C(1) << (23U - level) : 0;
}

static void clock_advance(void *context, uint64_t ticks)
{
    rc_clock_device_t *clock = context;

    if (clock == NULL || ticks == 0) {
        return;
    }
    clock->ticks += ticks;
    clock->pending_interrupts |= interrupt_bit(clock->interrupt_level);
}

static uint32_t clock_poll(void *context)
{
    rc_clock_device_t *clock = context;
    uint32_t pending;

    if (clock == NULL) {
        return 0;
    }
    pending = clock->pending_interrupts;
    clock->pending_interrupts = 0;
    return pending;
}

static rc_device_result_t clock_io(void *context, rc_io_command_t command,
                                   uint8_t modifier, rc_word_t *word)
{
    rc_clock_device_t *clock = context;

    (void)modifier;
    if (clock == NULL || word == NULL || command != RC_IO_SENSE) {
        return RC_DEVICE_DISCONNECTED;
    }
    *word = (rc_word_t)((clock->ticks & UINT64_C(0x3f)) << 8);
    return RC_DEVICE_ACCEPTED;
}

void rc_clock_device_init(rc_clock_device_t *clock, unsigned interrupt_level)
{
    if (clock == NULL) {
        return;
    }
    clock->device.context = clock;
    clock->device.io = clock_io;
    clock->device.advance = clock_advance;
    clock->device.poll = clock_poll;
    clock->ticks = 0;
    clock->interrupt_level = interrupt_level;
    clock->pending_interrupts = 0;
}

void rc_clock_device_set_ticks(rc_clock_device_t *clock, uint64_t ticks)
{
    if (clock != NULL) {
        clock->ticks = ticks;
    }
}

enum {
    RC_READER_PARITY_ERROR = 1U << 22,
    RC_READER_END_OF_TAPE = 1U << 18
};

static void tape_set_file_error(char *error, size_t error_size,
                                const char *operation, const char *path)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s %s: %s", operation, path,
                       strerror(errno));
    }
}

static bool byte_has_even_parity(uint8_t value)
{
    unsigned ones = 0;

    while (value != 0) {
        ones += value & 1U;
        value >>= 1;
    }
    return (ones & 1U) == 0;
}

static void reader_request_interrupt(rc_reader_device_t *reader)
{
    reader->pending_interrupts |= interrupt_bit(reader->interrupt_level);
}

static uint32_t reader_poll(void *context)
{
    rc_reader_device_t *reader = context;
    uint32_t pending;

    if (reader == NULL) {
        return 0;
    }
    pending = reader->pending_interrupts;
    reader->pending_interrupts = 0;
    return pending;
}

static void reader_end_tape(rc_reader_device_t *reader)
{
    if (reader->file != NULL) {
        (void)fclose(reader->file);
        reader->file = NULL;
    }
    reader->tape_present = false;
    reader->status = RC_READER_END_OF_TAPE;
    reader->path[0] = '\0';
    reader_request_interrupt(reader);
}

static rc_device_result_t reader_io(void *context, rc_io_command_t command,
                                    uint8_t modifier, rc_word_t *word)
{
    rc_reader_device_t *reader = context;
    int input;
    uint8_t character;
    bool even_parity;

    if (reader == NULL || word == NULL) {
        return RC_DEVICE_DISCONNECTED;
    }
    if (command == RC_IO_SENSE) {
        *word = reader->status |
            (reader->tape_present ? 0U : RC_READER_END_OF_TAPE);
        return RC_DEVICE_ACCEPTED;
    }
    if (command != RC_IO_READ || modifier > 2U) {
        return RC_DEVICE_DISCONNECTED;
    }
    ++reader->read_operations;
    if (!reader->tape_present || reader->file == NULL) {
        reader_end_tape(reader);
        return RC_DEVICE_ACCEPTED;
    }
    do {
        input = fgetc(reader->file);
        if (input == EOF) {
            reader_end_tape(reader);
            return RC_DEVICE_ACCEPTED;
        }
        character = (uint8_t)input;
        if (character == 0) {
            ++reader->null_bytes_skipped;
        }
    } while (character == 0);

    ++reader->characters_read;
    even_parity = byte_has_even_parity(character);
    if (modifier == 0U) {
        reader->status = character & UINT8_C(0x7f);
        if (even_parity) {
            reader->status |= RC_READER_PARITY_ERROR;
            ++reader->parity_errors;
        }
    } else if (modifier == 1U) {
        reader->status = character & UINT8_C(0x7f);
        if (!even_parity) {
            reader->status |= RC_READER_PARITY_ERROR;
            ++reader->parity_errors;
        }
    } else {
        reader->status = character;
        if (!even_parity) {
            reader->status |= RC_READER_PARITY_ERROR;
            ++reader->parity_errors;
        }
    }
    reader_request_interrupt(reader);
    return RC_DEVICE_ACCEPTED;
}

void rc_reader_device_init(rc_reader_device_t *reader,
                           unsigned interrupt_level)
{
    if (reader == NULL) {
        return;
    }
    *reader = (rc_reader_device_t){0};
    reader->device.context = reader;
    reader->device.io = reader_io;
    reader->device.poll = reader_poll;
    reader->interrupt_level = interrupt_level;
    reader->status = RC_READER_END_OF_TAPE;
}

int rc_reader_device_mount(rc_reader_device_t *reader, const char *path,
                           char *error, size_t error_size)
{
    FILE *file;
    size_t path_length;

    if (reader == NULL || path == NULL || path[0] == '\0') {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "invalid reader tape path");
        }
        return -1;
    }
    path_length = strlen(path);
    if (path_length >= sizeof(reader->path)) {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "reader tape path is too long");
        }
        return -1;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        tape_set_file_error(error, error_size, "cannot open", path);
        return -1;
    }
    rc_reader_device_unmount(reader);
    reader->file = file;
    reader->tape_present = true;
    reader->status = 0;
    (void)memcpy(reader->path, path, path_length + 1U);
    reader_request_interrupt(reader);
    return 0;
}

int rc_reader_device_rewind(rc_reader_device_t *reader, char *error,
                            size_t error_size)
{
    if (reader == NULL || !reader->tape_present || reader->file == NULL) {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "no paper tape is mounted");
        }
        return -1;
    }
    if (fseek(reader->file, 0, SEEK_SET) != 0) {
        tape_set_file_error(error, error_size, "cannot rewind", reader->path);
        return -1;
    }
    clearerr(reader->file);
    reader->status = 0;
    reader_request_interrupt(reader);
    return 0;
}

void rc_reader_device_unmount(rc_reader_device_t *reader)
{
    if (reader == NULL) {
        return;
    }
    if (reader->file != NULL) {
        (void)fclose(reader->file);
        reader->file = NULL;
    }
    reader->tape_present = false;
    reader->status = RC_READER_END_OF_TAPE;
    reader->path[0] = '\0';
}

void rc_reader_device_destroy(rc_reader_device_t *reader)
{
    rc_reader_device_unmount(reader);
}

enum {
    RC_PUNCH_TIMER = 1U << 21,
    RC_PUNCH_END_OF_TAPE = 1U << 18
};

static void punch_request_interrupt(rc_punch_device_t *punch)
{
    punch->pending_interrupts |= interrupt_bit(punch->interrupt_level);
}

static uint32_t punch_poll(void *context)
{
    rc_punch_device_t *punch = context;
    uint32_t pending;

    if (punch == NULL) {
        return 0;
    }
    pending = punch->pending_interrupts;
    punch->pending_interrupts = 0;
    return pending;
}

static void punch_output_error(rc_punch_device_t *punch)
{
    ++punch->write_errors;
    punch->status = RC_PUNCH_END_OF_TAPE;
    if (punch->file != NULL) {
        (void)fclose(punch->file);
        punch->file = NULL;
    }
    punch->tape_present = false;
    punch_request_interrupt(punch);
}

static rc_device_result_t punch_io(void *context, rc_io_command_t command,
                                   uint8_t modifier, rc_word_t *word)
{
    rc_punch_device_t *punch = context;
    uint8_t character;
    bool even_parity;

    if (punch == NULL || word == NULL) {
        return RC_DEVICE_DISCONNECTED;
    }
    if (command == RC_IO_SENSE) {
        *word = punch->status |
            (punch->tape_present ? 0U : RC_PUNCH_TIMER);
        return RC_DEVICE_ACCEPTED;
    }
    if (command != RC_IO_WRITE || modifier > 2U) {
        return RC_DEVICE_DISCONNECTED;
    }
    ++punch->write_operations;
    punch->status = 0;
    if (!punch->tape_present || punch->file == NULL) {
        punch->status = RC_PUNCH_TIMER;
        punch_request_interrupt(punch);
        return RC_DEVICE_ACCEPTED;
    }
    character = (uint8_t)*word;
    if (modifier < 2U) {
        character &= UINT8_C(0x7f);
        even_parity = byte_has_even_parity(character);
        if ((modifier == 0U && even_parity) ||
            (modifier == 1U && !even_parity)) {
            character |= UINT8_C(0x80);
        }
    }
    if (fputc(character, punch->file) == EOF) {
        punch_output_error(punch);
        return RC_DEVICE_ACCEPTED;
    }
    ++punch->characters_written;
    punch_request_interrupt(punch);
    return RC_DEVICE_ACCEPTED;
}

void rc_punch_device_init(rc_punch_device_t *punch,
                          unsigned interrupt_level)
{
    if (punch == NULL) {
        return;
    }
    *punch = (rc_punch_device_t){0};
    punch->device.context = punch;
    punch->device.io = punch_io;
    punch->device.poll = punch_poll;
    punch->interrupt_level = interrupt_level;
    punch->status = RC_PUNCH_TIMER;
}

int rc_punch_device_mount(rc_punch_device_t *punch, const char *path,
                          bool replace, char *error, size_t error_size)
{
    FILE *file;
    size_t path_length;

    if (punch == NULL || path == NULL || path[0] == '\0') {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "invalid punch tape path");
        }
        return -1;
    }
    path_length = strlen(path);
    if (path_length >= sizeof(punch->path)) {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "punch tape path is too long");
        }
        return -1;
    }
    if (replace) {
        file = fopen(path, "wb");
    } else {
        int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);

        file = descriptor < 0 ? NULL : fdopen(descriptor, "wb");
        if (descriptor >= 0 && file == NULL) {
            int saved_errno = errno;

            (void)close(descriptor);
            (void)unlink(path);
            errno = saved_errno;
        }
    }
    if (file == NULL) {
        tape_set_file_error(error, error_size,
                            replace ? "cannot replace" : "cannot create",
                            path);
        return -1;
    }
    rc_punch_device_unmount(punch);
    punch->file = file;
    punch->tape_present = true;
    punch->status = 0;
    (void)memcpy(punch->path, path, path_length + 1U);
    return 0;
}

int rc_punch_device_flush(rc_punch_device_t *punch, char *error,
                          size_t error_size)
{
    if (punch == NULL || !punch->tape_present || punch->file == NULL) {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "no output paper tape is mounted");
        }
        return -1;
    }
    if (fflush(punch->file) != 0 || fsync(fileno(punch->file)) != 0) {
        tape_set_file_error(error, error_size, "cannot flush", punch->path);
        return -1;
    }
    return 0;
}

void rc_punch_device_unmount(rc_punch_device_t *punch)
{
    if (punch == NULL) {
        return;
    }
    if (punch->file != NULL) {
        (void)fclose(punch->file);
        punch->file = NULL;
    }
    punch->tape_present = false;
    punch->status = RC_PUNCH_TIMER;
    punch->path[0] = '\0';
}

void rc_punch_device_destroy(rc_punch_device_t *punch)
{
    if (punch != NULL && punch->tape_present) {
        (void)rc_punch_device_flush(punch, NULL, 0);
    }
    rc_punch_device_unmount(punch);
}

static void console_emit(rc_console_device_t *console, uint8_t character)
{
    if (console->output != NULL) {
        console->output(console->output_context, character);
    }
}

static void console_request(rc_console_device_t *console, unsigned level)
{
    console->pending_interrupts |= interrupt_bit(level);
}

static uint8_t console_pop(rc_console_device_t *console)
{
    uint8_t character = console->input[console->input_head];

    console->input_head = (console->input_head + 1U) % RC_CONSOLE_QUEUE_SIZE;
    --console->input_count;
    return character;
}

static void console_complete_read(rc_console_device_t *console,
                                  uint8_t character)
{
    if (console->protocol == RC_TERMINAL_V2) {
        rc_terminal_v2_complete_read(console, character);
        return;
    }
    console->status = (rc_word_t)(character & UINT8_C(0x7f));
    console->echo_on_sense = true;
    console->busy = false;
    console->timeout_ticks = 0;
    console_request(console, console->io_interrupt_level);
}

static rc_device_result_t console_io(void *context, rc_io_command_t command,
                                     uint8_t modifier, rc_word_t *word)
{
    rc_console_device_t *console = context;

    if (console == NULL || word == NULL) {
        return RC_DEVICE_DISCONNECTED;
    }
    if (console->protocol == RC_TERMINAL_V2)
        return rc_terminal_v2_io(console, command, modifier, word);
    switch (command) {
    case RC_IO_SENSE:
        if (console->busy) {
            return RC_DEVICE_BUSY;
        }
        *word = console->status;
        if (console->echo_on_sense && console->status != 27U) {
            console_emit(console, (uint8_t)(console->status & 0x7fU));
        }
        console->echo_on_sense = false;
        return RC_DEVICE_ACCEPTED;
    case RC_IO_CONTROL:
        console_request(console, console->io_interrupt_level);
        return RC_DEVICE_ACCEPTED;
    case RC_IO_READ:
        if (console->busy) {
            return RC_DEVICE_BUSY;
        }
        if (console->input_count == 0) {
            console->busy = true;
            console->timeout_ticks = RC_CONSOLE_TIMEOUT_TICKS;
        } else {
            console_complete_read(console, console_pop(console));
        }
        return RC_DEVICE_ACCEPTED;
    case RC_IO_WRITE:
        console->status = *word & UINT32_C(0x7f);
        console_emit(console, (uint8_t)console->status);
        console_request(console, console->io_interrupt_level);
        console->busy = false;
        console->timeout_ticks = 0;
        return RC_DEVICE_ACCEPTED;
    }
    return RC_DEVICE_DISCONNECTED;
}

static void console_advance(void *context, uint64_t ticks)
{
    rc_console_device_t *console = context;

    if (console == NULL || !console->busy || ticks == 0) {
        return;
    }
    if (ticks < console->timeout_ticks) {
        console->timeout_ticks -= ticks;
        return;
    }
    console->timeout_ticks = 0;
    console->busy = false;
    console->status = UINT32_C(1) << 21;
    console_request(console, console->io_interrupt_level);
}

static uint32_t console_poll(void *context)
{
    rc_console_device_t *console = context;
    uint32_t pending;

    if (console == NULL) {
        return 0;
    }
    pending = console->pending_interrupts;
    console->pending_interrupts = 0;
    return pending;
}

void rc_console_device_init(rc_console_device_t *console,
                            unsigned io_interrupt_level,
                            unsigned operator_interrupt_level)
{
    if (console == NULL) {
        return;
    }
    *console = (rc_console_device_t){0};
    console->device.context = console;
    console->device.io = console_io;
    console->device.advance = console_advance;
    console->device.poll = console_poll;
    console->io_interrupt_level = io_interrupt_level;
    console->operator_interrupt_level = operator_interrupt_level;
}

void rc_console_device_set_output(rc_console_device_t *console,
                                  rc_console_output_fn output,
                                  void *context)
{
    if (console != NULL) {
        console->output = output;
        console->output_context = context;
    }
}

bool rc_console_device_set_protocol(rc_console_device_t *console,
                                    struct rc_machine *machine,
                                    rc_terminal_protocol_t protocol)
{
    if (console == NULL || console->busy || console->input_count != 0 ||
        (protocol != RC_TERMINAL_CLASSIC && protocol != RC_TERMINAL_V2) ||
        (protocol == RC_TERMINAL_V2 && machine == NULL)) return false;
    console->protocol = protocol;
    console->machine = machine;
    console->terminal_type = 1;
    console->prompt = 7;
    console->echo_on_sense = false;
    console->echo_length = 0;
    console->read_modifier = 0;
    return true;
}

bool rc_console_device_submit(rc_console_device_t *console,
                              uint8_t character)
{
    size_t tail;

    if (console == NULL) {
        return false;
    }
    character &= UINT8_C(0x7f);
    if (character == 27U && !console->busy) {
        console->input_head = 0;
        console->input_count = 0;
        console_request(console, console->operator_interrupt_level);
        return true;
    }
    if (console->busy && console->input_count == 0) {
        console_complete_read(console, character);
        return true;
    }
    if (console->input_count == RC_CONSOLE_QUEUE_SIZE - 1U) {
        return false;
    }
    tail = (console->input_head + console->input_count) %
           RC_CONSOLE_QUEUE_SIZE;
    console->input[tail] = character;
    ++console->input_count;
    console->busy = false;
    console->timeout_ticks = 0;
    return true;
}

void rc_console_device_attention(rc_console_device_t *console)
{
    if (console == NULL) {
        return;
    }
    console->input_head = 0;
    console->input_count = 0;
    console_request(console, console->operator_interrupt_level);
}

size_t rc_console_device_input_count(const rc_console_device_t *console)
{
    return console != NULL ? console->input_count : 0;
}

size_t rc_console_device_discard_input(rc_console_device_t *console)
{
    size_t count;

    if (console == NULL) return 0;
    count = console->input_count;
    console->input_head = 0;
    console->input_count = 0;
    return count;
}

enum {
    RC433X_MESSAGE_WORDS = 8,
    RC433X_WORDS_PER_SECTOR = RC433X_SECTOR_BYTES / 3,
    RC433X_OVERLAY_HEADER_SIZE = 48,
    RC433X_OVERLAY_RECORD_SIZE = 8 + RC433X_SECTOR_BYTES + 8
};

static const uint8_t rc433x_overlay_magic[8] = {
    'S', '4', 'O', 'V', 'L', '0', '0', '1'
};

struct rc_rc433x_overlay_sector {
    uint64_t sector;
    uint8_t data[RC433X_SECTOR_BYTES];
    struct rc_rc433x_overlay_sector *next;
};

static void rc433x_set_error(char *error, size_t error_size,
                             const char *message)
{
    if (error != NULL && error_size != 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

static void rc433x_set_file_error(char *error, size_t error_size,
                                  const char *action, const char *path)
{
    if (error != NULL && error_size != 0) {
        (void)snprintf(error, error_size, "%s %s: %s", action, path,
                       strerror(errno));
    }
}

static uint32_t rc433x_get_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint64_t rc433x_get_u64(const uint8_t *bytes)
{
    uint64_t value = 0;
    unsigned index;

    for (index = 0; index < 8; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static void rc433x_put_u32(uint8_t *bytes, uint32_t value)
{
    unsigned index;

    for (index = 0; index < 4; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static void rc433x_put_u64(uint8_t *bytes, uint64_t value)
{
    unsigned index;

    for (index = 0; index < 8; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint64_t rc433x_record_hash(uint64_t sector,
                                   const uint8_t data[RC433X_SECTOR_BYTES])
{
    uint8_t encoded_sector[8];
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;

    rc433x_put_u64(encoded_sector, sector);
    for (index = 0; index < sizeof(encoded_sector); ++index) {
        hash ^= encoded_sector[index];
        hash *= UINT64_C(1099511628211);
    }
    for (index = 0; index < RC433X_SECTOR_BYTES; ++index) {
        hash ^= data[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void rc433x_clear_overlay(rc_rc433x_device_t *disk)
{
    struct rc_rc433x_overlay_sector *entry = disk->overlay;

    while (entry != NULL) {
        struct rc_rc433x_overlay_sector *next = entry->next;
        free(entry);
        entry = next;
    }
    disk->overlay = NULL;
    disk->overlay_sectors = 0;
}

static struct rc_rc433x_overlay_sector *rc433x_find_overlay_sector(
    rc_rc433x_device_t *disk, uint64_t sector)
{
    struct rc_rc433x_overlay_sector *entry;

    for (entry = disk->overlay; entry != NULL; entry = entry->next) {
        if (entry->sector == sector) {
            return entry;
        }
    }
    return NULL;
}

static int rc433x_overlay_pointer_compare(const void *left, const void *right)
{
    const struct rc_rc433x_overlay_sector *const *left_entry = left;
    const struct rc_rc433x_overlay_sector *const *right_entry = right;

    if ((*left_entry)->sector < (*right_entry)->sector) {
        return -1;
    }
    if ((*left_entry)->sector > (*right_entry)->sector) {
        return 1;
    }
    return 0;
}

static bool rc433x_save_overlay(rc_rc433x_device_t *disk,
                                char *error, size_t error_size)
{
    struct rc_rc433x_overlay_sector **entries = NULL;
    struct rc_rc433x_overlay_sector *entry;
    uint8_t header[RC433X_OVERLAY_HEADER_SIZE] = {0};
    char temporary[RC433X_OVERLAY_PATH_SIZE + 16] = {0};
    size_t path_length;
    size_t index = 0;
    int descriptor = -1;
    FILE *file = NULL;
    bool ok = false;

    if (!disk->persistent_overlay) {
        return true;
    }
    if (disk->overlay_sectors > SIZE_MAX / sizeof(*entries)) {
        rc433x_set_error(error, error_size, "overlay has too many sectors");
        return false;
    }
    if (disk->overlay_sectors != 0) {
        entries = malloc((size_t)disk->overlay_sectors * sizeof(*entries));
        if (entries == NULL) {
            rc433x_set_error(error, error_size,
                             "cannot allocate overlay index");
            return false;
        }
        for (entry = disk->overlay; entry != NULL; entry = entry->next) {
            entries[index++] = entry;
        }
        if (index != disk->overlay_sectors) {
            rc433x_set_error(error, error_size,
                             "overlay sector count is inconsistent");
            goto done;
        }
        qsort(entries, index, sizeof(*entries),
              rc433x_overlay_pointer_compare);
    }

    path_length = strlen(disk->overlay_path);
    if (path_length + sizeof(".tmp.XXXXXX") > sizeof(temporary)) {
        rc433x_set_error(error, error_size, "overlay path is too long");
        goto done;
    }
    (void)memcpy(temporary, disk->overlay_path, path_length);
    (void)memcpy(temporary + path_length, ".tmp.XXXXXX",
                 sizeof(".tmp.XXXXXX"));
    descriptor = mkstemp(temporary);
    if (descriptor < 0) {
        rc433x_set_file_error(error, error_size,
                              "cannot create temporary overlay for",
                              disk->overlay_path);
        goto done;
    }
    file = fdopen(descriptor, "wb");
    if (file == NULL) {
        rc433x_set_file_error(error, error_size,
                              "cannot open temporary overlay for",
                              disk->overlay_path);
        (void)close(descriptor);
        descriptor = -1;
        goto done;
    }
    descriptor = -1;

    (void)memcpy(header, rc433x_overlay_magic,
                 sizeof(rc433x_overlay_magic));
    rc433x_put_u32(header + 8, 1);
    rc433x_put_u32(header + 12, RC433X_OVERLAY_HEADER_SIZE);
    rc433x_put_u32(header + 16, RC433X_SECTOR_BYTES);
    rc433x_put_u64(header + 24, disk->base_image_bytes);
    rc433x_put_u64(header + 32, disk->base_image_hash);
    rc433x_put_u64(header + 40, disk->overlay_sectors);
    if (fwrite(header, 1, sizeof(header), file) != sizeof(header)) {
        rc433x_set_file_error(error, error_size,
                              "cannot write temporary overlay for",
                              disk->overlay_path);
        goto done;
    }
    for (index = 0; index < disk->overlay_sectors; ++index) {
        uint8_t encoded[8];
        uint64_t hash = rc433x_record_hash(entries[index]->sector,
                                            entries[index]->data);

        rc433x_put_u64(encoded, entries[index]->sector);
        if (fwrite(encoded, 1, sizeof(encoded), file) != sizeof(encoded) ||
            fwrite(entries[index]->data, 1, RC433X_SECTOR_BYTES, file) !=
                RC433X_SECTOR_BYTES) {
            rc433x_set_file_error(error, error_size,
                                  "cannot write temporary overlay for",
                                  disk->overlay_path);
            goto done;
        }
        rc433x_put_u64(encoded, hash);
        if (fwrite(encoded, 1, sizeof(encoded), file) != sizeof(encoded)) {
            rc433x_set_file_error(error, error_size,
                                  "cannot write temporary overlay for",
                                  disk->overlay_path);
            goto done;
        }
    }
    if (fflush(file) != 0 || fsync(fileno(file)) != 0) {
        rc433x_set_file_error(error, error_size,
                              "cannot flush temporary overlay for",
                              disk->overlay_path);
        goto done;
    }
    if (fclose(file) != 0) {
        file = NULL;
        rc433x_set_file_error(error, error_size,
                              "cannot close temporary overlay for",
                              disk->overlay_path);
        goto done;
    }
    file = NULL;
    if (rename(temporary, disk->overlay_path) != 0) {
        rc433x_set_file_error(error, error_size, "cannot replace overlay",
                              disk->overlay_path);
        goto done;
    }
    ok = true;

done:
    if (file != NULL) {
        (void)fclose(file);
    } else if (descriptor >= 0) {
        (void)close(descriptor);
    }
    if (!ok && temporary[0] != '\0') {
        (void)unlink(temporary);
    }
    free(entries);
    return ok;
}

static bool rc433x_load_overlay(rc_rc433x_device_t *disk,
                                char *error, size_t error_size)
{
    uint8_t header[RC433X_OVERLAY_HEADER_SIZE];
    uint64_t record_count;
    uint64_t expected_size;
    uint64_t record_index;
    long file_size;
    FILE *file = fopen(disk->overlay_path, "rb");
    bool ok = false;

    if (file == NULL) {
        if (errno == ENOENT) {
            return true;
        }
        rc433x_set_file_error(error, error_size, "cannot open overlay",
                              disk->overlay_path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 ||
        (file_size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        rc433x_set_file_error(error, error_size,
                              "cannot determine overlay size for",
                              disk->overlay_path);
        goto done;
    }
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        rc433x_set_error(error, error_size, "overlay header is truncated");
        goto done;
    }
    if (memcmp(header, rc433x_overlay_magic,
               sizeof(rc433x_overlay_magic)) != 0 ||
        rc433x_get_u32(header + 8) != 1 ||
        rc433x_get_u32(header + 12) != RC433X_OVERLAY_HEADER_SIZE ||
        rc433x_get_u32(header + 16) != RC433X_SECTOR_BYTES ||
        rc433x_get_u32(header + 20) != 0) {
        rc433x_set_error(error, error_size,
                         "overlay header is invalid or unsupported");
        goto done;
    }
    if (rc433x_get_u64(header + 24) != disk->base_image_bytes ||
        rc433x_get_u64(header + 32) != disk->base_image_hash) {
        rc433x_set_error(error, error_size,
                         "overlay does not match the base image");
        goto done;
    }
    record_count = rc433x_get_u64(header + 40);
    if (record_count >
        (UINT64_MAX - RC433X_OVERLAY_HEADER_SIZE) /
            RC433X_OVERLAY_RECORD_SIZE) {
        rc433x_set_error(error, error_size, "overlay record count is invalid");
        goto done;
    }
    expected_size = RC433X_OVERLAY_HEADER_SIZE +
                    record_count * RC433X_OVERLAY_RECORD_SIZE;
    if ((uint64_t)file_size != expected_size) {
        rc433x_set_error(error, error_size,
                         "overlay size does not match its record count");
        goto done;
    }
    for (record_index = 0; record_index < record_count; ++record_index) {
        struct rc_rc433x_overlay_sector *entry;
        uint8_t encoded[8];
        uint64_t stored_hash;

        entry = malloc(sizeof(*entry));
        if (entry == NULL) {
            rc433x_set_error(error, error_size,
                             "cannot allocate overlay sector");
            goto done;
        }
        if (fread(encoded, 1, sizeof(encoded), file) != sizeof(encoded) ||
            fread(entry->data, 1, RC433X_SECTOR_BYTES, file) !=
                RC433X_SECTOR_BYTES) {
            free(entry);
            rc433x_set_error(error, error_size, "overlay record is truncated");
            goto done;
        }
        entry->sector = rc433x_get_u64(encoded);
        if (fread(encoded, 1, sizeof(encoded), file) != sizeof(encoded)) {
            free(entry);
            rc433x_set_error(error, error_size, "overlay checksum is truncated");
            goto done;
        }
        stored_hash = rc433x_get_u64(encoded);
        if (stored_hash != rc433x_record_hash(entry->sector, entry->data)) {
            free(entry);
            rc433x_set_error(error, error_size,
                             "overlay record checksum does not match");
            goto done;
        }
        if (rc433x_find_overlay_sector(disk, entry->sector) != NULL) {
            free(entry);
            rc433x_set_error(error, error_size,
                             "overlay contains a duplicate sector");
            goto done;
        }
        entry->next = disk->overlay;
        disk->overlay = entry;
        ++disk->overlay_sectors;
    }
    ok = true;

done:
    if (fclose(file) != 0 && ok) {
        rc433x_set_file_error(error, error_size, "cannot close overlay",
                              disk->overlay_path);
        ok = false;
    }
    if (!ok) {
        rc433x_clear_overlay(disk);
    }
    return ok;
}

static bool rc433x_read_message(rc_rc433x_device_t *disk,
                                rc_addr_t address,
                                rc_word_t message[RC433X_MESSAGE_WORDS])
{
    size_t index;

    address &= ~UINT32_C(1);
    for (index = 0; index < RC433X_MESSAGE_WORDS; ++index) {
        if (!rc_machine_read_word(disk->machine,
                                  rc_word(address + (rc_addr_t)index * 2U),
                                  &message[index])) {
            return false;
        }
    }
    return true;
}

static bool rc433x_read_data(rc_rc433x_device_t *disk, rc_addr_t core_address,
                             uint64_t first_sector, uint32_t sector_count)
{
    uint8_t data[RC433X_SECTOR_BYTES];
    uint32_t sector_index;

    core_address &= ~UINT32_C(1);
    for (sector_index = 0; sector_index < sector_count; ++sector_index) {
        struct rc_rc433x_overlay_sector *entry;
        uint64_t sector = first_sector + sector_index;
        unsigned word_index;

        entry = rc433x_find_overlay_sector(disk, sector);
        if (entry != NULL) {
            for (word_index = 0; word_index < RC433X_SECTOR_BYTES;
                 ++word_index) {
                data[word_index] = entry->data[word_index];
            }
        } else {
            if (sector >= disk->image.sector_count) {
                break;
            }
            if (!rc_image_read_sector(&disk->image, sector, data)) {
                return false;
            }
        }
        ++disk->sectors_read;
        for (word_index = 0; word_index < RC433X_WORDS_PER_SECTOR;
             ++word_index) {
            size_t byte_index = (size_t)word_index * 3U;
            rc_word_t value = (rc_word_t)data[byte_index] |
                ((rc_word_t)data[byte_index + 1U] << 8) |
                ((rc_word_t)data[byte_index + 2U] << 16);
            rc_addr_t destination = rc_word(
                core_address + (rc_addr_t)sector_index * 512U +
                (rc_addr_t)word_index * 2U);

            if (!rc_machine_write_word(disk->machine, destination, value)) {
                return false;
            }
        }
    }
    return true;
}

static bool rc433x_write_data(rc_rc433x_device_t *disk,
                              rc_addr_t core_address,
                              uint64_t first_sector,
                              uint32_t sector_count)
{
    uint8_t direct_data[RC433X_SECTOR_BYTES];
    uint32_t sector_index;

    core_address &= ~UINT32_C(1);
    for (sector_index = 0; sector_index < sector_count; ++sector_index) {
        struct rc_rc433x_overlay_sector *entry;
        uint8_t *data;
        uint64_t sector = first_sector + sector_index;
        unsigned word_index;

        if (disk->write_through) {
            entry = NULL;
            data = direct_data;
        } else {
            entry = rc433x_find_overlay_sector(disk, sector);
            if (entry == NULL) {
                entry = malloc(sizeof(*entry));
                if (entry == NULL) {
                    return false;
                }
                entry->sector = sector;
                entry->next = disk->overlay;
                disk->overlay = entry;
                ++disk->overlay_sectors;
            }
            data = entry->data;
        }
        for (word_index = 0; word_index < RC433X_WORDS_PER_SECTOR;
             ++word_index) {
            rc_addr_t source = rc_word(
                core_address + (rc_addr_t)sector_index * 512U +
                (rc_addr_t)word_index * 2U);
            rc_word_t value;
            size_t byte_index = (size_t)word_index * 3U;

            if (!rc_machine_read_word(disk->machine, source, &value)) {
                return false;
            }
            data[byte_index] = (uint8_t)value;
            data[byte_index + 1U] = (uint8_t)(value >> 8);
            data[byte_index + 2U] = (uint8_t)(value >> 16);
        }
        if (disk->write_through &&
            !rc_image_write_sector(&disk->image, sector, direct_data)) {
            return false;
        }
        ++disk->sectors_written;
    }
    return true;
}

static rc_device_result_t rc433x_io(void *context, rc_io_command_t command,
                                    uint8_t modifier, rc_word_t *word)
{
    rc_rc433x_device_t *disk = context;
    rc_word_t message[RC433X_MESSAGE_WORDS];
    uint32_t operation;
    rc_addr_t core_address;
    uint64_t first_sector;
    uint32_t sector_count;
    uint64_t transfer_halfwords;

    if (disk == NULL || disk->machine == NULL || word == NULL) {
        return RC_DEVICE_DISCONNECTED;
    }
    if (command == RC_IO_SENSE) {
        *word = disk->status;
        return RC_DEVICE_ACCEPTED;
    }
    if (command != RC_IO_CONTROL) {
        return RC_DEVICE_DISCONNECTED;
    }
    if (modifier != 0) {
        return RC_DEVICE_ACCEPTED;
    }

    disk->status = 0;
    if (!rc433x_read_message(disk, *word, message)) {
        disk->status = UINT32_C(1) << 23;
        *word = disk->status;
        return RC_DEVICE_ACCEPTED;
    }
    operation = message[0] >> 12;
    core_address = message[1] & ~UINT32_C(1);
    first_sector = message[3];
    sector_count = message[6] >> 12;
    transfer_halfwords = (uint64_t)sector_count * 512U;

    if ((uint64_t)core_address + transfer_halfwords >
        disk->machine->core.halfword_count) {
        disk->status = UINT32_C(1) << 23;
    } else if (operation == 3U) {
        ++disk->read_operations;
        if (!rc433x_read_data(disk, core_address, first_sector,
                              sector_count)) {
            disk->status = UINT32_C(1) << 22;
        }
    } else if (operation == 5U && disk->readonly) {
        ++disk->rejected_writes;
        disk->status = UINT32_C(1) << 22;
    } else if (operation == 5U) {
        ++disk->write_operations;
        if (!rc433x_write_data(disk, core_address, first_sector,
                               sector_count)) {
            if (disk->write_through) {
                ++disk->persistence_failures;
            }
            disk->status = UINT32_C(1) << 22;
        } else if (disk->persistent_overlay &&
                   !rc433x_save_overlay(disk, NULL, 0)) {
            ++disk->persistence_failures;
            disk->status = UINT32_C(1) << 22;
        }
    } else {
        disk->status = UINT32_C(1) << 23;
    }
    *word = disk->status;
    return RC_DEVICE_ACCEPTED;
}

bool rc_disk_read_core(rc_rc433x_device_t *disk, rc_addr_t address,
                       uint64_t sector, uint32_t count)
{
    if (disk == NULL || disk->machine == NULL ||
        (uint64_t)address + (uint64_t)count * 512U >
            disk->machine->core.halfword_count) {
        return false;
    }
    ++disk->read_operations;
    return rc433x_read_data(disk, address, sector, count);
}

bool rc_disk_write_core(rc_rc433x_device_t *disk, rc_addr_t address,
                        uint64_t sector, uint32_t count)
{
    if (disk == NULL || disk->machine == NULL || disk->readonly ||
        (uint64_t)address + (uint64_t)count * 512U >
            disk->machine->core.halfword_count) {
        return false;
    }
    ++disk->write_operations;
    return rc433x_write_data(disk, address, sector, count);
}

static int rc433x_device_init(rc_rc433x_device_t *disk,
                             struct rc_machine *machine,
                             const char *path, bool readonly,
                             bool write_through,
                             char *error, size_t error_size)
{
    size_t path_length;

    if (disk == NULL || machine == NULL) {
        return -1;
    }
    *disk = (rc_rc433x_device_t){0};
    path_length = strlen(path);
    if (path_length >= sizeof(disk->base_image_path)) {
        rc433x_set_error(error, error_size, "base image path is too long");
        return -1;
    }
    if ((write_through
             ? rc_image_open_writable(&disk->image, path, error, error_size)
             : rc_image_open_readonly(&disk->image, path, error, error_size))
        != 0) {
        return -1;
    }
    disk->device.context = disk;
    disk->device.io = rc433x_io;
    disk->machine = machine;
    disk->readonly = readonly;
    disk->write_through = write_through;
    (void)memcpy(disk->base_image_path, path, path_length + 1U);
    return 0;
}

int rc_rc433x_device_init_readonly(rc_rc433x_device_t *disk,
                                   struct rc_machine *machine,
                                   const char *path, char *error,
                                   size_t error_size)
{
    return rc433x_device_init(disk, machine, path, true, false,
                              error, error_size);
}

int rc_rc433x_device_init_cow(rc_rc433x_device_t *disk,
                              struct rc_machine *machine,
                              const char *path, char *error,
                              size_t error_size)
{
    return rc433x_device_init(disk, machine, path, false, false,
                              error, error_size);
}

int rc_rc433x_device_init_writable(rc_rc433x_device_t *disk,
                                   struct rc_machine *machine,
                                   const char *path, char *error,
                                   size_t error_size)
{
    return rc433x_device_init(disk, machine, path, false, true,
                              error, error_size);
}

int rc_rc433x_device_init_persistent(rc_rc433x_device_t *disk,
                                     struct rc_machine *machine,
                                     const char *path,
                                     const char *overlay_path,
                                     char *error, size_t error_size)
{
    rc_image_info_t base_info;
    struct stat base_status;
    struct stat overlay_status;

    if (disk == NULL || machine == NULL || path == NULL) {
        rc433x_set_error(error, error_size,
                         "invalid persistent disk arguments");
        return -1;
    }
    if (overlay_path == NULL || overlay_path[0] == '\0') {
        rc433x_set_error(error, error_size, "persistent overlay path is empty");
        return -1;
    }
    if (strlen(overlay_path) >= RC433X_OVERLAY_PATH_SIZE) {
        rc433x_set_error(error, error_size,
                         "persistent overlay path is too long");
        return -1;
    }
    if (strcmp(path, overlay_path) == 0 ||
        (stat(path, &base_status) == 0 &&
         stat(overlay_path, &overlay_status) == 0 &&
         base_status.st_dev == overlay_status.st_dev &&
         base_status.st_ino == overlay_status.st_ino)) {
        rc433x_set_error(error, error_size,
                         "overlay path refers to the base image");
        return -1;
    }
    if (rc_image_inspect(path, &base_info, error,
                         (unsigned long)error_size) != 0) {
        return -1;
    }
    if (base_info.trailing_bytes != 0) {
        rc433x_set_error(error, error_size,
                         "base image has a partial sector");
        return -1;
    }
    if (rc433x_device_init(disk, machine, path, false, false,
                           error, error_size) != 0) {
        return -1;
    }
    disk->base_image_bytes = base_info.byte_size;
    disk->base_image_hash = base_info.checksum_fnv1a;
    (void)strcpy(disk->overlay_path, overlay_path);
    disk->persistent_overlay = true;
    if (!rc433x_load_overlay(disk, error, error_size)) {
        rc_rc433x_device_destroy(disk);
        return -1;
    }
    return 0;
}

int rc_rc433x_device_flush(rc_rc433x_device_t *disk, char *error,
                           size_t error_size)
{
    if (disk == NULL || disk->machine == NULL) {
        rc433x_set_error(error, error_size, "RC433X device is not attached");
        return -1;
    }
    return rc433x_save_overlay(disk, error, error_size) ? 0 : -1;
}

void rc_rc433x_device_destroy(rc_rc433x_device_t *disk)
{
    if (disk != NULL) {
        rc433x_clear_overlay(disk);
        rc_image_close(&disk->image);
        disk->machine = NULL;
    }
}
