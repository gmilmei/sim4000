#include "sim4000/machine.h"
#include "sim4000/floating.h"

#include <string.h>

enum {
    RC_OP_AW = 0,
    RC_OP_IO = 1,
    RC_OP_BL = 2,
    RC_OP_HL = 3,
    RC_OP_LA = 4,
    RC_OP_LO = 5,
    RC_OP_LX = 6,
    RC_OP_WA = 7,
    RC_OP_WS = 8,
    RC_OP_AM = 9,
    RC_OP_WM = 10,
    RC_OP_AL = 11,
    RC_OP_ML = 12,
    RC_OP_JL = 13,
    RC_OP_JD = 14,
    RC_OP_JE = 15,
    RC_OP_XL = 16,
    RC_OP_BS = 17,
    RC_OP_BA = 18,
    RC_OP_BZ = 19,
    RC_OP_RL = 20,
    RC_OP_SP = 21,
    RC_OP_KL = 22,
    RC_OP_RS = 23,
    RC_OP_WD = 24,
    RC_OP_RX = 25,
    RC_OP_HS = 26,
    RC_OP_XS = 27,
    RC_OP_PL = 28,
    RC_OP_PS = 29,
    RC_OP_MS = 30,
    RC_OP_IS = 31,
    RC_OP_CI = 32,
    RC_OP_AC = 33,
    RC_OP_NS = 34,
    RC_OP_ND = 35,
    RC_OP_AS = 36,
    RC_OP_AD = 37,
    RC_OP_LS = 38,
    RC_OP_LD = 39,
    RC_OP_SH = 40,
    RC_OP_SL = 41,
    RC_OP_SE = 42,
    RC_OP_SN = 43,
    RC_OP_SO = 44,
    RC_OP_SZ = 45,
    RC_OP_SX = 46,
    RC_OP_IC = 47,
    RC_OP_FA = 48,
    RC_OP_FS = 49,
    RC_OP_FM = 50,
    RC_OP_KS = 51,
    RC_OP_FD = 52,
    RC_OP_CF = 53,
    RC_OP_DL = 54,
    RC_OP_DS = 55,
    RC_OP_AA = 56,
    RC_OP_SS = 57,
    RC_MAX_AM_CHAIN = 1024
};

enum {
    RC_AUTOLOAD_CHARACTER_MASK = 0x3f,
    RC_AUTOLOAD_END_BUFFER = 0x800000,
    RC_AUTOLOAD_ERROR_MASK = 0x7ff000
};

static bool register_address(rc_addr_t address, unsigned *number)
{
    address &= ~UINT32_C(1);
    if (address >= 8U) {
        return false;
    }
    *number = (unsigned)(address / 2U);
    return true;
}

bool rc_machine_init(rc_machine_t *machine, size_t halfword_count)
{
    if (machine == NULL) {
        return false;
    }
    memset(machine, 0, sizeof(*machine));
    if (!rc_core_init(&machine->core, halfword_count)) {
        return false;
    }
    rc_cpu_reset(&machine->cpu);
    machine->ix_enabled = true;
    return true;
}

void rc_machine_destroy(rc_machine_t *machine)
{
    if (machine == NULL) {
        return;
    }
    rc_core_destroy(&machine->core);
    memset(&machine->cpu, 0, sizeof(machine->cpu));
}

void rc_machine_reset(rc_machine_t *machine)
{
    if (machine == NULL) {
        return;
    }
    rc_core_reset(&machine->core);
    rc_cpu_reset(&machine->cpu);
    machine->instructions_executed = 0;
}

bool rc_machine_read_halfword(const rc_machine_t *machine, rc_addr_t address,
                              rc_halfword_t *value)
{
    unsigned number;

    if (machine == NULL || value == NULL) {
        return false;
    }
    if (register_address(address, &number)) {
        rc_word_t word = machine->cpu.w[number];
        *value = (address & 1U) == 0
            ? (rc_halfword_t)(word >> RC_HALFWORD_BITS)
            : (rc_halfword_t)(word & RC_HALFWORD_MASK);
        return true;
    }
    return rc_core_read_halfword(&machine->core, address, value);
}

bool rc_machine_write_halfword(rc_machine_t *machine, rc_addr_t address,
                               rc_halfword_t value)
{
    unsigned number;
    rc_word_t previous;
    rc_word_t current;
    rc_addr_t aligned = address & ~UINT32_C(1);

    if (machine == NULL) {
        return false;
    }
    if (register_address(address, &number)) {
        rc_word_t word = machine->cpu.w[number];
        if ((address & 1U) == 0) {
            word = ((rc_word_t)rc_halfword(value) << RC_HALFWORD_BITS) |
                   (word & RC_HALFWORD_MASK);
        } else {
            word = (word & UINT32_C(0x00fff000)) | rc_halfword(value);
        }
        machine->cpu.w[number] = word;
        return true;
    }
    if (machine->write_observer == NULL) {
        return rc_core_write_halfword(&machine->core, address, value);
    }
    if (!rc_core_read_word(&machine->core, aligned, &previous)) {
        return rc_core_write_halfword(&machine->core, address, value);
    }
    if (!rc_core_write_halfword(&machine->core, address, value) ||
        !rc_core_read_word(&machine->core, aligned, &current)) {
        return false;
    }
    machine->write_observer(machine->write_observer_context, aligned,
                            previous, current);
    return true;
}

bool rc_machine_read_word(const rc_machine_t *machine, rc_addr_t address,
                          rc_word_t *value)
{
    unsigned number;

    if (machine == NULL || value == NULL) {
        return false;
    }
    if (register_address(address, &number)) {
        *value = machine->cpu.w[number];
        return true;
    }
    return rc_core_read_word(&machine->core, address, value);
}

bool rc_machine_write_word(rc_machine_t *machine, rc_addr_t address,
                           rc_word_t value)
{
    unsigned number;
    rc_word_t previous;
    rc_addr_t aligned = address & ~UINT32_C(1);

    if (machine == NULL) {
        return false;
    }
    if (register_address(address, &number)) {
        machine->cpu.w[number] = rc_word(value);
        return true;
    }
    if (machine->write_observer == NULL) {
        return rc_core_write_word(&machine->core, aligned, value);
    }
    if (!rc_core_read_word(&machine->core, aligned, &previous) ||
        !rc_core_write_word(&machine->core, aligned, value)) {
        return false;
    }
    machine->write_observer(machine->write_observer_context, aligned,
                            previous, rc_word(value));
    return true;
}

