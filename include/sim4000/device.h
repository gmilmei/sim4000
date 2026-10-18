#ifndef SIM4000_DEVICE_H
#define SIM4000_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "sim4000/image.h"
#include "sim4000/types.h"

struct rc_machine;
struct rc_rc433x_overlay_sector;

enum {
    RC_DEVICE_COUNT = 64,
    RC_CONSOLE_QUEUE_SIZE = 32,
    RC_DEVICE_TICKS_PER_SECOND = 39,
    RC_CONSOLE_TIMEOUT_TICKS = RC_DEVICE_TICKS_PER_SECOND + 2,
    RC433X_OVERLAY_PATH_SIZE = 512,
    RC_READER_PATH_SIZE = 512,
    RC_PUNCH_PATH_SIZE = 512
};

typedef enum {
    RC_IO_SENSE = 0,
    RC_IO_CONTROL = 1,
    RC_IO_READ = 2,
    RC_IO_WRITE = 3
} rc_io_command_t;

typedef enum {
    RC_DEVICE_ACCEPTED = 0,
    RC_DEVICE_BUSY,
    RC_DEVICE_DISCONNECTED
} rc_device_result_t;

typedef rc_device_result_t (*rc_device_io_fn)(void *context,
                                              rc_io_command_t command,
                                              uint8_t modifier,
                                              rc_word_t *word);
typedef void (*rc_device_advance_fn)(void *context, uint64_t ticks);
typedef uint32_t (*rc_device_poll_fn)(void *context);

typedef struct {
    void *context;
    rc_device_io_fn io;
    rc_device_advance_fn advance;
    rc_device_poll_fn poll;
} rc_device_t;

typedef struct {
    rc_device_t device;
    uint64_t ticks;
    unsigned interrupt_level;
    uint32_t pending_interrupts;
} rc_clock_device_t;

typedef void (*rc_console_output_fn)(void *context, uint8_t character);

typedef enum {
    RC_TERMINAL_CLASSIC = 0,
    RC_TERMINAL_V2
} rc_terminal_protocol_t;

typedef struct {
    rc_device_t device;
    unsigned io_interrupt_level;
    unsigned operator_interrupt_level;
    uint8_t input[RC_CONSOLE_QUEUE_SIZE];
    size_t input_head;
    size_t input_count;
    rc_word_t status;
    uint64_t timeout_ticks;
    uint32_t pending_interrupts;
    bool busy;
    bool echo_on_sense;
    rc_console_output_fn output;
    void *output_context;
    rc_terminal_protocol_t protocol;
    struct rc_machine *machine;
    uint8_t terminal_type;
    uint8_t prompt;
    uint8_t read_modifier;
    uint8_t echo[3];
    size_t echo_length;
} rc_console_device_t;

typedef struct {
    rc_device_t device;
    FILE *file;
    rc_word_t status;
    unsigned interrupt_level;
    uint32_t pending_interrupts;
    uint64_t read_operations;
    uint64_t characters_read;
    uint64_t null_bytes_skipped;
    uint64_t parity_errors;
    char path[RC_READER_PATH_SIZE];
    bool tape_present;
} rc_reader_device_t;

typedef struct {
    rc_device_t device;
    FILE *file;
    rc_word_t status;
    unsigned interrupt_level;
    uint32_t pending_interrupts;
    uint64_t write_operations;
    uint64_t characters_written;
    uint64_t write_errors;
    char path[RC_PUNCH_PATH_SIZE];
    bool tape_present;
} rc_punch_device_t;

typedef struct {
    rc_device_t device;
    struct rc_machine *machine;
    rc_image_t image;
    struct rc_rc433x_overlay_sector *overlay;
    rc_word_t status;
    bool readonly;
    bool write_through;
    uint64_t read_operations;
    uint64_t sectors_read;
    uint64_t write_operations;
    uint64_t sectors_written;
    uint64_t overlay_sectors;
    uint64_t rejected_writes;
    uint64_t persistence_failures;
    uint64_t base_image_bytes;
    uint64_t base_image_hash;
    char base_image_path[RC433X_OVERLAY_PATH_SIZE];
    char overlay_path[RC433X_OVERLAY_PATH_SIZE];
    bool persistent_overlay;
} rc_rc433x_device_t;

