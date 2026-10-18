#define _POSIX_C_SOURCE 200809L

#include "sim4000/device.h"
#include "sim4000/machine.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
    HOST_INTERVENTION = 1U << 23,
    HOST_END = 1U << 18,
    HOST_FAULT = 1U << 14,
    HOST_BUFFER_SIZE = 6144,
    HOST_NAME_SIZE = 81
};

/* Resolve each component beneath a pinned directory, never through symlinks.
 * Exclusive output creation also prevents truncation and hard-link writes. */
static int open_relative(int root, const char *name, bool output)
{
    char path[HOST_NAME_SIZE + 1];
    char *component;
    int directory;
    int result = -1;

    if (name[0] == '\0' || name[0] == '/' || strchr(name, '\\') != NULL ||
        strchr(name, ':') != NULL) {
        return -1;
    }
    (void)strcpy(path, name);
    directory = dup(root);
    if (directory < 0) {
        return -1;
    }
    component = path;
    for (;;) {
        char *slash = strchr(component, '/');
        int next;
        if (slash != NULL) {
            *slash = '\0';
        }
        if (component[0] == '\0' || strcmp(component, ".") == 0 ||
            strcmp(component, "..") == 0) {
            break;
        }
        if (slash == NULL) {
            struct stat info;
            result = openat(directory, component,
                            O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK |
                            (output ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY),
                            0600);
            if (result >= 0 &&
                (fstat(result, &info) != 0 || !S_ISREG(info.st_mode))) {
                (void)close(result);
                result = -1;
            }
            break;
        }
        next = openat(directory, component,
                      O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0) {
            break;
        }
        (void)close(directory);
        directory = next;
        component = slash + 1;
    }
    (void)close(directory);
    return result;
}

static size_t transfer_bytes(const rc_hostfiles_device_t *host)
{
    size_t count = (size_t)(host->transfer_size / 2U) * 3U;
    return count > HOST_BUFFER_SIZE ? HOST_BUFFER_SIZE : count;
}

static bool valid_range(const rc_hostfiles_device_t *host, rc_addr_t address,
                         size_t count)
{
    size_t halfwords = ((count + 2U) / 3U) * 2U;
    return address <= host->machine->core.halfword_count &&
           halfwords <= host->machine->core.halfword_count - address;
}

static bool read_core(rc_hostfiles_device_t *host, rc_addr_t address,
                       uint8_t *bytes, size_t count)
{
    size_t i;
    if (!valid_range(host, address, count)) {
        return false;
    }
    for (i = 0; i < count; i += 3U) {
        rc_word_t word;
        if (!rc_machine_read_word(host->machine,
                                  address + (rc_addr_t)(i / 3U * 2U), &word)) {
            return false;
        }
        bytes[i] = (uint8_t)(word >> 16);
        bytes[i + 1U] = (uint8_t)(word >> 8);
        bytes[i + 2U] = (uint8_t)word;
    }
    return true;
}

static void close_file(rc_hostfiles_device_t *host)
{
    if (host->file != NULL) {
        if (fclose(host->file) != 0) {
            host->status = HOST_FAULT;
        }
        host->file = NULL;
    }
}

static void open_file(rc_hostfiles_device_t *host, rc_addr_t address,
                       bool output)
{
    uint8_t name[HOST_NAME_SIZE + 1];
    size_t count = transfer_bytes(host);
    int fd;
    close_file(host);
    if (count == 0 || count > HOST_NAME_SIZE ||
        !read_core(host, address, name, count)) {
        host->status = HOST_FAULT;
        return;
    }
    name[count] = 0;
    fd = open_relative(host->root_fd, (const char *)name, output);
    host->characters = (rc_word_t)count;
    if (fd < 0) {
        host->status = HOST_INTERVENTION;
        return;
    }
    host->file = fdopen(fd, output ? "wb" : "rb");
    if (host->file == NULL) {
        (void)close(fd);
        host->status = HOST_FAULT;
        return;
    }
    host->output_mode = output;
    host->text_mode = false;
    host->end_marker_sent = false;
}

