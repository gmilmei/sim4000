#include "sim4000/config.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_SIZE = 2048,
    TOKEN_SIZE = 512
};

typedef enum {
    TOKEN_END,
    TOKEN_WORD,
    TOKEN_EQUALS,
    TOKEN_COMMA
} token_kind_t;

typedef struct {
    const char *cursor;
    token_kind_t kind;
    char text[TOKEN_SIZE];
} lexer_t;

typedef struct {
    rc_config_t *config;
    rc_config_error_t *error;
    size_t line;
    unsigned char used_devices[64];
    unsigned char used_interrupts[24];
    int core_defined;
    int monitor_defined;
    int operator_escape_defined;
    int operator_date_defined;
    int input_delete_defined;
    int date_defined;
    int throttle_defined;
    int cpu_defined;
    int protection_defined;
} parser_t;

static int fail(parser_t *parser, const char *format, ...)
{
    if (parser->error != NULL) {
        va_list arguments;
        parser->error->line = parser->line;
        va_start(arguments, format);
        (void)vsnprintf(parser->error->message,
                        sizeof(parser->error->message), format, arguments);
        va_end(arguments);
    }
    return -1;
}

static int same_word(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (toupper((unsigned char)*left) != toupper((unsigned char)*right)) {
            return 0;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static int lexer_next(lexer_t *lexer)
{
    size_t length = 0;

    while (isspace((unsigned char)*lexer->cursor)) {
        ++lexer->cursor;
    }
    lexer->text[0] = '\0';
    if (*lexer->cursor == '\0' || *lexer->cursor == ';') {
        lexer->kind = TOKEN_END;
        return 0;
    }
    if (*lexer->cursor == '=') {
        ++lexer->cursor;
        lexer->kind = TOKEN_EQUALS;
        return 0;
    }
    if (*lexer->cursor == ',') {
        ++lexer->cursor;
        lexer->kind = TOKEN_COMMA;
        return 0;
    }
    while (*lexer->cursor != '\0' && *lexer->cursor != ';' &&
           *lexer->cursor != '=' && *lexer->cursor != ',' &&
           !isspace((unsigned char)*lexer->cursor)) {
        if (length + 1 >= sizeof(lexer->text)) {
            return -1;
        }
        lexer->text[length++] = *lexer->cursor++;
    }
    lexer->text[length] = '\0';
    lexer->kind = TOKEN_WORD;
    return 0;
}

static int expect(parser_t *parser, lexer_t *lexer, token_kind_t kind,
                  const char *description)
{
    if (lexer_next(lexer) != 0) {
        return fail(parser, "token is too long");
    }
    if (lexer->kind != kind) {
        return fail(parser, "expected %s", description);
    }
    return 0;
}

static int parse_u32(parser_t *parser, const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text[0] == '\0' || text[0] == '-') {
        return fail(parser, "invalid non-negative number: %s", text);
    }
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed > UINT32_MAX) {
        return fail(parser, "invalid number: %s", text);
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int parse_int(parser_t *parser, const char *text, int *value)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (text[0] == '\0' || errno != 0 || *end != '\0' ||
        parsed < -9999L || parsed > 9999L) {
        return fail(parser, "invalid signed number: %s", text);
    }
    *value = (int)parsed;
    return 0;
}

static int device_kind(const char *name, rc_device_kind_t *kind)
{
    static const struct {
        const char *name;
        rc_device_kind_t kind;
    } kinds[] = {
        {"READER", RC_DEVICE_READER},
        {"RC749", RC_DEVICE_RC749},
        {"PUNCH", RC_DEVICE_PUNCH},
        {"RC610", RC_DEVICE_RC610},
        {"CONSOLE", RC_DEVICE_CONSOLE},
        {"RC851", RC_DEVICE_RC851},
        {"CLOCK", RC_DEVICE_CLOCK},
        {"DFC403", RC_DEVICE_DFC403},
        {"RC4819", RC_DEVICE_RC4819},
        {"RC4819RAM", RC_DEVICE_RC4819_RAM},
        {"RC433X", RC_DEVICE_RC433X},
        {"RC433XRAM", RC_DEVICE_RC433X_RAM},
        {"HOSTFILES", RC_DEVICE_HOSTFILES}
    };
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        if (same_word(name, kinds[i].name)) {
            *kind = kinds[i].kind;
            return 0;
        }
    }
    return -1;
}

