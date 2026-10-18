#include "sim4000/catalog.h"

#include "sim4000/image.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    RC_WORDS_PER_SECTOR = 256,
    RC_HALFWORDS_PER_SECTOR = 512,
    RC_CATALOG_ENTRY_WORDS = 17,
    RC_CHAIN_HEADER_HALFWORDS = 34,
    RC_MAX_SLICES = 4096
};

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size != 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

static void set_numbered_error(char *error, size_t error_size,
                               const char *format, uint64_t value)
{
    if (error != NULL && error_size != 0) {
        (void)snprintf(error, error_size, format,
                       (unsigned long long)value);
    }
}

static uint32_t unpack_word(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16);
}

static bool read_sector_words(rc_image_t *image, uint64_t sector,
                              uint32_t words[RC_WORDS_PER_SECTOR])
{
    uint8_t bytes[RC433X_SECTOR_BYTES];
    size_t index;

    if (!rc_image_read_sector(image, sector, bytes)) {
        return false;
    }
    for (index = 0; index < RC_WORDS_PER_SECTOR; ++index) {
        words[index] = unpack_word(bytes + index * 3U);
    }
    return true;
}

static void decode_name(const uint32_t *words, char name[RC_CATALOG_NAME_BYTES + 1])
{
    size_t output = 0;
    size_t word_index;

    for (word_index = 0; word_index < 4; ++word_index) {
        int shift;

        for (shift = 16; shift >= 0; shift -= 8) {
            uint8_t character = (uint8_t)(words[word_index] >> shift);

            if (character == 0) {
                name[output] = '\0';
                return;
            }
            name[output++] = character >= 0x20U && character <= 0x7eU
                                 ? (char)character : '?';
        }
    }
    name[output] = '\0';
}

static int32_t signed_word(uint32_t word)
{
    return (word & UINT32_C(0x800000)) != 0
               ? (int32_t)(word | UINT32_C(0xff000000))
               : (int32_t)word;
}

static int signed_halfword(uint16_t halfword)
{
    return (halfword & UINT16_C(0x0800)) != 0
               ? (int)halfword - 4096 : (int)halfword;
}

static bool append_chain_slice(rc_image_t *image, uint16_t slice,
                               uint32_t slice_length, uint16_t *halfwords,
                               size_t halfword_count, size_t *copied,
                               char *error, size_t error_size)
{
    uint64_t first_sector = (uint64_t)slice * slice_length;
    uint32_t segment;

    for (segment = 0; segment < slice_length && *copied < halfword_count;
         ++segment) {
        uint32_t words[RC_WORDS_PER_SECTOR];
        size_t word_index;

        if (first_sector + segment >= image->sector_count ||
            !read_sector_words(image, first_sector + segment, words)) {
            set_numbered_error(error, error_size,
                               "cannot read chain-table sector %llu",
                               first_sector + segment);
            return false;
        }
        for (word_index = 0;
             word_index < RC_WORDS_PER_SECTOR && *copied < halfword_count;
             ++word_index) {
            halfwords[(*copied)++] = (uint16_t)(words[word_index] >> 12);
            if (*copied < halfword_count) {
                halfwords[(*copied)++] =
                    (uint16_t)(words[word_index] & UINT32_C(0x0fff));
            }
        }
    }
    return true;
}

static bool read_chain_table(rc_image_t *image, uint32_t slice_length,
                             uint16_t last_slice, uint16_t first_chain_slice,
                             uint16_t **result, char *error,
                             size_t error_size)
{
    size_t count = RC_CHAIN_HEADER_HALFWORDS + (size_t)last_slice + 1U;
    uint16_t *halfwords = calloc(count, sizeof(*halfwords));
    bool visited[RC_MAX_SLICES] = {false};
    uint16_t slice = first_chain_slice;
    size_t copied = 0;

    if (halfwords == NULL) {
        set_error(error, error_size, "cannot allocate chain table");
        return false;
    }
    while (copied < count) {
        int link;
        int next;

        if (slice > last_slice || visited[slice]) {
            set_error(error, error_size,
                      "invalid or cyclic chain-table slice chain");
            free(halfwords);
            return false;
        }
        visited[slice] = true;
        if (!append_chain_slice(image, slice, slice_length, halfwords,
                                count, &copied, error, error_size)) {
            free(halfwords);
            return false;
        }
        if (copied == count) {
            break;
        }
        if (RC_CHAIN_HEADER_HALFWORDS + (size_t)slice >= copied) {
            set_error(error, error_size,
                      "chain-table slice link is not available");
            free(halfwords);
            return false;
        }
        link = signed_halfword(
            halfwords[RC_CHAIN_HEADER_HALFWORDS + (size_t)slice]);
        if (link == 0) {
            set_error(error, error_size, "chain table ends prematurely");
            free(halfwords);
            return false;
        }
        next = (int)slice + link;
        if (next < 0 || next > last_slice) {
            set_error(error, error_size,
                      "chain-table slice link is outside the device");
            free(halfwords);
            return false;
        }
        slice = (uint16_t)next;
    }
    *result = halfwords;
    return true;
}