void rc_machine_set_write_observer(rc_machine_t *machine,
                                   rc_machine_write_observer_t observer,
                                   void *context)
{
    if (machine != NULL) {
        machine->write_observer = observer;
        machine->write_observer_context = context;
    }
}

bool rc_machine_attach_device(rc_machine_t *machine, unsigned device_number,
                              rc_device_t *device)
{
    if (machine == NULL || device_number >= RC_DEVICE_COUNT) {
        return false;
    }
    machine->devices[device_number] = device;
    return true;
}

bool rc_machine_detach_device(rc_machine_t *machine, unsigned device_number,
                              const rc_device_t *device)
{
    if (machine == NULL || device_number >= RC_DEVICE_COUNT ||
        machine->devices[device_number] != device) {
        return false;
    }
    machine->devices[device_number] = NULL;
    return true;
}

void rc_machine_advance_time(rc_machine_t *machine, uint64_t ticks)
{
    unsigned device_number;

    if (machine == NULL || ticks == 0) {
        return;
    }
    machine->virtual_time_ticks += ticks;
    for (device_number = 0; device_number < RC_DEVICE_COUNT;
         ++device_number) {
        rc_device_t *device = machine->devices[device_number];
        if (device != NULL && device->advance != NULL) {
            device->advance(device->context, ticks);
        }
    }
}

void rc_machine_poll_devices(rc_machine_t *machine)
{
    unsigned device_number;

    if (machine == NULL) {
        return;
    }
    for (device_number = 0; device_number < RC_DEVICE_COUNT;
         ++device_number) {
        rc_device_t *device = machine->devices[device_number];
        if (device != NULL && device->poll != NULL) {
            machine->cpu.interrupt_requests |=
                device->poll(device->context) & RC_WORD_MASK;
        }
    }
}

static bool fetch_instruction(const rc_machine_t *machine, rc_addr_t address,
                              rc_instruction_t *instruction)
{
    rc_halfword_t operation;
    rc_halfword_t displacement;

    if (!rc_machine_read_halfword(machine, address, &operation) ||
        !rc_machine_read_halfword(machine, rc_word(address + 1U),
                                  &displacement)) {
        return false;
    }
    *instruction = rc_instruction_decode(
        ((rc_word_t)operation << RC_HALFWORD_BITS) | displacement);
    return true;
}

static bool effective_address(const rc_machine_t *machine,
                              const rc_instruction_t *instruction,
                              rc_addr_t instruction_address,
                              rc_word_t initial_displacement,
                              rc_word_t *address)
{
    rc_word_t result = initial_displacement;

    if (instruction->relative) {
        result = rc_word(result + instruction_address);
    }
    if (instruction->index_register != 0U) {
        result = rc_word(result +
                         machine->cpu.w[instruction->index_register]);
    }
    if (instruction->indirect) {
        if (!rc_machine_read_word(machine, result, &result)) {
            return false;
        }
    }
    *address = result;
    return true;
}

static bool enter_interrupt(rc_machine_t *machine)
{
    unsigned interrupt_number;
    rc_word_t service_address;

    if (!rc_cpu_next_interrupt(&machine->cpu, &interrupt_number)) {
        return false;
    }
    rc_cpu_clear_interrupt(&machine->cpu, interrupt_number / 2U);
    if (!rc_machine_write_word(machine, 8U, (rc_word_t)interrupt_number) ||
        !rc_machine_write_word(machine, 10U,
                               machine->cpu.instruction_counter) ||
        !rc_machine_read_word(machine, 12U, &service_address)) {
        return false;
    }
    service_address &= ~UINT32_C(1);
    if ((size_t)service_address >= machine->core.halfword_count) {
        return false;
    }
    machine->cpu.instruction_counter = service_address;
    machine->cpu.monitor_mode = true;
    machine->cpu.interrupts_enabled = false;
    ++machine->interrupts_serviced;
    return true;
}

static rc_step_result_t address_exception(rc_machine_t *machine)
{
    rc_cpu_request_interrupt(&machine->cpu, 0);
    return enter_interrupt(machine) ? RC_STEP_OK : RC_STEP_ADDRESS_EXCEPTION;
}

/* CPU-only checks: host/device/debugger accessors intentionally bypass PR. */
static bool cpu_write_allowed(const rc_machine_t *machine, rc_addr_t address)
{
    uint8_t key;
    return !machine->protection_enabled ||
        (rc_core_read_key(&machine->core, address, &key) &&
         (machine->cpu.monitor_mode ||
          !rc_cpu_key_is_protected(&machine->cpu, key)));
}

static bool cpu_write_word(rc_machine_t *machine, rc_addr_t address, rc_word_t value)
{
    return cpu_write_allowed(machine, address) &&
           rc_machine_write_word(machine, address, value);
}

static bool cpu_write_halfword(rc_machine_t *machine, rc_addr_t address,
                                rc_halfword_t value)
{
    return cpu_write_allowed(machine, address) &&
           rc_machine_write_halfword(machine, address, value);
}

static bool privileged_opcode(uint8_t opcode)
{
    return opcode == RC_OP_AW || opcode == RC_OP_IO || opcode == RC_OP_IC ||
           opcode == RC_OP_ML || opcode == RC_OP_JD || opcode == RC_OP_JE ||
           opcode == RC_OP_PL || opcode == RC_OP_KS;
}

/* Optional IX compatibility instruction, not a general RC8000 CPU profile.
 * Reference: Jacobsen V_2.1 UCODE/IX.ASM and OPUTIL/FLUTIL2 macros.
 * The inline word encodes signed displacement:type in two 12-bit halves.
 * Keep all reads ahead of register/flag changes, including register aliases. */