const char *rc_device_kind_name(rc_device_kind_t kind)
{
    switch (kind) {
    case RC_DEVICE_READER: return "READER";
    case RC_DEVICE_RC749: return "RC749";
    case RC_DEVICE_PUNCH: return "PUNCH";
    case RC_DEVICE_RC610: return "RC610";
    case RC_DEVICE_CONSOLE: return "CONSOLE";
    case RC_DEVICE_RC851: return "RC851";
    case RC_DEVICE_CLOCK: return "CLOCK";
    case RC_DEVICE_DFC403: return "DFC403";
    case RC_DEVICE_RC4819: return "RC4819";
    case RC_DEVICE_RC4819_RAM: return "RC4819RAM";
    case RC_DEVICE_RC433X: return "RC433X";
    case RC_DEVICE_RC433X_RAM: return "RC433XRAM";
    case RC_DEVICE_HOSTFILES: return "HOSTFILES";
    }
    return "UNKNOWN";
}

static int kind_has_device_number(rc_device_kind_t kind)
{
    return kind != RC_DEVICE_RC4819 && kind != RC_DEVICE_RC4819_RAM;
}

static size_t required_interrupts(rc_device_kind_t kind)
{
    switch (kind) {
    case RC_DEVICE_CONSOLE:
    case RC_DEVICE_RC851:
    case RC_DEVICE_DFC403:
        return 2;
    case RC_DEVICE_READER:
    case RC_DEVICE_RC749:
    case RC_DEVICE_PUNCH:
    case RC_DEVICE_RC610:
    case RC_DEVICE_CLOCK:
    case RC_DEVICE_HOSTFILES:
        return 1;
    default:
        return 0;
    }
}

static int parse_parameter_value(parser_t *parser, lexer_t *lexer,
                                 char *value, size_t value_size);

static const rc_device_config_t *configured_device(const rc_config_t *config,
                                                   int device_number)
{
    size_t index;

    for (index = 0; index < config->device_count; ++index) {
        if (config->devices[index].device_number == device_number) {
            return &config->devices[index];
        }
    }
    return NULL;
}

static int parse_coresize(parser_t *parser, lexer_t *lexer)
{
    uint32_t minimum;
    uint32_t maximum;

    if (parser->core_defined) {
        return fail(parser, "CORESIZE is already defined");
    }
    if (lexer_next(lexer) != 0) {
        return fail(parser, "token is too long");
    }
    /* Accept the old whitespace-only syntax for existing configurations. */
    if (lexer->kind == TOKEN_EQUALS &&
        expect(parser, lexer, TOKEN_WORD, "minimum core size") != 0) {
        return -1;
    }
    if (lexer->kind != TOKEN_WORD) {
        return fail(parser, "expected minimum core size");
    }
    if (parse_u32(parser, lexer->text, &minimum) != 0) {
        return -1;
    }
    if (lexer_next(lexer) != 0) {
        return fail(parser, "token is too long");
    }
    if (lexer->kind == TOKEN_COMMA) {
        if (expect(parser, lexer, TOKEN_WORD, "maximum core size") != 0 ||
            parse_u32(parser, lexer->text, &maximum) != 0 ||
            expect(parser, lexer, TOKEN_END, "end of line") != 0) {
            return -1;
        }
    } else if (lexer->kind == TOKEN_END) {
        maximum = minimum;
    } else {
        return fail(parser, "expected comma or end of line after CORESIZE");
    }

    if (minimum < 16384) {
        minimum = 16384;
    }
    if (maximum > 8388607) {
        maximum = 8388607;
    }
    if (maximum < minimum) {
        return fail(parser, "minimum core size exceeds maximum core size");
    }
    parser->config->core_min_halfwords = minimum;
    parser->config->core_max_halfwords = maximum;
    parser->core_defined = 1;
    return 0;
}

static int parse_monitor(parser_t *parser, lexer_t *lexer)
{
    char key[TOKEN_SIZE];
    char value[TOKEN_SIZE];

    if (parser->monitor_defined) {
        return fail(parser, "MONITOR is already defined");
    }
    if (expect(parser, lexer, TOKEN_WORD, "monitor parameter") != 0) {
        return -1;
    }
    (void)strcpy(key, lexer->text);
    if (!same_word(key, "FILE")) {
        return fail(parser, "unsupported MONITOR parameter: %s", key);
    }
    if (parse_parameter_value(parser, lexer, value, sizeof(value)) != 0 ||
        expect(parser, lexer, TOKEN_END, "end of line") != 0) {
        return -1;
    }
    (void)strcpy(parser->config->monitor_path, value);
    parser->monitor_defined = 1;
    return 0;
}

