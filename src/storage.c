#include "checksum.h"
#include "storage.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef __TICE__
#include <fileioc.h>
#else
#include <stdint.h>
#include <stdlib.h>
#endif

#define JOURNAL_HEADER_SIZE 16u
#define JOURNAL_VERSION 1u
#define PROFILE_WIRE_SIZE 76u
#define STORAGE_BUFFER_SIZE (LEVEL_WIRE_MAX + JOURNAL_HEADER_SIZE)
#define STORAGE_PATH_SIZE 512u

static char g_error[80];
/* Shared transaction workspace avoids large nested stack frames on the CE. */
static uint8_t s_storage_file[STORAGE_BUFFER_SIZE];
static uint8_t s_saved_payload[LEVEL_WIRE_MAX];
static Level s_level_scratch;
#ifndef __TICE__
static char g_directory[STORAGE_PATH_SIZE] = ".";
static size_t g_write_limit = SIZE_MAX;

static FILE *open_host_file(const char *path, const char *mode)
{
#ifdef _MSC_VER
    FILE *file = NULL;
    if (fopen_s(&file, path, mode) != 0) return NULL;
    return file;
#else
    return fopen(path, mode);
#endif
}
#endif

static void set_error(const char *message)
{
    size_t n = strlen(message);
    if (n >= sizeof(g_error)) n = sizeof(g_error) - 1;
    memcpy(g_error, message, n);
    g_error[n] = '\0';
}

const char *storage_error(void)
{
    return g_error;
}

static void put_u16(uint8_t *out, size_t *at, uint16_t value)
{
    out[(*at)++] = (uint8_t)value;
    out[(*at)++] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *out, size_t *at, uint32_t value)
{
    out[(*at)++] = (uint8_t)value;
    out[(*at)++] = (uint8_t)(value >> 8);
    out[(*at)++] = (uint8_t)(value >> 16);
    out[(*at)++] = (uint8_t)(value >> 24);
}

static uint16_t get_u16(const uint8_t *data, size_t *at)
{
    uint16_t value = (uint16_t)data[*at] | ((uint16_t)data[*at + 1] << 8);
    *at += 2;
    return value;
}

static uint32_t get_u32(const uint8_t *data, size_t *at)
{
    uint32_t value = (uint32_t)data[*at]
                   | ((uint32_t)data[*at + 1] << 8)
                   | ((uint32_t)data[*at + 2] << 16)
                   | ((uint32_t)data[*at + 3] << 24);
    *at += 4;
    return value;
}


void profile_default(Profile *profile)
{
    if (profile) memset(profile, 0, sizeof(*profile));
}

#ifndef __TICE__
void storage_set_directory(const char *path)
{
    size_t n;
    if (!path || !*path) path = ".";
    n = strlen(path);
    if (n >= sizeof(g_directory)) {
        set_error("Storage directory path is too long");
        return;
    }
    memcpy(g_directory, path, n + 1);
    set_error("");
}

/* Test-only fault injection: limit the next backend write, then reset. */
void storage_test_set_write_limit(size_t bytes)
{
    g_write_limit = bytes;
}

static int make_path(char *out, size_t capacity, const char *name)
{
    int n = snprintf(out, capacity, "%s/%s.bin", g_directory, name);
    return n >= 0 && (size_t)n < capacity;
}
#else
void storage_set_directory(const char *path)
{
    (void)path;
}
#endif

