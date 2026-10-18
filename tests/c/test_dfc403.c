#include "sim4000/config.h"
#include "sim4000/dfc403.h"
#include "sim4000/machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { \
    (void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++failures; } } while (0)

static void control(rc_dfc403_device_t *controller, uint8_t modifier,
                      rc_word_t value)
{
    CHECK(controller->device.io(controller, RC_IO_CONTROL, modifier, &value) ==
          RC_DEVICE_ACCEPTED);
}

static rc_word_t sense(rc_dfc403_device_t *controller, uint8_t modifier)
{
    rc_word_t value = 0;
    CHECK(controller->device.io(controller, RC_IO_SENSE, modifier, &value) ==
          RC_DEVICE_ACCEPTED);
    return value;
}

int main(void)
{
    rc_machine_t machine;
    rc_dfc403_device_t controller;
    rc_config_t config;
    rc_config_error_t config_error;
    char error[256];
    rc_word_t word;
    uint8_t bytes[RC433X_SECTOR_BYTES];
    CHECK(rc_machine_init(&machine, 8192));
    rc_dfc403_init(&controller, &machine, 23, 22);
    CHECK((sense(&controller, 0) & (1U << 15)) != 0);
    CHECK(rc_dfc403_mount(&controller, 0,
        SIM4000_SOURCE_DIR "/src/disks/UNIT0", error, sizeof(error)) == 0);
    CHECK(rc_dfc403_mount_ram(&controller, 1, 2, error, sizeof(error)) == 0);
    CHECK(rc_dfc403_mount_ram(&controller, 1, 2, error, sizeof(error)) != 0);
    CHECK(rc_dfc403_mount_ram(&controller, 8, 2, error, sizeof(error)) != 0);
    CHECK(rc_dfc403_mount_ram(&controller, 2, 0, error, sizeof(error)) != 0);
    CHECK(rc_dfc403_mount_ram(&controller, 2, 407, error, sizeof(error)) != 0);
    CHECK((sense(&controller, 0) & (1U << 15)) == 0);
    control(&controller, 1, 0);
    control(&controller, 2, 1);
    control(&controller, 3, 100);
    CHECK(controller.device.poll(&controller) == 1U);
    CHECK(controller.device.poll(&controller) == 0);
    CHECK(rc_image_read_sector(&controller.drives[0].storage.image, 0, bytes));
    CHECK(rc_machine_read_word(&machine, 100, &word));
    CHECK(word == ((rc_word_t)bytes[0] | ((rc_word_t)bytes[1] << 8) |
                   ((rc_word_t)bytes[2] << 16)));
    CHECK(rc_machine_write_word(&machine, 100, 0x123456));
    control(&controller, 4, 100);
    control(&controller, 3, 612);
    CHECK(rc_machine_read_word(&machine, 612, &word) && word == 0x123456);
    uint8_t after[RC433X_SECTOR_BYTES];
    CHECK(rc_image_read_sector(&controller.drives[0].storage.image, 0, after));
    CHECK(memcmp(bytes, after, sizeof(bytes)) == 0);
    /* An absent file sector leaves core untouched, as in RC4818.PAS. */
    control(&controller, 9, 100);
    CHECK(controller.device.poll(&controller) == 3U);
    control(&controller, 1, 100U << 12);
    control(&controller, 3, 100);
    CHECK(rc_machine_read_word(&machine, 100, &word) && word == 0x123456);
    CHECK((sense(&controller, 0) & (1U << 22)) == 0);
    control(&controller, 8, 0);
    CHECK(controller.device.poll(&controller) == 3U);
    control(&controller, 1, 1U << 12);
    control(&controller, 3, 100);
    CHECK((sense(&controller, 8) & (1U << 8)) != 0);
    CHECK((sense(&controller, 0) & (1U << 13)) != 0);
    control(&controller, 1, 0);
    control(&controller, 3, 8190);
    CHECK((sense(&controller, 8) & (1U << 7)) != 0);
    control(&controller, 7, 1);
    CHECK((sense(&controller, 0) & 2U) != 0);
    CHECK((sense(&controller, 8) & 0x800001U) == 0x800001U);
    control(&controller, 1, 8);
    control(&controller, 2, 2); /* Cross a track boundary. */
    control(&controller, 4, 100);
    control(&controller, 3, 1124);
    CHECK(rc_machine_read_word(&machine, 1124, &word) && word == 0x123456);
    control(&controller, 9, 1);
    control(&controller, 1, (1U << 12) | (19U << 6) | 8U);
    control(&controller, 3, 100);
    CHECK((sense(&controller, 0) & (1U << 22)) != 0);
    control(&controller, 10, 2); /* Seek below cylinder zero. */
    CHECK((sense(&controller, 8) & (1U << 5)) != 0);
    control(&controller, 8, 0);
    CHECK((sense(&controller, 8) & (1U << 5)) == 0);
    control(&controller, 1, (63U << 6) | 31U);
    CHECK(controller.drives[1].head == 19 && controller.drives[1].sector == 8);
    control(&controller, 2, 0);
    (void)controller.device.poll(&controller);
    control(&controller, 3, 100);
    CHECK(controller.device.poll(&controller) == 0);
    control(&controller, 5, 100);
    CHECK(controller.device.poll(&controller) == 1U);
    control(&controller, 6, 0);
    CHECK(controller.device.poll(&controller) == 1U);
    control(&controller, 7, 7);
    CHECK((sense(&controller, 0) & (1U << 15)) != 0);
    control(&controller, 15, 0);
    CHECK(controller.unit == 0 && controller.command == 0);

    CHECK(rc_config_parse_text("DEFINE RC4819 DFC=15 UNITNO=0 FILE=x\n",
                               &config, &config_error) != 0);
    CHECK(rc_config_parse_text(
        "DEFINE CLOCK DEVNO=15 INTNO=23\nDEFINE RC4819 DFC=15 UNITNO=0 FILE=x\n",
        &config, &config_error) != 0);
    CHECK(rc_config_parse_text(
        "DEFINE DFC403 DEVNO=15 INTNO=23,22\n"
        "DEFINE RC4819 DFC=15 UNITNO=0 FILE=x\n"
        "DEFINE RC4819RAM DFC=15 UNITNO=0 CYLS=2\n",
        &config, &config_error) != 0);
    CHECK(rc_config_parse_text(
        "DEFINE DFC403 DEVNO=15 INTNO=23,22\n"
        "DEFINE RC4819RAM DFC=15 UNITNO=0 CYLS=407\n",
        &config, &config_error) != 0);
    rc_dfc403_destroy(&controller);
    rc_machine_destroy(&machine);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