static int parse_control_key(parser_t *parser, const char *text,
                             uint8_t *value, const char *description)
{
    unsigned char character;

    if (same_word(text, "NONE")) {
        *value = RC_CONFIG_SHORTCUT_DISABLED;
        return 0;
    }
    if (strlen(text) != 6U ||
        toupper((unsigned char)text[0]) != 'C' ||
        toupper((unsigned char)text[1]) != 'T' ||
        toupper((unsigned char)text[2]) != 'R' ||
        toupper((unsigned char)text[3]) != 'L' || text[4] != '-') {
        return fail(parser, "invalid %s: %s", description, text);
    }
    character = (unsigned char)toupper((unsigned char)text[5]);
    if (character < '@' || character > '_') {
        return fail(parser, "invalid %s: %s", description, text);
    }
    *value = character & UINT8_C(0x1f);
    return 0;
}

static int parse_alt_key(parser_t *parser, const char *text, uint8_t *value)
{
    unsigned char character;

    if (same_word(text, "NONE")) {
        *value = RC_CONFIG_SHORTCUT_DISABLED;
        return 0;
    }
    if (strlen(text) != 5U ||
        toupper((unsigned char)text[0]) != 'A' ||
        toupper((unsigned char)text[1]) != 'L' ||
        toupper((unsigned char)text[2]) != 'T' || text[3] != '-') {
        return fail(parser, "invalid date shortcut: %s", text);
    }
    character = (unsigned char)toupper((unsigned char)text[4]);
    if (character < 'A' || character > 'Z') {
        return fail(parser, "invalid date shortcut: %s", text);
    }
    *value = character;
    return 0;
}

static int parse_operator(parser_t *parser, lexer_t *lexer)
{
    char key[TOKEN_SIZE];
    char value[TOKEN_SIZE];

    if (expect(parser, lexer, TOKEN_WORD, "operator parameter") != 0) {
        return -1;
    }
    (void)strcpy(key, lexer->text);
    if (parse_parameter_value(parser, lexer, value, sizeof(value)) != 0 ||
        expect(parser, lexer, TOKEN_END, "end of line") != 0) {
        return -1;
    }
    if (same_word(key, "ESCAPE")) {
        if (parser->operator_escape_defined) {
            return fail(parser, "OPERATOR ESCAPE is already defined");
        }
        if (parse_control_key(parser, value,
                              &parser->config->operator_escape,
                              "operator escape") != 0) {
            return -1;
        }
        parser->operator_escape_defined = 1;
    } else if (same_word(key, "DATE")) {
        if (parser->operator_date_defined) {
            return fail(parser, "OPERATOR DATE is already defined");
        }
        if (parse_alt_key(parser, value,
                          &parser->config->operator_date_key) != 0) {
            return -1;
        }
        parser->operator_date_defined = 1;
    } else {
        return fail(parser, "unsupported OPERATOR parameter: %s", key);
    }
    return 0;
}

static int parse_input(parser_t *parser, lexer_t *lexer)
{
    char key[TOKEN_SIZE];
    char value[TOKEN_SIZE];

    if (expect(parser, lexer, TOKEN_WORD, "input parameter") != 0) {
        return -1;
    }
    (void)strcpy(key, lexer->text);
    if (!same_word(key, "DEL")) {
        return fail(parser, "unsupported INPUT parameter: %s", key);
    }
    if (parser->input_delete_defined) {
        return fail(parser, "INPUT DEL is already defined");
    }
    if (parse_parameter_value(parser, lexer, value, sizeof(value)) != 0 ||
        expect(parser, lexer, TOKEN_END, "end of line") != 0 ||
        parse_control_key(parser, value,
                          &parser->config->console_delete_mapping,
                          "DEL mapping") != 0) {
        return -1;
    }
    parser->input_delete_defined = 1;
    return 0;
}

static int parse_date(parser_t *parser, lexer_t *lexer)
{
    char key[TOKEN_SIZE];
    char value[TOKEN_SIZE];

    if (parser->date_defined) {
        return fail(parser, "DATE is already defined");
    }
    if (expect(parser, lexer, TOKEN_WORD, "date parameter") != 0) {
        return -1;
    }
    (void)strcpy(key, lexer->text);
    if (!same_word(key, "YEAROFFSET")) {
        return fail(parser, "unsupported DATE parameter: %s", key);
    }
    if (parse_parameter_value(parser, lexer, value, sizeof(value)) != 0 ||
        expect(parser, lexer, TOKEN_END, "end of line") != 0 ||
        parse_int(parser, value, &parser->config->date_year_offset) != 0) {
        return -1;
    }
    parser->date_defined = 1;
    return 0;
}

