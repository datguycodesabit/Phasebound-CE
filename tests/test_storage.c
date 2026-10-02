#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define TEST_MKDIR(path) _mkdir(path)
#define TEST_RMDIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define TEST_MKDIR(path) mkdir(path, 0777)
#define TEST_RMDIR(path) rmdir(path)
#endif

#include "../src/level.h"
#include "../src/storage.h"

/* Deliberately test-only host backend fault injection; kept out of public API. */
extern void storage_test_set_write_limit(size_t bytes);

static uint32_t test_checksum32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffu;
    size_t i;
    for (i = 0; i < size; ++i) {
        unsigned bit;
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

static void rewrite_checksum(uint8_t *data, size_t size)
{
    uint32_t sum = test_checksum32(data, size - 4);
    data[size - 4] = (uint8_t)sum;
    data[size - 3] = (uint8_t)(sum >> 8);
    data[size - 2] = (uint8_t)(sum >> 16);
    data[size - 1] = (uint8_t)(sum >> 24);
}

static void remove_test_files(void)
{
    static const char *const names[] = {
        "NDS00A.bin", "NDS00B.bin", "NDS01A.bin", "NDS01B.bin",
        "NDPROFA.bin", "NDPROFB.bin", "NDX00.bin"
    };
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        char path[128];
        snprintf(path, sizeof(path), "tests/.storage-test/%s", names[i]);
        remove(path);
    }
    TEST_RMDIR("tests/.storage-test");
}

static void corrupt_file_byte(const char *name, long offset)
{
    char path[128];
    FILE *file;
    int ch;
    snprintf(path, sizeof(path), "tests/.storage-test/%s", name);
    file = fopen(path, "r+b");
    assert(file != NULL);
    assert(fseek(file, offset, SEEK_SET) == 0);
    ch = fgetc(file);
    assert(ch != EOF);
    assert(fseek(file, offset, SEEK_SET) == 0);
    assert(fputc(ch ^ 0x40, file) != EOF);
    assert(fclose(file) == 0);
}

static void assert_level_equal(const Level *a, const Level *b)
{
    size_t i;
    assert(strcmp(a->name, b->name) == 0);
    assert(a->width == b->width);
    assert(a->start_x == b->start_x && a->start_y == b->start_y);
    assert(a->object_count == b->object_count);
    assert(a->start_mode == b->start_mode);
    assert(a->start_gravity == b->start_gravity);
    assert(a->start_speed == b->start_speed);
    for (i = 0; i < (size_t)a->width * LEVEL_ROWS; ++i) assert(a->tiles[i] == b->tiles[i]);
    for (i = 0; i < a->object_count; ++i) {
        assert(a->objects[i].x == b->objects[i].x);
        assert(a->objects[i].y == b->objects[i].y);
        assert(a->objects[i].type == b->objects[i].type);
        assert(a->objects[i].value == b->objects[i].value);
    }
}

static void test_level_codec_and_validation(void)
{
    Level source, decoded;
    uint8_t wire[LEVEL_WIRE_MAX];
    size_t size;
    char error[80];

    level_blank(&source, "codec", 20);
    assert(source.width == 32);
    assert(source.start_x == 32 && source.start_y == 212);
    assert(level_tile(&source, 0, 0) == TILE_BLOCK);
    assert(level_tile(&source, 0, LEVEL_ROWS - 1) == TILE_BLOCK);
    assert(level_validate(&source, error, sizeof(error)));

    size = level_encode(&source, wire, sizeof(wire));
    assert(size > 0 && size < sizeof(wire));
    assert(level_decode(&decoded, wire, size, error, sizeof(error)));
    assert_level_equal(&source, &decoded);

    wire[size - 1] ^= 0x80;
    assert(!level_decode(&decoded, wire, size, error, sizeof(error)));
    assert(strstr(error, "checksum") != NULL);

    size = level_encode(&source, wire, sizeof(wire));
    wire[38] = MODE_COUNT; /* Invalid starting mode, with a valid checksum. */
    rewrite_checksum(wire, size);
    assert(!level_decode(&decoded, wire, size, error, sizeof(error)));

    source.tiles[2 * LEVEL_ROWS + 13] = TILE_BLOCK;
    assert(!level_validate(&source, error, sizeof(error)));
    source.tiles[2 * LEVEL_ROWS + 13] = TILE_EMPTY;
    source.object_count = 0;
    assert(!level_validate(&source, error, sizeof(error)));
}

static void test_journal_failure_and_recovery(void)
{
    Level first, second, loaded;
    Profile profile, loaded_profile;

    level_blank(&first, "durable A", 64);
    assert(storage_save_level(0, &first));
    assert(storage_load_level(0, &loaded));
    assert_level_equal(&first, &loaded);

    level_blank(&second, "new B", 96);
    second.objects[0].x = (uint16_t)((second.width - 1u) * TILE_SIZE);
    storage_test_set_write_limit(0); /* No space even for the journal header. */
    assert(!storage_save_level(0, &second));
    assert(storage_load_level(0, &loaded));
    assert_level_equal(&first, &loaded);

    storage_test_set_write_limit(12); /* Simulate a full device mid-record. */
    assert(!storage_save_level(0, &second));
    assert(storage_load_level(0, &loaded));
    assert_level_equal(&first, &loaded);

    assert(storage_save_level(0, &second));
    assert(storage_load_level(0, &loaded));
    assert_level_equal(&second, &loaded);
    corrupt_file_byte("NDS00B.bin", 8); /* A changed generation must fail checksum. */
    assert(storage_load_level(0, &loaded));
    assert_level_equal(&first, &loaded); /* The older valid journal copy survives. */
    assert(storage_save_level(0, &second));
    assert(storage_load_level(0, &loaded));
    assert_level_equal(&second, &loaded);

    profile_default(&profile);
    profile.best[0] = 78;
    profile.completed[0] = 1;
    profile.attempts[0] = 19;
    profile.endless_best = 421;
    profile.color = 3;
    profile.reduced_effects = 1;
    assert(storage_save_profile(&profile));
    {
        Profile newer = profile;
        newer.best[0] = 88;
        newer.attempts[0] = 30;
        storage_test_set_write_limit(0);
        assert(!storage_save_profile(&newer));
        assert(storage_load_profile(&loaded_profile));
        assert(memcmp(&profile, &loaded_profile, sizeof(profile)) == 0);
        assert(storage_save_profile(&newer));
        profile = newer;
    }
    assert(storage_load_profile(&loaded_profile));
    assert(memcmp(&profile, &loaded_profile, sizeof(profile)) == 0);

    assert(storage_export_level(0, &second));
    {
        char path[128];
        FILE *file;
        snprintf(path, sizeof(path), "tests/.storage-test/NDX00.bin");
        file = fopen(path, "wb");
        assert(file != NULL);
        assert(fwrite("bad", 1, 3, file) == 3);
        assert(fclose(file) == 0);
        assert(!storage_import_level(0, &loaded));
        assert(storage_load_level(0, &loaded));
        assert_level_equal(&second, &loaded);
    }
    assert(storage_export_level(0, &second));
    assert(storage_import_level(0, &loaded));
    assert_level_equal(&second, &loaded);
    assert(storage_load_level(0, &loaded));
    assert_level_equal(&second, &loaded);
}

void test_storage(void)
{
    remove_test_files();
    TEST_MKDIR("tests/.storage-test");
    storage_set_directory("tests/.storage-test");
    test_level_codec_and_validation();
    test_journal_failure_and_recovery();
    remove_test_files();
}