static void decode_entry(const uint32_t words[RC_CATALOG_ENTRY_WORDS],
                         uint32_t segment, uint16_t slot,
                         rc_catalog_entry_t *entry)
{
    uint16_t key = (uint16_t)(words[0] & UINT32_C(0x0fff));

    *entry = (rc_catalog_entry_t){0};
    entry->catalog_segment = segment;
    entry->slot = slot;
    entry->first_slice = (uint16_t)(words[0] >> 12);
    entry->name_key = (uint16_t)(key >> 3);
    entry->permission_key = (uint8_t)(key & UINT16_C(7));
    entry->lower_base = words[1];
    entry->upper_base = words[2];
    decode_name(words + 3, entry->name);
    entry->size_segments = signed_word(words[7]);
    decode_name(words + 8, entry->document);
    entry->short_clock = words[12];
    entry->file = words[13];
    entry->block = words[14];
    entry->contents = (uint16_t)(words[15] >> 12);
    entry->entry = (uint16_t)(words[15] & UINT32_C(0x0fff));
    entry->final_tail_word = words[16];
}

static bool read_catalog_segment(rc_image_t *image, uint64_t sector,
                                 uint32_t catalog_segment,
                                 rc_catalog_t *catalog, char *error,
                                 size_t error_size)
{
    uint32_t words[RC_WORDS_PER_SECTOR];
    uint16_t slot;

    if (!read_sector_words(image, sector, words)) {
        set_numbered_error(error, error_size,
                           "cannot read catalog sector %llu", sector);
        return false;
    }
    for (slot = 0; slot < RC_CATALOG_ENTRIES_PER_SEGMENT; ++slot) {
        const uint32_t *source = words + (size_t)slot * RC_CATALOG_ENTRY_WORDS;
        rc_catalog_entry_t entry;

        if (source[0] == UINT32_C(0xffffff)) {
            continue;
        }
        decode_entry(source, catalog_segment, slot, &entry);
        if (entry.name[0] == '\0') {
            continue;
        }
        catalog->entries[catalog->entry_count++] = entry;
    }
    return true;
}

