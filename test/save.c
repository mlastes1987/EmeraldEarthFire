#include "global.h"
#include "pokemon_storage_system.h"
#include "event_data.h"
#include "save_layout.h"
#include "config/dexnav.h"
#include "battle_setup.h"
#include "load_save.h"
#include "agb_flash.h"
#include "gba/flash_internal.h"
#include "test/test.h"

// These sizes alone do not establish compatibility. save_layout.h also locks the
// current Emerald Earth Fire field offsets and sizes during normal ROM builds.
#define T_SAVEBLOCK1_SIZE 15568
#define T_SAVEBLOCK2_SIZE 3884
#if IS_FRLG
#define T_SAVEBLOCK3_SIZE 4
#else
#define T_SAVEBLOCK3_SIZE 212
#endif
#define T_POKEMONSTORAGE_SIZE 34144

TEST("SaveBlock1 size matches the supported save layout")
{
    EXPECT_EQ(sizeof(struct SaveBlock1), T_SAVEBLOCK1_SIZE);
}

TEST("SaveBlock2 size matches the supported save layout")
{
    EXPECT_EQ(sizeof(struct SaveBlock2), T_SAVEBLOCK2_SIZE);
}

TEST("SaveBlock3 size matches the supported save layout")
{
    EXPECT_EQ(sizeof(struct SaveBlock3), T_SAVEBLOCK3_SIZE);
}

TEST("PokemonStorage size matches the supported save layout")
{
    EXPECT_EQ(sizeof(struct PokemonStorage), T_POKEMONSTORAGE_SIZE);
}

#undef T_SAVEBLOCK1_SIZE
#undef T_SAVEBLOCK2_SIZE
#undef T_SAVEBLOCK3_SIZE
#undef T_POKEMONSTORAGE_SIZE

#if !IS_FRLG
TEST("Current 16-badge save bytes retain Hoenn and Kanto event state")
{
    // Independent raw-byte fixture in the supported on-disk layout. This checks
    // that the event accessors read both regions from their existing locations.
    static const u8 savedBlock[0x3CD0] = {
        [0x1348] = 0x80, // Hoenn badge 1: flag 0x867
        [0x135A] = 0x01, // Kanto badge 1: flag 0x8F0
        [0x1368 + (0x4050 - 0x4000) * 2] = 0x34,
        [0x1368 + (0x4050 - 0x4000) * 2 + 1] = 0x12,
        [0x1368 + (0x4105 - 0x4000) * 2] = 0x78,
        [0x1368 + (0x4105 - 0x4000) * 2 + 1] = 0x56,
        [0x1368 + (0x4119 - 0x4000) * 2] = 0xBC,
        [0x1368 + (0x4119 - 0x4000) * 2 + 1] = 0x9A,
        [0x159C] = 0xEF,
        [0x159D] = 0xCD,
        [0x159E] = 0xAB,
        [0x159F] = 0x89,
    };

    memcpy(gSaveBlock1Ptr, savedBlock, sizeof(savedBlock));
    EXPECT(FlagGet(FLAG_BADGE01_GET));
    EXPECT(FlagGet(FLAG_KANTO_BADGE01_GET));
    EXPECT(!FlagGet(FLAG_BADGE02_GET));
    EXPECT(!FlagGet(FLAG_KANTO_BADGE02_GET));
    EXPECT_EQ(VarGet(VAR_LITTLEROOT_TOWN_STATE), 0x1234);
    EXPECT_EQ(VarGet(VAR_MAP_SCENE_PALLET_TOWN_OAK), 0x5678);
    EXPECT_EQ(VarGet(VAR_MAP_SCENE_VIRIDIAN_CITY_GYM_DOOR), 0x9ABC);

    VarSet(VAR_MAP_SCENE_PALLET_TOWN_OAK, 0x1357);
    VarSet(VAR_MAP_SCENE_VIRIDIAN_CITY_GYM_DOOR, 0x2468);
    EXPECT_EQ(VarGet(VAR_MAP_SCENE_PALLET_TOWN_OAK), 0x1357);
    EXPECT_EQ(VarGet(VAR_MAP_SCENE_VIRIDIAN_CITY_GYM_DOOR), 0x2468);
    EXPECT_EQ(VarGet(VAR_LITTLEROOT_TOWN_STATE), 0x1234);
    EXPECT_EQ(gSaveBlock1Ptr->gameStats[0], 0x89ABCDEF);
}

