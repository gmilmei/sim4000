#define _POSIX_C_SOURCE 200809L

#include "sim4000/rc749.h"
#include "sim4000/machine.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Protocol reference: Henrik Ascanius Jacobsen, V_1.2_DB CTRL/RC749.PAS.
 * Upstream copyright/permission: doc/impl/V_1.2_DB_LICENSE.txt.
 * Read-only tagged TAP subset used by Jacobsen's V_1.2_DB RC749:
 * little-endian tags, odd-byte padding, matching trailers, tape marks,
 * erase-gap markers, EOM and the record-error flag. No host x86 assumptions. */
struct rc749_record {
    off_t offset;
    uint32_t tag;
};

static int fail(char *error, size_t size, const char *message)
{
    if (error != NULL && size != 0) (void)snprintf(error, size, "%s", message);
    return -1;
}

static bool tag_at(FILE *file, off_t offset, uint32_t *tag)
{
    unsigned char bytes[4];
    if (fseeko(file, offset, SEEK_SET) != 0 || fread(bytes, 1, 4, file) != 4)
        return false;
    *tag = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    return true;
}

static int mount_fd(rc_rc749_device_t *tape, int fd, const char *name,
                     char *error, size_t error_size)
{
    struct stat info;
    FILE *file;
    struct rc749_record *records = NULL;
    size_t count = 0, capacity = 0;
    off_t offset = 0;
    bool valid = true;
    if (strlen(name) >= sizeof(tape->path) || fstat(fd, &info) != 0 ||
        !S_ISREG(info.st_mode) || info.st_size < 0) {
        (void)close(fd);
        return fail(error, error_size, "RC749 requires a regular tape file");
    }
    file = fdopen(fd, "rb");
    if (file == NULL) {
        (void)close(fd);
        return fail(error, error_size, "cannot open RC749 stream");
    }
    (void)setvbuf(file, NULL, _IONBF, 0);
    while (offset < info.st_size) {
        uint32_t tag, trailer;
        off_t next;
        if (info.st_size - offset < 4 || !tag_at(file, offset, &tag)) {
            valid = false; break;
        }
        if (tag == UINT32_C(0xfffffffe)) { offset += 4; continue; }
        if (tag == UINT32_C(0xffffffff)) {
            valid = info.st_size - offset == 4;
            break;
        }
        next = offset + 4;
        if (tag != 0) {
            uint32_t length = tag & UINT32_C(0x00ffffff);
            off_t payload = (off_t)length + (off_t)(length & 1U);
            if (length == 0 || (tag & UINT32_C(0x7f000000)) != 0 ||
                info.st_size - next < payload + 4 ||
                !tag_at(file, next + payload, &trailer) || trailer != tag) {
                valid = false; break;
            }
            next += payload + 4;
        }
        if (count == capacity) {
            size_t grown = capacity == 0 ? 64 : capacity * 2;
            struct rc749_record *allocation;
            if (grown < capacity || grown > SIZE_MAX / sizeof(*records)) {
                valid = false; break;
            }
            allocation = realloc(records, grown * sizeof(*records));
            if (allocation == NULL) { valid = false; break; }
            records = allocation;
            capacity = grown;
        }
        records[count++] = (struct rc749_record){offset, tag};
        offset = next;
    }
    if (!valid) {
        free(records);
        (void)fclose(file);
        return fail(error, error_size, "invalid or unreadable RC749 TAP records");
    }
    /* A failed replacement must not destroy a previously mounted tape. */
    rc_rc749_unmount(tape);
    tape->file = file;
    tape->records = records;
    tape->record_count = count;
    tape->status = RC749_HIGH_DENSITY | RC749_BOT;
    (void)strcpy(tape->path, name);
    return 0;
}

int rc_rc749_mount(rc_rc749_device_t *tape, const char *path,
                   char *error, size_t error_size)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return fail(error, error_size, "cannot open RC749 tape read-only");
    return mount_fd(tape, fd, path, error, error_size);
}

int rc_rc749_set_root(rc_rc749_device_t *tape, const char *path,
                      char *error, size_t error_size)
{
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return fail(error, error_size, "cannot open RC749 guest mount root");
    if (tape->root_fd >= 0) (void)close(tape->root_fd);
    tape->root_fd = fd;
    return 0;
}

void rc_rc749_unmount(rc_rc749_device_t *tape)
{
    if (tape->file != NULL) (void)fclose(tape->file);
    free(tape->records);
    tape->file = NULL;
    tape->records = NULL;
    tape->record_count = tape->position = 0;
    tape->transferred_bytes = 0;
    tape->fault = false;
    tape->status = RC749_HIGH_DENSITY | RC749_INTERVENTION;
    tape->path[0] = '\0';
}

void rc_rc749_destroy(rc_rc749_device_t *tape)
{
    rc_rc749_unmount(tape);
    if (tape->root_fd >= 0) (void)close(tape->root_fd);
    tape->root_fd = -1;
}

