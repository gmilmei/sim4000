#define _POSIX_C_SOURCE 200809L
#include "sim4000/rc610.h"
#include "sim4000/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; \
} } while (0)

typedef struct {
    uint8_t bytes[4096];
    size_t count;
    unsigned writes, flushes, closes;
    bool short_write, bad_flush, bad_close;
} capture_t;

static size_t capture_write(void *context, const uint8_t *bytes, size_t count)
{
    capture_t *s = context;
    ++s->writes;
    if (s->short_write && count != 0) --count;
    if (count > sizeof(s->bytes) - s->count) return 0;
    memcpy(s->bytes + s->count, bytes, count);
    s->count += count;
    return count;
}
static int capture_flush(void *context)
{
    capture_t *s = context;
    ++s->flushes;
    return s->bad_flush ? -1 : 0;
}
static int capture_close(void *context)
{
    capture_t *s = context;
    ++s->closes;
    return s->bad_close ? -1 : 0;
}
static void setup(rc610_device_t *p, capture_t *s)
{
    *s = (capture_t){0};
    CHECK(rc610_init(p, 10));
    CHECK(rc610_connect(p, (rc610_sink_t){capture_write, capture_flush, capture_close},
                        s, NULL, 0) == 0);
    CHECK(rc610_status(p) == 0);
}
static void put(rc610_device_t *p, unsigned c)
{
    rc_word_t word = c;
    CHECK(p->device.io(p, RC_IO_WRITE, 0, &word) == RC_DEVICE_ACCEPTED);
    CHECK(word == c);
}
static void complete(rc610_device_t *p)
{
    p->device.advance(p, 0);
    CHECK(p->printing);
    CHECK(p->device.poll(p) == 0);
    p->device.advance(p, 1);
    CHECK(!p->printing);
    CHECK(p->device.poll(p) == UINT32_C(0x002000));
    p->device.advance(p, 100);
    CHECK(p->device.poll(p) == 0);
}
static void test_io(void)
{
    rc610_device_t p;
    capture_t s;
    setup(&p, &s);
    for (uint8_t m = 0; m < 16; ++m) {
        rc_word_t word = UINT32_C(0x1234c1);
        CHECK(p.device.io(&p, RC_IO_WRITE, m, &word) == RC_DEVICE_ACCEPTED);
        CHECK(word == UINT32_C(0x1234c1));
    }
    CHECK(p.line_length == 16);
    CHECK(p.device.poll(&p) == 0);
    put(&p, 10);
    for (uint8_t m = 0; m < 16; ++m) {
        rc_word_t word = 123;
        CHECK(p.device.io(&p, RC_IO_READ, m, &word) == RC_DEVICE_ACCEPTED);
        CHECK(word == 123);
        CHECK(p.device.io(&p, RC_IO_CONTROL, m, &word) == RC_DEVICE_ACCEPTED);
        CHECK(word == 123);
        CHECK(p.device.io(&p, RC_IO_WRITE, m, &word) == RC_DEVICE_BUSY);
        CHECK(p.device.io(&p, RC_IO_SENSE, m, &word) == RC_DEVICE_BUSY);
        CHECK(word == 123);
    }
    CHECK(s.writes == 0 && s.flushes == 0);
    complete(&p);
    CHECK(s.count == 17 && s.flushes == 1);
    CHECK(memcmp(s.bytes, "AAAAAAAAAAAAAAAA\n", 17) == 0);
    CHECK(rc610_close(&p, NULL, 0) == 0 && s.closes == 1);
}
static void test_characters(void)
{
    static const char expected[] =
        " !\"%&'()*+,-./0123456789:;<=>?ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]_abcdefghijklmnopqrstuvwxyz{|}\n";
    rc610_device_t p;
    capture_t s;
    setup(&p, &s);
    for (unsigned c = 32; c < 128; ++c) put(&p, c);
    CHECK(p.line_length == 89 && p.ignored_characters == 7);
    put(&p, 10);
    complete(&p);
    CHECK(s.count == sizeof(expected) - 1);
    CHECK(memcmp(s.bytes, expected, sizeof(expected) - 1) == 0);
    for (unsigned c = 0; c < 32; ++c)
        if (c < 10 || c > 13) put(&p, c);
    CHECK(p.line_length == 0 && p.ignored_characters == 35);
    for (unsigned i = 0; i < 133; ++i) put(&p, 'X');
    CHECK(p.line_length == 132 && p.overflow_characters == 1);
    put(&p, 13);
    complete(&p);
    CHECK(s.count == 223 && s.bytes[222] == 13);
    put(&p, 'Y'); put(&p, 11); complete(&p);
    put(&p, 'Z'); put(&p, 12); complete(&p);
    CHECK(s.count == 227 && memcmp(s.bytes + 223, "Y\vZ\f", 4) == 0);
    CHECK(s.flushes == 4);
    put(&p, 'Q');
    CHECK(rc610_close(&p, NULL, 0) == 0);
    CHECK(s.count == 227 && p.incomplete_characters == 1);
}
static void test_conditions(void)
{
    rc610_device_t p;
    capture_t s;
    rc_word_t word;
    setup(&p, &s);
    CHECK(rc610_set_fault(&p, UINT32_C(0xe00000)));
    rc610_set_paper(&p, false);
    word = 0;
    CHECK(p.device.io(&p, RC_IO_SENSE, 0, &word) == RC_DEVICE_ACCEPTED);
    CHECK(word == UINT32_C(0xe40000));
    CHECK(rc610_status(&p) == word);
    CHECK(!rc610_set_fault(&p, UINT32_C(0x100000)));
    put(&p, 'A');
    CHECK(rc610_status(&p) == UINT32_C(0x240000));
    rc610_master_clear(&p);
    CHECK(rc610_status(&p) == UINT32_C(0x040000));
    rc610_set_paper(&p, true);
    CHECK(rc610_status(&p) == 0);
    rc610_set_remote(&p, false);
    CHECK(rc610_status(&p) == UINT32_C(0x800000));
    put(&p, 10);
    CHECK(rc610_status(&p) == 0);
    p.device.advance(&p, 1);
    CHECK(p.printing && s.writes == 0 && p.device.poll(&p) == 0);
    rc610_set_remote(&p, true);
    complete(&p);
    CHECK(s.count == 2 && memcmp(s.bytes, "A\n", 2) == 0);
    put(&p, 'B'); put(&p, 10);
    rc610_set_remote(&p, false);
    CHECK(p.remote && p.local_pending);
    rc610_set_paper(&p, false);
    complete(&p);
    CHECK(s.count == 4 && memcmp(s.bytes, "A\nB\n", 4) == 0);
    CHECK(!p.remote && rc610_status(&p) == UINT32_C(0x840000));
    rc610_set_remote(&p, true);
    put(&p, 'C'); put(&p, 10); complete(&p);
    CHECK(s.count == 4 && p.failed_prints == 1);
    rc610_set_paper(&p, true);
    put(&p, 'D'); put(&p, 10); complete(&p);
    CHECK(s.count == 6 && memcmp(s.bytes + 4, "D\n", 2) == 0);
    CHECK(rc610_close(&p, NULL, 0) == 0);
}
static void test_errors(void)
{
    for (unsigned mode = 0; mode < 3; ++mode) {
        rc610_device_t p;
        capture_t s;
        setup(&p, &s);
        s.short_write = mode == 0;
        s.bad_flush = mode == 1;
        s.bad_close = mode == 2;
        put(&p, 'A'); put(&p, 10);
        CHECK(rc610_close(&p, NULL, 0) == -1 && p.connected);
        complete(&p);
        CHECK(s.flushes == 1);
        if (mode < 2) {
            CHECK(rc610_status(&p) == UINT32_C(0x040000));
            CHECK(p.output_errors == 1 && p.failed_prints == 1);
            CHECK(s.count == (mode == 0 ? 1U : 2U));
            rc610_set_paper(&p, false);
            rc610_set_paper(&p, true);
            CHECK(rc610_status(&p) == UINT32_C(0x040000));
            put(&p, 'B'); put(&p, 10); complete(&p);
            CHECK(s.writes == 1); /* Never replay a partial or failed record. */
        }
        CHECK(rc610_close(&p, NULL, 0) == -1);
        CHECK(!p.connected && s.closes == 1);
    }
}
static void test_file_visibility(void)
{
    char directory[] = "/tmp/sim4000-rc610-XXXXXX";
    char path[512], linkpath[512];
    rc610_device_t p;
    struct stat info;
    FILE *reader;
    CHECK(mkdtemp(directory) != NULL);
    (void)snprintf(path, sizeof(path), "%s/output", directory);
    (void)snprintf(linkpath, sizeof(linkpath), "%s/link", directory);
    CHECK(rc610_init(&p, 10));
    CHECK(rc610_open(&p, path, NULL, 0) == 0);
    CHECK(stat(path, &info) == 0 && (info.st_mode & 0777) == 0600);
    reader = fopen(path, "rb");
    CHECK(reader != NULL);
    if (reader != NULL) {
        static const unsigned controls[] = {13, 11, 12, 10};
        for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
            put(&p, 'A'); put(&p, controls[i]);
            CHECK(fgetc(reader) == EOF); /* Not printed yet. */
            complete(&p);
            clearerr(reader);
            CHECK(fgetc(reader) == 'A');
            CHECK(fgetc(reader) == (int)controls[i]);
            CHECK(fgetc(reader) == EOF); /* Visible before device close. */
        }
        put(&p, 'Z');
        CHECK(rc610_close(&p, NULL, 0) == 0);
        clearerr(reader);
        CHECK(fgetc(reader) == EOF);
        CHECK(fclose(reader) == 0);
    } else CHECK(rc610_close(&p, NULL, 0) == 0);
    CHECK(rc610_open(&p, path, NULL, 0) == -1);
    CHECK(stat(path, &info) == 0 && info.st_size == 8);
    CHECK(rc610_open_append(&p, path, NULL, 0) == 0);
    put(&p, 'B'); put(&p, 10); complete(&p);
    CHECK(stat(path, &info) == 0 && info.st_size == 10);
    CHECK(rc610_close(&p, NULL, 0) == 0);
    CHECK(rc610_open_append(&p, path, NULL, 0) == 0);
    put(&p, 'C'); put(&p, 13); complete(&p);
    CHECK(rc610_close(&p, NULL, 0) == 0);
    reader = fopen(path, "rb");
    CHECK(reader != NULL);
    if (reader != NULL) {
        char bytes[13] = {0};
        CHECK(fread(bytes, 1, 12, reader) == 12);
        CHECK(memcmp(bytes, "A\rA\vA\fA\nB\nC\r", 12) == 0);
        CHECK(fclose(reader) == 0);
    }
    CHECK(symlink(path, linkpath) == 0);
    CHECK(rc610_open(&p, linkpath, NULL, 0) == -1);
    CHECK(rc610_open_append(&p, linkpath, NULL, 0) == -1);
    CHECK(unlink(linkpath) == 0);
    CHECK(mkfifo(linkpath, 0600) == 0);
    CHECK(rc610_open(&p, linkpath, NULL, 0) == -1);
    CHECK(rc610_open_append(&p, linkpath, NULL, 0) == -1);
    CHECK(rc610_open_append(&p, "/dev/null", NULL, 0) == -1);
    CHECK(rc610_open(&p, directory, NULL, 0) == -1);
    CHECK(unlink(linkpath) == 0);
    CHECK(unlink(path) == 0);
    CHECK(rmdir(directory) == 0);
}
static void test_config(void)
{
    rc_config_t config;
    rc_config_error_t error;
    CHECK(rc_config_parse_text("DEFINE RC610 DEVNO=7 INTNO=10 FILE=printer.txt APPEND=YES\n",
                              &config, &error) == 0);
    CHECK(config.device_count == 1 && config.devices[0].kind == RC_DEVICE_RC610);
    CHECK(config.devices[0].append && config.devices[0].device_number == 7);
    CHECK(config.devices[0].interrupts[0] == 10);
    CHECK(strcmp(config.devices[0].path, "printer.txt") == 0);
    CHECK(rc_config_parse_text("DEFINE RC610 DEVNO=7 INTNO=10\n", &config, &error) == 0);
    CHECK(!config.devices[0].append && config.devices[0].path[0] == '\0');
    static const char *const invalid[] = {
        "DEFINE RC610 DEVNO=7\n", "DEFINE RC610 DEVNO=7 INTNO=10,12\n",
        "DEFINE RC610 DEVNO=7 INTNO=10 APPEND=YES\n",
        "DEFINE RC610 DEVNO=7 INTNO=10 FILE=x APPEND=maybe\n",
        "DEFINE RC610 DEVNO=7 INTNO=10 FILE=x APPEND=YES APPEND=NO\n",
        "DEFINE RC610 DEVNO=7 INTNO=10 ROOT=x\n",
        "DEFINE RC610 DEVNO=7 INTNO=10 CYLS=2\n",
        "DEFINE RC610 DEVNO=7 INTNO=10 OVERLAY=x\n",
        "DEFINE RC610 DEVNO=7 INTNO=10 WRITABLE=YES\n",
        "DEFINE CLOCK DEVNO=3 INTNO=11 APPEND=YES\n",
        "DEFINE RC610 DEVNO=7 INTNO=10\nDEFINE CLOCK DEVNO=7 INTNO=11\n",
        "DEFINE RC610 DEVNO=7 INTNO=10\nDEFINE READER DEVNO=0 INTNO=10\n"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        CHECK(rc_config_parse_text(invalid[i], &config, &error) != 0);
}
int main(void)
{
    rc610_device_t p;
    CHECK(!rc610_init(NULL, 10));
    CHECK(!rc610_init(&p, 24));
    test_io();
    test_characters();
    test_conditions();
    test_errors();
    test_file_visibility();
    test_config();
    printf("RC610: %u checks, %u failures\n", checks, failures);
    return failures != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
