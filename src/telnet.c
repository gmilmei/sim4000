#define _POSIX_C_SOURCE 200809L

#include "telnet.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum {
    TELNET_SE = 240,
    TELNET_BREAK = 243,
    TELNET_SB = 250,
    TELNET_WILL = 251,
    TELNET_WONT = 252,
    TELNET_DO = 253,
    TELNET_DONT = 254,
    TELNET_IAC = 255,
    TELNET_OPT_ECHO = 1,
    TELNET_OPT_SUPPRESS_GO_AHEAD = 3,
    TELNET_DATA = 0,
    TELNET_COMMAND,
    TELNET_OPTION,
    TELNET_SUBNEGOTIATION,
    TELNET_SUBNEGOTIATION_COMMAND,
    ANSI_INPUT_DATA = 0,
    ANSI_INPUT_ESCAPE,
    ANSI_INPUT_CSI,
    ANSI_INPUT_SS3,
    ANSI_INPUT_STRING,
    ANSI_INPUT_STRING_ESCAPE,
    RC851_NORMAL = 0,
    RC851_CURSOR_COLUMN,
    RC851_CURSOR_ROW
};

static void set_error(char *error, size_t error_size, const char *format, ...)
{
    va_list arguments;

    if (error == NULL || error_size == 0) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    return flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ? -1 : 0;
}

static bool output_byte(rc_telnet_endpoint_t *endpoint, uint8_t byte)
{
    size_t tail;

    if (endpoint->output_count == RC_TELNET_OUTPUT_BUFFER_SIZE) {
        ++endpoint->dropped_output;
        return false;
    }
    tail = (endpoint->output_head + endpoint->output_count) %
           RC_TELNET_OUTPUT_BUFFER_SIZE;
    endpoint->output[tail] = byte;
    ++endpoint->output_count;
    return true;
}

static void flush_output_cr(rc_telnet_endpoint_t *endpoint)
{
    if (endpoint->client_fd >= 0 && endpoint->output_pending_cr) {
        endpoint->output_pending_cr = false;
        (void)output_byte(endpoint, UINT8_C('\r'));
        (void)output_byte(endpoint, 0);
    }
}

static void output_text(rc_telnet_endpoint_t *endpoint, const char *text)
{
    if (endpoint->client_fd < 0) {
        return;
    }
    flush_output_cr(endpoint);
    while (*text != '\0') {
        (void)output_byte(endpoint, (uint8_t)*text++);
    }
}

static void output_position(rc_telnet_endpoint_t *endpoint)
{
    char sequence[24];

    (void)snprintf(sequence, sizeof(sequence), "\033[%u;%uH",
                   (unsigned)endpoint->rc851_row,
                   (unsigned)endpoint->rc851_column);
    output_text(endpoint, sequence);
}

static void output_nvt(rc_telnet_endpoint_t *endpoint, uint8_t character)
{
    if (endpoint->client_fd < 0 || character == UINT8_C(0x7f)) {
        return;
    }
    if (endpoint->output_pending_cr) {
        endpoint->output_pending_cr = false;
        (void)output_byte(endpoint, UINT8_C('\r'));
        (void)output_byte(endpoint,
                          character == UINT8_C('\n') ? UINT8_C('\n') : 0);
        if (character == UINT8_C('\n')) {
            return;
        }
    }
    if (character == UINT8_C('\r')) {
        endpoint->output_pending_cr = true;
    } else {
        (void)output_byte(endpoint, character);
    }
}

static void telnet_reply(rc_telnet_endpoint_t *endpoint, uint8_t command,
                         uint8_t option)
{
    (void)output_byte(endpoint, TELNET_IAC);
    (void)output_byte(endpoint, command);
    (void)output_byte(endpoint, option);
}

static bool input_byte(rc_telnet_endpoint_t *endpoint, uint8_t byte)
{
    size_t tail;

    if (byte == UINT8_C(0x7f) &&
        endpoint->delete_mapping != UINT8_C(0xff)) {
        byte = endpoint->delete_mapping;
    }
    if (byte > UINT8_C(0x7f)) {
        return true;
    }
    if (endpoint->input_count == RC_TELNET_INPUT_BUFFER_SIZE) {
        ++endpoint->dropped_input;
        return false;
    }
    tail = (endpoint->input_head + endpoint->input_count) %
           RC_TELNET_INPUT_BUFFER_SIZE;
    endpoint->input[tail] = byte;
    ++endpoint->input_count;
    return true;
}

