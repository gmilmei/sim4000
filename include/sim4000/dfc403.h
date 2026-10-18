#ifndef SIM4000_DFC403_H
#define SIM4000_DFC403_H

#include "sim4000/device.h"

enum { RC4819_UNITS = 8, RC4819_CYLINDERS = 406,
       RC4819_SECTORS_PER_CYLINDER = 180 };

typedef struct {
    rc_rc433x_device_t storage;
    uint8_t *ram;
    unsigned ram_cylinders;
    int cylinder;
    unsigned first_cylinder;
    unsigned head;
    unsigned sector;
    unsigned count;
    rc_word_t status0;
    rc_word_t status32;
    bool present;
    bool memory;
} rc_rc4819_drive_t;

typedef struct {
    rc_device_t device;
    struct rc_machine *machine;
    rc_rc4819_drive_t drives[RC4819_UNITS];
    unsigned data_interrupt;
    unsigned head_interrupt;
    unsigned unit;
    uint8_t command;
    rc_word_t status0;
    rc_word_t status32;
    uint32_t pending_interrupts;
    uint64_t data_operations;
    uint64_t seek_operations;
} rc_dfc403_device_t;

void rc_dfc403_init(rc_dfc403_device_t *controller,
                    struct rc_machine *machine, unsigned data_interrupt,
                    unsigned head_interrupt);
int rc_dfc403_mount(rc_dfc403_device_t *controller, unsigned unit,
                    const char *path, char *error, size_t error_size);
int rc_dfc403_mount_ram(rc_dfc403_device_t *controller, unsigned unit,
                        unsigned cylinders, char *error, size_t error_size);
void rc_dfc403_destroy(rc_dfc403_device_t *controller);

#endif
