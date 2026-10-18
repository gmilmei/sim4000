#ifndef SIM4000_OPERATOR_H
#define SIM4000_OPERATOR_H

#include <stdbool.h>

/* Wrapping applies only to emulator output, never guest terminal data. */
void rc_operator_output_enable(bool enabled);
int rc_operator_printf(const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
int rc_operator_puts(const char *text);
int rc_operator_putchar(int character);

/* Callback input lets the caller keep servicing Telnet and host signals. */
void rc_operator_input_begin(void);
bool rc_operator_input_ready(void);
void rc_operator_input_read(void);
char *rc_operator_input_take(void);
void rc_operator_input_end(void);

#endif
