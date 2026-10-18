#define _POSIX_C_SOURCE 200809L

#include "sim4000/core_image.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

static void set_path_error(char *error, size_t error_size,
                           const char *operation, const char *path)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s %s: %s", operation, path,
                       strerror(errno));
    }
}

static bool range_fits(const rc_machine_t *machine, rc_addr_t start_address,
                       size_t word_count)
{
    size_t start = (size_t)start_address;

    return machine != NULL && machine->core.data != NULL &&
           (start_address & UINT32_C(1)) == 0 &&
           start <= machine->core.halfword_count &&
           word_count <= (machine->core.halfword_count - start) / 2;
}

int rc_core_image_load(const char *path, rc_machine_t *machine,
                       rc_addr_t start_address, size_t *word_count,
                       char *error, size_t error_size)
{
    FILE *file;
    long byte_size;
    size_t count;
    rc_word_t *words;
    size_t index;

    if (path == NULL || machine == NULL || word_count == NULL ||
        (start_address & UINT32_C(1)) != 0) {
        set_error(error, error_size, "invalid core-image load arguments");
        return -1;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        set_path_error(error, error_size, "cannot open", path);
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (byte_size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        set_path_error(error, error_size, "cannot inspect", path);
        (void)fclose(file);
        return -1;
    }
    if (byte_size == 0 || byte_size % 3 != 0) {
        set_error(error, error_size,
                  byte_size == 0 ? "core image is empty" :
                  "core-image size is not a multiple of three bytes");
        (void)fclose(file);
        return -1;
    }
    count = (size_t)byte_size / 3;
    if (!range_fits(machine, start_address, count)) {
        set_error(error, error_size, "core image does not fit at that address");
        (void)fclose(file);
        return -1;
    }
    if (count > SIZE_MAX / sizeof(*words) ||
        (words = malloc(count * sizeof(*words))) == NULL) {
        set_error(error, error_size, "cannot allocate core-image buffer");
        (void)fclose(file);
        return -1;
    }
    for (index = 0; index < count; ++index) {
        unsigned char bytes[3];

        if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) {
            set_path_error(error, error_size, "cannot read", path);
            free(words);
            (void)fclose(file);
            return -1;
        }
        words[index] = ((rc_word_t)bytes[0] << 16) |
                       ((rc_word_t)bytes[1] << 8) |
                       (rc_word_t)bytes[2];
    }
    if (fclose(file) != 0) {
        set_path_error(error, error_size, "cannot close", path);
        free(words);
        return -1;
    }
    for (index = 0; index < count; ++index) {
        if (!rc_machine_write_word(machine,
                                   start_address + (rc_addr_t)(index * 2),
                                   words[index])) {
            set_error(error, error_size, "core write failed unexpectedly");
            free(words);
            return -1;
        }
    }
    free(words);
    *word_count = count;
    return 0;
}

int rc_core_image_save(const char *path, const rc_machine_t *machine,
                       rc_addr_t start_address, size_t word_count,
                       char *error, size_t error_size)
{
    char *temporary_path;
    size_t path_length;
    int descriptor;
    FILE *file;
    size_t index;
    bool failed = false;

    if (path == NULL || word_count == 0 ||
        !range_fits(machine, start_address, word_count)) {
        set_error(error, error_size, "invalid core-image save range");
        return -1;
    }
    path_length = strlen(path);
    if (path_length > SIZE_MAX - sizeof(".tmp.XXXXXX")) {
        set_error(error, error_size, "core-image path is too long");
        return -1;
    }
    temporary_path = malloc(path_length + sizeof(".tmp.XXXXXX"));
    if (temporary_path == NULL) {
        set_error(error, error_size, "cannot allocate temporary path");
        return -1;
    }
    (void)snprintf(temporary_path, path_length + sizeof(".tmp.XXXXXX"),
                   "%s.tmp.XXXXXX", path);
    descriptor = mkstemp(temporary_path);
    if (descriptor < 0) {
        set_path_error(error, error_size, "cannot create", temporary_path);
        free(temporary_path);
        return -1;
    }
    file = fdopen(descriptor, "wb");
    if (file == NULL) {
        set_path_error(error, error_size, "cannot open", temporary_path);
        (void)close(descriptor);
        (void)unlink(temporary_path);
        free(temporary_path);
        return -1;
    }
    for (index = 0; index < word_count; ++index) {
        rc_word_t word;
        unsigned char bytes[3];

        if (!rc_machine_read_word(machine,
                                  start_address + (rc_addr_t)(index * 2),
                                  &word)) {
            set_error(error, error_size, "core read failed unexpectedly");
            failed = true;
            break;
        }
        bytes[0] = (unsigned char)(word >> 16);
        bytes[1] = (unsigned char)(word >> 8);
        bytes[2] = (unsigned char)word;
        if (fwrite(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) {
            set_path_error(error, error_size, "cannot write", temporary_path);
            failed = true;
            break;
        }
    }
    if (!failed && (fflush(file) != 0 || fsync(descriptor) != 0)) {
        set_path_error(error, error_size, "cannot flush", temporary_path);
        failed = true;
    }
    if (fclose(file) != 0 && !failed) {
        set_path_error(error, error_size, "cannot close", temporary_path);
        failed = true;
    }
    if (!failed && rename(temporary_path, path) != 0) {
        set_path_error(error, error_size, "cannot replace", path);
        failed = true;
    }
    if (failed) {
        (void)unlink(temporary_path);
    }
    free(temporary_path);
    return failed ? -1 : 0;
}