static rc_step_result_t execute_ix(rc_machine_t *machine,
                                   const rc_instruction_t *instruction,
                                   rc_word_t address)
{
    rc_word_t extension, origin, lower, upper;
    rc_word_t index = machine->cpu.w[instruction->result_register];
    if (!rc_machine_read_word(machine, machine->cpu.instruction_counter, &extension))
        return address_exception(machine);
    int32_t displacement = rc_sign_extend_halfword((rc_halfword_t)(extension >> 12));
    int32_t type = rc_sign_extend_halfword((rc_halfword_t)(extension & RC_HALFWORD_MASK));
    rc_addr_t bounds = rc_word(address + (rc_word_t)displacement) & ~UINT32_C(1);
    if (!rc_machine_read_word(machine, address & ~UINT32_C(1), &origin) ||
        !rc_machine_read_word(machine, bounds, &lower) ||
        !rc_machine_read_word(machine, rc_word(bounds - 2U), &upper))
        return address_exception(machine);
    /* The DOS macro shifts a 24-bit value left-aligned in a 32-bit register;
     * x86 masks the count to five bits. Reproduce that convention explicitly. */
    if (type > 0) index = rc_word(index << ((unsigned)type & 31U));
    bool valid = rc_sign_extend_word(index) > rc_sign_extend_word(lower) &&
                 rc_sign_extend_word(index) < rc_sign_extend_word(upper);
    if (valid) {
        machine->cpu.w[instruction->result_register] = rc_word(origin + index);
        machine->cpu.exception_22 = machine->cpu.exception_23 = 0;
    } else {
        if ((machine->cpu.interrupt_mask & UINT32_C(0x200000)) == 0) {
            machine->cpu.w[instruction->result_register] = rc_word(origin + upper);
        } else {
            rc_cpu_request_interrupt(&machine->cpu, 2);
        }
        machine->cpu.exception_22 = machine->cpu.exception_23 = 1;
    }
    machine->cpu.instruction_counter = rc_word(machine->cpu.instruction_counter + 2U);
    return RC_STEP_OK;
}

static rc_device_result_t autoload_io(rc_machine_t *machine,
                                      rc_io_command_t command,
                                      rc_word_t *transfer)
{
    rc_device_t *device = machine->devices[0];
    rc_device_result_t result = RC_DEVICE_DISCONNECTED;

    if (device != NULL && device->io != NULL) {
        result = device->io(device->context, command, 0, transfer);
    }
    machine->last_io_device = 0;
    machine->last_io_command = (uint8_t)command;
    machine->last_io_modifier = 0;
    ++machine->io_operations;
    return result;
}

static rc_step_result_t autoload_word(rc_machine_t *machine,
                                      rc_addr_t address)
{
    rc_word_t value = 0;
    unsigned character;

    for (character = 0; character < 4U; ++character) {
        rc_word_t status;
        rc_device_result_t result;

        do {
            status = 0;
            do {
                result = autoload_io(machine, RC_IO_READ, &status);
            } while (result == RC_DEVICE_BUSY);
            if (result == RC_DEVICE_DISCONNECTED) {
                if (machine->protection_enabled)
                    (void)rc_machine_control_reset(machine);
                else
                    rc_cpu_reset(&machine->cpu);
                return RC_STEP_STOPPED;
            }

            do {
                result = autoload_io(machine, RC_IO_SENSE, &status);
            } while (result == RC_DEVICE_BUSY);
            if (result == RC_DEVICE_DISCONNECTED ||
                (status & RC_AUTOLOAD_ERROR_MASK) != 0) {
                if (machine->protection_enabled)
                    (void)rc_machine_control_reset(machine);
                else
                    rc_cpu_reset(&machine->cpu);
                return RC_STEP_STOPPED;
            }
        } while ((status & RC_AUTOLOAD_END_BUFFER) != 0);

        value = rc_word((value << 6) |
                        (status & RC_AUTOLOAD_CHARACTER_MASK));
    }
    if (!rc_machine_write_word(machine, address, value)) {
        return address_exception(machine);
    }
    if (machine->protection_enabled &&
        !rc_core_write_key(&machine->core, address, 0)) {
        return address_exception(machine);
    }
    machine->cpu.exception_22 = 0;
    machine->cpu.exception_23 = 0;
    return RC_STEP_OK;
}

rc_control_result_t rc_machine_control_reset(rc_machine_t *machine)
{
    if (machine == NULL) {
        return RC_CONTROL_ADDRESS_ERROR;
    }
    if (!machine->cpu.reset_mode &&
        !rc_machine_write_word(machine, 10,
                               machine->cpu.instruction_counter)) {
        return RC_CONTROL_ADDRESS_ERROR;
    }
    machine->cpu.interrupts_enabled = false;
    machine->cpu.monitor_mode = true;
    machine->cpu.reset_key = false;
    machine->cpu.autoload_key = false;
    machine->cpu.reset_mode = true;
    machine->cpu.stopped = true;
    return RC_CONTROL_OK;
}

rc_control_result_t rc_machine_control_start(rc_machine_t *machine)
{
    rc_word_t address;

    if (machine == NULL) {
        return RC_CONTROL_ADDRESS_ERROR;
    }
    if (!machine->cpu.reset_mode) {
        return RC_CONTROL_NOT_IN_RESET;
    }
    if (!rc_machine_read_word(machine, 14, &address)) {
        return RC_CONTROL_ADDRESS_ERROR;
    }
    machine->cpu.instruction_counter = address & UINT32_C(0x007ffffe);
    machine->cpu.reset_mode = false;
    machine->cpu.stopped = false;
    return RC_CONTROL_OK;
}

rc_control_result_t rc_machine_control_autoload(rc_machine_t *machine)
{
    rc_step_result_t result;

    if (machine == NULL) {
        return RC_CONTROL_ADDRESS_ERROR;
    }
    if (!machine->cpu.reset_mode) {
        return RC_CONTROL_NOT_IN_RESET;
    }
    machine->cpu.instruction_counter = 0;
    machine->cpu.stopped = false;
    result = autoload_word(machine, 0);
    if (result != RC_STEP_OK) {
        return result == RC_STEP_ADDRESS_EXCEPTION
            ? RC_CONTROL_ADDRESS_ERROR : RC_CONTROL_AUTOLOAD_ERROR;
    }
    machine->cpu.instruction_counter = 0;
    machine->cpu.reset_mode = false;
    machine->cpu.stopped = false;
    return RC_CONTROL_OK;
}

