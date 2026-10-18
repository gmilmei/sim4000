#include "sim4000/config.h"
#include "sim4000/machine.h"

#include <stdlib.h>
#include <string.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { \
    (void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++failures; } } while (0)
enum { TERM = 1U << 20, BLIND = 1U << 19, BS = 1U << 18,
       CAN = 1U << 17, ATT = 1U << 16 };
typedef struct { uint8_t bytes[32]; size_t count; } capture_t;

static void output(void *context, uint8_t character)
{
    capture_t *capture = context;
    CHECK(capture->count < sizeof(capture->bytes));
    if (capture->count < sizeof(capture->bytes)) capture->bytes[capture->count++] = character;
}

static rc_device_result_t io(rc_console_device_t *console, rc_io_command_t command,
                             uint8_t modifier, rc_word_t *value)
{
    return console->device.io(console, command, modifier, value);
}

static void set_type(rc_console_device_t *console, uint8_t type, uint8_t prompt)
{
    rc_word_t address = 80;
    for (unsigned i = 0; i < 8; ++i) CHECK(rc_machine_write_word(console->machine, 80 + i * 2U, 0));
    CHECK(rc_machine_write_word(console->machine, 82, type));
    CHECK(rc_machine_write_word(console->machine, 86, (rc_word_t)prompt << 8));
    CHECK(io(console, RC_IO_CONTROL, 0, &address) == RC_DEVICE_ACCEPTED);
}

static rc_word_t read_byte(rc_console_device_t *console, uint8_t byte, uint8_t modifier)
{
    rc_word_t word = 0;
    CHECK(io(console, RC_IO_READ, modifier, &word) == RC_DEVICE_ACCEPTED);
    CHECK(console->busy);
    CHECK(rc_console_device_submit(console, byte));
    CHECK(io(console, RC_IO_SENSE, 0, &word) == RC_DEVICE_ACCEPTED);
    return word;
}

static void config_tests(void)
{
    rc_config_t config;
    rc_config_error_t error;
    CHECK(rc_config_parse_text("DEFINE CONSOLE DEVNO=4 INTNO=20,19\n", &config, &error) == 0);
    CHECK(config.devices[0].terminal_protocol == RC_TERMINAL_CLASSIC);
    CHECK(rc_config_parse_text("DEFINE CONSOLE DEVNO=4 INTNO=20,19 PROTOCOL=CLASSIC\n", &config, &error) == 0);
    CHECK(config.devices[0].terminal_protocol == RC_TERMINAL_CLASSIC);
    CHECK(rc_config_parse_text("DEFINE RC851 DEVNO=5 INTNO=18,17 PROTOCOL=v2\n", &config, &error) == 0);
    CHECK(config.devices[0].terminal_protocol == RC_TERMINAL_V2);
    CHECK(rc_config_parse_text("DEFINE CONSOLE DEVNO=4 INTNO=20,19 PROTOCOL=V2 PROTOCOL=V2\n", &config, &error) != 0);
    CHECK(rc_config_parse_text("DEFINE CONSOLE DEVNO=4 INTNO=20,19 PROTOCOL=RC8000\n", &config, &error) != 0);
    CHECK(rc_config_parse_text("DEFINE CLOCK DEVNO=8 INTNO=3 PROTOCOL=V2\n", &config, &error) != 0);
    CHECK(rc_config_parse_text("DEFINE RC749 DEVNO=16 INTNO=11 PROTOCOL=V2\n", &config, &error) != 0);
}

