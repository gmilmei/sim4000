#include "sim4000/image.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *error, unsigned long error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

int rc_image_create_blank(const char *path, char *error, size_t error_size)
{
    const unsigned char track[9 * RC433X_SECTOR_BYTES] = {0};
    FILE *file;
    bool failed = false;
    int saved_error = 0;

    if (path == NULL || path[0] == '\0') {
        set_error(error, (unsigned long)error_size, "missing disk image filename");
        return -1;
    }
    /* C11 exclusive creation also refuses existing and dangling symlinks. */
    file = fopen(path, "wbx");
    if (file == NULL) {
        if (error != NULL && error_size != 0) {
            (void)snprintf(error, error_size, "cannot create %s: %s", path,
                           strerror(errno));
        }
        return -1;
    }
    for (unsigned i = 0; i < RC433X_BLANK_SECTORS / 9; ++i) {
        if (fwrite(track, 1, sizeof(track), file) != sizeof(track)) {
            failed = true;
            saved_error = errno != 0 ? errno : EIO;
            break;
        }
    }
    if (fclose(file) != 0 && !failed) {
        failed = true;
        saved_error = errno != 0 ? errno : EIO;
    }
    if (failed) {
        bool removed = remove(path) == 0;
        if (error != NULL && error_size != 0) {
            (void)snprintf(error, error_size, "cannot write %s: %s%s", path,
                           strerror(saved_error), removed ? "" : "; partial file remains");
        }
        return -1;
    }
    return 0;
}

int rc_image_inspect(const char *path, rc_image_info_t *info,
                     char *error, unsigned long error_size)
{
    unsigned char buffer[16384];
    uint64_t size = 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    FILE *file;
    size_t count;

    if (path == NULL || info == NULL) {
        set_error(error, error_size, "invalid image inspection arguments");
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
            set_error(error, error_size, "image is too large");
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
        set_error(error, error_size, "cannot close image file");
        return -1;
    }

    info->byte_size = size;
    info->complete_sectors = size / RC433X_SECTOR_BYTES;
    info->trailing_bytes = (uint32_t)(size % RC433X_SECTOR_BYTES);
    info->checksum_fnv1a = hash;
    return 0;
}

static int open_image(rc_image_t *image, const char *path, bool writable,
                      char *error, size_t error_size)
{
    FILE *file;
    long size;

    if (image == NULL || path == NULL) {
        set_error(error, (unsigned long)error_size,
                  "invalid image open arguments");
        return -1;
    }
    *image = (rc_image_t){0};
    file = fopen(path, writable ? "r+b" : "rb");
    if (file == NULL) {
        char message[256];
        (void)snprintf(message, sizeof(message), "cannot open %s: %s", path,
                       strerror(errno));
        set_error(error, (unsigned long)error_size, message);
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        set_error(error, (unsigned long)error_size,
                  "cannot determine image size");
        (void)fclose(file);
        return -1;
    }
    if ((unsigned long)size % RC433X_SECTOR_BYTES != 0) {
        set_error(error, (unsigned long)error_size,
                  "image size is not a multiple of 768 bytes");
        (void)fclose(file);
        return -1;
    }
    image->file = file;
    image->sector_count = (uint64_t)(unsigned long)size /
                          RC433X_SECTOR_BYTES;
    image->writable = writable;
    return 0;
}

int rc_image_open_readonly(rc_image_t *image, const char *path,
                           char *error, size_t error_size)
{
    return open_image(image, path, false, error, error_size);
}

int rc_image_open_writable(rc_image_t *image, const char *path,
                           char *error, size_t error_size)
{
    return open_image(image, path, true, error, error_size);
}

void rc_image_close(rc_image_t *image)
{
    if (image != NULL && image->file != NULL) {
        (void)fclose((FILE *)image->file);
        image->file = NULL;
        image->sector_count = 0;
        image->writable = false;
    }
}

bool rc_image_write_sector(rc_image_t *image, uint64_t sector,
                           const uint8_t data[RC433X_SECTOR_BYTES])
{
    uint64_t offset;
    FILE *file;

    if (image == NULL || image->file == NULL || !image->writable ||
        data == NULL || sector > UINT64_MAX / RC433X_SECTOR_BYTES) {
        return false;
    }
    offset = sector * RC433X_SECTOR_BYTES;
    if (offset > (uint64_t)LONG_MAX) {
        return false;
    }
    file = image->file;
    if (fseek(file, (long)offset, SEEK_SET) != 0 ||
        fwrite(data, 1, RC433X_SECTOR_BYTES, file) != RC433X_SECTOR_BYTES ||
        fflush(file) != 0) {
        return false;
    }
    if (sector >= image->sector_count) {
        image->sector_count = sector + 1U;
    }
    return true;
}

bool rc_image_read_sector(rc_image_t *image, uint64_t sector,
                          uint8_t data[RC433X_SECTOR_BYTES])
{
    uint64_t offset;
    FILE *file;

    if (image == NULL || image->file == NULL || data == NULL ||
        sector >= image->sector_count) {
        return false;
    }
    offset = sector * RC433X_SECTOR_BYTES;
    if (offset > (uint64_t)LONG_MAX) {
        return false;
    }
    file = image->file;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(data, 1, RC433X_SECTOR_BYTES, file) ==
               RC433X_SECTOR_BYTES;
}
