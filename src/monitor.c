#include "sim4000/monitor.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

int rc_monitor_inspect(const char *path, rc_monitor_info_t *info,
                       char *error, size_t error_size)
{
    unsigned char buffer[16384];
    uint64_t size = 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    FILE *file;
    size_t count;

    if (path == NULL || info == NULL) {
        set_error(error, error_size, "invalid monitor inspection arguments");
        return -1;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        char message[256];
        (void)snprintf(message, sizeof(message), "cannot open %s: %s", path,
                       strerror(errno));
        set_error(error, error_size, message);
        return -1;
    }
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        size_t i;
        if (UINT64_MAX - size < count) {
            set_error(error, error_size, "monitor file is too large");
            (void)fclose(file);
            return -1;
        }
        size += count;
        for (i = 0; i < count; ++i) {
            hash ^= buffer[i];
            hash *= UINT64_C(1099511628211);
        }
    }
    if (ferror(file) != 0) {
        char message[256];
        (void)snprintf(message, sizeof(message), "cannot read %s: %s", path,
                       strerror(errno));
        set_error(error, error_size, message);
        (void)fclose(file);
        return -1;
    }
    if (fclose(file) != 0) {
        set_error(error, error_size, "cannot close monitor file");
        return -1;
    }

    info->byte_size = size;
    info->word_count = size / 3;
    info->trailing_bytes = (uint32_t)(size % 3);
    info->checksum_fnv1a = hash;
    return 0;
}

int rc_monitor_load(const char *path, rc_core_t *core,
                    bool set_disc_load_flag,
                    rc_addr_t *load_address, rc_addr_t *entry_address,
                    char *error, size_t error_size)
{
    rc_monitor_info_t info;
    rc_addr_t address;
    FILE *file;
    uint64_t word_index;

    if (core == NULL || core->data == NULL || load_address == NULL ||
        entry_address == NULL) {
        set_error(error, error_size, "invalid monitor load arguments");
        return -1;
    }
    if (rc_monitor_inspect(path, &info, error, error_size) != 0) {
        return -1;
    }
    if (info.trailing_bytes != 0) {
        set_error(error, error_size,
                  "monitor size is not a multiple of three bytes");
        return -1;
    }
    if (core->halfword_count > UINT32_MAX) {
        set_error(error, error_size, "core address range is too large");
        return -1;
    }

    address = (rc_addr_t)(core->halfword_count / 2);
    if (info.word_count > (core->halfword_count - (size_t)address) / 2) {
        set_error(error, error_size, "monitor does not fit in core store");
        return -1;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        char message[256];
        (void)snprintf(message, sizeof(message), "cannot open %s: %s", path,
                       strerror(errno));
        set_error(error, error_size, message);
        return -1;
    }
    for (word_index = 0; word_index < info.word_count; ++word_index) {
        unsigned char bytes[3];
        rc_word_t word;
        rc_addr_t target = address + (rc_addr_t)(word_index * 2);

        if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) {
            set_error(error, error_size, "short read while loading monitor");
            (void)fclose(file);
            return -1;
        }
        word = ((rc_word_t)bytes[0] << 16) |
               ((rc_word_t)bytes[1] << 8) |
               (rc_word_t)bytes[2];
        if (!rc_core_write_word(core, target, word)) {
            set_error(error, error_size, "core write failed while loading monitor");
            (void)fclose(file);
            return -1;
        }
    }
    if (fclose(file) != 0) {
        set_error(error, error_size, "cannot close monitor file");
        return -1;
    }

    if (set_disc_load_flag &&
        !rc_core_write_word(core, address + 2, UINT32_C(2))) {
        set_error(error, error_size, "cannot set monitor disc load flag");
        return -1;
    }
    if (!rc_core_write_word(core, 14, address + 6)) {
        set_error(error, error_size, "cannot set monitor start entry");
        return -1;
    }

    *load_address = address;
    *entry_address = address + 6;
    return 0;
}
