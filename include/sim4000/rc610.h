#ifndef SIM4000_RC610_H
#define SIM4000_RC610_H

#include "sim4000/device.h"

enum { RC610_COLUMNS = 132, RC610_PATH_SIZE = 512 };
#define RC610_INTERVENTION UINT32_C(0x800000)
#define RC610_PARITY UINT32_C(0x400000)
#define RC610_TIMER UINT32_C(0x200000)
#define RC610_PAPER_OUT UINT32_C(0x040000)

/* A print-operation stream, not rendered paper. Short writes are failures.
   If supplied, flush runs after every write, before the completion interrupt.
   An unbuffered sink may omit flush; a buffered sink must provide it. */
typedef struct {
    size_t (*write)(void *context, const uint8_t *bytes, size_t count);
    int (*flush)(void *context);
    int (*close)(void *context);
} rc610_sink_t;

typedef struct {
    rc_device_t device;
    rc610_sink_t sink;
    void *sink_context;
    char path[RC610_PATH_SIZE];
    uint8_t line[RC610_COLUMNS];
    size_t line_length;
    uint8_t print_control;
    unsigned interrupt_level;
    rc_word_t latched_status;
    uint32_t pending_interrupts;
    bool connected;
    bool remote;
    bool paper_present;
    bool printing;
    bool printing_started;
    bool print_with_paper;
    bool local_pending;
    bool output_failed;
    uint64_t write_operations;
    uint64_t ignored_characters;
    uint64_t overflow_characters;
    uint64_t incomplete_characters;
    uint64_t completed_prints;
    uint64_t failed_prints;
    uint64_t output_bytes;
    uint64_t output_errors;
} rc610_device_t;

/* Call once on a fresh object; close an existing device before reinitializing. */
bool rc610_init(rc610_device_t *printer, unsigned interrupt_level);
rc_word_t rc610_status(const rc610_device_t *printer);
/* Connect only an idle, empty, disconnected device. Ownership passes on success. */
int rc610_connect(rc610_device_t *printer, rc610_sink_t sink, void *context,
                  char *error, size_t error_size);
/* Exclusive creation only, including rejection of existing files/symlinks/FIFOs. */
int rc610_open(rc610_device_t *printer, const char *path,
               char *error, size_t error_size);
/* Append to a regular file only; never follow symlinks or open a FIFO/device. */
int rc610_open_append(rc610_device_t *printer, const char *path,
                      char *error, size_t error_size);
/* Refuses an active print; discards/counts an unprinted partial line, never emits it. */
int rc610_close(rc610_device_t *printer, char *error, size_t error_size);
void rc610_set_remote(rc610_device_t *printer, bool remote);
void rc610_set_paper(rc610_device_t *printer, bool present);
/* Explicit hardware fault injection, not a guest Control instruction. */
bool rc610_set_fault(rc610_device_t *printer, rc_word_t mask);
/* Clears the timer latch only; does not truncate output or reset CPU/job state. */
void rc610_master_clear(rc610_device_t *printer);

#endif