static int parse_parameter_value(parser_t *parser, lexer_t *lexer,
                                 char *value, size_t value_size)
{
    if (expect(parser, lexer, TOKEN_EQUALS, "'='") != 0 ||
        expect(parser, lexer, TOKEN_WORD, "parameter value") != 0) {
        return -1;
    }
    if (strlen(lexer->text) >= value_size) {
        return fail(parser, "parameter value is too long");
    }
    (void)strcpy(value, lexer->text);
    return 0;
}

static int parse_device(parser_t *parser, lexer_t *lexer)
{
    rc_device_config_t device;
    char kind_name[TOKEN_SIZE];
    int have_devno = 0;
    int have_intno = 0;
    int have_path = 0;
    int have_controller = 0;
    int have_unit = 0;
    int have_cylinders = 0;
    int have_segments = 0;
    int have_overlay = 0;
    int have_writable = 0;
    int have_root = 0;
    int have_protocol = 0;
    int have_append = 0;

    if (parser->config->device_count >= RC_CONFIG_MAX_DEVICES) {
        return fail(parser, "too many device declarations");
    }
    if (expect(parser, lexer, TOKEN_WORD, "device kind") != 0) {
        return -1;
    }
    (void)strcpy(kind_name, lexer->text);
    memset(&device, 0, sizeof(device));
    device.device_number = -1;
    device.controller_number = -1;
    device.unit_number = -1;
    if (device_kind(kind_name, &device.kind) != 0) {
        return fail(parser, "unknown device kind: %s", kind_name);
    }

    for (;;) {
        char key[TOKEN_SIZE];
        char value[TOKEN_SIZE];
        uint32_t number;

        if (lexer_next(lexer) != 0) {
            return fail(parser, "token is too long");
        }
        if (lexer->kind == TOKEN_END) {
            break;
        }
        if (lexer->kind != TOKEN_WORD) {
            return fail(parser, "expected device parameter name");
        }
        (void)strcpy(key, lexer->text);
        if (parse_parameter_value(parser, lexer, value, sizeof(value)) != 0) {
            return -1;
        }

        if (same_word(key, "APPEND")) {
            if (device.kind != RC_DEVICE_RC610 || have_append)
                return fail(parser, "APPEND is supported once for RC610 only");
            if (same_word(value, "YES")) device.append = true;
            else if (same_word(value, "NO")) device.append = false;
            else return fail(parser, "APPEND must be YES or NO");
            have_append = 1;
        } else if (same_word(key, "PROTOCOL")) {
            if (have_protocol) return fail(parser, "duplicate PROTOCOL");
            if (device.kind != RC_DEVICE_CONSOLE && device.kind != RC_DEVICE_RC851)
                return fail(parser, "PROTOCOL is supported only for CONSOLE or RC851");
            if (same_word(value, "CLASSIC")) device.terminal_protocol = RC_TERMINAL_CLASSIC;
            else if (same_word(value, "V2")) device.terminal_protocol = RC_TERMINAL_V2;
            else return fail(parser, "PROTOCOL must be CLASSIC or V2");
            have_protocol = 1;
        } else if (same_word(key, "DEVNO")) {
            if (have_devno || parse_u32(parser, value, &number) != 0) {
                return have_devno ? fail(parser, "duplicate DEVNO") : -1;
            }
            if (number > 63) {
                return fail(parser, "device number is out of range");
            }
            device.device_number = (int)number;
            have_devno = 1;
        } else if (same_word(key, "INTNO")) {
            const char *next_token_start;

            if (have_intno || parse_u32(parser, value, &number) != 0) {
                return have_intno ? fail(parser, "duplicate INTNO") : -1;
            }
            if (number < 3 || number > 23) {
                return fail(parser, "interrupt number is out of range");
            }
            device.interrupts[0] = (int)number;
            device.interrupt_count = 1;
            have_intno = 1;
            next_token_start = lexer->cursor;
            if (lexer_next(lexer) != 0) {
                return fail(parser, "token is too long");
            }
            if (lexer->kind == TOKEN_COMMA) {
                if (expect(parser, lexer, TOKEN_WORD,
                           "second interrupt number") != 0 ||
                    parse_u32(parser, lexer->text, &number) != 0) {
                    return -1;
                }
                if (number < 3 || number > 23) {
                    return fail(parser, "interrupt number is out of range");
                }
                device.interrupts[1] = (int)number;
                device.interrupt_count = 2;
            } else {
                lexer->cursor = next_token_start;
            }
        } else if (same_word(key, "FILE")) {
            if (have_path) {
                return fail(parser, "duplicate FILE");
            }
            (void)strcpy(device.path, value);
            have_path = 1;
        } else if (same_word(key, "ROOT")) {
            if (have_root || (have_path && device.kind != RC_DEVICE_RC749)) {
                return fail(parser, "duplicate ROOT or conflicting FILE");
            }
            if (device.kind == RC_DEVICE_RC749) (void)strcpy(device.root_path, value);
            else (void)strcpy(device.path, value);
            have_root = 1;
        } else if (same_word(key, "OVERLAY")) {
            if (have_overlay) {
                return fail(parser, "duplicate OVERLAY");
            }
            (void)strcpy(device.overlay_path, value);
            have_overlay = 1;
        } else if (same_word(key, "WRITABLE")) {
            if (have_writable) {
                return fail(parser, "duplicate WRITABLE");
            }
            if (same_word(value, "YES")) {
                device.writable = true;
            } else if (same_word(value, "NO")) {
                device.writable = false;
            } else {
                return fail(parser, "WRITABLE must be YES or NO");
            }
            have_writable = 1;
        } else if (same_word(key, "DFC")) {
            if (have_controller || parse_u32(parser, value, &number) != 0) {
                return have_controller ? fail(parser, "duplicate DFC") : -1;
            }
            if (number > 63) {
                return fail(parser, "DFC device number is out of range");
            }
            device.controller_number = (int)number;
            have_controller = 1;
        } else if (same_word(key, "UNITNO")) {
            if (have_unit || parse_u32(parser, value, &number) != 0) {
                return have_unit ? fail(parser, "duplicate UNITNO") : -1;
            }
            if (number > 7) {
                return fail(parser, "disk unit number is out of range");
            }
            device.unit_number = (int)number;
            have_unit = 1;
        } else if (same_word(key, "CYLS")) {
            if (have_cylinders || parse_u32(parser, value, &number) != 0) {
                return have_cylinders ? fail(parser, "duplicate CYLS") : -1;
            }
            device.cylinders = number;
            have_cylinders = 1;
        } else if (same_word(key, "SEGS")) {
            if (have_segments || parse_u32(parser, value, &number) != 0) {
                return have_segments ? fail(parser, "duplicate SEGS") : -1;
            }
            device.segments = number;
            have_segments = 1;
        } else {
            return fail(parser, "unsupported parameter %s for %s", key,
                        kind_name);
        }
    }

    if (kind_has_device_number(device.kind) && !have_devno) {
        return fail(parser, "%s requires DEVNO", kind_name);
    }
    if (device.kind == RC_DEVICE_HOSTFILES &&
        (!have_root || have_path || device.device_number != 63 ||
         have_controller || have_unit || have_cylinders || have_segments)) {
        return fail(parser, "HOSTFILES requires ROOT, DEVNO=63 and INTNO");
    }
    if (have_root && device.kind != RC_DEVICE_HOSTFILES && device.kind != RC_DEVICE_RC749) {
        return fail(parser, "ROOT is supported only for HOSTFILES or RC749");
    }
    if (device.kind == RC_DEVICE_RC749 &&
        (have_controller || have_unit || have_cylinders || have_segments)) {
        return fail(parser, "RC749 supports DEVNO, INTNO, FILE and ROOT only");
    }
    if (required_interrupts(device.kind) != device.interrupt_count) {
        return fail(parser, "%s requires %zu interrupt number(s)", kind_name,
                    required_interrupts(device.kind));
    }
    if ((device.kind == RC_DEVICE_RC433X ||
         device.kind == RC_DEVICE_RC4819) && !have_path) {
        return fail(parser, "%s requires FILE", kind_name);
    }
    if (have_overlay && device.kind != RC_DEVICE_RC433X) {
        return fail(parser, "OVERLAY is supported only for RC433X");
    }
    if (device.kind == RC_DEVICE_RC610 &&
        (have_controller || have_unit || have_cylinders || have_segments))
        return fail(parser, "RC610 supports DEVNO, INTNO, FILE and APPEND only");
    if (have_append && !have_path)
        return fail(parser, "RC610 APPEND requires FILE");
    if (have_writable && device.kind != RC_DEVICE_RC433X) {
        return fail(parser, "WRITABLE is supported only for RC433X");
    }
    if (have_overlay && device.writable) {
        return fail(parser, "RC433X OVERLAY and WRITABLE=YES conflict");
    }
    if ((device.kind == RC_DEVICE_RC4819 ||
         device.kind == RC_DEVICE_RC4819_RAM) &&
        (!have_controller || !have_unit)) {
        return fail(parser, "%s requires DFC and UNITNO", kind_name);
    }
    if (device.kind == RC_DEVICE_RC4819_RAM && !have_cylinders) {
        return fail(parser, "RC4819RAM requires CYLS");
    }
    if (device.kind == RC_DEVICE_RC433X_RAM && !have_segments) {
        return fail(parser, "RC433XRAM requires SEGS");
    }
    if (device.kind == RC_DEVICE_RC4819 ||
        device.kind == RC_DEVICE_RC4819_RAM) {
        int found = 0;
        for (size_t i = 0; i < parser->config->device_count; ++i) {
            const rc_device_config_t *previous = &parser->config->devices[i];
            if (previous->device_number == device.controller_number &&
                previous->kind == RC_DEVICE_DFC403) found = 1;
            if ((previous->kind == RC_DEVICE_RC4819 ||
                 previous->kind == RC_DEVICE_RC4819_RAM) &&
                previous->controller_number == device.controller_number &&
                previous->unit_number == device.unit_number)
                return fail(parser, "DFC %d unit %d is already used",
                            device.controller_number, device.unit_number);
        }
        if (!found) return fail(parser, "RC4819 requires a preceding DFC403 controller");
        if (have_devno) return fail(parser, "RC4819 drives use DFC and UNITNO, not DEVNO");
        if (device.kind == RC_DEVICE_RC4819_RAM &&
            (device.cylinders == 0 || device.cylinders > 406))
            return fail(parser, "RC4819RAM CYLS must be between 1 and 406");
    }

    if (have_devno) {
        if (parser->used_devices[device.device_number] != 0) {
            return fail(parser, "device number %d is already used",
                        device.device_number);
        }
        parser->used_devices[device.device_number] = 1;
    }
    if (device.interrupt_count > 0) {
        size_t i;
        for (i = 0; i < device.interrupt_count; ++i) {
            int interrupt = device.interrupts[i];
            if (parser->used_interrupts[interrupt] != 0) {
                return fail(parser, "interrupt number %d is already used",
                            interrupt);
            }
            parser->used_interrupts[interrupt] = 1;
        }
    }
    parser->config->devices[parser->config->device_count++] = device;
    return 0;
}