static void input_data(rc_hostfiles_device_t *host, rc_addr_t address)
{
    uint8_t bytes[HOST_BUFFER_SIZE];
    size_t count = 0;
    size_t maximum = transfer_bytes(host);
    size_t i;
    if (host->file == NULL || host->output_mode) {
        host->status = HOST_INTERVENTION;
        return;
    }
    if (!valid_range(host, address, maximum)) {
        host->status = HOST_FAULT;
        return;
    }
    while (count < maximum) {
        int c = host->end_marker_sent ? EOF : fgetc(host->file);
        if (c == EOF) {
            if (ferror(host->file)) {
                host->status = HOST_FAULT;
            } else if (host->text_mode && !host->end_marker_sent) {
                bytes[count++] = 25;
                host->end_marker_sent = true;
            } else if (count == 0) {
                host->status = HOST_END;
            }
            break;
        }
        if (host->text_mode && (c == 0 || c == 13)) {
            continue;
        }
        bytes[count++] = (uint8_t)c;
    }
    while (count % 3U != 0) {
        bytes[count++] = 0;
    }
    for (i = 0; i < count; i += 3U) {
        rc_word_t word = ((rc_word_t)bytes[i] << 16) |
                         ((rc_word_t)bytes[i + 1U] << 8) | bytes[i + 2U];
        if (!rc_machine_write_word(host->machine,
                                   address + (rc_addr_t)(i / 3U * 2U), word)) {
            host->status = HOST_FAULT;
            break;
        }
    }
    host->characters = (rc_word_t)count;
}

static void output_data(rc_hostfiles_device_t *host, rc_addr_t address)
{
    uint8_t bytes[HOST_BUFFER_SIZE];
    size_t maximum = transfer_bytes(host);
    size_t i;
    if (host->file == NULL || !host->output_mode) {
        host->status = HOST_INTERVENTION;
        return;
    }
    if (!read_core(host, address, bytes, maximum)) {
        host->status = HOST_FAULT;
        return;
    }
    for (i = 0; i < maximum; ++i) {
        uint8_t c = bytes[i];
        if (host->text_mode) {
            if (host->end_marker_sent) {
                continue;
            }
            if (c == 25) {
                host->end_marker_sent = true;
                ++i;
                break;
            }
            if (c == 0 || c == 13) {
                continue;
            }
        }
        if (fputc(c, host->file) == EOF) {
            host->status = HOST_FAULT;
            break;
        }
    }
    if (fflush(host->file) != 0) {
        host->status = HOST_FAULT;
    }
    host->characters = (rc_word_t)i;
}

static uint32_t hostfiles_poll(void *context)
{
    rc_hostfiles_device_t *host = context;
    uint32_t pending = host->pending_interrupts;
    host->pending_interrupts = 0;
    return pending;
}

static rc_device_result_t hostfiles_io(void *context, rc_io_command_t command,
                                       uint8_t modifier, rc_word_t *word)
{
    rc_hostfiles_device_t *host = context;
    rc_addr_t address;
    if (host == NULL || word == NULL) {
        return RC_DEVICE_DISCONNECTED;
    }
    if (command == RC_IO_SENSE) {
        if (modifier == 0) {
            *word = host->status;
        } else if (modifier == 1) {
            *word = host->characters;
        } else {
            return RC_DEVICE_DISCONNECTED;
        }
        return RC_DEVICE_ACCEPTED;
    }
    if (command != RC_IO_CONTROL) {
        return RC_DEVICE_DISCONNECTED;
    }
    if (modifier == 1) {
        host->text_mode = (*word & 7U) != 0;
        return RC_DEVICE_ACCEPTED;
    }
    if (modifier == 2) {
        host->transfer_size = rc_word(*word);
        return RC_DEVICE_ACCEPTED;
    }
    address = rc_word(*word) & ~UINT32_C(1);
    host->status = 0;
    host->characters = 0;
    switch (modifier) {
    case 3: input_data(host, address); break;
    case 4: output_data(host, address); break;
    case 11: open_file(host, address, false); break;
    case 12: open_file(host, address, true); break;
    case 13: close_file(host); break;
    default: return RC_DEVICE_DISCONNECTED;
    }
    host->pending_interrupts |= UINT32_C(1) << (23U - host->interrupt_level);
    return RC_DEVICE_ACCEPTED;
}

int rc_hostfiles_device_init(rc_hostfiles_device_t *host,
                             struct rc_machine *machine, const char *root,
                             unsigned interrupt_level, char *error,
                             size_t error_size)
{
    if (host == NULL || machine == NULL || root == NULL ||
        interrupt_level >= 24U) {
        return -1;
    }
    memset(host, 0, sizeof(*host));
    host->root_fd = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (host->root_fd < 0) {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "cannot open HOSTFILES root %s: %s",
                           root, strerror(errno));
        }
        return -1;
    }
    host->machine = machine;
    host->interrupt_level = interrupt_level;
    host->device = (rc_device_t){host, hostfiles_io, NULL, hostfiles_poll};
    return 0;
}

void rc_hostfiles_device_destroy(rc_hostfiles_device_t *host)
{
    if (host != NULL) {
        close_file(host);
        if (host->root_fd >= 0) {
            (void)close(host->root_fd);
            host->root_fd = -1;
        }
    }
}