static void process_terminal_input(rc_telnet_endpoint_t *endpoint,
                                   uint8_t byte)
{
    if (endpoint->ansi_input_state == ANSI_INPUT_DATA &&
        endpoint->attention_mode == RC_TELNET_ATTENTION_CHARACTER &&
        byte == endpoint->attention_character) {
        rc_console_device_attention(endpoint->console);
        ++endpoint->attention_requests;
        return;
    }
    switch (endpoint->ansi_input_state) {
    case ANSI_INPUT_DATA:
        if (byte == UINT8_C(0x1b)) {
            endpoint->ansi_input_state = ANSI_INPUT_ESCAPE;
        } else {
            (void)input_byte(endpoint, byte);
        }
        break;
    case ANSI_INPUT_ESCAPE:
        endpoint->ansi_parameter = 0;
        endpoint->ansi_have_parameter = false;
        if (byte == (uint8_t)'[') {
            endpoint->ansi_input_state = ANSI_INPUT_CSI;
        } else if (byte == (uint8_t)'O') {
            endpoint->ansi_input_state = ANSI_INPUT_SS3;
        } else if (byte == (uint8_t)']' || byte == (uint8_t)'P' ||
                   byte == (uint8_t)'_' || byte == (uint8_t)'^') {
            endpoint->ansi_input_state = ANSI_INPUT_STRING;
        } else {
            endpoint->ansi_input_state = ANSI_INPUT_DATA;
        }
        break;
    case ANSI_INPUT_CSI:
        if (byte >= (uint8_t)'0' && byte <= (uint8_t)'9') {
            endpoint->ansi_have_parameter = true;
            if (endpoint->ansi_parameter < 1000U) {
                endpoint->ansi_parameter =
                    endpoint->ansi_parameter * 10U +
                    (unsigned)(byte - (uint8_t)'0');
            }
        } else if (byte >= UINT8_C(0x40) && byte <= UINT8_C(0x7e)) {
            if (byte == (uint8_t)'~' && endpoint->ansi_have_parameter &&
                endpoint->ansi_parameter == 3U) {
                (void)input_byte(endpoint, UINT8_C(0x7f));
            }
            endpoint->ansi_input_state = ANSI_INPUT_DATA;
        }
        break;
    case ANSI_INPUT_SS3:
        endpoint->ansi_input_state = ANSI_INPUT_DATA;
        break;
    case ANSI_INPUT_STRING:
        if (byte == UINT8_C(0x07)) {
            endpoint->ansi_input_state = ANSI_INPUT_DATA;
        } else if (byte == UINT8_C(0x1b)) {
            endpoint->ansi_input_state = ANSI_INPUT_STRING_ESCAPE;
        }
        break;
    case ANSI_INPUT_STRING_ESCAPE:
        endpoint->ansi_input_state = byte == (uint8_t)'\\'
            ? ANSI_INPUT_DATA : ANSI_INPUT_STRING;
        break;
    default:
        endpoint->ansi_input_state = ANSI_INPUT_DATA;
        break;
    }
}

static void process_data_byte(rc_telnet_endpoint_t *endpoint, uint8_t byte)
{
    if (endpoint->input_pending_cr) {
        process_terminal_input(endpoint, UINT8_C('\r'));
        endpoint->input_pending_cr = false;
        if (byte == 0 || byte == UINT8_C('\n')) {
            return;
        }
    }
    if (byte == TELNET_IAC) {
        endpoint->telnet_state = TELNET_COMMAND;
    } else if (byte == UINT8_C('\r')) {
        endpoint->input_pending_cr = true;
    } else {
        process_terminal_input(endpoint, byte);
    }
}