static int parse_telnet(parser_t *parser, lexer_t *lexer)
{
    rc_telnet_config_t endpoint;
    int have_device = 0;
    int have_address = 0;
    int have_port = 0;
    int have_attention = 0;
    size_t index;

    if (parser->config->telnet_endpoint_count >=
        RC_CONFIG_MAX_TELNET_ENDPOINTS) {
        return fail(parser, "too many TELNET endpoints");
    }
    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.device_number = -1;
    (void)strcpy(endpoint.address, "127.0.0.1");
    endpoint.attention_mode = RC_TELNET_ATTENTION_CHARACTER;
    endpoint.attention_character = UINT8_C(0x01);
    for (;;) {
        char key[TOKEN_SIZE];
        char value[TOKEN_SIZE];
        uint32_t number;

        if (lexer_next(lexer) != 0) {
            return fail(parser, "token is too long");
        }
        if (lexer->kind == TOKEN_END) {
            break;
        }
        if (lexer->kind != TOKEN_WORD) {
            return fail(parser, "expected TELNET parameter name");
        }
        (void)strcpy(key, lexer->text);
        if (parse_parameter_value(parser, lexer, value, sizeof(value)) != 0) {
            return -1;
        }
        if (same_word(key, "DEVNO")) {
            if (have_device || parse_u32(parser, value, &number) != 0) {
                return have_device ? fail(parser, "duplicate DEVNO") : -1;
            }
            if (number > 63) {
                return fail(parser, "device number is out of range");
            }
            endpoint.device_number = (int)number;
            have_device = 1;
        } else if (same_word(key, "ADDRESS")) {
            if (have_address) {
                return fail(parser, "duplicate ADDRESS");
            }
            if (strlen(value) >= sizeof(endpoint.address)) {
                return fail(parser, "TELNET address is too long");
            }
            (void)strcpy(endpoint.address, value);
            have_address = 1;
        } else if (same_word(key, "PORT")) {
            if (have_port || parse_u32(parser, value, &number) != 0) {
                return have_port ? fail(parser, "duplicate PORT") : -1;
            }
            if (number == 0 || number > UINT16_MAX) {
                return fail(parser, "TELNET port is out of range");
            }
            endpoint.port = (uint16_t)number;
            have_port = 1;
        } else if (same_word(key, "ATTENTION")) {
            if (have_attention) {
                return fail(parser, "duplicate ATTENTION");
            }
            if (same_word(value, "CONNECT")) {
                endpoint.attention_mode = RC_TELNET_ATTENTION_CONNECT;
                endpoint.attention_character = 0;
            } else if (same_word(value, "NONE")) {
                endpoint.attention_mode = RC_TELNET_ATTENTION_NONE;
                endpoint.attention_character = 0;
            } else {
                if (parse_control_key(parser, value,
                                      &endpoint.attention_character,
                                      "TELNET attention key") != 0 ||
                    endpoint.attention_character ==
                        RC_CONFIG_SHORTCUT_DISABLED) {
                    return -1;
                }
                endpoint.attention_mode = RC_TELNET_ATTENTION_CHARACTER;
            }
            have_attention = 1;
        } else {
            return fail(parser, "unsupported TELNET parameter %s", key);
        }
    }
    if (!have_device || !have_port) {
        return fail(parser, "TELNET requires DEVNO and PORT");
    }
    {
        const rc_device_config_t *device =
            configured_device(parser->config, endpoint.device_number);

        if (device == NULL) {
            return fail(parser,
                        "TELNET device %d must be defined before its endpoint",
                        endpoint.device_number);
        }
        if (device->kind != RC_DEVICE_CONSOLE &&
            device->kind != RC_DEVICE_RC851) {
            return fail(parser, "TELNET device %d is not a terminal",
                        endpoint.device_number);
        }
    }
    for (index = 0; index < parser->config->telnet_endpoint_count; ++index) {
        const rc_telnet_config_t *other =
            &parser->config->telnet_endpoints[index];

        if (other->device_number == endpoint.device_number) {
            return fail(parser, "TELNET device %d already has an endpoint",
                        endpoint.device_number);
        }
        if (other->port == endpoint.port &&
            strcmp(other->address, endpoint.address) == 0) {
            return fail(parser, "TELNET address and port are already used");
        }
    }
    parser->config
        ->telnet_endpoints[parser->config->telnet_endpoint_count++] = endpoint;
    return 0;
}

