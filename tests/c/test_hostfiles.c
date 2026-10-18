#define _POSIX_C_SOURCE 200809L

#include "sim4000/config.h"
#include "sim4000/machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { \
    (void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++failures; } } while (0)

static void control(rc_hostfiles_device_t *host, uint8_t modifier,
                     rc_word_t value)
{
    CHECK(host->device.io(host, RC_IO_CONTROL, modifier, &value) ==
          RC_DEVICE_ACCEPTED);
}

static void name(rc_hostfiles_device_t *host, const char *text, bool output)
{
    uint8_t bytes[81] = {0};
    size_t count = (strlen(text) + 3U) / 3U * 3U;
    size_t i;
    CHECK(count <= sizeof(bytes));
    (void)memcpy(bytes, text, strlen(text));
    for (i = 0; i < count; i += 3U) {
        CHECK(rc_machine_write_word(host->machine, (rc_addr_t)(100U + i / 3U * 2U),
              ((rc_word_t)bytes[i] << 16) |
              ((rc_word_t)bytes[i + 1U] << 8) | bytes[i + 2U]));
    }
    control(host, 2, (rc_word_t)(count / 3U * 2U));
    control(host, output ? 12 : 11, 100);
}

int main(void)
{
    char root[] = "/tmp/sim4000-hostfiles-XXXXXX";
    char path[256];
    char error[256];
    rc_machine_t machine;
    rc_hostfiles_device_t host;
    rc_config_t config;
    rc_config_error_t config_error;
    rc_word_t value = 0;
    FILE *file;
    const unsigned char binary[] = {0, 13, 25, 26, 128, 255};
    unsigned char received[16] = {0};

    CHECK(rc_config_parse_text(
        "DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT=exchange\n",
        &config, &config_error) == 0);
    CHECK(config.devices[0].kind == RC_DEVICE_HOSTFILES);
    CHECK(strcmp(config.devices[0].path, "exchange") == 0);
    CHECK(rc_config_parse_text("DEFINE HOSTFILES DEVNO=63 INTNO=21\n",
                               &config, &config_error) != 0);
    CHECK(rc_config_parse_text("DEFINE HOSTFILES DEVNO=62 INTNO=21 ROOT=x\n",
                               &config, &config_error) != 0);
    CHECK(rc_config_parse_text("DEFINE READER DEVNO=0 INTNO=9 ROOT=x\n",
                               &config, &config_error) != 0);
    CHECK(rc_config_parse_text(
        "DEFINE HOSTFILES DEVNO=63 INTNO=21 ROOT=x FILE=y\n",
        &config, &config_error) != 0);
    CHECK(mkdtemp(root) != NULL);
    CHECK(rc_machine_init(&machine, 1024));
    CHECK(rc_hostfiles_device_init(&host, &machine, root, 21,
                                   error, sizeof(error)) == 0);
    CHECK(rc_machine_attach_device(&machine, 63, &host.device));
    (void)snprintf(path, sizeof(path), "%s/input", root);
    file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fwrite("ab\r\nc", 1, 5, file) == 5);
    CHECK(fclose(file) == 0);
    name(&host, "input", false);
    CHECK(host.status == 0);
    CHECK(host.device.poll(&host) == 4);
    CHECK(host.device.poll(&host) == 0);
    control(&host, 1, 4);
    control(&host, 2, 4);
    control(&host, 3, 200);
    CHECK(host.status == 0 && host.characters == 6);
    CHECK(rc_machine_read_word(&machine, 200, &value));
    CHECK(value == UINT32_C(0x61620a));
    CHECK(rc_machine_read_word(&machine, 202, &value));
    CHECK(value == UINT32_C(0x631900));
    control(&host, 3, 200);
    CHECK(host.status == (1U << 18) && host.characters == 0);

    name(&host, "textout", true);
    control(&host, 1, 2);
    CHECK(rc_machine_write_word(&machine, 200, UINT32_C(0x610d0a)));
    CHECK(rc_machine_write_word(&machine, 202, UINT32_C(0x621978)));
    control(&host, 2, 4);
    control(&host, 4, 200);
    CHECK(host.status == 0);
    CHECK(host.characters == 5);
    control(&host, 13, 0);
    (void)snprintf(path, sizeof(path), "%s/textout", root);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    CHECK(fread(received, 1, sizeof(received), file) == 3);
    CHECK(memcmp(received, "a\nb", 3) == 0);
    CHECK(fclose(file) == 0);
    name(&host, "textout", true);
    CHECK(host.status == (1U << 23));
    CHECK(host.file == NULL);

    (void)snprintf(path, sizeof(path), "%s/binary", root);
    file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fwrite(binary, 1, sizeof(binary), file) == sizeof(binary));
    CHECK(fclose(file) == 0);
    name(&host, "binary", false);
    control(&host, 2, 4);
    control(&host, 3, 200);
    CHECK(host.characters == 6 && host.status == 0);
    CHECK(rc_machine_read_word(&machine, 200, &value));
    CHECK(value == UINT32_C(0x000d19));
    CHECK(rc_machine_read_word(&machine, 202, &value));
    CHECK(value == UINT32_C(0x1a80ff));
    name(&host, "binout", true);
    control(&host, 2, 4);
    control(&host, 4, 200);
    control(&host, 13, 0);
    (void)snprintf(path, sizeof(path), "%s/binout", root);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    CHECK(fread(received, 1, sizeof(received), file) == sizeof(binary));
    CHECK(memcmp(received, binary, sizeof(binary)) == 0);
    CHECK(fclose(file) == 0);

    (void)snprintf(path, sizeof(path), "%s/link", root);
    CHECK(symlink("input", path) == 0);
    name(&host, "link", false);
    CHECK(host.status == (1U << 23));
    (void)snprintf(path, sizeof(path), "%s/dirlink", root);
    CHECK(symlink("/tmp", path) == 0);
    name(&host, "dirlink/escape", true);
    CHECK(host.status == (1U << 23));
    name(&host, "../escape", true);
    CHECK(host.status == (1U << 23));
    name(&host, "/tmp/escape", true);
    CHECK(host.status == (1U << 23));
    name(&host, "a\\b", true);
    CHECK(host.status == (1U << 23));
    name(&host, "missing", false);
    CHECK(host.status == (1U << 23));
    control(&host, 3, 200);
    CHECK(host.status == (1U << 23));
    name(&host, "binary", false);
    control(&host, 2, 4);
    control(&host, 3, 1022);
    CHECK(host.status == (1U << 14));
    CHECK(host.device.io(&host, RC_IO_SENSE, 0, &value) == RC_DEVICE_ACCEPTED);
    CHECK(value == (1U << 14));
    CHECK(host.device.io(&host, RC_IO_SENSE, 1, &value) == RC_DEVICE_ACCEPTED);
    CHECK(value == 0);
    rc_hostfiles_device_destroy(&host);
    rc_machine_destroy(&machine);
    {
        const char *files[] = {"input", "textout", "binary", "binout", "link", "dirlink"};
        size_t i;
        for (i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
            (void)snprintf(path, sizeof(path), "%s/%s", root, files[i]);
            CHECK(unlink(path) == 0);
        }
    }
    CHECK(rmdir(root) == 0);
    if (failures != 0) {
        return EXIT_FAILURE;
    }
    (void)puts("HOSTFILES protocol and sandbox checks passed");
    return EXIT_SUCCESS;
}