static void process_telnet_byte(rc_telnet_endpoint_t *endpoint, uint8_t byte)
{
    switch (endpoint->telnet_state) {
    case TELNET_DATA:
        process_data_byte(endpoint, byte);
        break;
    case TELNET_COMMAND:
        if (byte == TELNET_IAC) {
            endpoint->telnet_state = TELNET_DATA;
        } else if (byte == TELNET_WILL || byte == TELNET_WONT ||
                   byte == TELNET_DO || byte == TELNET_DONT) {
            endpoint->telnet_command = byte;
            endpoint->telnet_state = TELNET_OPTION;
        } else if (byte == TELNET_SB) {
            endpoint->telnet_state = TELNET_SUBNEGOTIATION;
        } else if (byte == TELNET_BREAK) {
            rc_console_device_attention(endpoint->console);
            ++endpoint->attention_requests;
            endpoint->telnet_state = TELNET_DATA;
        } else {
            endpoint->telnet_state = TELNET_DATA;
        }
        break;
    case TELNET_OPTION:
        if (endpoint->telnet_command == TELNET_WILL &&
            byte != TELNET_OPT_SUPPRESS_GO_AHEAD) {
            telnet_reply(endpoint, TELNET_DONT, byte);
        } else if (endpoint->telnet_command == TELNET_DO &&
                   byte != TELNET_OPT_ECHO &&
                   byte != TELNET_OPT_SUPPRESS_GO_AHEAD) {
            telnet_reply(endpoint, TELNET_WONT, byte);
        }
        endpoint->telnet_state = TELNET_DATA;
        break;
    case TELNET_SUBNEGOTIATION:
        if (byte == TELNET_IAC) {
            endpoint->telnet_state = TELNET_SUBNEGOTIATION_COMMAND;
        }
        break;
    case TELNET_SUBNEGOTIATION_COMMAND:
        endpoint->telnet_state = byte == TELNET_SE
            ? TELNET_DATA : TELNET_SUBNEGOTIATION;
        break;
    default:
        endpoint->telnet_state = TELNET_DATA;
        break;
    }
}

void rc_telnet_endpoint_receive_bytes(rc_telnet_endpoint_t *endpoint,
                                      const uint8_t *bytes, size_t count)
{
    size_t index;

    if (endpoint == NULL || (bytes == NULL && count != 0)) {
        return;
    }
    for (index = 0; index < count; ++index) {
        process_telnet_byte(endpoint, bytes[index]);
    }
}

static void disconnect_client(rc_telnet_endpoint_t *endpoint)
{
    if (endpoint->client_fd >= 0) {
        (void)close(endpoint->client_fd);
    }
    endpoint->client_fd = -1;
    endpoint->output_head = 0;
    endpoint->output_count = 0;
    endpoint->telnet_state = TELNET_DATA;
    endpoint->ansi_input_state = ANSI_INPUT_DATA;
    endpoint->input_pending_cr = false;
    endpoint->output_pending_cr = false;
}

void rc_telnet_endpoint_begin_session(rc_telnet_endpoint_t *endpoint)
{
    if (endpoint == NULL) {
        return;
    }
    endpoint->telnet_state = TELNET_DATA;
    endpoint->ansi_input_state = ANSI_INPUT_DATA;
    endpoint->input_pending_cr = false;
    endpoint->output_pending_cr = false;
    if (endpoint->attention_mode == RC_TELNET_ATTENTION_CONNECT &&
        endpoint->console != NULL) {
        rc_console_device_attention(endpoint->console);
        ++endpoint->attention_requests;
    }
}

static void accept_clients(rc_telnet_endpoint_t *endpoint)
{
    for (;;) {
        int client = accept(endpoint->listener_fd, NULL, NULL);

        if (client < 0) {
            return;
        }
        if (set_nonblocking(client) != 0) {
            (void)close(client);
            continue;
        }
        if (endpoint->client_fd >= 0) {
            static const char busy[] = "Terminal is already connected.\r\n";

            (void)send(client, busy, sizeof(busy) - 1U, MSG_NOSIGNAL);
            (void)close(client);
            ++endpoint->rejected_connections;
            continue;
        }
        endpoint->client_fd = client;
        ++endpoint->accepted_connections;
        rc_telnet_endpoint_begin_session(endpoint);
        telnet_reply(endpoint, TELNET_WILL, TELNET_OPT_ECHO);
        telnet_reply(endpoint, TELNET_WILL, TELNET_OPT_SUPPRESS_GO_AHEAD);
        telnet_reply(endpoint, TELNET_DO, TELNET_OPT_SUPPRESS_GO_AHEAD);
        if (endpoint->terminal_kind == RC_TELNET_TERMINAL_RC851) {
            output_text(endpoint, "\033[2J\033[H");
            output_position(endpoint);
        }
    }
}

static void receive_input(rc_telnet_endpoint_t *endpoint)
{
    uint8_t buffer[512];

    for (;;) {
        ssize_t count = recv(endpoint->client_fd, buffer, sizeof(buffer), 0);
        if (count == 0) {
            if (endpoint->input_pending_cr) {
                process_terminal_input(endpoint, UINT8_C('\r'));
                endpoint->input_pending_cr = false;
            }
            disconnect_client(endpoint);
            return;
        }
        if (count < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                disconnect_client(endpoint);
            }
            return;
        }
        rc_telnet_endpoint_receive_bytes(endpoint, buffer, (size_t)count);
    }
}

