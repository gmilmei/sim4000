#define _POSIX_C_SOURCE 200809L
#include "sim4000/config.h"
#include "sim4000/machine.h"
#include "sim4000/rc749.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { \
    (void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
    ++failures; } } while (0)

static void tag(FILE *file, uint32_t value)
{
    unsigned char bytes[4] = {(unsigned char)value, (unsigned char)(value >> 8),
                             (unsigned char)(value >> 16), (unsigned char)(value >> 24)};
    CHECK(fwrite(bytes, 1, 4, file) == 4);
}

static void record(FILE *file, const char *bytes, size_t count, bool parity)
{
    uint32_t value = (uint32_t)count | (parity ? UINT32_C(0x80000000) : 0);
    tag(file, value);
    CHECK(fwrite(bytes, 1, count, file) == count);
    if ((count & 1U) != 0) CHECK(fputc(0, file) != EOF);
    tag(file, value);
}

static void control(rc_rc749_device_t *tape, uint8_t modifier, rc_word_t word)
{
    CHECK(tape->device.io(tape, RC_IO_CONTROL, modifier, &word) == RC_DEVICE_ACCEPTED);
}

static void guest_name(rc_rc749_device_t *tape, const char *name)
{
    unsigned char bytes[81] = {0};
    size_t count = (strlen(name) + 3U) / 3U * 3U;
    CHECK(count <= sizeof(bytes));
    memcpy(bytes, name, strlen(name));
    for (size_t i = 0; i < count; i += 3)
        CHECK(rc_machine_write_word(tape->machine, (rc_addr_t)(102 + i / 3 * 2),
              ((rc_word_t)bytes[i] << 16) | ((rc_word_t)bytes[i + 1] << 8) | bytes[i + 2]));
    control(tape, 1, (rc_word_t)(100 + count / 3 * 2));
    control(tape, 12, 100);
}