static int parse_throttle(parser_t *parser, lexer_t *lexer)
{
    char value[TOKEN_SIZE];
    if (parser->throttle_defined) {
        return fail(parser, "THROTTLE is already defined");
    }
    if (expect(parser, lexer, TOKEN_WORD, "throttle parameter") != 0) {
        return -1;
    }
    if (!same_word(lexer->text, "VALUE")) {
        return fail(parser, "expected THROTTLE VALUE");
    }
    if (parse_parameter_value(parser, lexer, value, sizeof(value)) != 0 ||
        expect(parser, lexer, TOKEN_END, "end of line") != 0) {
        return -1;
    }
    if (!rc_throttle_parse(value, &parser->config->throttle)) {
        return fail(parser, "invalid throttle: use OFF, 1..100%%, 1..1000000000IPS, or ORIGINAL");
    }
    parser->throttle_defined = 1;
    return 0;
}

static int parse_protection(parser_t *parser, lexer_t *lexer)
{
    char value[TOKEN_SIZE];
    if (parser->protection_defined) return fail(parser, "PROTECTION is already defined");
    if (expect(parser, lexer, TOKEN_WORD, "protection parameter") != 0) return -1;
    if (!same_word(lexer->text, "MODE")) return fail(parser, "expected PROTECTION MODE");
    if (parse_parameter_value(parser, lexer, value, sizeof(value)) != 0 ||
        expect(parser, lexer, TOKEN_END, "end of line") != 0) return -1;
    if (!same_word(value, "OFF") && !same_word(value, "RC4000"))
        return fail(parser, "PROTECTION MODE must be OFF or RC4000");
    parser->config->protection_enabled = same_word(value, "RC4000");
    parser->protection_defined = 1;
    return 0;
}