static int backend_read(const char *name, uint8_t *data, size_t capacity, size_t *size)
{
#ifdef __TICE__
    uint8_t handle = ti_Open(name, "r");
    uint16_t bytes;
    size_t got;
    int closed;
    if (!handle) return 0;
    bytes = ti_GetSize(handle);
    if ((size_t)bytes > capacity) {
        ti_Close(handle);
        return 0;
    }
    got = ti_Read(data, 1, bytes, handle);
    closed = ti_Close(handle);
    if (got != bytes || !closed) return 0;
    *size = bytes;
    return 1;
#else
    char path[STORAGE_PATH_SIZE];
    FILE *file;
    long end;
    size_t got;
    if (!make_path(path, sizeof(path), name)) return 0;
    file = open_host_file(path, "rb");
    if (!file) return 0;
    if (fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < 0
        || (unsigned long)end > capacity || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    got = fread(data, 1, (size_t)end, file);
    if (fclose(file) != 0 || got != (size_t)end) return 0;
    *size = got;
    return 1;
#endif
}

static int backend_write(const char *name, const uint8_t *data, size_t size)
{
#ifdef __TICE__
    uint8_t handle = ti_Open(name, "w");
    size_t written;
    int archived, closed;
    if (!handle) return 0;
    written = ti_Write(data, 1, size, handle);
    if (written != size) {
        ti_Close(handle);
        ti_Delete(name);
        return 0;
    }
    archived = ti_SetArchiveStatus(true, handle);
    closed = ti_Close(handle);
    if (!archived || !closed) {
        ti_Delete(name);
        return 0;
    }
    return 1;
#else
    char path[STORAGE_PATH_SIZE];
    FILE *file;
    size_t bytes_to_write = size;
    size_t written;
    int close_result;
    int success;
    size_t write_limit = g_write_limit;
    g_write_limit = SIZE_MAX;
    if (!make_path(path, sizeof(path), name)) return 0;
    file = open_host_file(path, "wb");
    if (!file) return 0;
    if (write_limit < bytes_to_write) bytes_to_write = write_limit;
    written = fwrite(data, 1, bytes_to_write, file);
    success = bytes_to_write == size && written == size && fflush(file) == 0;
    close_result = fclose(file);
    success = success && close_result == 0;
    if (!success) {
        remove(path);
        return 0;
    }
    return 1;
#endif
}

static int backend_is_committed(const char *name)
{
#ifdef __TICE__
    uint8_t handle = ti_Open(name, "r");
    int archived;
    int closed;
    if (!handle) return 0;
    archived = ti_IsArchived(handle);
    closed = ti_Close(handle);
    return archived && closed;
#else
    (void)name;
    return 1;
#endif
}

static void backend_remove(const char *name)
{
#ifdef __TICE__
    (void)ti_Delete(name);
#else
    char path[STORAGE_PATH_SIZE];
    if (make_path(path, sizeof(path), name)) remove(path);
#endif
}

static void journal_name(char *out, unsigned slot, unsigned copy)
{
    if (slot == CUSTOM_SLOTS) {
        memcpy(out, copy ? "NDPROFB" : "NDPROFA", 8);
    } else {
        out[0] = 'N'; out[1] = 'D'; out[2] = 'S';
        out[3] = (char)('0' + slot / 10u);
        out[4] = (char)('0' + slot % 10u);
        out[5] = copy ? 'B' : 'A';
        out[6] = '\0';
        out[7] = '\0';
    }
}

static size_t journal_wrap(const uint8_t *payload, size_t payload_size,
                           uint32_t generation, uint8_t *out, size_t capacity)
{
    size_t at = 0;
    uint32_t sum;
    if (payload_size > 0xffffu || capacity < JOURNAL_HEADER_SIZE + payload_size) return 0;
    out[at++] = 'N'; out[at++] = 'D'; out[at++] = 'J'; out[at++] = 'R';
    put_u16(out, &at, JOURNAL_VERSION);
    put_u16(out, &at, (uint16_t)payload_size);
    put_u32(out, &at, generation);
    memcpy(out + at, payload, payload_size);
    at += payload_size;
    sum = checksum32(out, at);
    put_u32(out, &at, sum);
    return at;
}

static const uint8_t *journal_payload(const uint8_t *data, size_t size,
                                     size_t *payload_size, uint32_t *generation)
{
    size_t at = 4;
    uint16_t version, len;
    uint32_t expected_checksum;
    if (size < JOURNAL_HEADER_SIZE || memcmp(data, "NDJR", 4) != 0) return NULL;
    version = get_u16(data, &at);
    len = get_u16(data, &at);
    *generation = get_u32(data, &at);
    if (version != JOURNAL_VERSION || size != JOURNAL_HEADER_SIZE + len
        || len > LEVEL_WIRE_MAX) return NULL;
    expected_checksum = (uint32_t)data[size - 4]
                      | ((uint32_t)data[size - 3] << 8)
                      | ((uint32_t)data[size - 2] << 16)
                      | ((uint32_t)data[size - 1] << 24);
    if (checksum32(data, size - 4) != expected_checksum) return NULL;
    *payload_size = len;
    return data + at;
}

static int generation_after(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) > 0;
}