TEST("Initializing the extended event table preserves adjacent saved data")
{
    memset(gSaveBlock1Ptr->flags, 0xFF, sizeof(gSaveBlock1Ptr->flags));
    memset(gSaveBlock1Ptr->vars, 0xFF, sizeof(gSaveBlock1Ptr->vars));
    gSaveBlock1Ptr->objectEventTemplates[OBJECT_EVENT_TEMPLATES_COUNT - 1].flagId = 0x1234;
    gSaveBlock1Ptr->gameStats[0] = 0x89ABCDEF;

    InitEventData();

    for (u32 i = 0; i < sizeof(gSaveBlock1Ptr->flags); i++)
        EXPECT_EQ(gSaveBlock1Ptr->flags[i], 0);
    for (u32 i = 0; i < VARS_COUNT; i++)
        EXPECT_EQ(gSaveBlock1Ptr->vars[i], 0);
    EXPECT_EQ(gSaveBlock1Ptr->objectEventTemplates[OBJECT_EVENT_TEMPLATES_COUNT - 1].flagId, 0x1234);
    EXPECT_EQ(gSaveBlock1Ptr->gameStats[0], 0x89ABCDEF);
}

TEST("DexNav state uses independent slots without changing the save layout")
{
    u8 savedFlags[sizeof(gSaveBlock1Ptr->flags)];

    EXPECT(!FlagGet(DN_FLAG_DEXNAV_GET));
    EXPECT(!FlagGet(DN_FLAG_DETECTOR_MODE));
    FlagSet(FLAG_SYS_POKEDEX_GET);
    EXPECT(FlagGet(DN_FLAG_DEXNAV_GET));
    EXPECT(FlagGet(DN_FLAG_DETECTOR_MODE));

    memcpy(savedFlags, gSaveBlock1Ptr->flags, sizeof(savedFlags));
    FlagSet(DN_FLAG_SEARCHING);
    EXPECT(FlagGet(DN_FLAG_SEARCHING));
    EXPECT_EQ(memcmp(savedFlags, gSaveBlock1Ptr->flags, sizeof(savedFlags)), 0);

    VarSet(VAR_LITTLEROOT_TOWN_STATE, 0x1234);
    VarSet(VAR_MAP_SCENE_PALLET_TOWN_OAK, 0x5678);
    VarSet(DN_VAR_SPECIES, 0x4019); // Water encounter, Pikachu
    VarSet(DN_VAR_STEP_COUNTER, 99);
    EXPECT_EQ(VarGet(DN_VAR_SPECIES), 0x4019);
    EXPECT_EQ(VarGet(DN_VAR_STEP_COUNTER), 99);
    EXPECT_EQ(VarGet(VAR_LITTLEROOT_TOWN_STATE), 0x1234);
    EXPECT_EQ(VarGet(VAR_MAP_SCENE_PALLET_TOWN_OAK), 0x5678);

    InitEventData();
    EXPECT(!FlagGet(DN_FLAG_SEARCHING));
    EXPECT_EQ(VarGet(DN_VAR_SPECIES), 0);
    EXPECT_EQ(VarGet(DN_VAR_STEP_COUNTER), 0);
}

TEST("Trainer defeats and Kanto story flags can be set and cleared independently")
{
    InitEventData();
    FlagSet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE); // Legacy Sidney bit, 0x605
    EXPECT(!HasTrainerBeenFought(TRAINER_SIDNEY));
    SetTrainerFlag(TRAINER_SIDNEY);
    FlagClear(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE);
    EXPECT(HasTrainerBeenFought(TRAINER_SIDNEY));
    FlagSet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE);
    ClearTrainerFlag(TRAINER_SIDNEY);
    EXPECT(FlagGet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE));
    EXPECT(!HasTrainerBeenFought(TRAINER_SIDNEY));
    FlagToggle(TRAINER_FLAGS_START + TRAINER_SIDNEY);
    EXPECT(HasTrainerBeenFought(TRAINER_SIDNEY));
    EXPECT(FlagGet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE));
}

