#ifndef SIM4000_CPU_H
#define SIM4000_CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "sim4000/types.h"

enum {
    RC_REGISTER_COUNT = 4,
    RC_INTERRUPT_COUNT = 24
};

typedef struct {
    rc_word_t w[RC_REGISTER_COUNT];
    rc_addr_t instruction_counter;
    uint32_t interrupt_mask;
    uint32_t interrupt_requests;
    uint8_t protection_register; /* RC bit 0 (host 0x80) is always set. */
    uint8_t exception_23;
    uint8_t exception_22;
    uint8_t exception_21;
    bool interrupts_enabled;
    bool monitor_mode;
    bool reset_key;
    bool autoload_key;
    bool reset_mode;
    bool stopped;
} rc_cpu_t;

void rc_cpu_reset(rc_cpu_t *cpu);
void rc_cpu_set_protection_register(rc_cpu_t *cpu, uint8_t value);
/* Invalid CPU/key inputs conservatively report protected. This predicate
 * describes PR only; monitor mode and profile enablement are separate. */
bool rc_cpu_key_is_protected(const rc_cpu_t *cpu, unsigned key);
void rc_cpu_request_interrupt(rc_cpu_t *cpu, unsigned interrupt_level);
void rc_cpu_clear_interrupt(rc_cpu_t *cpu, unsigned interrupt_level);
bool rc_cpu_next_interrupt(const rc_cpu_t *cpu, unsigned *interrupt_number);

#endif
