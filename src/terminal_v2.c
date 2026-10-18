#include "terminal_v2.h"
#include "sim4000/machine.h"

/* Source-guided protocol: Henrik Jacobsen, V_2.1 CTRL/CONS.PAS and
 * CONVTABS.PAS, commit 78a43d744d0e0fbf3ecf6c4510a7e1c95881486d.
 * Permission/credit is recorded in doc/impl/PROVENANCE_REVIEW.md.
 * This is an opt-in guest protocol, not an RC8000 CPU or UART model. */
enum {
    TERMINATE = 1U << 20,
    BLIND_CHARACTER = 1U << 19,
    BACKSPACE = 1U << 18,
    CANCEL = 1U << 17,
    ATTENTION = 1U << 16,
    BLIND_MODE = 1,
    NO_BACKSPACE_ECHO = 2,
    ATTRIBUTE_WORDS = 8
};

static void request(rc_console_device_t *console)
{
    if (console->io_interrupt_level < 24)
        console->pending_interrupts |= UINT32_C(1) << (23U - console->io_interrupt_level);
}

static void emit(rc_console_device_t *console, uint8_t character)
{
    if (console->output != NULL) console->output(console->output_context, character);
}

void rc_terminal_v2_complete_read(rc_console_device_t *console, uint8_t character)
{
    rc_word_t flags = 0;
    uint8_t value = character & UINT8_C(0x7f);
    console->echo_length = 0;
    if (value == 0) {
        flags = BLIND_CHARACTER;
    } else if (value >= 32 && value != 127) {
        console->echo[0] = value;
        console->echo_length = 1;
    } else {
        /* DEL selects entry zero; a literal NUL is a blind character instead. */
        uint8_t key = value == 127 ? 0 : value;
        value = key;
        if (console->terminal_type == 2) {
            if (key != 28) flags = TERMINATE;
            if (key == 0) value = 127;
            if (key == 27) { value = 0; flags |= ATTENTION; }
            if (key == 28) value = 0;
        } else {
            console->echo[0] = key;
            console->echo_length = 1;
            if (key == 0) {
                flags = BACKSPACE;
                console->echo[0] = console->terminal_type == 0 ? (uint8_t)'_' : 0;
            } else if (key == 5) {
                flags = CANCEL; console->echo[0] = (uint8_t)'^';
            } else if (key == 8) {
                flags = BACKSPACE; value = 0;
                console->echo[0] = 8; console->echo[1] = ' '; console->echo[2] = 8;
                console->echo_length = 3;
            } else if (key == 10 || key == 13) {
                flags = TERMINATE; value = 10;
                if (key == 13) { console->echo[1] = 10; console->echo_length = 2; }
            } else if (key == 27) {
                flags = TERMINATE | ATTENTION; value = 0;
                if (console->terminal_type == 1) console->echo_length = 0;
            } else if (console->terminal_type == 1) {
                console->echo_length = 0;
                if (key == 9 || key == 24) {
                    console->echo[0] = key; console->echo_length = 1;
                } else if (key == 28) value = 0;
                else flags = TERMINATE;
            }
        }
    }
    if ((console->read_modifier & BLIND_MODE) != 0 ||
        ((flags & BACKSPACE) != 0 && (console->read_modifier & NO_BACKSPACE_ECHO) != 0))
        console->echo_length = 0;
    console->status = flags | value;
    console->echo_on_sense = console->echo_length != 0;
    console->busy = false;
    console->timeout_ticks = 0;
    request(console);
}

static rc_device_result_t attributes(rc_console_device_t *console, uint8_t modifier,
                                      rc_word_t *word)
{
    rc_word_t message[ATTRIBUTE_WORDS] = {0};
    rc_addr_t address = *word & UINT32_C(0x00fffffe);
    rc_machine_t *machine = console->machine;
    if (console->busy) return RC_DEVICE_BUSY;
    /* Validate the complete block before any core or attribute mutation.
       Never wrap a block across the end of the architectural address space. */
    if (machine == NULL || machine->core.halfword_count < ATTRIBUTE_WORDS * 2U ||
        address > machine->core.halfword_count - ATTRIBUTE_WORDS * 2U)
        return RC_DEVICE_DISCONNECTED;
    if (modifier == 0) {
        for (unsigned i = 0; i < ATTRIBUTE_WORDS; ++i)
            if (!rc_machine_read_word(machine, address + i * 2U, &message[i]))
                return RC_DEVICE_DISCONNECTED;
        if ((message[1] & UINT32_C(0x3ff)) > 2) return RC_DEVICE_BUSY;
        console->terminal_type = (uint8_t)(message[1] & UINT32_C(0x3ff));
        console->prompt = (uint8_t)(message[3] >> 8);
    } else {
        message[1] = console->terminal_type | (1U << 11); /* echo.yes */
        message[3] = (rc_word_t)console->prompt << 8;
        for (unsigned i = 0; i < ATTRIBUTE_WORDS; ++i)
            if (!rc_machine_write_word(machine, address + i * 2U, message[i]))
                return RC_DEVICE_DISCONNECTED;
    }
    /* Attribute controls are synchronous and do not assert an I/O interrupt. */
    return RC_DEVICE_ACCEPTED;
}

rc_device_result_t rc_terminal_v2_io(rc_console_device_t *console,
                                     rc_io_command_t command,
                                     uint8_t modifier, rc_word_t *word)
{
    switch (command) {
    case RC_IO_SENSE:
        if (console->busy) return RC_DEVICE_BUSY;
        *word = console->status;
        if (console->echo_on_sense)
            for (size_t i = 0; i < console->echo_length; ++i) emit(console, console->echo[i]);
        console->echo_on_sense = false;
        return RC_DEVICE_ACCEPTED;
    case RC_IO_CONTROL:
        if (modifier <= 1) return attributes(console, modifier, word);
        /* Disconnect and expander protocols are deliberately deferred. */
        return RC_DEVICE_DISCONNECTED;
    case RC_IO_READ:
        if (console->busy) return RC_DEVICE_BUSY;
        console->read_modifier = modifier;
        console->echo_on_sense = false;
        if (console->input_count == 0) {
            console->busy = true;
            console->timeout_ticks = RC_CONSOLE_TIMEOUT_TICKS;
        } else {
            uint8_t character = console->input[console->input_head];
            console->input_head = (console->input_head + 1U) % RC_CONSOLE_QUEUE_SIZE;
            --console->input_count;
            rc_terminal_v2_complete_read(console, character);
        }
        return RC_DEVICE_ACCEPTED;
    case RC_IO_WRITE:
        console->status = modifier == 1 ? console->prompt : *word & UINT32_C(0x7f);
        console->echo_on_sense = false;
        console->echo_length = 0;
        console->busy = false;
        console->timeout_ticks = 0;
        if (console->status != 0) emit(console, (uint8_t)console->status);
        request(console);
        return RC_DEVICE_ACCEPTED;
    }
    return RC_DEVICE_DISCONNECTED;
}
