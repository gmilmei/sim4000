#ifndef SIM4000_TERMINAL_V2_H
#define SIM4000_TERMINAL_V2_H

#include "sim4000/device.h"

rc_device_result_t rc_terminal_v2_io(rc_console_device_t *console,
                                     rc_io_command_t command,
                                     uint8_t modifier, rc_word_t *word);
void rc_terminal_v2_complete_read(rc_console_device_t *console, uint8_t character);

#endif
