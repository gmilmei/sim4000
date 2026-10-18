#ifndef SIM4000_MACHINE_H
#define SIM4000_MACHINE_H

#include <stdbool.h>
#include <stddef.h>

#include "sim4000/core.h"
#include "sim4000/cpu.h"
#include "sim4000/decode.h"
#include "sim4000/device.h"

typedef enum {
    RC_STEP_OK = 0,
    RC_STEP_STOPPED,
    RC_STEP_ADDRESS_EXCEPTION,
    RC_STEP_ILLEGAL_INSTRUCTION,
    RC_STEP_UNIMPLEMENTED
} rc_step_result_t;

typedef enum {
    RC_CONTROL_OK = 0,
    RC_CONTROL_NOT_IN_RESET,
    RC_CONTROL_ADDRESS_ERROR,
    RC_CONTROL_AUTOLOAD_ERROR
} rc_control_result_t;

typedef enum {
    RC_CONTROL_INTERRUPT_MASK,
    RC_CONTROL_INTERRUPT_REQUESTS,
    RC_CONTROL_EXCEPTION_21,
    RC_CONTROL_EXCEPTION_22,
    RC_CONTROL_EXCEPTION_23
} rc_control_register_t;

typedef void (*rc_machine_write_observer_t)(void *context,
                                            rc_addr_t address,
                                            rc_word_t previous_value,
                                            rc_word_t new_value);

typedef struct rc_machine {
    rc_core_t core;
    rc_cpu_t cpu;
    bool ix_enabled; /* Optional opcode 61; retained across CPU/core reset. */
    bool protection_enabled; /* Explicit opt-in, retained across reset. */
    rc_instruction_t last_instruction;
    rc_addr_t last_instruction_address;
    rc_word_t last_effective_address;
    rc_device_t *devices[RC_DEVICE_COUNT];
    uint8_t last_io_device;
    uint8_t last_io_command;
    uint8_t last_io_modifier;
    uint64_t instructions_executed;
    uint64_t interrupts_serviced;
    uint64_t io_operations;
    uint64_t virtual_time_ticks;
    rc_machine_write_observer_t write_observer;
    void *write_observer_context;
} rc_machine_t;

bool rc_machine_init(rc_machine_t *machine, size_t halfword_count);
void rc_machine_destroy(rc_machine_t *machine);
void rc_machine_reset(rc_machine_t *machine);

bool rc_machine_read_halfword(const rc_machine_t *machine, rc_addr_t address,
                              rc_halfword_t *value);
bool rc_machine_write_halfword(rc_machine_t *machine, rc_addr_t address,
                               rc_halfword_t value);
bool rc_machine_read_word(const rc_machine_t *machine, rc_addr_t address,
                          rc_word_t *value);
bool rc_machine_write_word(rc_machine_t *machine, rc_addr_t address,
                           rc_word_t value);
void rc_machine_set_write_observer(rc_machine_t *machine,
                                   rc_machine_write_observer_t observer,
                                   void *context);
bool rc_machine_attach_device(rc_machine_t *machine, unsigned device_number,
                              rc_device_t *device);
bool rc_machine_detach_device(rc_machine_t *machine, unsigned device_number,
                              const rc_device_t *device);
void rc_machine_advance_time(rc_machine_t *machine, uint64_t ticks);
void rc_machine_poll_devices(rc_machine_t *machine);

rc_control_result_t rc_machine_control_reset(rc_machine_t *machine);
rc_control_result_t rc_machine_control_start(rc_machine_t *machine);
rc_control_result_t rc_machine_control_autoload(rc_machine_t *machine);
void rc_machine_control_interrupts(rc_machine_t *machine, bool enabled);
bool rc_machine_set_control_register(rc_machine_t *machine,
                                     rc_control_register_t control,
                                     rc_word_t value);

rc_step_result_t rc_machine_step(rc_machine_t *machine);

#endif
