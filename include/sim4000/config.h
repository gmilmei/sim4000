#ifndef SIM4000_CONFIG_H
#define SIM4000_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sim4000/throttle.h"
#include "sim4000/device.h"

enum {
    RC_CONFIG_MAX_DEVICES = 64,
    RC_CONFIG_MAX_TELNET_ENDPOINTS = 64,
    RC_CONFIG_MAX_INTERRUPTS = 2,
    RC_CONFIG_ADDRESS_SIZE = 256,
    RC_CONFIG_PATH_SIZE = 512,
    RC_CONFIG_ERROR_SIZE = 256,
    RC_CONFIG_SHORTCUT_DISABLED = 255,
    RC_CONFIG_OPERATOR_ESCAPE_DISABLED = RC_CONFIG_SHORTCUT_DISABLED
};

typedef enum {
    RC_DEVICE_READER,
    RC_DEVICE_PUNCH,
    RC_DEVICE_RC610,
    RC_DEVICE_RC749,
    RC_DEVICE_CONSOLE,
    RC_DEVICE_RC851,
    RC_DEVICE_CLOCK,
    RC_DEVICE_DFC403,
    RC_DEVICE_RC4819,
    RC_DEVICE_RC4819_RAM,
    RC_DEVICE_RC433X,
    RC_DEVICE_RC433X_RAM,
    RC_DEVICE_HOSTFILES
} rc_device_kind_t;

typedef struct {
    rc_device_kind_t kind;
    int device_number;
    int interrupts[RC_CONFIG_MAX_INTERRUPTS];
    size_t interrupt_count;
    int controller_number;
    int unit_number;
    uint32_t cylinders;
    uint32_t segments;
    char path[RC_CONFIG_PATH_SIZE];
    char root_path[RC_CONFIG_PATH_SIZE];
    char overlay_path[RC_CONFIG_PATH_SIZE];
    bool writable;
    bool append;
    rc_terminal_protocol_t terminal_protocol;
} rc_device_config_t;

typedef enum {
    RC_TELNET_ATTENTION_NONE = 0,
    RC_TELNET_ATTENTION_CHARACTER,
    RC_TELNET_ATTENTION_CONNECT
} rc_telnet_attention_t;

typedef struct {
    int device_number;
    uint16_t port;
    char address[RC_CONFIG_ADDRESS_SIZE];
    uint8_t attention_character;
    rc_telnet_attention_t attention_mode;
} rc_telnet_config_t;

typedef struct {
    uint32_t core_min_halfwords;
    uint32_t core_max_halfwords;
    bool ix_enabled;
    bool protection_enabled;
    uint8_t operator_escape;
    uint8_t operator_date_key;
    uint8_t console_delete_mapping;
    int date_year_offset;
    rc_throttle_config_t throttle;
    char monitor_path[RC_CONFIG_PATH_SIZE];
    rc_device_config_t devices[RC_CONFIG_MAX_DEVICES];
    size_t device_count;
    rc_telnet_config_t telnet_endpoints[RC_CONFIG_MAX_TELNET_ENDPOINTS];
    size_t telnet_endpoint_count;
} rc_config_t;

typedef struct {
    size_t line;
    char message[RC_CONFIG_ERROR_SIZE];
} rc_config_error_t;

void rc_config_init(rc_config_t *config);
int rc_config_parse_file(const char *path, rc_config_t *config,
                         rc_config_error_t *error);
int rc_config_parse_text(const char *text, rc_config_t *config,
                         rc_config_error_t *error);
const char *rc_device_kind_name(rc_device_kind_t kind);

#endif
