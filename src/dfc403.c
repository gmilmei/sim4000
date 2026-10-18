#include "sim4000/dfc403.h"
#include "sim4000/machine.h"

#include <stdlib.h>
#include <string.h>

/* Native 24-bit forms of the status registers in historical RC4818.PAS. */
enum {
    INTERVENTION = 1U << 23, PARITY = 1U << 22,
    TIMER = 1U << 21, OVERRUN = 1U << 20,
    LOCAL = 1U << 15, FAULT = 1U << 14, SYNC_ERROR = 1U << 13,
    HEADS_MOVING = 1U << 12,
    WRONG_INDEX = 1U << 8, ADDRESS_ERROR = 1U << 7,
    DROP_OUT = 1U << 6, SEEK_ERROR = 1U << 5, FAULT32 = 1U << 4
};

static uint32_t interrupt_bit(unsigned level)
{
    return level < 24U ? UINT32_C(1) << (23U - level) : 0;
}

static void status(rc_dfc403_device_t *controller, rc_rc4819_drive_t *drive,
                    rc_word_t s0, rc_word_t s32)
{
    controller->status0 |= s0 & (PARITY | TIMER | OVERRUN);
    controller->status32 |= s32 & (WRONG_INDEX | ADDRESS_ERROR);
    drive->status0 |= s0 & (INTERVENTION | LOCAL | FAULT | HEADS_MOVING);
    drive->status32 |= s32 & (SEEK_ERROR | FAULT32);
}

static void load_core_address(rc_dfc403_device_t *controller,
                               rc_rc4819_drive_t *drive)
{
    controller->status0 &= ~(rc_word_t)(PARITY | TIMER | OVERRUN);
    controller->status32 &= ~(rc_word_t)(WRONG_INDEX | ADDRESS_ERROR);
    drive->status0 &= ~(rc_word_t)INTERVENTION;
    drive->status32 &= ~(rc_word_t)DROP_OUT;
}

static uint32_t poll_interrupts(void *context)
{
    rc_dfc403_device_t *controller = context;
    uint32_t pending = controller->pending_interrupts;
    controller->pending_interrupts = 0;
    return pending;
}

static bool ram_transfer(rc_dfc403_device_t *controller,
                          rc_rc4819_drive_t *drive, rc_addr_t address,
                          uint64_t sector, unsigned count, bool writing)
{
    uint64_t words = (uint64_t)count * 256U;
    uint64_t offset = sector * RC433X_SECTOR_BYTES;
    for (uint64_t i = 0; i < words; ++i) {
        uint8_t *bytes = drive->ram + offset + i * 3U;
        rc_addr_t core = address + (rc_addr_t)(i * 2U);
        rc_word_t word;
        if (writing) {
            if (!rc_machine_read_word(controller->machine, core, &word))
                return false;
            bytes[0] = (uint8_t)word;
            bytes[1] = (uint8_t)(word >> 8);
            bytes[2] = (uint8_t)(word >> 16);
        } else {
            word = (rc_word_t)bytes[0] | ((rc_word_t)bytes[1] << 8) |
                   ((rc_word_t)bytes[2] << 16);
            if (!rc_machine_write_word(controller->machine, core, word))
                return false;
        }
    }
    return true;
}

static void transfer(rc_dfc403_device_t *controller, rc_rc4819_drive_t *drive,
                       rc_word_t word, bool writing)
{
    rc_addr_t address = word & ~UINT32_C(1);
    uint64_t first = 0;
    load_core_address(controller, drive);
    if (drive->count == 0) return;
    ++controller->data_operations;
    if (drive->cylinder < 0 || drive->cylinder >= RC4819_CYLINDERS) {
        status(controller, drive, 0, SEEK_ERROR);
    } else if (drive->first_cylinder != (unsigned)drive->cylinder) {
        status(controller, drive, 0, WRONG_INDEX);
    } else {
        first = (uint64_t)(unsigned)drive->cylinder * 180U +
                drive->head * 9U + drive->sector;
        if ((uint64_t)address + (uint64_t)drive->count * 512U >
                controller->machine->core.halfword_count ||
            first + drive->count > RC4819_CYLINDERS * 180U) {
            status(controller, drive, 0, ADDRESS_ERROR);
        } else if (drive->memory &&
                   first + drive->count > drive->ram_cylinders * 180U) {
            status(controller, drive, PARITY, 0);
        } else {
            bool ok = drive->memory
                ? ram_transfer(controller, drive, address, first,
                               drive->count, writing)
                : writing
                    ? rc_disk_write_core(&drive->storage, address, first,
                                         drive->count)
                    : rc_disk_read_core(&drive->storage, address, first,
                                        drive->count);
            if (!ok) status(controller, drive, PARITY, 0);
            else drive->cylinder = (int)((first + drive->count - 1U) / 180U);
        }
    }
    controller->pending_interrupts |= interrupt_bit(controller->data_interrupt);
}