static int read_catalog(const char *path, rc_catalog_t *catalog, bool main_catalog,
                         char *error, size_t error_size)
{
    rc_image_t image = {0};
    rc_catalog_entry_t main_entry = {0};
    uint32_t header[RC_WORDS_PER_SECTOR];
    uint16_t *chain = NULL;
    bool visited[RC_MAX_SLICES] = {false};
    uint16_t slice;
    uint32_t remaining;
    uint32_t segment_number = 0;
    int result = -1;

    if (path == NULL || catalog == NULL) {
        set_error(error, error_size, "invalid catalog arguments");
        return -1;
    }
    *catalog = (rc_catalog_t){0};
    if (main_catalog) {
        rc_catalog_t device_catalog;
        const rc_catalog_entry_t *entry;

        if (read_catalog(path, &device_catalog, false, error, error_size) != 0)
            return -1;
        entry = rc_catalog_find(&device_catalog, "catalog");
        if (entry == NULL || entry->size_segments <= 0 ||
            strcmp(entry->document, device_catalog.document) != 0) {
            set_error(error, error_size,
                      "main catalog area 'catalog' is missing or not on this device");
            rc_catalog_destroy(&device_catalog);
            return -1;
        }
        main_entry = *entry;
        rc_catalog_destroy(&device_catalog);
    }
    if (rc_image_open_readonly(&image, path, error, error_size) != 0) {
        return -1;
    }
    if (!read_sector_words(&image, 0, header)) {
        set_error(error, error_size, "cannot read catalog chain head");
        goto done;
    }

    catalog->first_slice = (uint16_t)(header[0] >> 12);
    decode_name(header + 3, catalog->name);
    catalog->segment_count = header[7];
    decode_name(header + 8, catalog->document);
    catalog->slice_length = header[13];
    catalog->last_slice = (uint16_t)(header[14] >> 12);
    catalog->first_chain_slice =
        (uint16_t)(header[14] & UINT32_C(0x0fff));
    catalog->chain_state = (uint16_t)(header[15] >> 12);
    if (main_catalog) {
        catalog->first_slice = main_entry.first_slice;
        catalog->segment_count = (uint32_t)main_entry.size_segments;
        (void)memcpy(catalog->name, main_entry.name, sizeof(catalog->name));
        (void)memcpy(catalog->document, main_entry.document,
                     sizeof(catalog->document));
    }

    if (catalog->name[0] == '\0' || catalog->segment_count == 0 ||
        catalog->segment_count > image.sector_count ||
        catalog->slice_length == 0 ||
        catalog->last_slice >= RC_MAX_SLICES ||
        catalog->first_slice > catalog->last_slice ||
        catalog->first_chain_slice > catalog->last_slice) {
        set_error(error, error_size, "invalid catalog chain head");
        goto done;
    }
    if (!read_chain_table(&image, catalog->slice_length,
                          catalog->last_slice, catalog->first_chain_slice,
                          &chain, error, error_size)) {
        goto done;
    }
    catalog->entries = calloc(
        (size_t)catalog->segment_count * RC_CATALOG_ENTRIES_PER_SEGMENT,
        sizeof(*catalog->entries));
    if (catalog->entries == NULL) {
        set_error(error, error_size, "cannot allocate catalog entries");
        goto done;
    }

    remaining = catalog->segment_count;
    slice = catalog->first_slice;
    while (remaining != 0) {
        uint32_t segment;
        int link;
        int next;

        if (slice > catalog->last_slice || visited[slice]) {
            set_error(error, error_size,
                      "invalid or cyclic catalog slice chain");
            goto done;
        }
        visited[slice] = true;
        for (segment = 0;
             segment < catalog->slice_length && remaining != 0; ++segment) {
            uint64_t sector = (uint64_t)slice * catalog->slice_length + segment;

            if (sector >= image.sector_count ||
                !read_catalog_segment(&image, sector, segment_number, catalog,
                                      error, error_size)) {
                if (sector >= image.sector_count) {
                    set_numbered_error(error, error_size,
                                       "catalog sector %llu is outside image",
                                       sector);
                }
                goto done;
            }
            ++segment_number;
            --remaining;
        }
        if (remaining == 0) {
            break;
        }
        link = signed_halfword(
            chain[RC_CHAIN_HEADER_HALFWORDS + (size_t)slice]);
        if (link == 0) {
            set_error(error, error_size, "catalog slice chain ends prematurely");
            goto done;
        }
        next = (int)slice + link;
        if (next < 0 || next > catalog->last_slice) {
            set_error(error, error_size,
                      "catalog slice link is outside the device");
            goto done;
        }
        slice = (uint16_t)next;
    }
    result = 0;

done:
    free(chain);
    rc_image_close(&image);
    if (result != 0) {
        rc_catalog_destroy(catalog);
    }
    return result;
}

int rc_catalog_read(const char *path, rc_catalog_t *catalog,
                    char *error, size_t error_size)
{
    return read_catalog(path, catalog, false, error, error_size);
}

int rc_catalog_read_main(const char *path, rc_catalog_t *catalog,
                         char *error, size_t error_size)
{
    return read_catalog(path, catalog, true, error, error_size);
}

void rc_catalog_destroy(rc_catalog_t *catalog)
{
    if (catalog != NULL) {
        free(catalog->entries);
        *catalog = (rc_catalog_t){0};
    }
}

const rc_catalog_entry_t *rc_catalog_find(const rc_catalog_t *catalog,
                                          const char *name)
{
    size_t index;

    if (catalog == NULL || name == NULL) {
        return NULL;
    }
    for (index = 0; index < catalog->entry_count; ++index) {
        if (strcmp(catalog->entries[index].name, name) == 0) {
            return &catalog->entries[index];
        }
    }
    return NULL;
}