void rc_machine_control_interrupts(rc_machine_t *machine, bool enabled)
{
    if (machine != NULL) {
        machine->cpu.interrupts_enabled = enabled;
    }
}

bool rc_machine_set_control_register(rc_machine_t *machine,
                                     rc_control_register_t control,
                                     rc_word_t value)
{
    if (machine == NULL) {
        return false;
    }
    switch (control) {
    case RC_CONTROL_INTERRUPT_MASK:
        machine->cpu.interrupt_mask = rc_word(value) |
            (machine->protection_enabled ? UINT32_C(0x800000) : 0U);
        return true;
    case RC_CONTROL_INTERRUPT_REQUESTS:
        machine->cpu.interrupt_requests = rc_word(value);
        return true;
    case RC_CONTROL_EXCEPTION_21:
        if (value > 4U) {
            return false;
        }
        machine->cpu.exception_21 = (uint8_t)value;
        return true;
    case RC_CONTROL_EXCEPTION_22:
        if (value > 1U) {
            return false;
        }
        machine->cpu.exception_22 = (uint8_t)value;
        return true;
    case RC_CONTROL_EXCEPTION_23:
        if (value > 1U) {
            return false;
        }
        machine->cpu.exception_23 = (uint8_t)value;
        return true;
    }
    return false;
}

static void set_integer_result(rc_machine_t *machine, rc_word_t left,
                               rc_word_t right, bool subtract,
                               unsigned result_register)
{
    uint64_t wide;
    rc_word_t result;
    bool overflow;

    if (subtract) {
        result = rc_word(left - right);
        machine->cpu.exception_23 = left >= right;
        overflow = ((left ^ right) & (left ^ result) &
                    UINT32_C(0x00800000)) != 0;
    } else {
        wide = (uint64_t)left + (uint64_t)right;
        result = rc_word((rc_word_t)wide);
        machine->cpu.exception_23 = wide > RC_WORD_MASK;
        overflow = ((~(left ^ right) & (left ^ result)) &
                    UINT32_C(0x00800000)) != 0;
    }
    machine->cpu.exception_22 = overflow;
    machine->cpu.w[result_register] = result;
    if (overflow) {
        rc_cpu_request_interrupt(&machine->cpu, 1);
    }
}

static unsigned previous_register(unsigned result_register)
{
    return result_register == 0U ? 3U : result_register - 1U;
}

static rc_addr_t previous_word_address(rc_addr_t address)
{
    address &= ~UINT32_C(1);
    return address == 0U ? 6U : address - 2U;
}

static uint64_t register_pair(const rc_machine_t *machine,
                              unsigned result_register)
{
    unsigned previous = previous_register(result_register);
    return ((uint64_t)machine->cpu.w[previous] << RC_WORD_BITS) |
           machine->cpu.w[result_register];
}

static void set_register_pair(rc_machine_t *machine, unsigned result_register,
                              uint64_t value)
{
    unsigned previous = previous_register(result_register);
    machine->cpu.w[previous] =
        (rc_word_t)((value >> RC_WORD_BITS) & RC_WORD_MASK);
    machine->cpu.w[result_register] = (rc_word_t)(value & RC_WORD_MASK);
}

static int64_t signed_register_pair(const rc_machine_t *machine,
                                    unsigned result_register)
{
    const uint64_t pair_mask = UINT64_C(0x0000ffffffffffff);
    uint64_t value = register_pair(machine, result_register);

    if ((value & UINT64_C(0x0000800000000000)) == 0) {
        return (int64_t)value;
    }
    return -(int64_t)(((~value) + 1U) & pair_mask);
}

static uint64_t arithmetic_shift_right(uint64_t value, unsigned count,
                                       unsigned width)
{
    uint64_t mask = (UINT64_C(1) << width) - 1U;
    uint64_t sign = UINT64_C(1) << (width - 1U);

    value &= mask;
    if (count >= width) {
        return (value & sign) != 0 ? mask : 0;
    }
    if (count == 0U) {
        return value;
    }
    if ((value & sign) != 0) {
        value = (value >> count) | (mask ^ ((UINT64_C(1) <<
                                             (width - count)) - 1U));
    } else {
        value >>= count;
    }
    return value & mask;
}

static bool arithmetic_left_overflow(uint64_t original, uint64_t result,
                                     unsigned count, unsigned width)
{
    uint64_t mask = (UINT64_C(1) << width) - 1U;

    original &= mask;
    if (count >= width) {
        return original != 0;
    }
    return arithmetic_shift_right(result, count, width) != original;
}

static uint64_t normalize_left(uint64_t value, unsigned width,
                               unsigned *count)
{
    uint64_t mask = (UINT64_C(1) << width) - 1U;
    uint64_t sign = UINT64_C(1) << (width - 1U);
    uint64_t next = sign >> 1;

    value &= mask;
    *count = 0;
    if (value == 0) {
        return 0;
    }
    while (((value & sign) != 0) == ((value & next) != 0)) {
        value = (value << 1) & mask;
        ++*count;
    }
    return value;
}

