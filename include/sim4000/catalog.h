#ifndef SIM4000_CATALOG_H
#define SIM4000_CATALOG_H

#include <stddef.h>
#include <stdint.h>

enum {
    RC_CATALOG_NAME_BYTES = 12,
    RC_CATALOG_ENTRIES_PER_SEGMENT = 15
};

typedef struct {
    uint32_t catalog_segment;
    uint16_t slot;
    uint16_t first_slice;
    uint16_t name_key;
    uint8_t permission_key;
    uint32_t lower_base;
    uint32_t upper_base;
    char name[RC_CATALOG_NAME_BYTES + 1];
    int32_t size_segments;
    char document[RC_CATALOG_NAME_BYTES + 1];
    uint32_t short_clock;
    uint32_t file;
    uint32_t block;
    uint16_t contents;
    uint16_t entry;
    uint32_t final_tail_word;
} rc_catalog_entry_t;

typedef struct {
    char name[RC_CATALOG_NAME_BYTES + 1];
    char document[RC_CATALOG_NAME_BYTES + 1];
    uint32_t segment_count;
    uint32_t slice_length;
    uint16_t first_slice;
    uint16_t last_slice;
    uint16_t first_chain_slice;
    uint16_t chain_state;
    rc_catalog_entry_t *entries;
    size_t entry_count;
} rc_catalog_t;

int rc_catalog_read(const char *path, rc_catalog_t *catalog,
                    char *error, size_t error_size);
/* Read the monitor's main catalog area, located through the device catalog.
 * The supported fixture names it "catalog" on the same document. */
int rc_catalog_read_main(const char *path, rc_catalog_t *catalog,
                         char *error, size_t error_size);
void rc_catalog_destroy(rc_catalog_t *catalog);
const rc_catalog_entry_t *rc_catalog_find(const rc_catalog_t *catalog,
                                          const char *name);

#endif
