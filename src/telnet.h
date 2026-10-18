#ifndef SIM4000_TELNET_H
#define SIM4000_TELNET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sim4000/device.h"
#include "sim4000/config.h"

enum {
    RC_TELNET_INPUT_BUFFER_SIZE = 4096,
    RC_TELNET_OUTPUT_BUFFER_SIZE = 8192,
    RC_TELNET_ADDRESS_SIZE = 256
};

typedef enum {
    RC_TELNET_TERMINAL_CONSOLE = 0,
    RC_TELNET_TERMINAL_RC851
} rc_telnet_terminal_t;

typedef struct {
    int listener_fd;
    int client_fd;
    int device_number;
    uint16_t port;
    char address[RC_TELNET_ADDRESS_SIZE];
    rc_console_device_t *console;
    rc_telnet_terminal_t terminal_kind;
    uint8_t delete_mapping;
    rc_telnet_attention_t attention_mode;
    uint8_t attention_character;
    uint8_t input[RC_TELNET_INPUT_BUFFER_SIZE];
    size_t input_head;
    size_t input_count;
    uint8_t output[RC_TELNET_OUTPUT_BUFFER_SIZE];
    size_t output_head;
    size_t output_count;
    unsigned telnet_state;
    uint8_t telnet_command;
    unsigned ansi_input_state;
    unsigned ansi_parameter;
    bool ansi_have_parameter;
    bool input_pending_cr;
    bool output_pending_cr;
    unsigned rc851_state;
    uint8_t rc851_row;
    uint8_t rc851_column;
    bool rc851_scroll;
    uint64_t accepted_connections;
    uint64_t rejected_connections;
    uint64_t dropped_input;
    uint64_t dropped_output;
    uint64_t attention_requests;
} rc_telnet_endpoint_t;

int rc_telnet_endpoint_init(rc_telnet_endpoint_t *endpoint,
                            const char *address, uint16_t port,
                            int device_number, rc_console_device_t *console,
                            rc_telnet_terminal_t terminal_kind,
                            uint8_t delete_mapping,
                            rc_telnet_attention_t attention_mode,
                            uint8_t attention_character, char *error,
                            size_t error_size);
void rc_telnet_endpoint_destroy(rc_telnet_endpoint_t *endpoint);
void rc_telnet_endpoint_begin_session(rc_telnet_endpoint_t *endpoint);
void rc_telnet_endpoint_pump(rc_telnet_endpoint_t *endpoint);
void rc_telnet_endpoint_output(void *context, uint8_t character);
void rc_telnet_endpoint_receive_bytes(rc_telnet_endpoint_t *endpoint,
                                      const uint8_t *bytes, size_t count);

#endif