rc_step_result_t rc_machine_step(rc_machine_t *machine)
{
    rc_instruction_t instruction;
    rc_addr_t instruction_address;
    rc_word_t displacement;
    rc_word_t address;
    rc_word_t operand;
    unsigned interrupt_number;
    unsigned am_count = 0;

    if (machine == NULL || machine->cpu.stopped) {
        return RC_STEP_STOPPED;
    }

    rc_machine_poll_devices(machine);

    if (machine->protection_enabled) {
        machine->cpu.interrupt_mask |= UINT32_C(0x800000);
    }

    if (rc_cpu_next_interrupt(&machine->cpu, &interrupt_number) &&
        !enter_interrupt(machine)) {
        return RC_STEP_ADDRESS_EXCEPTION;
    }

    displacement = 0;
    do {
        instruction_address = machine->cpu.instruction_counter;
        if (!fetch_instruction(machine, instruction_address, &instruction)) {
            return address_exception(machine);
        }
        machine->cpu.instruction_counter = rc_word(instruction_address + 2U);
        if (machine->protection_enabled) {
            uint8_t key;
            if (!rc_core_read_key(&machine->core, instruction_address, &key)) {
                return address_exception(machine);
            }
            bool protected = rc_cpu_key_is_protected(&machine->cpu, key);
            if (!machine->cpu.monitor_mode && protected) {
                return address_exception(machine);
            }
            machine->cpu.monitor_mode = protected;
        }
        displacement = rc_word((rc_word_t)instruction.displacement + displacement);
        if (!effective_address(machine, &instruction, instruction_address,
                               displacement, &address)) {
            return address_exception(machine);
        }
        machine->last_instruction = instruction;
        machine->last_instruction_address = instruction_address;
        machine->last_effective_address = address;
        ++machine->instructions_executed;
        if (instruction.opcode != RC_OP_AM) {
            break;
        }
        displacement = address;
        if (++am_count == RC_MAX_AM_CHAIN) {
            return RC_STEP_UNIMPLEMENTED;
        }
    } while (true);

    if (machine->protection_enabled && !machine->cpu.monitor_mode &&
        privileged_opcode(instruction.opcode)) {
        return address_exception(machine);
    }

    switch (instruction.opcode) {
    case RC_OP_AW:
        return autoload_word(machine, address);
    case RC_OP_IO: {
        unsigned device_number = (unsigned)((address >> 6) & 63U);
        uint8_t command = (uint8_t)(address & 3U);
        uint8_t modifier = (uint8_t)((address >> 2) & 15U);
        rc_device_t *device = machine->devices[device_number];
        rc_device_result_t device_result = RC_DEVICE_DISCONNECTED;
        rc_word_t transfer = machine->cpu.w[instruction.result_register];

        if (device != NULL && device->io != NULL) {
            device_result = device->io(device->context,
                                       (rc_io_command_t)command,
                                       modifier, &transfer);
        }
        machine->last_io_device = (uint8_t)device_number;
        machine->last_io_command = command;
        machine->last_io_modifier = modifier;
        ++machine->io_operations;
        machine->cpu.exception_22 =
            device_result == RC_DEVICE_DISCONNECTED;
        machine->cpu.exception_23 = device_result == RC_DEVICE_BUSY;
        if (device_result == RC_DEVICE_ACCEPTED) {
            machine->cpu.w[instruction.result_register] = rc_word(transfer);
        }
        return RC_STEP_OK;
    }
    case RC_OP_IC:
        machine->cpu.interrupt_requests &= ~address;
        machine->cpu.interrupt_requests &= RC_WORD_MASK;
        return RC_STEP_OK;
    case RC_OP_ML:
        if (!rc_machine_read_word(machine, address, &operand)) {
            return address_exception(machine);
        }
        machine->cpu.interrupt_mask = operand | UINT32_C(0x00800000);
        return RC_STEP_OK;
    case RC_OP_XL: {
        rc_halfword_t halfword;

        if (!rc_machine_read_halfword(machine, address, &halfword)) {
            return address_exception(machine);
        }
        machine->cpu.exception_21 = (uint8_t)((halfword >> 2) & 1U);
        machine->cpu.exception_22 = (uint8_t)((halfword >> 1) & 1U);
        machine->cpu.exception_23 = (uint8_t)(halfword & 1U);
        return RC_STEP_OK;
    }
    case RC_OP_XS: {
        rc_halfword_t exceptions =
            (rc_halfword_t)(((unsigned)machine->cpu.exception_21 << 2) |
                            ((unsigned)machine->cpu.exception_22 << 1) |
                            (unsigned)machine->cpu.exception_23);

        if (!cpu_write_halfword(machine, address, exceptions)) {
            return address_exception(machine);
        }
        return RC_STEP_OK;
    }
    case RC_OP_MS:
        if (!cpu_write_word(machine, address,
                                   machine->cpu.interrupt_mask)) {
            return address_exception(machine);
        }
        return RC_STEP_OK;
    case RC_OP_IS:
        if (!cpu_write_word(machine, address,
                                   machine->cpu.interrupt_requests)) {
            return address_exception(machine);
        }
        return RC_STEP_OK;
    case RC_OP_CI: {
        unsigned previous = previous_register(instruction.result_register);
        rc_float_t converted;
        bool overflow;

        machine->cpu.exception_22 = 0;
        machine->cpu.exception_23 = 0;
        overflow = rc_float_from_integer(
            rc_sign_extend_word(
                machine->cpu.w[instruction.result_register]),
            rc_sign_extend_word(address), &converted);
        rc_float_pack(converted, false, &machine->cpu.w[previous],
                      &machine->cpu.w[instruction.result_register]);
        if (overflow) {
            machine->cpu.exception_22 = 1;
            rc_cpu_request_interrupt(&machine->cpu, 2);
        }
        return RC_STEP_OK;
    }
    case RC_OP_CF: {
        unsigned previous = previous_register(instruction.result_register);
        rc_float_t source = rc_float_unpack(
            machine->cpu.w[previous],
            machine->cpu.w[instruction.result_register]);
        int32_t converted;

        machine->cpu.exception_22 = 0;
        machine->cpu.exception_23 = 0;
        if (rc_float_to_integer(source, rc_sign_extend_word(address),
                                &converted)) {
            machine->cpu.exception_22 = 1;
            rc_cpu_request_interrupt(&machine->cpu, 1);
        } else {
            machine->cpu.w[instruction.result_register] =
                rc_word((rc_word_t)converted);
        }
        return RC_STEP_OK;
    }
    case RC_OP_AL:
        machine->cpu.w[instruction.result_register] = address;
        return RC_STEP_OK;
    case RC_OP_AC:
        machine->cpu.exception_22 = address == UINT32_C(0x00800000);
        machine->cpu.exception_23 = address == 0;
        machine->cpu.w[instruction.result_register] = rc_word(0U - address);
        if (machine->cpu.exception_22 != 0) {
            rc_cpu_request_interrupt(&machine->cpu, 1);
        }
        return RC_STEP_OK;
    case RC_OP_WM:
        if (!rc_machine_read_word(machine, address, &operand)) {
            return address_exception(machine);
        }
        set_register_pair(
            machine, instruction.result_register,
            (uint64_t)((int64_t)rc_sign_extend_word(
                           machine->cpu.w[instruction.result_register]) *
                       (int64_t)rc_sign_extend_word(operand)));
        return RC_STEP_OK;
    case RC_OP_WD: {
        int64_t dividend;
        int32_t divisor;
        int64_t quotient;
        int64_t remainder;

        if (!rc_machine_read_word(machine, address, &operand)) {
            return address_exception(machine);
        }
        machine->cpu.exception_22 = 0;
        machine->cpu.exception_23 = 0;
        dividend = signed_register_pair(machine,
                                        instruction.result_register);
        divisor = rc_sign_extend_word(operand);
        if (divisor == 0) {
            machine->cpu.exception_22 = 1;
            rc_cpu_request_interrupt(&machine->cpu, 1);
            return RC_STEP_OK;
        }
        quotient = dividend / divisor;
        remainder = dividend % divisor;
        if (quotient < INT64_C(-8388608) ||
            quotient > INT64_C(8388607)) {
            machine->cpu.exception_22 = 1;
            rc_cpu_request_interrupt(&machine->cpu, 1);
            return RC_STEP_OK;
        }
        machine->cpu.w[previous_register(instruction.result_register)] =
            rc_word((rc_word_t)remainder);
        machine->cpu.w[instruction.result_register] =
            rc_word((rc_word_t)quotient);
        return RC_STEP_OK;
    }
    case RC_OP_FA:
    case RC_OP_FS:
    case RC_OP_FM:
    case RC_OP_FD: {
        unsigned previous = previous_register(instruction.result_register);
        rc_addr_t previous_address = previous_word_address(address);
        rc_word_t first_operand;
        rc_word_t second_operand;
        rc_float_t left;
        rc_float_t right;
        rc_float_t product;
        bool overflow;
        bool division_by_zero = false;

        machine->cpu.exception_22 = 0;
        machine->cpu.exception_23 = 0;
        if (!rc_machine_read_word(machine, previous_address,
                                  &first_operand) ||
            !rc_machine_read_word(machine, address, &second_operand)) {
            return address_exception(machine);
        }
        left = rc_float_unpack(machine->cpu.w[previous],
                               machine->cpu.w[instruction.result_register]);
        right = rc_float_unpack(first_operand, second_operand);
        if (instruction.opcode == RC_OP_FA ||
            instruction.opcode == RC_OP_FS) {
            overflow = rc_float_add(left, right,
                                    instruction.opcode == RC_OP_FS,
                                    &product);
        } else if (instruction.opcode == RC_OP_FM) {
            overflow = rc_float_multiply(left, right, &product);
        } else {
            overflow = rc_float_divide(left, right, &product,
                                       &division_by_zero);
        }
        if (division_by_zero) {
            machine->cpu.exception_22 = 1;
            rc_cpu_request_interrupt(&machine->cpu, 2);
            return RC_STEP_OK;
        }
        rc_float_pack(product, machine->cpu.exception_21 != 0,
                      &machine->cpu.w[previous],
                      &machine->cpu.w[instruction.result_register]);
        if (overflow) {
            machine->cpu.exception_22 = 1;
            rc_cpu_request_interrupt(&machine->cpu, 2);
        }
        return RC_STEP_OK;
    }
    case RC_OP_BA:
    case RC_OP_BS: {
        rc_halfword_t halfword;

        if (!rc_machine_read_halfword(machine, address, &halfword)) {
            return address_exception(machine);
        }
        set_integer_result(
            machine, machine->cpu.w[instruction.result_register],
            rc_word((rc_word_t)rc_sign_extend_halfword(halfword)),
            instruction.opcode == RC_OP_BS, instruction.result_register);
        return RC_STEP_OK;
    }
    case RC_OP_NS:
    case RC_OP_ND: {
        uint64_t value;
        unsigned count;
        rc_halfword_t stored_count;

        if (instruction.opcode == RC_OP_NS) {
            value = machine->cpu.w[instruction.result_register];
            value = normalize_left(value, RC_WORD_BITS, &count);
            machine->cpu.w[instruction.result_register] = (rc_word_t)value;
        } else {
            value = register_pair(machine, instruction.result_register);
            value = normalize_left(value, 48U, &count);
            set_register_pair(machine, instruction.result_register, value);
        }
        stored_count = value == 0
            ? UINT16_C(0x0800)
            : rc_halfword((rc_halfword_t)(0U - count));
        if (!cpu_write_halfword(machine, address, stored_count)) {
            return address_exception(machine);
        }
        return RC_STEP_OK;
    }
    case RC_OP_LA:
    case RC_OP_LO:
    case RC_OP_LX:
    case RC_OP_WA:
    case RC_OP_WS:
        if (!rc_machine_read_word(machine, address, &operand)) {
            return address_exception(machine);
        }
        if (instruction.opcode == RC_OP_LA) {
            machine->cpu.w[instruction.result_register] &= operand;
        } else if (instruction.opcode == RC_OP_LO) {
            machine->cpu.w[instruction.result_register] |= operand;
        } else if (instruction.opcode == RC_OP_LX) {
            machine->cpu.w[instruction.result_register] ^= operand;
        } else {
            set_integer_result(machine,
                               machine->cpu.w[instruction.result_register],
                               operand, instruction.opcode == RC_OP_WS,
                               instruction.result_register);
        }
        machine->cpu.w[instruction.result_register] =
            rc_word(machine->cpu.w[instruction.result_register]);
        return RC_STEP_OK;
    case RC_OP_JL:
    case RC_OP_JD:
    case RC_OP_JE:
        if (!rc_machine_read_word(machine, address, &operand) ||
            (instruction.opcode == RC_OP_JL && !cpu_write_allowed(machine, address))) {
            return address_exception(machine);
        }
        if (instruction.result_register != 0U) {
            machine->cpu.w[instruction.result_register] =
                machine->cpu.instruction_counter;
        }
        if (instruction.opcode == RC_OP_JD) {
            machine->cpu.interrupts_enabled = false;
        } else if (instruction.opcode == RC_OP_JE) {
            machine->cpu.interrupts_enabled = true;
        }
        machine->cpu.instruction_counter = address & ~UINT32_C(1);
        return RC_STEP_OK;
    case RC_OP_HL: {
        rc_halfword_t halfword;
        if (!rc_machine_read_halfword(machine, address, &halfword)) {
            return address_exception(machine);
        }
        machine->cpu.w[instruction.result_register] =
            (machine->cpu.w[instruction.result_register] &
             UINT32_C(0x00fff000)) | halfword;
        return RC_STEP_OK;
    }
    case RC_OP_HS:
        if (!cpu_write_halfword(
                machine, address,
                (rc_halfword_t)(machine->cpu.w[instruction.result_register] &
                                RC_HALFWORD_MASK))) {
            return address_exception(machine);
        }
        return RC_STEP_OK;
    case RC_OP_RL:
        if (!rc_machine_read_word(machine, address, &operand)) {
            return address_exception(machine);
        }
        machine->cpu.w[instruction.result_register] = operand;
        return RC_STEP_OK;
    case RC_OP_RS:
        if (!cpu_write_word(machine, address,
                                   machine->cpu.w[instruction.result_register])) {
            return address_exception(machine);
        }
        return RC_STEP_OK;
    case RC_OP_RX:
    {
        rc_word_t source = machine->cpu.w[instruction.result_register];
        if (!rc_machine_read_word(machine, address, &operand)) {
            return address_exception(machine);
        }
        if (machine->protection_enabled) {
            machine->cpu.w[instruction.result_register] = operand;
        }
        if (!cpu_write_word(machine, address, source)) {
            return address_exception(machine);
        }
        machine->cpu.w[instruction.result_register] = operand;
        return RC_STEP_OK;
    }
    case RC_OP_KL:
    case RC_OP_KS:
    case RC_OP_PL:
    case RC_OP_PS:
    case RC_OP_SP:
        if (machine->protection_enabled) {
            uint8_t key;
            rc_halfword_t halfword;
            if (instruction.opcode == RC_OP_PL) {
                if (!rc_machine_read_halfword(machine, address, &halfword))
                    return address_exception(machine);
                rc_cpu_set_protection_register(&machine->cpu, (uint8_t)halfword);
            } else if (instruction.opcode == RC_OP_PS) {
                if (!cpu_write_halfword(machine, address,
                                         machine->cpu.protection_register))
                    return address_exception(machine);
            } else {
                if (!rc_core_read_key(&machine->core, address, &key))
                    return address_exception(machine);
                if (instruction.opcode == RC_OP_KL) {
                    machine->cpu.w[instruction.result_register] = key;
                } else if (instruction.opcode == RC_OP_KS) {
                    if (!rc_core_write_key(&machine->core, address,
                            (uint8_t)(machine->cpu.w[instruction.result_register] & 7U)))
                        return address_exception(machine);
                } else if (!rc_cpu_key_is_protected(&machine->cpu, key)) {
                    machine->cpu.instruction_counter =
                        rc_word(machine->cpu.instruction_counter + 2U);
                }
            }
            return RC_STEP_OK;
        }
        if (!rc_machine_read_word(machine, address, &operand)) {
            return address_exception(machine);
        }
        if (instruction.opcode == RC_OP_KL) {
            machine->cpu.w[instruction.result_register] = 0;
        } else if (instruction.opcode == RC_OP_PS) {
            if (!cpu_write_halfword(machine, address,
                                           UINT16_C(0x0080))) {
                return address_exception(machine);
            }
        } else if (instruction.opcode == RC_OP_SP) {
            machine->cpu.instruction_counter =
                rc_word(machine->cpu.instruction_counter + 2U);
        }
        return RC_STEP_OK;
    case RC_OP_BZ: {
        rc_halfword_t halfword;
        if (!rc_machine_read_halfword(machine, address, &halfword)) {
            return address_exception(machine);
        }
        machine->cpu.w[instruction.result_register] = halfword;
        return RC_STEP_OK;
    }
    case RC_OP_BL: {
        rc_halfword_t halfword;
        if (!rc_machine_read_halfword(machine, address, &halfword)) {
            return address_exception(machine);
        }
        machine->cpu.w[instruction.result_register] =
            rc_word((rc_word_t)rc_sign_extend_halfword(halfword));
        return RC_STEP_OK;
    }
    case RC_OP_AS:
    case RC_OP_LS: {
        int32_t count = rc_sign_extend_word(address);
        rc_word_t original = machine->cpu.w[instruction.result_register];
        rc_word_t result;

        if (instruction.opcode == RC_OP_AS) {
            machine->cpu.exception_22 = 0;
            machine->cpu.exception_23 = 0;
        }
        if (count < 0) {
            unsigned magnitude = (unsigned)(-(int64_t)count);
            result = instruction.opcode == RC_OP_AS
                ? (rc_word_t)arithmetic_shift_right(original, magnitude,
                                                    RC_WORD_BITS)
                : (magnitude >= RC_WORD_BITS ? 0U : original >> magnitude);
        } else {
            unsigned magnitude = (unsigned)count;
            result = magnitude >= RC_WORD_BITS
                ? 0U : rc_word(original << magnitude);
            if (instruction.opcode == RC_OP_AS &&
                arithmetic_left_overflow(original, result, magnitude,
                                         RC_WORD_BITS)) {
                machine->cpu.exception_22 = 1;
                rc_cpu_request_interrupt(&machine->cpu, 1);
            }
        }
        machine->cpu.w[instruction.result_register] = result;
        return RC_STEP_OK;
    }
    case RC_OP_AD:
    case RC_OP_LD: {
        int32_t count = rc_sign_extend_word(address);
        uint64_t original = register_pair(
            machine, instruction.result_register);
        uint64_t result;
        const uint64_t pair_mask = UINT64_C(0x0000ffffffffffff);

        if (instruction.opcode == RC_OP_AD) {
            machine->cpu.exception_22 = 0;
            machine->cpu.exception_23 = 0;
        }
        if (count < 0) {
            unsigned magnitude = (unsigned)(-(int64_t)count);
            result = instruction.opcode == RC_OP_AD
                ? arithmetic_shift_right(original, magnitude, 48U)
                : (magnitude >= 48U ? 0U : original >> magnitude);
        } else {
            unsigned magnitude = (unsigned)count;
            result = magnitude >= 48U
                ? 0U : (original << magnitude) & pair_mask;
            if (instruction.opcode == RC_OP_AD &&
                arithmetic_left_overflow(original, result, magnitude, 48U)) {
                machine->cpu.exception_22 = 1;
                rc_cpu_request_interrupt(&machine->cpu, 1);
            }
        }
        set_register_pair(machine, instruction.result_register, result);
        return RC_STEP_OK;
    }
    case RC_OP_DL: {
        unsigned previous = previous_register(instruction.result_register);
        rc_addr_t previous_address = previous_word_address(address);

        if (!rc_machine_read_word(machine, address, &operand)) {
            return address_exception(machine);
        }
        machine->cpu.w[instruction.result_register] = operand;
        if (!rc_machine_read_word(machine, previous_address, &operand)) {
            return address_exception(machine);
        }
        machine->cpu.w[previous] = operand;
        return RC_STEP_OK;
    }
    case RC_OP_DS: {
        unsigned previous = previous_register(instruction.result_register);
        rc_addr_t previous_address = previous_word_address(address);

        operand = machine->cpu.w[instruction.result_register];
        if (!cpu_write_word(machine, address, operand)) {
            return address_exception(machine);
        }
        operand = machine->cpu.w[previous];
        if (!cpu_write_word(machine, previous_address, operand)) {
            return address_exception(machine);
        }
        return RC_STEP_OK;
    }
    case RC_OP_AA:
    case RC_OP_SS: {
        const rc_word_t sign_bit = UINT32_C(0x00800000);
        unsigned previous = previous_register(instruction.result_register);
        rc_addr_t previous_address = previous_word_address(address);
        rc_word_t low_left = machine->cpu.w[instruction.result_register];
        rc_word_t low_operand;
        rc_word_t high_left;
        rc_word_t high_operand;
        rc_word_t high_result;
        uint64_t low_wide;
        uint64_t high_wide;
        uint64_t borrow_or_carry;
        bool overflow;

        if (!rc_machine_read_word(machine, address, &low_operand)) {
            return address_exception(machine);
        }
        if (instruction.opcode == RC_OP_AA) {
            low_wide = (uint64_t)low_left + (uint64_t)low_operand;
            borrow_or_carry = low_wide > RC_WORD_MASK ? 1U : 0U;
        } else {
            low_wide = (uint64_t)rc_word(low_left - low_operand);
            borrow_or_carry = low_left < low_operand ? 1U : 0U;
        }
        machine->cpu.w[instruction.result_register] =
            (rc_word_t)(low_wide & RC_WORD_MASK);

        if (!rc_machine_read_word(machine, previous_address, &high_operand)) {
            return address_exception(machine);
        }
        high_left = machine->cpu.w[previous];
        if (instruction.opcode == RC_OP_AA) {
            high_wide = (uint64_t)high_left + (uint64_t)high_operand +
                        borrow_or_carry;
            high_result = (rc_word_t)(high_wide & RC_WORD_MASK);
            machine->cpu.exception_23 = high_wide > RC_WORD_MASK;
            overflow = ((~(high_left ^ high_operand) &
                         (high_left ^ high_result) & sign_bit) != 0);
        } else {
            uint64_t subtrahend = (uint64_t)high_operand +
                                  borrow_or_carry;
            high_result = rc_word(high_left - (rc_word_t)subtrahend);
            machine->cpu.exception_23 =
                (uint64_t)high_left >= subtrahend;
            overflow = (((high_left ^ high_operand) &
                         (high_left ^ high_result) & sign_bit) != 0);
        }
        machine->cpu.w[previous] = high_result;
        machine->cpu.exception_22 = overflow;
        if (overflow) {
            rc_cpu_request_interrupt(&machine->cpu, 1);
        }
        return RC_STEP_OK;
    }
    case RC_OP_SH:
    case RC_OP_SL:
    case RC_OP_SE:
    case RC_OP_SN: {
        int32_t left =
            rc_sign_extend_word(machine->cpu.w[instruction.result_register]);
        int32_t right = rc_sign_extend_word(address);
        bool skip = (instruction.opcode == RC_OP_SH && left > right) ||
                    (instruction.opcode == RC_OP_SL && left < right) ||
                    (instruction.opcode == RC_OP_SE && left == right) ||
                    (instruction.opcode == RC_OP_SN && left != right);
        if (skip) {
            machine->cpu.instruction_counter =
                rc_word(machine->cpu.instruction_counter + 2U);
        }
        return RC_STEP_OK;
    }
    case RC_OP_SO:
        if ((machine->cpu.w[instruction.result_register] & address) == address) {
            machine->cpu.instruction_counter =
                rc_word(machine->cpu.instruction_counter + 2U);
        }
        return RC_STEP_OK;
    case RC_OP_SZ:
        if ((machine->cpu.w[instruction.result_register] & address) == 0) {
            machine->cpu.instruction_counter =
                rc_word(machine->cpu.instruction_counter + 2U);
        }
        return RC_STEP_OK;
    case RC_OP_SX: {
        unsigned exceptions = (unsigned)machine->cpu.exception_23 |
                              ((unsigned)machine->cpu.exception_22 << 1) |
                              ((unsigned)machine->cpu.exception_21 << 2);
        if ((exceptions & (unsigned)(address & 7U)) == 0) {
            machine->cpu.instruction_counter =
                rc_word(machine->cpu.instruction_counter + 2U);
        }
        return RC_STEP_OK;
    }
    default:
        if (instruction.opcode == 61 && machine->ix_enabled)
            return execute_ix(machine, &instruction, address);
        if (!rc_opcode_is_assigned(instruction.opcode)) {
            rc_cpu_request_interrupt(&machine->cpu, 0);
            return enter_interrupt(machine) ? RC_STEP_OK
                                            : RC_STEP_ILLEGAL_INSTRUCTION;
        }
        return RC_STEP_UNIMPLEMENTED;
    }
}