TEST("Legacy migration preserves every recorded bit and does not replay on new saves")
{
    u8 legacyFlags[sizeof(gSaveBlock1Ptr->flags)];

    for (u32 i = 0; i < sizeof(legacyFlags); i++)
        legacyFlags[i] = (i * 37) ^ 0xA5;
    memcpy(gSaveBlock1Ptr->flags, legacyFlags, sizeof(legacyFlags));
    gSaveBlock3Ptr->dexNavChain = 17;
    EXPECT(UpgradeTrainerFlagsSave());
    EXPECT_EQ(memcmp(legacyFlags, gSaveBlock1Ptr->flags, sizeof(legacyFlags)), 0);
    EXPECT_EQ(memcmp(gSaveBlock3Ptr->trainerFlags, legacyFlags + 0x500 / 8, 108), 0);
    EXPECT_EQ(memcmp(gSaveBlock3Ptr->trainerFlags + 108, legacyFlags + 0x500 / 8, 96), 0);
    EXPECT_EQ(gSaveBlock3Ptr->dexNavChain, 17);
    EXPECT_EQ(gSaveBlock3Ptr->trainerFlagsVersion, TRAINER_FLAGS_SAVE_VERSION);

    // Diverge the two stores, then simulate a second load of the versioned record.
    SetTrainerFlag(TRAINER_SIDNEY);
    FlagClear(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE);
    PrepareTrainerFlagsForSave();
    EXPECT(UpgradeTrainerFlagsSave());
    EXPECT(HasTrainerBeenFought(TRAINER_SIDNEY));
    EXPECT(!FlagGet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE));
}

TEST("Trainer extension rejects damaged records and unknown versions")
{
    InitEventData();
    EXPECT_EQ(gSaveBlock3Ptr->trainerFlagsChecksum, 0x3DB5); // Independent CRC-16 vector: 204 zero bytes
    SetTrainerFlag(TRAINER_SIDNEY);
    PrepareTrainerFlagsForSave();
    EXPECT(UpgradeTrainerFlagsSave());
    gSaveBlock3Ptr->trainerFlags[0] ^= 1;
    EXPECT(!UpgradeTrainerFlagsSave());
    EXPECT(HasTrainerBeenFought(TRAINER_SIDNEY));
    gSaveBlock3Ptr->trainerFlagsVersion++;
    EXPECT(!UpgradeTrainerFlagsSave());
    gSaveBlock3Ptr->trainerFlagsVersion = 0;
    gSaveBlock3Ptr->trainerFlagsChecksum = 1;
    EXPECT(!UpgradeTrainerFlagsSave());
}

TEST("Trainer flag boundaries stay inside their own storage")
{
    InitEventData();
    gSaveBlock3Ptr->dexNavChain = 19;
    FlagSet(TRAINER_FLAGS_START);
    FlagSet(TRAINER_FLAGS_END);
    EXPECT_EQ(gSaveBlock3Ptr->trainerFlags[0], 1);
    EXPECT_EQ(gSaveBlock3Ptr->trainerFlags[203], 0x80);
    EXPECT_EQ(gSaveBlock3Ptr->dexNavChain, 19);
    for (u32 i = 0; i < sizeof(gSaveBlock1Ptr->flags); i++)
        EXPECT_EQ(gSaveBlock1Ptr->flags[i], 0);

    EXPECT_EQ(GetFlagPointer(FLAGS_COUNT), NULL);
    EXPECT_EQ(GetFlagPointer(TRAINER_FLAGS_START - 1), NULL);
    EXPECT_EQ(GetFlagPointer(TRAINER_FLAGS_END + 1), NULL);
    EXPECT_EQ(GetFlagPointer(SPECIAL_FLAGS_END + 1), NULL);
    EXPECT_EQ(GetFlagPointer(0xFFFF), NULL);
    InitEventData();
    EXPECT(!FlagGet(TRAINER_FLAGS_START));
    EXPECT(!FlagGet(TRAINER_FLAGS_END));
    EXPECT_EQ(gSaveBlock3Ptr->dexNavChain, 19);
}

