#ifndef SIM4000_RC749_H
#define SIM4000_RC749_H

#include "sim4000/device.h"

struct rc749_record;

enum {
    RC749_MAX_RECORD_BYTES = 7680,
    RC749_INTERVENTION = 1U << 23,
    RC749_PARITY = 1U << 22,
    RC749_OVERRUN = 1U << 20,
    RC749_BLOCK_LENGTH = 1U << 19,
    RC749_EOT = 1U << 18,
    RC749_BOT = 1U << 17,
    RC749_TAPE_MARK = 1U << 16,
    RC749_HIGH_DENSITY = 1U << 14
};

typedef struct {
    rc_device_t device;
    struct rc_machine *machine;
    FILE *file;
    struct rc749_record *records;
    size_t record_count;
    size_t position;
    unsigned interrupt_level;
    uint32_t pending_interrupts;
    rc_word_t status;
    rc_addr_t last_core;
    size_t transferred_bytes;
    int root_fd;
    bool fault;
    char path[RC_READER_PATH_SIZE];
    uint64_t reads;
    uint64_t bytes_read;
    uint64_t errors;
    uint64_t rejected_writes;
} rc_rc749_device_t;

void rc_rc749_init(rc_rc749_device_t *tape, struct rc_machine *machine,
                    unsigned interrupt_level);
int rc_rc749_set_root(rc_rc749_device_t *tape, const char *path,
                      char *error, size_t error_size);
int rc_rc749_mount(rc_rc749_device_t *tape, const char *path,
                   char *error, size_t error_size);
int rc_rc749_rewind(rc_rc749_device_t *tape, char *error, size_t error_size);
void rc_rc749_unmount(rc_rc749_device_t *tape);
void rc_rc749_destroy(rc_rc749_device_t *tape);

#endif