static int parse_cpu(parser_t *parser, lexer_t *lexer)
{
    char value[TOKEN_SIZE];
    if (parser->cpu_defined) return fail(parser, "CPU is already defined");
    if (expect(parser, lexer, TOKEN_WORD, "CPU parameter") != 0) return -1;
    if (!same_word(lexer->text, "IX")) return fail(parser, "expected CPU IX");
    if (parse_parameter_value(parser, lexer, value, sizeof(value)) != 0 ||
        expect(parser, lexer, TOKEN_END, "end of line") != 0) return -1;
    if (!same_word(value, "YES") && !same_word(value, "NO"))
        return fail(parser, "CPU IX must be YES or NO");
    parser->config->ix_enabled = same_word(value, "YES");
    parser->cpu_defined = 1;
    return 0;
}

static int parse_line(parser_t *parser, const char *line)
{
    lexer_t lexer = {line, TOKEN_END, {0}};

    if (lexer_next(&lexer) != 0) {
        return fail(parser, "token is too long");
    }
    if (lexer.kind == TOKEN_END) {
        return 0;
    }
    if (lexer.kind != TOKEN_WORD) {
        return fail(parser, "expected directive");
    }
    if (same_word(lexer.text, "CORESIZE")) {
        return parse_coresize(parser, &lexer);
    }
    if (same_word(lexer.text, "CPU")) return parse_cpu(parser, &lexer);
    if (same_word(lexer.text, "PROTECTION")) return parse_protection(parser, &lexer);
    if (same_word(lexer.text, "MONITOR")) {
        return parse_monitor(parser, &lexer);
    }
    if (same_word(lexer.text, "OPERATOR")) {
        return parse_operator(parser, &lexer);
    }
    if (same_word(lexer.text, "DATE")) {
        return parse_date(parser, &lexer);
    }
    if (same_word(lexer.text, "THROTTLE")) {
        return parse_throttle(parser, &lexer);
    }
    if (same_word(lexer.text, "INPUT")) {
        return parse_input(parser, &lexer);
    }
    if (same_word(lexer.text, "DEFINE")) {
        return parse_device(parser, &lexer);
    }
    if (same_word(lexer.text, "TELNET")) {
        return parse_telnet(parser, &lexer);
    }
    return fail(parser, "unknown directive: %s", lexer.text);
}