TEST("A legacy flash save migrates once and retains independent flags on reload")
{
    u16 sector;

    CheckForFlashMemory();
    EXPECT(gFlashMemoryPresent);
    ClearSaveData();
    Save_ResetSaveCounters();
    InitEventData();
    FlagSet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE);
    gSaveBlock3Ptr->dexNavChain = 17;
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);

    // Turn just the reserved tail into a legacy record; the original four-byte
    // prefix, sector data, checksum, rotation, and footer are unchanged.
    sector = NUM_SECTORS_PER_SLOT * (gSaveCounter % NUM_SAVE_SLOTS) + gLastWrittenSector;
    ReadFlash(sector, 0, (u8 *)&gSaveDataBuffer, SECTOR_SIZE);
    EXPECT_EQ(gSaveDataBuffer.id, SECTOR_ID_SAVEBLOCK2);
    memset(gSaveDataBuffer.saveBlock3Chunk + 4, 0, SAVE_BLOCK_3_CHUNK_SIZE - 4);
    EXPECT_EQ(ProgramFlashSectorAndVerify(sector, (u8 *)&gSaveDataBuffer), 0);

    ClearSav1();
    ClearSav3();
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT(FlagGet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE));
    EXPECT(HasTrainerBeenFought(TRAINER_SIDNEY));
    EXPECT_EQ(gSaveBlock3Ptr->dexNavChain, 17);

    FlagClear(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE);
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    ClearSav1();
    ClearSav3();
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT(!FlagGet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE));
    EXPECT(HasTrainerBeenFought(TRAINER_SIDNEY));
    EXPECT_EQ(gSaveBlock3Ptr->dexNavChain, 17);
}

TEST("Normal load detects damaged trainer bits outside the primary sector checksum")
{
    u16 sector;

    CheckForFlashMemory();
    EXPECT(gFlashMemoryPresent);
    ClearSaveData();
    Save_ResetSaveCounters();
    InitEventData();
    SetTrainerFlag(TRAINER_SIDNEY);
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    sector = NUM_SECTORS_PER_SLOT * (gSaveCounter % NUM_SAVE_SLOTS) + gLastWrittenSector;
    ReadFlash(sector, 0, (u8 *)&gSaveDataBuffer, SECTOR_SIZE);
    gSaveDataBuffer.saveBlock3Chunk[8] ^= 1;
    EXPECT_EQ(ProgramFlashSectorAndVerify(sector, (u8 *)&gSaveDataBuffer), 0);
    ClearSav3();
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_CORRUPT);
}

TEST("A damaged newest trainer extension falls back to the valid backup save")
{
    u16 sector;

    CheckForFlashMemory();
    EXPECT(gFlashMemoryPresent);
    ClearSaveData();
    Save_ResetSaveCounters();
    InitEventData();
    SetTrainerFlag(TRAINER_SIDNEY);
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);

    ClearTrainerFlag(TRAINER_SIDNEY);
    FlagSet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE);
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    sector = NUM_SECTORS_PER_SLOT * (gSaveCounter % NUM_SAVE_SLOTS) + gLastWrittenSector;
    ReadFlash(sector, 0, (u8 *)&gSaveDataBuffer, SECTOR_SIZE);
    gSaveDataBuffer.saveBlock3Chunk[8] ^= 1;
    EXPECT_EQ(ProgramFlashSectorAndVerify(sector, (u8 *)&gSaveDataBuffer), 0);
    ClearSav1();
    ClearSav3();
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_ERROR); // Existing backup-recovery status
    EXPECT(HasTrainerBeenFought(TRAINER_SIDNEY));
    EXPECT(!FlagGet(FLAG_HIDE_SEAFOAM_ISLANDS_B1F_WATER_STONE));
}

TEST("Empty or invalid-sector saves do not copy unvalidated data")
{
    CheckForFlashMemory();
    EXPECT(gFlashMemoryPresent);
    ClearSaveData();
    Save_ResetSaveCounters();
    InitEventData();
    FlagSet(FLAG_SYS_GAME_CLEAR);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_EMPTY);
    EXPECT(FlagGet(FLAG_SYS_GAME_CLEAR));

    memset(&gSaveDataBuffer, 0, sizeof(gSaveDataBuffer));
    gSaveDataBuffer.signature = SECTOR_SIGNATURE;
    gSaveDataBuffer.id = 0xFFFF;
    EXPECT_EQ(ProgramFlashSectorAndVerify(0, (u8 *)&gSaveDataBuffer), 0);
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_CORRUPT);
    EXPECT(FlagGet(FLAG_SYS_GAME_CLEAR));
}

