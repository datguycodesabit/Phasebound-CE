#ifndef NEON_STORAGE_H
#define NEON_STORAGE_H
#include "level.h"
typedef struct {
    uint8_t best[CAMPAIGN_COUNT], completed[CAMPAIGN_COUNT];
    uint32_t attempts[CAMPAIGN_COUNT], endless_best;
    uint8_t color, reduced_effects;
} Profile;
void profile_default(Profile *profile);
bool storage_load_profile(Profile *profile);
bool storage_save_profile(const Profile *profile);
bool storage_load_level(unsigned slot, Level *level);
bool storage_save_level(unsigned slot, const Level *level);
/* Sharing: NDX00..NDX09 are standalone level AppVars, independent of
   internal journal saves. Import validates before replacing the slot. */
bool storage_export_level(unsigned slot, const Level *level);
bool storage_import_level(unsigned slot, Level *level);
const char *storage_error(void);
/* Host test backend: base directory must already exist. */
void storage_set_directory(const char *path);
#endif
