#ifndef SIM4000_IMAGE_H
#define SIM4000_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    RC433X_SECTOR_BYTES = 768,
    RC433X_BLANK_SECTORS = 180
};

typedef struct {
    uint64_t byte_size;
    uint64_t complete_sectors;
    uint32_t trailing_bytes;
    uint64_t checksum_fnv1a;
} rc_image_info_t;

typedef struct {
    void *file;
    uint64_t sector_count;
    bool writable;
} rc_image_t;

int rc_image_inspect(const char *path, rc_image_info_t *info,
                     char *error, unsigned long error_size);
/* Create an unlabelled, zero-filled starter image; never replace any path. */
int rc_image_create_blank(const char *path, char *error, size_t error_size);
int rc_image_open_readonly(rc_image_t *image, const char *path,
                           char *error, size_t error_size);
int rc_image_open_writable(rc_image_t *image, const char *path,
                           char *error, size_t error_size);
void rc_image_close(rc_image_t *image);
bool rc_image_read_sector(rc_image_t *image, uint64_t sector,
                          uint8_t data[RC433X_SECTOR_BYTES]);
bool rc_image_write_sector(rc_image_t *image, uint64_t sector,
                           const uint8_t data[RC433X_SECTOR_BYTES]);

#endif