TEST("Hoenn and Kanto trainer identities have independent defeats")
{
    InitEventData();
    EXPECT_NE(TRAINER_SAWYER_1, TRAINER_YOUNGSTER_BEN);
    SetTrainerFlag(TRAINER_SAWYER_1);
    EXPECT(!HasTrainerBeenFought(TRAINER_YOUNGSTER_BEN));
    SetTrainerFlag(TRAINER_YOUNGSTER_BEN);
    ClearTrainerFlag(TRAINER_SAWYER_1);
    EXPECT(HasTrainerBeenFought(TRAINER_YOUNGSTER_BEN));
    EXPECT(!HasTrainerBeenFought(TRAINER_SAWYER_1));
}

TEST("V1 trainer saves preserve Hoenn bits and seed Kanto defeats once")
{
    memset(gSaveBlock3Ptr->trainerFlags, 0, sizeof(gSaveBlock3Ptr->trainerFlags));
    gSaveBlock3Ptr->trainerFlags[261 / 8] = 1 << (261 % 8);
    gSaveBlock3Ptr->trainerFlagsVersion = TRAINER_FLAGS_SAVE_VERSION_V1;
    gSaveBlock3Ptr->trainerFlagsChecksum = 0x126D; // Independent v1 CRC for Sidney's bit
    VarSet(VAR_STARTER_MON, 2);
    EXPECT(UpgradeTrainerFlagsSave());
    EXPECT(HasTrainerBeenFought(TRAINER_SIDNEY));
    EXPECT(HasTrainerBeenFought(FRLG_TRAINER_ID(261)));
    EXPECT_EQ(VarGet(VAR_KANTO_STARTER), 2);
    ClearTrainerFlag(FRLG_TRAINER_ID(261));
    PrepareTrainerFlagsForSave();
    EXPECT(UpgradeTrainerFlagsSave());
    EXPECT(HasTrainerBeenFought(TRAINER_SIDNEY));
    EXPECT(!HasTrainerBeenFought(FRLG_TRAINER_ID(261)));
}

TEST("Kanto trainer bits survive the second sector and detect damage there")
{
    u16 sector;
    CheckForFlashMemory();
    EXPECT(gFlashMemoryPresent);
    ClearSaveData();
    Save_ResetSaveCounters();
    InitEventData();
    SetTrainerFlag(TRAINER_LEADER_BROCK);
    EXPECT_EQ(TrySavingData(SAVE_NORMAL), SAVE_STATUS_OK);
    ClearSav3();
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_OK);
    EXPECT(HasTrainerBeenFought(TRAINER_LEADER_BROCK));
    sector = NUM_SECTORS_PER_SLOT * (gSaveCounter % NUM_SAVE_SLOTS)
           + (gLastWrittenSector + 1) % NUM_SECTORS_PER_SLOT;
    ReadFlash(sector, 0, (u8 *)&gSaveDataBuffer, SECTOR_SIZE);
    EXPECT_EQ(gSaveDataBuffer.id, SECTOR_ID_SAVEBLOCK1_START);
    gSaveDataBuffer.saveBlock3Chunk[0] ^= 1;
    EXPECT_EQ(ProgramFlashSectorAndVerify(sector, (u8 *)&gSaveDataBuffer), 0);
    ClearSav3();
    EXPECT_EQ(LoadGameSave(SAVE_NORMAL), SAVE_STATUS_CORRUPT);
}

TEST("Variable access rejects gaps and out-of-range IDs")
{
    EXPECT_EQ(GetVarPointer(VARS_END + 1), NULL);
    EXPECT_EQ(GetVarPointer(SPECIAL_VARS_START - 1), NULL);
    EXPECT_EQ(GetVarPointer(SPECIAL_VARS_END + 1), NULL);
    EXPECT_EQ(GetVarPointer(TESTING_VARS_START + 8), NULL);
    EXPECT_EQ(GetVarPointer(0xFFFF), NULL);
    EXPECT_NE(GetVarPointer(VARS_END), NULL);
    EXPECT_NE(GetVarPointer(SPECIAL_VARS_END), NULL);
    EXPECT_NE(GetVarPointer(TESTING_VAR_UNUSED_7), NULL);
    gSaveBlock1Ptr->gameStats[0] = 0x12345678;
    EXPECT(!VarSet(VARS_END + 1, 0xFFFF));
    EXPECT_EQ(gSaveBlock1Ptr->gameStats[0], 0x12345678);
}
#endif