static int profile_encode(const Profile *profile, uint8_t *out, size_t capacity)
{
    size_t at = 0, i;
    uint32_t sum;
    if (!profile || capacity < PROFILE_WIRE_SIZE) return 0;
    for (i = 0; i < CAMPAIGN_COUNT; ++i)
        if (profile->best[i] > 100 || profile->completed[i] > 1) return 0;
    if (profile->reduced_effects > 1) return 0;
    out[at++] = 'N'; out[at++] = 'D'; out[at++] = 'P'; out[at++] = 'F';
    put_u16(out, &at, JOURNAL_VERSION);
    memcpy(out + at, profile->best, CAMPAIGN_COUNT); at += CAMPAIGN_COUNT;
    memcpy(out + at, profile->completed, CAMPAIGN_COUNT); at += CAMPAIGN_COUNT;
    for (i = 0; i < CAMPAIGN_COUNT; ++i) put_u32(out, &at, profile->attempts[i]);
    put_u32(out, &at, profile->endless_best);
    out[at++] = profile->color;
    out[at++] = profile->reduced_effects;
    sum = checksum32(out, at);
    put_u32(out, &at, sum);
    return (int)at;
}

static int profile_decode(Profile *profile, const uint8_t *data, size_t size)
{
    Profile decoded;
    size_t at = 0, i;
    uint16_t version;
    uint32_t stored;
    if (!profile || !data || size != PROFILE_WIRE_SIZE || memcmp(data, "NDPF", 4)) return 0;
    at = 4;
    version = get_u16(data, &at);
    if (version != JOURNAL_VERSION) return 0;
    memset(&decoded, 0, sizeof(decoded));
    memcpy(decoded.best, data + at, CAMPAIGN_COUNT); at += CAMPAIGN_COUNT;
    memcpy(decoded.completed, data + at, CAMPAIGN_COUNT); at += CAMPAIGN_COUNT;
    for (i = 0; i < CAMPAIGN_COUNT; ++i) decoded.attempts[i] = get_u32(data, &at);
    decoded.endless_best = get_u32(data, &at);
    decoded.color = data[at++];
    decoded.reduced_effects = data[at++];
    stored = get_u32(data, &at);
    if (checksum32(data, size - 4) != stored || decoded.reduced_effects > 1) return 0;
    for (i = 0; i < CAMPAIGN_COUNT; ++i)
        if (decoded.best[i] > 100 || decoded.completed[i] > 1) return 0;
    *profile = decoded;
    return 1;
}

typedef struct {
    uint32_t generation;
    unsigned copy;
    char name[8];
    int valid;
} JournalRecord;

static int load_journal(unsigned slot, JournalRecord *best, int profile_payload)
{
    size_t file_size, payload_size;
    uint32_t generation;
    unsigned copy;
    const uint8_t *payload;
    memset(best, 0, sizeof(*best));
    for (copy = 0; copy < 2; ++copy) {
        char name[8] = {0};
        journal_name(name, slot, copy);
        if (!backend_read(name, s_storage_file, sizeof(s_storage_file), &file_size)
            || !backend_is_committed(name)) continue;
        payload = journal_payload(s_storage_file, file_size, &payload_size, &generation);
        if (!payload) continue;
        if (profile_payload) {
            Profile check;
            if (!profile_decode(&check, payload, payload_size)) continue;
        } else {
            if (!level_decode(&s_level_scratch, payload, payload_size, NULL, 0)) continue;
        }
        if (!best->valid || generation_after(generation, best->generation)) {
            best->generation = generation;
            best->copy = copy;
            memcpy(best->name, name, sizeof(best->name));
            best->valid = 1;
        }
    }
    return best->valid;
}

static int read_journal_payload(const JournalRecord *record,
                                const uint8_t **payload, size_t *payload_size)
{
    size_t file_size;
    uint32_t generation;
    if (!backend_read(record->name, s_storage_file, sizeof(s_storage_file), &file_size)
        || !backend_is_committed(record->name)) return 0;
    *payload = journal_payload(s_storage_file, file_size, payload_size, &generation);
    return *payload && generation == record->generation;
}

static int save_journal(unsigned slot, const uint8_t *payload, size_t payload_size,
                        int profile_payload)
{
    JournalRecord current;
    size_t size;
    uint32_t generation = 1;
    unsigned target_copy = 0;
    char target_name[8];
    if (profile_payload) {
        Profile check;
        if (!profile_decode(&check, payload, payload_size)) {
            set_error("Profile data is invalid");
            return 0;
        }
    } else {
        if (!level_decode(&s_level_scratch, payload, payload_size, NULL, 0)) {
            set_error("Level data is invalid");
            return 0;
        }
    }
    if (load_journal(slot, &current, profile_payload)) {
        generation = current.generation + 1u;
        target_copy = 1u - current.copy;
    }
    size = journal_wrap(payload, payload_size, generation, s_storage_file,
                        sizeof(s_storage_file));
    if (!size) {
        set_error("Journal record is too large");
        return 0;
    }
    journal_name(target_name, slot, target_copy);
    if (!backend_write(target_name, s_storage_file, size)) {
        set_error("Could not write or archive save data");
        return 0;
    }
    if (!backend_is_committed(target_name)) {
        backend_remove(target_name);
        set_error("Save data was not committed to storage");
        return 0;
    }
    set_error("");
    return 1;
}