void rc_config_init(rc_config_t *config)
{
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->ix_enabled = true;
    config->core_min_halfwords = 131072;
    config->core_max_halfwords = 8388607;
    config->operator_escape = UINT8_C(0x05);
    config->operator_date_key = (uint8_t)'D';
    config->console_delete_mapping = UINT8_C(0x08);
}

static void parser_init(parser_t *parser, rc_config_t *config,
                        rc_config_error_t *error)
{
    memset(parser, 0, sizeof(*parser));
    parser->config = config;
    parser->error = error;
    rc_config_init(config);
    if (error != NULL) {
        error->line = 0;
        error->message[0] = '\0';
    }
}

int rc_config_parse_file(const char *path, rc_config_t *config,
                         rc_config_error_t *error)
{
    char line[LINE_SIZE];
    parser_t parser;
    FILE *file;

    if (path == NULL || config == NULL) {
        return -1;
    }
    parser_init(&parser, config, error);
    file = fopen(path, "r");
    if (file == NULL) {
        parser.line = 0;
        return fail(&parser, "cannot open %s: %s", path, strerror(errno));
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length;
        ++parser.line;
        length = strlen(line);
        if (length > 0 && line[length - 1] != '\n' && !feof(file)) {
            (void)fclose(file);
            return fail(&parser, "configuration line is too long");
        }
        if (parse_line(&parser, line) != 0) {
            (void)fclose(file);
            return -1;
        }
    }
    if (ferror(file) != 0) {
        (void)fclose(file);
        return fail(&parser, "error while reading configuration file");
    }
    if (fclose(file) != 0) {
        return fail(&parser, "error while closing configuration file");
    }
    return 0;
}

int rc_config_parse_text(const char *text, rc_config_t *config,
                         rc_config_error_t *error)
{
    char line[LINE_SIZE];
    parser_t parser;

    if (text == NULL || config == NULL) {
        return -1;
    }
    parser_init(&parser, config, error);
    while (*text != '\0') {
        size_t length = 0;
        while (text[length] != '\0' && text[length] != '\n') {
            if (length + 1 >= sizeof(line)) {
                ++parser.line;
                return fail(&parser, "configuration line is too long");
            }
            line[length] = text[length];
            ++length;
        }
        line[length] = '\0';
        ++parser.line;
        if (parse_line(&parser, line) != 0) {
            return -1;
        }
        text += length;
        if (*text == '\n') {
            ++text;
        }
    }
    return 0;
}