static rc_device_result_t io(void *context, rc_io_command_t command,
                              uint8_t modifier, rc_word_t *word)
{
    rc_dfc403_device_t *controller = context;
    rc_rc4819_drive_t *drive;
    if (controller == NULL || word == NULL) return RC_DEVICE_DISCONNECTED;
    drive = &controller->drives[controller->unit];
    if (command == RC_IO_SENSE) {
        if (modifier == 0) {
            *word = controller->status0 | drive->status0 |
                    (UINT32_C(1) << controller->unit) |
                    ((rc_word_t)controller->command << 8);
            if (!drive->present) *word |= LOCAL;
            if (drive->present &&
                ((controller->status32 | drive->status32) &
                 (WRONG_INDEX | ADDRESS_ERROR | DROP_OUT | SEEK_ERROR)) != 0)
                *word |= SYNC_ERROR;
        } else if (modifier == 8) {
            *word = controller->status32 | drive->status32 |
                    controller->unit | UINT32_C(0x800000);
        }
        return RC_DEVICE_ACCEPTED;
    }
    if (command != RC_IO_CONTROL) return RC_DEVICE_DISCONNECTED;
    controller->command = modifier;
    if (modifier == 7) {
        controller->unit = *word & 7U;
        return RC_DEVICE_ACCEPTED;
    }
    if (modifier == 15) {
        /* Historical master clear resets selection/command, not sticky errors. */
        controller->unit = 0;
        controller->command = 0;
        return RC_DEVICE_ACCEPTED;
    }
    if (!drive->present) return RC_DEVICE_ACCEPTED;
    switch (modifier) {
    case 1:
        drive->first_cylinder = (*word >> 12) & 511U;
        drive->head = (*word >> 6) & 63U;
        drive->sector = *word & 31U;
        if (drive->head > 19) drive->head = 19;
        if (drive->sector > 8) drive->sector = 8;
        break;
    case 2: drive->count = *word & 4095U; break;
    case 3: transfer(controller, drive, *word, false); break;
    case 4: transfer(controller, drive, *word, true); break;
    case 5:
        load_core_address(controller, drive);
        for (unsigned i = 0; i < 9; ++i) {
            /* Preserve the packed address behavior of RC4818.PAS. */
            rc_word_t value = rc_word(((rc_word_t)drive->cylinder << 20) |
                ((rc_word_t)drive->head << 14) |
                ((i + (unsigned)drive->cylinder % 3U) % 9U));
            if (!rc_machine_write_word(controller->machine,
                    (*word & ~UINT32_C(1)) + i * 2U, value)) {
                status(controller, drive, 0, ADDRESS_ERROR);
                break;
            }
        }
        controller->pending_interrupts |= interrupt_bit(controller->data_interrupt);
        break;
    case 6:
        load_core_address(controller, drive);
        controller->pending_interrupts |= interrupt_bit(controller->data_interrupt);
        break;
    case 8:
        drive->cylinder = 0;
        drive->status32 &= ~(rc_word_t)SEEK_ERROR;
        ++controller->seek_operations;
        controller->pending_interrupts |= interrupt_bit(controller->head_interrupt);
        break;
    case 9:
    case 10:
        drive->cylinder += (modifier == 9 ? 1 : -1) * (int)(*word & 511U);
        /* Keep repeated invalid seeks bounded instead of overflowing int. */
        if (drive->cylinder < -511) drive->cylinder = -511;
        if (drive->cylinder > 916) drive->cylinder = 916;
        ++controller->seek_operations;
        if (drive->cylinder < 0 || drive->cylinder >= RC4819_CYLINDERS)
            status(controller, drive, 0, SEEK_ERROR);
        controller->pending_interrupts |= interrupt_bit(controller->head_interrupt);
        break;
    default: break; /* Set-mode/unknown controls match the historical no-op. */
    }
    return RC_DEVICE_ACCEPTED;
}

void rc_dfc403_init(rc_dfc403_device_t *controller,
                    struct rc_machine *machine, unsigned data_interrupt,
                    unsigned head_interrupt)
{
    memset(controller, 0, sizeof(*controller));
    controller->machine = machine;
    controller->data_interrupt = data_interrupt;
    controller->head_interrupt = head_interrupt;
    controller->device.context = controller;
    controller->device.io = io;
    controller->device.poll = poll_interrupts;
}

static int mount_error(char *error, size_t size, const char *message)
{
    if (error != NULL && size != 0) (void)snprintf(error, size, "%s", message);
    return -1;
}

int rc_dfc403_mount(rc_dfc403_device_t *controller, unsigned unit,
                    const char *path, char *error, size_t error_size)
{
    if (unit >= RC4819_UNITS || controller->drives[unit].present)
        return mount_error(error, error_size, "invalid or occupied RC4819 unit");
    rc_rc4819_drive_t *drive = &controller->drives[unit];
    if (rc_rc433x_device_init_cow(&drive->storage, controller->machine,
                                 path, error, error_size) != 0) return -1;
    if (drive->storage.image.sector_count > RC4819_CYLINDERS * 180U) {
        rc_rc433x_device_destroy(&drive->storage);
        return mount_error(error, error_size, "RC4819 image exceeds 406 cylinders");
    }
    drive->present = true;
    return 0;
}

int rc_dfc403_mount_ram(rc_dfc403_device_t *controller, unsigned unit,
                        unsigned cylinders, char *error, size_t error_size)
{
    if (unit >= RC4819_UNITS || controller->drives[unit].present ||
        cylinders == 0 || cylinders > RC4819_CYLINDERS)
        return mount_error(error, error_size, "invalid RC4819RAM unit or capacity");
    rc_rc4819_drive_t *drive = &controller->drives[unit];
    drive->ram = calloc((size_t)cylinders * 180U, RC433X_SECTOR_BYTES);
    if (drive->ram == NULL)
        return mount_error(error, error_size, "cannot allocate RC4819RAM storage");
    drive->ram_cylinders = cylinders;
    drive->memory = true;
    drive->present = true;
    return 0;
}

void rc_dfc403_destroy(rc_dfc403_device_t *controller)
{
    for (unsigned i = 0; i < RC4819_UNITS; ++i) {
        rc_rc4819_drive_t *drive = &controller->drives[i];
        if (drive->present && !drive->memory)
            rc_rc433x_device_destroy(&drive->storage);
        free(drive->ram);
    }
    memset(controller, 0, sizeof(*controller));
}