bool storage_load_profile(Profile *profile)
{
    JournalRecord record;
    const uint8_t *payload;
    size_t payload_size;
    if (!profile) {
        set_error("Missing profile output");
        return false;
    }
    profile_default(profile);
    if (!load_journal(CUSTOM_SLOTS, &record, 1)) {
        set_error("No valid profile save found");
        return false;
    }
    if (!read_journal_payload(&record, &payload, &payload_size)
        || !profile_decode(profile, payload, payload_size)) {
        profile_default(profile);
        set_error("Profile save is invalid");
        return false;
    }
    set_error("");
    return true;
}

bool storage_save_profile(const Profile *profile)
{
    int size;
    if (!profile) {
        set_error("Missing profile input");
        return false;
    }
    size = profile_encode(profile, s_saved_payload, sizeof(s_saved_payload));
    if (!size) {
        set_error("Profile fields are invalid");
        return false;
    }
    return save_journal(CUSTOM_SLOTS, s_saved_payload, (size_t)size, 1) != 0;
}

bool storage_load_level(unsigned slot, Level *level)
{
    JournalRecord record;
    const uint8_t *payload;
    size_t payload_size;
    if (slot >= CUSTOM_SLOTS || !level) {
        set_error("Invalid level slot");
        return false;
    }
    if (!load_journal(slot, &record, 0)) {
        set_error("No valid level save found");
        return false;
    }
    if (!read_journal_payload(&record, &payload, &payload_size)
        || !level_decode(level, payload, payload_size, NULL, 0)) {
        set_error("Level save is invalid");
        return false;
    }
    set_error("");
    return true;
}

bool storage_save_level(unsigned slot, const Level *level)
{
    size_t size;
    if (slot >= CUSTOM_SLOTS || !level) {
        set_error("Invalid level slot");
        return false;
    }
    size = level_encode(level, s_saved_payload, sizeof(s_saved_payload));
    if (!size) {
        set_error("Level is invalid or too large");
        return false;
    }
    return save_journal(slot, s_saved_payload, size, 0) != 0;
}

static void export_name(char *out, unsigned slot)
{
    out[0] = 'N'; out[1] = 'D'; out[2] = 'X';
    out[3] = (char)('0' + slot / 10u);
    out[4] = (char)('0' + slot % 10u);
    out[5] = '\0';
}

bool storage_export_level(unsigned slot, const Level *level)
{
    char name[6];
    size_t size;
    if (slot >= CUSTOM_SLOTS || !level) {
        set_error("Invalid export slot");
        return false;
    }
    size = level_encode(level, s_saved_payload, sizeof(s_saved_payload));
    if (!size) {
        set_error("Level is invalid or too large");
        return false;
    }
    export_name(name, slot);
    /* NDX files are transfer copies, not journaled. On TI, opening an existing
       AppVar in write mode removes it before the replacement is complete; a
       failed export can therefore lose that old transfer copy. The slot's
       alternating journal remains intact and can be exported again. */
    if (!backend_write(name, s_saved_payload, size)) {
        set_error("Could not write or archive exported AppVar");
        return false;
    }
    if (!backend_is_committed(name)) {
        backend_remove(name);
        set_error("Export was not committed to storage");
        return false;
    }
    set_error("");
    return true;
}

bool storage_import_level(unsigned slot, Level *level)
{
    char name[6];
    size_t size, saved_size;
    if (slot >= CUSTOM_SLOTS || !level) {
        set_error("Invalid import slot");
        return false;
    }
    export_name(name, slot);
    if (!backend_read(name, s_storage_file, sizeof(s_saved_payload), &size)
        || !level_decode(&s_level_scratch, s_storage_file, size, NULL, 0)) {
        set_error("Exported AppVar is missing or invalid");
        return false;
    }
    saved_size = level_encode(&s_level_scratch, s_saved_payload, sizeof(s_saved_payload));
    if (!saved_size || !storage_save_level(slot, &s_level_scratch)) return false;
    if (!level_decode(level, s_saved_payload, saved_size, NULL, 0)) {
        set_error("Imported level could not be restored after saving");
        return false;
    }
    set_error("");
    return true;
}