static void submit_input(rc_telnet_endpoint_t *endpoint)
{
    while (endpoint->input_count != 0) {
        uint8_t byte = endpoint->input[endpoint->input_head];

        if (!rc_console_device_submit(endpoint->console, byte)) {
            return;
        }
        endpoint->input_head =
            (endpoint->input_head + 1U) % RC_TELNET_INPUT_BUFFER_SIZE;
        --endpoint->input_count;
    }
}

static void send_output(rc_telnet_endpoint_t *endpoint)
{
    while (endpoint->output_count != 0) {
        size_t contiguous = RC_TELNET_OUTPUT_BUFFER_SIZE - endpoint->output_head;
        ssize_t count;

        if (contiguous > endpoint->output_count) {
            contiguous = endpoint->output_count;
        }
        count = send(endpoint->client_fd,
                     &endpoint->output[endpoint->output_head], contiguous,
                     MSG_NOSIGNAL);
        if (count == 0) {
            disconnect_client(endpoint);
            return;
        }
        if (count < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                disconnect_client(endpoint);
            }
            return;
        }
        endpoint->output_head =
            (endpoint->output_head + (size_t)count) %
            RC_TELNET_OUTPUT_BUFFER_SIZE;
        endpoint->output_count -= (size_t)count;
    }
}

int rc_telnet_endpoint_init(rc_telnet_endpoint_t *endpoint,
                            const char *address, uint16_t port,
                            int device_number, rc_console_device_t *console,
                            rc_telnet_terminal_t terminal_kind,
                            uint8_t delete_mapping,
                            rc_telnet_attention_t attention_mode,
                            uint8_t attention_character, char *error,
                            size_t error_size)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *candidate;
    char service[16];
    int result;

    if (endpoint == NULL || address == NULL || console == NULL ||
        attention_mode < RC_TELNET_ATTENTION_NONE ||
        attention_mode > RC_TELNET_ATTENTION_CONNECT ||
        (attention_mode == RC_TELNET_ATTENTION_CHARACTER &&
         attention_character > UINT8_C(0x1f))) {
        set_error(error, error_size, "invalid TELNET endpoint arguments");
        return -1;
    }
    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->listener_fd = -1;
    endpoint->client_fd = -1;
    endpoint->device_number = device_number;
    endpoint->port = port;
    if (strlen(address) >= sizeof(endpoint->address)) {
        set_error(error, error_size, "TELNET address is too long");
        return -1;
    }
    (void)strcpy(endpoint->address, address);
    endpoint->console = console;
    endpoint->terminal_kind = terminal_kind;
    endpoint->delete_mapping = delete_mapping;
    endpoint->attention_mode = attention_mode;
    endpoint->attention_character = attention_character;
    endpoint->rc851_row = 1;
    endpoint->rc851_column = 1;
    (void)snprintf(service, sizeof(service), "%u", (unsigned)port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    result = getaddrinfo(address, service, &hints, &addresses);
    if (result != 0) {
        set_error(error, error_size, "cannot resolve TELNET address %s: %s",
                  address, gai_strerror(result));
        return -1;
    }
    for (candidate = addresses; candidate != NULL;
         candidate = candidate->ai_next) {
        int listener = socket(candidate->ai_family, candidate->ai_socktype,
                              candidate->ai_protocol);
        int enabled = 1;

        if (listener < 0) {
            continue;
        }
        (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enabled,
                         sizeof(enabled));
        if (set_nonblocking(listener) == 0 &&
            bind(listener, candidate->ai_addr, candidate->ai_addrlen) == 0 &&
            listen(listener, 4) == 0) {
            endpoint->listener_fd = listener;
            break;
        }
        (void)close(listener);
    }
    freeaddrinfo(addresses);
    if (endpoint->listener_fd < 0) {
        set_error(error, error_size, "cannot listen on %s:%u: %s", address,
                  (unsigned)port, strerror(errno));
        return -1;
    }
    return 0;
}

void rc_telnet_endpoint_destroy(rc_telnet_endpoint_t *endpoint)
{
    if (endpoint == NULL) {
        return;
    }
    disconnect_client(endpoint);
    if (endpoint->listener_fd >= 0) {
        (void)close(endpoint->listener_fd);
    }
    endpoint->listener_fd = -1;
}