int rc_rc749_rewind(rc_rc749_device_t *tape, char *error, size_t error_size)
{
    if (tape->file == NULL) return fail(error, error_size, "no RC749 tape mounted");
    tape->position = 0;
    tape->transferred_bytes = 0;
    tape->fault = false;
    clearerr(tape->file);
    tape->status = RC749_HIGH_DENSITY | RC749_BOT;
    return 0;
}

static void complete(rc_rc749_device_t *tape)
{
    if (tape->interrupt_level < 24)
        tape->pending_interrupts |= UINT32_C(1) << (23U - tape->interrupt_level);
}

static void fault(rc_rc749_device_t *tape, rc_word_t status)
{
    tape->status |= status;
    tape->fault = true;
    ++tape->errors;
}

static void move_tape(rc_rc749_device_t *tape, rc_word_t operation)
{
    if (operation == 4) {
        (void)rc_rc749_rewind(tape, NULL, 0);
    } else if (operation == 5) {
        rc_rc749_unmount(tape);
    } else if (operation <= 3) {
        bool backwards = operation >= 2;
        bool file = (operation & 1U) == 0;
        do {
            struct rc749_record record;
            if (backwards) {
                if (tape->position == 0) { tape->status |= RC749_BOT; break; }
                record = tape->records[--tape->position];
            } else {
                if (tape->position == tape->record_count) {
                    tape->status |= RC749_EOT; break;
                }
                record = tape->records[tape->position++];
            }
            if (record.tag == 0) { tape->status |= RC749_TAPE_MARK; break; }
            if (backwards && tape->position == 0) tape->status |= RC749_BOT;
            if (!file) break;
        } while (true);
    } else {
        tape->status |= RC749_INTERVENTION;
        ++tape->errors;
    }
}

static void input_record(rc_rc749_device_t *tape, rc_addr_t address)
{
    unsigned char bytes[RC749_MAX_RECORD_BYTES + 2] = {0};
    struct rc749_record record;
    uint32_t length, tag, trailer;
    size_t capacity, count, words;
    off_t padded;
    ++tape->reads;
    tape->transferred_bytes = 0;
    if (tape->position == tape->record_count) { tape->status |= RC749_EOT; return; }
    record = tape->records[tape->position];
    length = record.tag & UINT32_C(0x00ffffff);
    padded = (off_t)length + (off_t)(length & 1U);
    /* Revalidate tags before use in case the host changed a mounted file. */
    if (!tag_at(tape->file, record.offset, &tag) || tag != record.tag ||
        (tag != 0 && (!tag_at(tape->file, record.offset + 4 + padded, &trailer) ||
                      trailer != tag))) {
        fault(tape, RC749_INTERVENTION); return;
    }
    if (tag == 0) { ++tape->position; tape->status |= RC749_TAPE_MARK; return; }
    if (length > RC749_MAX_RECORD_BYTES) {
        ++tape->position; tape->status |= RC749_BLOCK_LENGTH; return;
    }
    if (address > tape->last_core || (address & 1U) != 0 ||
        tape->last_core >= tape->machine->core.halfword_count ||
        (tape->last_core & 1U) != 0) {
        fault(tape, RC749_OVERRUN); return;
    }
    capacity = ((size_t)(tape->last_core - address) / 2U + 1U) * 3U;
    count = length < capacity ? length : capacity;
    words = (count + 2U) / 3U;
    if (address >= tape->machine->core.halfword_count ||
        words * 2U > tape->machine->core.halfword_count - address) {
        fault(tape, RC749_OVERRUN); return;
    }
    if (fseeko(tape->file, record.offset + 4, SEEK_SET) != 0 ||
        fread(bytes, 1, length, tape->file) != length) {
        fault(tape, RC749_INTERVENTION); return;
    }
    /* Zero-pad only the transferred partial word, not discarded record bytes. */
    for (size_t i = count; i < words * 3U; ++i) bytes[i] = 0;
    for (size_t i = 0; i < words; ++i) {
        rc_word_t word = ((rc_word_t)bytes[i * 3U] << 16) |
                         ((rc_word_t)bytes[i * 3U + 1U] << 8) | bytes[i * 3U + 2U];
        if (!rc_machine_write_word(tape->machine, address + (rc_addr_t)(i * 2U), word)) {
            fault(tape, RC749_OVERRUN); return;
        }
    }
    ++tape->position;
    tape->transferred_bytes = count;
    tape->bytes_read += count;
    if (length > capacity) tape->status |= RC749_BLOCK_LENGTH;
    if ((tag & UINT32_C(0x80000000)) != 0) tape->status |= RC749_PARITY;
}