int main(void)
{
    rc_machine_t machine;
    rc_console_device_t console, classic, other;
    capture_t capture = {{0}, 0};
    rc_word_t word = 80, value;
    config_tests();
    CHECK(rc_machine_init(&machine, 256));
    rc_console_device_init(&console, 20, 19);
    CHECK(!rc_console_device_set_protocol(&console, NULL, RC_TERMINAL_V2));
    CHECK(rc_console_device_set_protocol(&console, &machine, RC_TERMINAL_V2));
    rc_console_device_set_output(&console, output, &capture);
    CHECK(console.terminal_type == 1 && console.prompt == 7);
    CHECK(io(&console, RC_IO_CONTROL, 1, &word) == RC_DEVICE_ACCEPTED);
    CHECK(rc_machine_read_word(&machine, 82, &value) && value == 0x801);
    CHECK(rc_machine_read_word(&machine, 86, &value) && value == 0x700);
    CHECK(console.device.poll(&console) == 0);
    set_type(&console, 2, '>');
    word = 80;
    CHECK(io(&console, RC_IO_CONTROL, 1, &word) == RC_DEVICE_ACCEPTED);
    for (unsigned i = 0; i < 8; ++i) {
        CHECK(rc_machine_read_word(&machine, 80 + i * 2U, &value));
        CHECK(value == (i == 1 ? 0x802U : i == 3 ? (rc_word_t)'>' << 8 : 0));
    }
    /* Invalid types and absent core preserve attributes and the entire buffer. */
    CHECK(rc_machine_write_word(&machine, 82, 3));
    CHECK(rc_machine_write_word(&machine, 86, '!' << 8));
    word = 80;
    CHECK(io(&console, RC_IO_CONTROL, 0, &word) == RC_DEVICE_BUSY);
    CHECK(console.terminal_type == 2 && console.prompt == '>');
    for (unsigned i = 0; i < 8; ++i) CHECK(rc_machine_write_word(&machine, 240 + i * 2U, 0xabcdef));
    word = 242;
    CHECK(io(&console, RC_IO_CONTROL, 1, &word) == RC_DEVICE_DISCONNECTED);
    CHECK(io(&console, RC_IO_CONTROL, 0, &word) == RC_DEVICE_DISCONNECTED);
    for (unsigned i = 0; i < 8; ++i)
        CHECK(rc_machine_read_word(&machine, 240 + i * 2U, &value) && value == 0xabcdef);
    word = 0xffffff;
    CHECK(io(&console, RC_IO_CONTROL, 1, &word) == RC_DEVICE_DISCONNECTED);
    word = 80;
    CHECK(io(&console, RC_IO_READ, 0, &word) == RC_DEVICE_ACCEPTED);
    CHECK(io(&console, RC_IO_CONTROL, 0, &word) == RC_DEVICE_BUSY);
    CHECK(io(&console, RC_IO_CONTROL, 1, &word) == RC_DEVICE_BUSY);
    CHECK(!rc_console_device_set_protocol(&console, &machine, RC_TERMINAL_CLASSIC));
    CHECK(io(&console, RC_IO_SENSE, 0, &word) == RC_DEVICE_BUSY);
    word = 'X';
    CHECK(io(&console, RC_IO_WRITE, 1, &word) == RC_DEVICE_ACCEPTED);
    CHECK(capture.count == 1 && capture.bytes[0] == '>');
    CHECK(!console.busy && console.timeout_ticks == 0);
    capture.count = 0;
    word = 0;
    CHECK(io(&console, RC_IO_WRITE, 0, &word) == RC_DEVICE_ACCEPTED);
    CHECK(capture.count == 0); /* V2 suppresses NUL output; classic does not. */

    /* Literal independent status expectations for all three control tables. */
    static const rc_word_t controls[3][32] = {
        {BS, 1, 2, 3, 4, CAN | 5, 6, 7, BS, 9, TERM | 10, 11, 12,
         TERM | 10, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
         TERM | ATT, 28, 29, 30, 31},
        {BS, TERM | 1, TERM | 2, TERM | 3, TERM | 4, CAN | 5,
         TERM | 6, TERM | 7, BS, 9, TERM | 10, TERM | 11, TERM | 12,
         TERM | 10, TERM | 14, TERM | 15, TERM | 16, TERM | 17, TERM | 18,
         TERM | 19, TERM | 20, TERM | 21, TERM | 22, TERM | 23, 24,
         TERM | 25, TERM | 26, TERM | ATT, 0, TERM | 29, TERM | 30, TERM | 31},
        {TERM | 127, TERM | 1, TERM | 2, TERM | 3, TERM | 4, TERM | 5,
         TERM | 6, TERM | 7, TERM | 8, TERM | 9, TERM | 10, TERM | 11,
         TERM | 12, TERM | 13, TERM | 14, TERM | 15, TERM | 16, TERM | 17,
         TERM | 18, TERM | 19, TERM | 20, TERM | 21, TERM | 22, TERM | 23,
         TERM | 24, TERM | 25, TERM | 26, TERM | ATT, 0, TERM | 29, TERM | 30, TERM | 31}
    };
    for (uint8_t type = 0; type < 3; ++type) {
        set_type(&console, type, '?');
        for (uint8_t modifier = 0; modifier < 4; ++modifier) {
            for (unsigned byte = 0; byte < 128; ++byte) {
                rc_word_t expected = byte == 0 ? BLIND : byte == 127 ? controls[type][0]
                    : byte < 32 ? controls[type][byte] : byte;
                capture.count = 0;
                CHECK(read_byte(&console, (uint8_t)byte, modifier) == expected);
                if ((modifier & 1U) != 0 ||
                    ((expected & BS) != 0 && (modifier & 2U) != 0)) CHECK(capture.count == 0);
                size_t echoed = capture.count;
                CHECK(io(&console, RC_IO_SENSE, 0, &word) == RC_DEVICE_ACCEPTED);
                CHECK(capture.count == echoed); /* Echo occurs only once. */
                CHECK(console.device.poll(&console) == (1U << 3));
            }
        }
    }
    set_type(&console, 0, 0);
    capture.count = 0;
    CHECK(read_byte(&console, 8, 0) == BS);
    CHECK(capture.count == 3 && memcmp(capture.bytes, "\b \b", 3) == 0);
    capture.count = 0;
    CHECK(read_byte(&console, 13, 0) == (TERM | 10));
    CHECK(capture.count == 2 && memcmp(capture.bytes, "\r\n", 2) == 0);
    capture.count = 0;
    CHECK(read_byte(&console, 127, 0) == BS && capture.count == 1 && capture.bytes[0] == '_');
    set_type(&console, 1, 7);
    capture.count = 0;
    CHECK(read_byte(&console, 127, 0) == BS && capture.count == 1 && capture.bytes[0] == 0);
    capture.count = 0;
    CHECK(rc_console_device_submit(&console, 'q'));
    CHECK(io(&console, RC_IO_READ, 1, &word) == RC_DEVICE_ACCEPTED);
    CHECK(io(&console, RC_IO_SENSE, 0, &word) == RC_DEVICE_ACCEPTED && word == 'q');
    CHECK(capture.count == 0); /* Queued and immediately completed input agree. */
    CHECK(io(&console, RC_IO_READ, 0, &word) == RC_DEVICE_ACCEPTED);
    console.device.advance(&console, RC_CONSOLE_TIMEOUT_TICKS);
    CHECK(!console.busy && console.status == (1U << 21));
    CHECK(io(&console, RC_IO_SENSE, 0, &word) == RC_DEVICE_ACCEPTED);
    CHECK(capture.count == 0);
    console.pending_interrupts = 0;
    CHECK(rc_console_device_submit(&console, 27));
    CHECK(console.device.poll(&console) == (1U << 4)); /* Idle ESC still requests attention. */
    CHECK(io(&console, RC_IO_CONTROL, 15, &word) == RC_DEVICE_DISCONNECTED);

    /* Two terminals may use different protocols; no shared type/prompt state. */
    rc_console_device_init(&classic, 18, 17);
    rc_console_device_init(&other, 16, 15);
    CHECK(rc_console_device_set_protocol(&other, &machine, RC_TERMINAL_V2));
    CHECK(other.terminal_type == 1 && other.prompt == 7 && classic.protocol == RC_TERMINAL_CLASSIC);
    rc_console_device_set_output(&classic, output, &capture);
    capture.count = 0; word = 'X';
    CHECK(io(&classic, RC_IO_WRITE, 1, &word) == RC_DEVICE_ACCEPTED);
    CHECK(capture.count == 1 && capture.bytes[0] == 'X');
    word = 0xffffff;
    CHECK(io(&classic, RC_IO_CONTROL, 1, &word) == RC_DEVICE_ACCEPTED);
    CHECK(word == 0xffffff && classic.device.poll(&classic) == (1U << 5));
    CHECK(read_byte(&classic, 13, 3) == 13); /* Legacy ignores V2 read modifiers. */
    CHECK(capture.bytes[capture.count - 1] == 13);

    /* Attribute control through a guest instruction, not just direct calls. */
    CHECK(rc_machine_attach_device(&machine, 4, &console.device));
    CHECK(rc_machine_write_word(&machine, 32, rc_instruction_encode(1, 0, false, false, 0, (4 << 6) | 5)));
    machine.cpu.stopped = false; machine.cpu.instruction_counter = 32; machine.cpu.w[0] = 80;
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(machine.cpu.exception_22 == 0 && machine.cpu.exception_23 == 0);
    CHECK(rc_machine_read_word(&machine, 82, &value) && value == 0x801);
    rc_machine_destroy(&machine);
    if (failures != 0) return EXIT_FAILURE;
    puts("Optional V2 attributes/input modes and classic isolation passed");
    return EXIT_SUCCESS;
}