void rc_telnet_endpoint_output(void *context, uint8_t character)
{
    rc_telnet_endpoint_t *endpoint = context;

    if (endpoint == NULL) {
        return;
    }
    if (endpoint->terminal_kind == RC_TELNET_TERMINAL_CONSOLE) {
        output_nvt(endpoint, character);
        return;
    }
    if (endpoint->rc851_state == RC851_CURSOR_COLUMN) {
        if (character >= 96U) {
            endpoint->rc851_column = (uint8_t)(character - 95U);
        } else if (character >= 64U) {
            endpoint->rc851_column = (uint8_t)(character - 31U);
        } else {
            endpoint->rc851_column = (uint8_t)(character + 33U);
        }
        endpoint->rc851_state = RC851_CURSOR_ROW;
        return;
    }
    if (endpoint->rc851_state == RC851_CURSOR_ROW) {
        unsigned row = character >= 96U ? (unsigned)character - 95U : 24U;

        endpoint->rc851_row = (uint8_t)(row > 24U ? 24U : row);
        if (endpoint->rc851_column > 80U) {
            endpoint->rc851_column = 80U;
        }
        endpoint->rc851_scroll = false;
        endpoint->rc851_state = RC851_NORMAL;
        output_position(endpoint);
        return;
    }
    if (character >= 32U && character <= 126U) {
        output_nvt(endpoint, character);
        ++endpoint->rc851_column;
        if (endpoint->rc851_column > 80U) {
            endpoint->rc851_column = 1;
            endpoint->rc851_scroll = true;
            if (endpoint->rc851_row < 24U) {
                ++endpoint->rc851_row;
            } else {
                output_text(endpoint, "\033[S");
            }
            output_position(endpoint);
        } else {
            endpoint->rc851_scroll = false;
        }
        return;
    }
    switch (character) {
    case 5:
        endpoint->rc851_column = 1;
        output_nvt(endpoint, UINT8_C('\r'));
        output_text(endpoint, "\033[2K");
        break;
    case 6:
        endpoint->rc851_state = RC851_CURSOR_COLUMN;
        break;
    case 7:
        output_nvt(endpoint, character);
        break;
    case 8:
        if (endpoint->rc851_column > 1U) {
            --endpoint->rc851_column;
            output_nvt(endpoint, character);
        }
        break;
    case 9:
        if (endpoint->rc851_row == 24U && endpoint->rc851_column > 76U) {
            output_text(endpoint, "\033[S");
        } else {
            unsigned column = (unsigned)endpoint->rc851_column + 4U -
                              (((unsigned)endpoint->rc851_column - 1U) & 3U);

            if (column > 80U) {
                column -= 80U;
                if (endpoint->rc851_row < 24U) {
                    ++endpoint->rc851_row;
                }
            }
            endpoint->rc851_column = (uint8_t)column;
            output_position(endpoint);
        }
        break;
    case 10:
        if (!endpoint->rc851_scroll) {
            if (endpoint->rc851_row < 24U) {
                ++endpoint->rc851_row;
            }
            output_nvt(endpoint, character);
        }
        endpoint->rc851_scroll = false;
        break;
    case 12:
        endpoint->rc851_row = 1;
        endpoint->rc851_column = 1;
        output_text(endpoint, "\033[2J\033[H");
        break;
    case 13:
        endpoint->rc851_column = 1;
        output_nvt(endpoint, character);
        break;
    case 24:
        if (endpoint->rc851_row == 24U && endpoint->rc851_column == 80U) {
            endpoint->rc851_column = 1;
            output_text(endpoint, "\033[S");
            output_position(endpoint);
        } else {
            ++endpoint->rc851_column;
            if (endpoint->rc851_column > 80U) {
                endpoint->rc851_column = 1;
                ++endpoint->rc851_row;
            }
            output_position(endpoint);
        }
        break;
    case 26:
        if (endpoint->rc851_row > 1U) {
            --endpoint->rc851_row;
            output_text(endpoint, "\033[A");
        }
        break;
    case 29:
        endpoint->rc851_row = 1;
        endpoint->rc851_column = 1;
        endpoint->rc851_scroll = false;
        output_text(endpoint, "\033[H");
        break;
    case 30:
        output_text(endpoint, "\033[K");
        break;
    case 31:
        output_text(endpoint, "\033[J");
        break;
    default:
        break;
    }
}

void rc_telnet_endpoint_pump(rc_telnet_endpoint_t *endpoint)
{
    if (endpoint == NULL || endpoint->listener_fd < 0) {
        return;
    }
    accept_clients(endpoint);
    if (endpoint->client_fd < 0) {
        return;
    }
    receive_input(endpoint);
    if (endpoint->client_fd < 0) {
        return;
    }
    submit_input(endpoint);
    if (endpoint->output_pending_cr) {
        flush_output_cr(endpoint);
    }
    send_output(endpoint);
}