static int open_guest(rc_rc749_device_t *tape, char *path)
{
    int directory, result = -1;
    char *component = path;
    if (tape->root_fd < 0 || path[0] == '\0' || path[0] == '/' ||
        strchr(path, '\\') != NULL || strchr(path, ':') != NULL) return -1;
    directory = dup(tape->root_fd);
    if (directory < 0) return -1;
    for (;;) {
        char *slash = strchr(component, '/');
        int next;
        if (slash != NULL) *slash = '\0';
        if (component[0] == '\0' || strcmp(component, ".") == 0 ||
            strcmp(component, "..") == 0) break;
        if (slash == NULL) {
            result = openat(directory, component,
                            O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
            break;
        }
        next = openat(directory, component,
                      O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0) break;
        (void)close(directory);
        directory = next;
        component = slash + 1;
    }
    (void)close(directory);
    return result;
}

static void load_guest(rc_rc749_device_t *tape, rc_addr_t address)
{
    char name[82], path[82];
    size_t count, length = 0;
    int fd;
    tape->transferred_bytes = 0;
    if (tape->root_fd < 0 || address >= tape->last_core ||
        tape->last_core >= tape->machine->core.halfword_count) {
        tape->status |= RC749_INTERVENTION; ++tape->errors; return;
    }
    count = (size_t)(tape->last_core - address) / 2U * 3U;
    if (count == 0 || count > 81) { tape->status |= RC749_OVERRUN; return; }
    for (size_t i = 0; i < count; i += 3) {
        rc_word_t word;
        if (!rc_machine_read_word(tape->machine, address + 2U + (rc_addr_t)(i / 3U * 2U), &word)) {
            tape->status |= RC749_OVERRUN; return;
        }
        name[i] = (char)(word >> 16);
        name[i + 1] = (char)(word >> 8);
        name[i + 2] = (char)word;
    }
    while (length < count && name[length] != '\0') ++length;
    for (size_t i = 0; i < length; ++i) {
        if ((unsigned char)name[i] < 32 || (unsigned char)name[i] > 126) {
            tape->status |= RC749_INTERVENTION; return;
        }
    }
    for (size_t i = length; i < count; ++i) {
        if (name[i] != '\0') { tape->status |= RC749_INTERVENTION; return; }
    }
    name[length] = '\0';
    (void)strcpy(path, name);
    fd = open_guest(tape, path);
    if (fd < 0 || mount_fd(tape, fd, name, NULL, 0) != 0) {
        tape->status |= RC749_INTERVENTION; ++tape->errors; return;
    }
    /* DB LoadTape counts the reserved leading word in its completion length. */
    tape->transferred_bytes = count + 3;
}

static uint32_t poll(void *context)
{
    rc_rc749_device_t *tape = context;
    uint32_t pending = tape->pending_interrupts;
    tape->pending_interrupts = 0;
    return pending;
}

static rc_device_result_t io(void *context, rc_io_command_t command,
                             uint8_t modifier, rc_word_t *word)
{
    rc_rc749_device_t *tape = context;
    if (word == NULL) return RC_DEVICE_DISCONNECTED;
    if (command == RC_IO_SENSE) {
        if (modifier == 0) *word = tape->status;
        else if (modifier == 1) *word = (rc_word_t)tape->transferred_bytes;
        else return RC_DEVICE_DISCONNECTED;
        return RC_DEVICE_ACCEPTED;
    }
    tape->status = RC749_HIGH_DENSITY;
    if (command == RC_IO_WRITE || (command == RC_IO_CONTROL &&
        (modifier == 4 || modifier == 5))) {
        ++tape->rejected_writes;
        tape->status |= RC749_INTERVENTION;
    } else if (command != RC_IO_CONTROL) {
        tape->status |= RC749_INTERVENTION;
    } else if (modifier == 0) {
        /* Set trail is an accepted no-op in the DB implementation. */
        if (tape->file == NULL) tape->status |= RC749_INTERVENTION;
        return RC_DEVICE_ACCEPTED;
    } else if (modifier == 1) {
        tape->last_core = *word & UINT32_C(0x00fffffe);
        if (tape->file == NULL) tape->status |= RC749_INTERVENTION;
        return RC_DEVICE_ACCEPTED;
    } else if (modifier == 12) {
        load_guest(tape, *word & UINT32_C(0x00fffffe));
    } else if (tape->file == NULL ||
               (tape->fault && !(modifier == 2 && (*word == 4 || *word == 5)))) {
        tape->status |= RC749_INTERVENTION;
    } else if (modifier == 2) {
        move_tape(tape, *word);
    } else if (modifier == 3 || modifier == 11) {
        input_record(tape, *word & UINT32_C(0x00fffffe));
    } else {
        tape->status |= RC749_INTERVENTION;
    }
    if (tape->file == NULL) tape->status |= RC749_INTERVENTION;
    complete(tape);
    return RC_DEVICE_ACCEPTED;
}

void rc_rc749_init(rc_rc749_device_t *tape, struct rc_machine *machine,
                    unsigned interrupt_level)
{
    memset(tape, 0, sizeof(*tape));
    tape->machine = machine;
    tape->interrupt_level = interrupt_level;
    tape->root_fd = -1;
    tape->status = RC749_HIGH_DENSITY | RC749_INTERVENTION;
    tape->device = (rc_device_t){tape, io, NULL, poll};
}