int main(void)
{
    char root[] = "/tmp/sim4000-rc749-XXXXXX";
    char path[256], bad[256], link[256], fifo[256], error[256];
    rc_machine_t machine;
    rc_rc749_device_t tape;
    rc_config_t config;
    rc_config_error_t config_error;
    rc_word_t value = 0;
    FILE *file;
    CHECK(rc_config_parse_text("DEFINE RC749 DEVNO=16 INTNO=11 FILE=a.tap ROOT=tapes\n",
                              &config, &config_error) == 0);
    CHECK(config.devices[0].kind == RC_DEVICE_RC749);
    CHECK(strcmp(config.devices[0].path, "a.tap") == 0);
    CHECK(strcmp(config.devices[0].root_path, "tapes") == 0);
    CHECK(rc_config_parse_text("DEFINE RC749 DEVNO=16 INTNO=11\n", &config, &config_error) == 0);
    CHECK(rc_config_parse_text("DEFINE RC749 DEVNO=16\n", &config, &config_error) != 0);
    CHECK(rc_config_parse_text("DEFINE RC749 DEVNO=16 INTNO=11 CYLS=1\n", &config, &config_error) != 0);
    CHECK(rc_config_parse_text("DEFINE RC749 DEVNO=16 INTNO=11 WRITABLE=YES\n", &config, &config_error) != 0);
    CHECK(rc_config_parse_text("DEFINE HOSTFILES DEVNO=63 INTNO=21 FILE=x ROOT=y\n", &config, &config_error) != 0);
    CHECK(mkdtemp(root) != NULL);
    snprintf(path, sizeof(path), "%s/input.tap", root);
    snprintf(bad, sizeof(bad), "%s/bad.tap", root);
    snprintf(link, sizeof(link), "%s/link.tap", root);
    snprintf(fifo, sizeof(fifo), "%s/pipe", root);
    file = fopen(path, "wb"); CHECK(file != NULL);
    record(file, "ABCde", 5, false);
    tag(file, UINT32_C(0xfffffffe));
    record(file, "FGHIJK", 6, false);
    tag(file, 0);
    record(file, "xyz", 3, true);
    tag(file, 0); tag(file, UINT32_C(0xffffffff));
    CHECK(fclose(file) == 0);
    CHECK(rc_machine_init(&machine, 1024));
    rc_rc749_init(&tape, &machine, 11);
    CHECK(rc_machine_attach_device(&machine, 16, &tape.device));
    CHECK(rc_rc749_mount(&tape, path, error, sizeof(error)) == 0);
    CHECK(tape.record_count == 5 && tape.status == (RC749_HIGH_DENSITY | RC749_BOT));
    control(&tape, 1, 202);
    CHECK(tape.device.poll(&tape) == 0); /* Address transfer does not complete an operation. */
    control(&tape, 11, 200);
    CHECK(tape.transferred_bytes == 5 && tape.position == 1);
    CHECK(tape.status == RC749_HIGH_DENSITY);
    CHECK(rc_machine_read_word(&machine, 200, &value) && value == UINT32_C(0x414243));
    CHECK(rc_machine_read_word(&machine, 202, &value) && value == UINT32_C(0x646500));
    CHECK(tape.device.poll(&tape) == (1U << 12));
    CHECK(tape.device.poll(&tape) == 0);
    machine.cpu.stopped = false;
    machine.cpu.instruction_counter = 32;
    machine.cpu.w[0] = 200;
    CHECK(rc_machine_write_word(&machine, 32,
          rc_instruction_encode(1, 0, false, false, 0, (16 << 6) | 45)));
    CHECK(rc_machine_step(&machine) == RC_STEP_OK);
    CHECK(tape.position == 2 && tape.transferred_bytes == 6);
    CHECK(machine.cpu.exception_22 == 0 && machine.cpu.exception_23 == 0);
    rc_machine_poll_devices(&machine);
    CHECK((machine.cpu.interrupt_requests & (1U << 12)) != 0);
    control(&tape, 2, 3); /* Restore the position for direct protocol checks. */
    control(&tape, 3, 200); /* Exact fit must not report an overlength block. */
    CHECK(tape.transferred_bytes == 6 && tape.status == RC749_HIGH_DENSITY);
    value = 0;
    CHECK(tape.device.io(&tape, RC_IO_SENSE, 1, &value) == RC_DEVICE_ACCEPTED && value == 6);
    control(&tape, 11, 200);
    CHECK(tape.transferred_bytes == 0 && (tape.status & RC749_TAPE_MARK) != 0);
    control(&tape, 11, 200);
    CHECK(tape.transferred_bytes == 3 && (tape.status & RC749_PARITY) != 0);
    control(&tape, 11, 200); control(&tape, 11, 200);
    CHECK(tape.position == 5 && (tape.status & RC749_EOT) != 0);
    control(&tape, 2, 4); control(&tape, 1, 200); control(&tape, 11, 200);
    CHECK(tape.transferred_bytes == 3 && (tape.status & RC749_BLOCK_LENGTH) != 0);
    control(&tape, 2, 4); control(&tape, 2, 0);
    CHECK(tape.position == 3 && (tape.status & RC749_TAPE_MARK) != 0);
    control(&tape, 2, 2);
    CHECK(tape.position == 2 && (tape.status & RC749_TAPE_MARK) != 0);
    control(&tape, 2, 3); CHECK(tape.position == 1);
    control(&tape, 2, 3); CHECK(tape.position == 0 && (tape.status & RC749_BOT) != 0);
    control(&tape, 2, 1); CHECK(tape.position == 1);
    control(&tape, 1, 1024); control(&tape, 11, 200);
    CHECK(tape.fault && tape.position == 1 && (tape.status & RC749_OVERRUN) != 0);
    control(&tape, 2, 4); CHECK(!tape.fault && tape.position == 0);
    control(&tape, 4, 200); control(&tape, 5, 0);
    CHECK((tape.status & RC749_INTERVENTION) != 0 && tape.position == 0);
    CHECK(tape.device.io(&tape, RC_IO_WRITE, 0, &value) == RC_DEVICE_ACCEPTED);
    CHECK(tape.rejected_writes == 3 && tape.position == 0);

    file = fopen(bad, "wb"); CHECK(file != NULL);
    tag(file, 3); CHECK(fwrite("abc", 1, 3, file) == 3); /* Missing pad and trailer. */
    CHECK(fclose(file) == 0);
    CHECK(rc_rc749_mount(&tape, bad, error, sizeof(error)) != 0);
    file = fopen(bad, "wb"); CHECK(file != NULL);
    tag(file, UINT32_C(0xffffffff)); tag(file, 0); CHECK(fclose(file) == 0);
    CHECK(rc_rc749_mount(&tape, bad, error, sizeof(error)) != 0);
    CHECK(tape.file != NULL && tape.record_count == 5);
    file = fopen(bad, "wb"); CHECK(file != NULL);
    tag(file, 2); CHECK(fwrite("ab", 1, 2, file) == 2); tag(file, 3);
    CHECK(fclose(file) == 0);
    CHECK(rc_rc749_mount(&tape, bad, error, sizeof(error)) != 0);
    file = fopen(bad, "wb"); CHECK(file != NULL);
    tag(file, UINT32_C(0x01000001)); CHECK(fclose(file) == 0);
    CHECK(rc_rc749_mount(&tape, bad, error, sizeof(error)) != 0);
    CHECK(rc_rc749_mount(&tape, root, error, sizeof(error)) != 0);
    CHECK(symlink(path, link) == 0 && mkfifo(fifo, 0600) == 0);
    CHECK(rc_rc749_mount(&tape, link, error, sizeof(error)) != 0);
    CHECK(rc_rc749_mount(&tape, fifo, error, sizeof(error)) != 0);
    guest_name(&tape, "input.tap");
    CHECK((tape.status & RC749_INTERVENTION) != 0); /* Guest mounting is opt-in. */
    CHECK(rc_rc749_set_root(&tape, root, error, sizeof(error)) == 0);
    guest_name(&tape, "input.tap"); CHECK(tape.status == (RC749_HIGH_DENSITY | RC749_BOT));
    CHECK(tape.transferred_bytes == 15);
    char nested[256], nested_file[256], nested_link[256];
    snprintf(nested, sizeof(nested), "%s/sub", root);
    snprintf(nested_file, sizeof(nested_file), "%s/sub/tape", root);
    snprintf(nested_link, sizeof(nested_link), "%s/alias", root);
    CHECK(mkdir(nested, 0700) == 0);
    file = fopen(nested_file, "wb"); CHECK(file != NULL); tag(file, 0); CHECK(fclose(file) == 0);
    CHECK(symlink(nested, nested_link) == 0);
    guest_name(&tape, "sub/tape"); CHECK(tape.record_count == 1);
    guest_name(&tape, "alias/tape"); CHECK((tape.status & RC749_INTERVENTION) != 0);
    guest_name(&tape, "input.tap"); CHECK(tape.record_count == 5);
    const char *denied[] = {"../input.tap", "/input.tap", "./input.tap", "a//b",
                           "link.tap", "pipe", "bad.tap", "C:input.tap", "a\\b"};
    for (size_t i = 0; i < sizeof(denied) / sizeof(denied[0]); ++i) {
        guest_name(&tape, denied[i]);
        CHECK((tape.status & RC749_INTERVENTION) != 0);
        CHECK(tape.file != NULL && tape.record_count == 5);
    }
    control(&tape, 2, 5); CHECK(tape.file == NULL);
    control(&tape, 11, 200); CHECK((tape.status & RC749_INTERVENTION) != 0);
    guest_name(&tape, "input.tap"); CHECK(tape.file != NULL);
    /* A file altered after mount must fail before touching core. */
    file = fopen(path, "r+b"); CHECK(file != NULL); tag(file, 4); CHECK(fclose(file) == 0);
    CHECK(rc_machine_write_word(&machine, 200, UINT32_C(0xabcdef)));
    control(&tape, 1, 202); control(&tape, 11, 200);
    CHECK(tape.fault && tape.position == 0 && tape.transferred_bytes == 0);
    CHECK(rc_machine_read_word(&machine, 200, &value) && value == UINT32_C(0xabcdef));
    control(&tape, 2, 5); CHECK(tape.file == NULL); /* Unload remains possible after a fault. */
    file = fopen(bad, "wb"); CHECK(file != NULL);
    char large[RC749_MAX_RECORD_BYTES + 1] = {0};
    record(file, large, sizeof(large), false); CHECK(fclose(file) == 0);
    CHECK(rc_rc749_mount(&tape, bad, error, sizeof(error)) == 0);
    control(&tape, 11, 200);
    CHECK(tape.position == 1 && tape.transferred_bytes == 0 && (tape.status & RC749_BLOCK_LENGTH) != 0);
    file = fopen(bad, "wb"); CHECK(file != NULL);
    record(file, large, RC749_MAX_RECORD_BYTES, false); CHECK(fclose(file) == 0);
    CHECK(rc_rc749_mount(&tape, bad, error, sizeof(error)) == 0);
    /* The buffer limit is supported, although this small core truncates it. */
    control(&tape, 1, 1022); control(&tape, 11, 0);
    CHECK(tape.transferred_bytes == 1536 && tape.position == 1);
    CHECK((tape.status & RC749_BLOCK_LENGTH) != 0 && !tape.fault);
    file = fopen(bad, "wb"); CHECK(file != NULL); CHECK(fclose(file) == 0);
    CHECK(rc_rc749_mount(&tape, bad, error, sizeof(error)) == 0);
    control(&tape, 11, 0); CHECK(tape.position == 0 && (tape.status & RC749_EOT) != 0);
    rc_rc749_destroy(&tape); rc_machine_destroy(&machine);
    CHECK(unlink(nested_link) == 0 && unlink(nested_file) == 0 && rmdir(nested) == 0);
    CHECK(unlink(path) == 0 && unlink(bad) == 0 && unlink(link) == 0 && unlink(fifo) == 0);
    CHECK(rmdir(root) == 0);
    if (failures != 0) return EXIT_FAILURE;
    puts("RC749 configuration, records, positioning and mount confinement passed");
    return EXIT_SUCCESS;
}
