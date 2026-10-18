#include "sim4000/cpu.h"

#include <string.h>

void rc_cpu_reset(rc_cpu_t *cpu)
{
    if (cpu == NULL) {
        return;
    }
    memset(cpu, 0, sizeof(*cpu));
    cpu->instruction_counter = 8;
    cpu->interrupt_mask = UINT32_C(0x00800000);
    cpu->protection_register = UINT8_C(0x80);
    cpu->monitor_mode = true;
    cpu->reset_mode = true;
    cpu->stopped = true;
}

void rc_cpu_set_protection_register(rc_cpu_t *cpu, uint8_t value)
{
    if (cpu != NULL) {
        cpu->protection_register = (uint8_t)(value | UINT8_C(0x80));
    }
}

bool rc_cpu_key_is_protected(const rc_cpu_t *cpu, unsigned key)
{
    if (cpu == NULL || key > 7U) {
        return true;
    }
    return ((unsigned)(cpu->protection_register | UINT8_C(0x80)) &
            (UINT32_C(0x80) >> key)) != 0;
}

void rc_cpu_request_interrupt(rc_cpu_t *cpu, unsigned interrupt_level)
{
    if (cpu != NULL && interrupt_level < RC_INTERRUPT_COUNT) {
        cpu->interrupt_requests |= UINT32_C(1) << (23U - interrupt_level);
    }
}

void rc_cpu_clear_interrupt(rc_cpu_t *cpu, unsigned interrupt_level)
{
    if (cpu != NULL && interrupt_level < RC_INTERRUPT_COUNT) {
        cpu->interrupt_requests &=
            ~(UINT32_C(1) << (23U - interrupt_level));
    }
}

bool rc_cpu_next_interrupt(const rc_cpu_t *cpu, unsigned *interrupt_number)
{
    uint32_t pending;
    unsigned number;

    if (cpu == NULL || interrupt_number == NULL) {
        return false;
    }
    pending = cpu->interrupt_requests & cpu->interrupt_mask;
    if (pending == 0 ||
        (!cpu->interrupts_enabled && (pending & UINT32_C(0x00800000)) == 0)) {
        return false;
    }
    for (number = 0; number < RC_INTERRUPT_COUNT; ++number) {
        if ((pending & (UINT32_C(1) << (23U - number))) != 0) {
            *interrupt_number = number * 2U;
            return true;
        }
    }
    return false;
}