void rc_clock_device_init(rc_clock_device_t *clock, unsigned interrupt_level);
void rc_clock_device_set_ticks(rc_clock_device_t *clock, uint64_t ticks);

void rc_console_device_init(rc_console_device_t *console,
                            unsigned io_interrupt_level,
                            unsigned operator_interrupt_level);
void rc_console_device_set_output(rc_console_device_t *console,
                                  rc_console_output_fn output,
                                  void *context);
/* Select a startup protocol before attaching media or submitting input. */
bool rc_console_device_set_protocol(rc_console_device_t *console,
                                    struct rc_machine *machine,
                                    rc_terminal_protocol_t protocol);
bool rc_console_device_submit(rc_console_device_t *console,
                              uint8_t character);
void rc_console_device_attention(rc_console_device_t *console);
size_t rc_console_device_input_count(const rc_console_device_t *console);
/* Discard queued bytes only; preserve in-flight I/O and interrupt state. */
size_t rc_console_device_discard_input(rc_console_device_t *console);

void rc_reader_device_init(rc_reader_device_t *reader,
                           unsigned interrupt_level);
int rc_reader_device_mount(rc_reader_device_t *reader, const char *path,
                           char *error, size_t error_size);
int rc_reader_device_rewind(rc_reader_device_t *reader, char *error,
                            size_t error_size);
void rc_reader_device_unmount(rc_reader_device_t *reader);
void rc_reader_device_destroy(rc_reader_device_t *reader);

void rc_punch_device_init(rc_punch_device_t *punch,
                          unsigned interrupt_level);
int rc_punch_device_mount(rc_punch_device_t *punch, const char *path,
                          bool replace, char *error, size_t error_size);
int rc_punch_device_flush(rc_punch_device_t *punch, char *error,
                          size_t error_size);
void rc_punch_device_unmount(rc_punch_device_t *punch);
void rc_punch_device_destroy(rc_punch_device_t *punch);

int rc_rc433x_device_init_readonly(rc_rc433x_device_t *disk,
                                   struct rc_machine *machine,
                                   const char *path, char *error,
                                   size_t error_size);
int rc_rc433x_device_init_cow(rc_rc433x_device_t *disk,
                              struct rc_machine *machine,
                              const char *path, char *error,
                              size_t error_size);
int rc_rc433x_device_init_writable(rc_rc433x_device_t *disk,
                                   struct rc_machine *machine,
                                   const char *path, char *error,
                                   size_t error_size);
int rc_rc433x_device_init_persistent(rc_rc433x_device_t *disk,
                                     struct rc_machine *machine,
                                     const char *path,
                                     const char *overlay_path,
                                     char *error, size_t error_size);
int rc_rc433x_device_flush(rc_rc433x_device_t *disk, char *error,
                           size_t error_size);
void rc_rc433x_device_destroy(rc_rc433x_device_t *disk);

/* Shared packed-sector backing store for the hardware-interface controller.
   These functions transfer core words, not RC433X monitor messages. */
bool rc_disk_read_core(rc_rc433x_device_t *disk, rc_addr_t address,
                       uint64_t sector, uint32_t count);
bool rc_disk_write_core(rc_rc433x_device_t *disk, rc_addr_t address,
                        uint64_t sector, uint32_t count);

typedef struct {
    rc_device_t device;
    struct rc_machine *machine;
    int root_fd;
    FILE *file;
    unsigned interrupt_level;
    uint32_t pending_interrupts;
    rc_word_t status;
    rc_word_t characters;
    rc_word_t transfer_size;
    bool output_mode;
    bool text_mode;
    bool end_marker_sent;
} rc_hostfiles_device_t;

int rc_hostfiles_device_init(rc_hostfiles_device_t *host,
                             struct rc_machine *machine, const char *root,
                             unsigned interrupt_level, char *error,
                             size_t error_size);
void rc_hostfiles_device_destroy(rc_hostfiles_device_t *host);

#endif
