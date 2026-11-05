#include "global.h"
#include "event_data.h"
#include "pokedex.h"
#include "regions.h"

#define SPECIAL_FLAGS_SIZE  (NUM_SPECIAL_FLAGS / 8)  // 8 flags per byte
#define TEMP_FLAGS_SIZE     (NUM_TEMP_FLAGS / 8)
#define DAILY_FLAGS_SIZE    (NUM_DAILY_FLAGS / 8)
#define TEMP_VARS_SIZE      (NUM_TEMP_VARS * 2)      // 1/2 var per byte

EWRAM_DATA u16 gSpecialVar_0x8000 = 0;
EWRAM_DATA u16 gSpecialVar_0x8001 = 0;
EWRAM_DATA u16 gSpecialVar_0x8002 = 0;
EWRAM_DATA u16 gSpecialVar_0x8003 = 0;
EWRAM_DATA u16 gSpecialVar_0x8004 = 0;
EWRAM_DATA u16 gSpecialVar_0x8005 = 0;
EWRAM_DATA u16 gSpecialVar_0x8006 = 0;
EWRAM_DATA u16 gSpecialVar_0x8007 = 0;
EWRAM_DATA u16 gSpecialVar_0x8008 = 0;
EWRAM_DATA u16 gSpecialVar_0x8009 = 0;
EWRAM_DATA u16 gSpecialVar_0x800A = 0;
EWRAM_DATA u16 gSpecialVar_0x800B = 0;
EWRAM_DATA u16 gSpecialVar_Result = 0;
EWRAM_DATA u16 gSpecialVar_LastTalked = 0;
EWRAM_DATA u16 gSpecialVar_Facing = 0;
EWRAM_DATA u16 gSpecialVar_MonBoxId = 0;
EWRAM_DATA u16 gSpecialVar_MonBoxPos = 0;
EWRAM_DATA u16 gSpecialVar_Unused_0x8014 = 0;
EWRAM_DATA static u8 sSpecialFlags[SPECIAL_FLAGS_SIZE] = {0};

#if TESTING
#define TEST_FLAGS_SIZE     1
#define TEST_VARS_SIZE      8
EWRAM_DATA static u8 sTestFlags[TEST_FLAGS_SIZE] = {0};
EWRAM_DATA static u16 sTestVars[TEST_VARS_SIZE] = {0};
#endif // TESTING

extern u16 *const gSpecialVars[];

const u16 gBadgeFlags[NUM_BADGES] =
{
    FLAG_BADGE01_GET,
    FLAG_BADGE02_GET,
    FLAG_BADGE03_GET,
    FLAG_BADGE04_GET,
    FLAG_BADGE05_GET,
    FLAG_BADGE06_GET,
    FLAG_BADGE07_GET,
    FLAG_BADGE08_GET,
};

// Kanto's badges. In a FireRed/LeafGreen build these names alias that build's own badge
// flags, so this table is the same set twice; in an Emerald build it is the second, real
// set. Keeping the table unconditional means no caller needs to know which build it is in.
const u16 gKantoBadgeFlags[NUM_KANTO_BADGES] =
{
    FLAG_KANTO_BADGE01_GET,
    FLAG_KANTO_BADGE02_GET,
    FLAG_KANTO_BADGE03_GET,
    FLAG_KANTO_BADGE04_GET,
    FLAG_KANTO_BADGE05_GET,
    FLAG_KANTO_BADGE06_GET,
    FLAG_KANTO_BADGE07_GET,
    FLAG_KANTO_BADGE08_GET,
};

static u32 BadgeCountForRegion(enum Region region)
{
    return (region == REGION_KANTO) ? NUM_KANTO_BADGES : NUM_BADGES;
}

u16 GetBadgeFlag(enum Region region, u32 index)
{
    if (index >= BadgeCountForRegion(region))
        return 0;
    if (region == REGION_KANTO)
        return gKantoBadgeFlags[index];
    return gBadgeFlags[index];
}

bool32 HasBadge(enum Region region, u32 index)
{
    return FlagGet(GetBadgeFlag(region, index)) != FALSE;
}

bool32 HasEveryBadgeInRegion(enum Region region)
{
    u32 i;

    for (i = 0; i < BadgeCountForRegion(region); i++)
    {
        if (!HasBadge(region, i))
            return FALSE;
    }
    return TRUE;
}

u32 CountBadges(enum Region region)
{
    u32 i, count = 0;

    for (i = 0; i < BadgeCountForRegion(region); i++)
    {
        if (HasBadge(region, i))
            count++;
    }
    return count;
}

u32 CountAllBadges(void)
{
    return CountBadges(REGION_HOENN) + CountBadges(REGION_KANTO);
}

void InitEventData(void)
{
    memset(gSaveBlock1Ptr->flags, 0, sizeof(gSaveBlock1Ptr->flags));
    memset(gSaveBlock1Ptr->vars, 0, sizeof(gSaveBlock1Ptr->vars));
    memset(sSpecialFlags, 0, sizeof(sSpecialFlags));
#if !IS_FRLG
    memset(gSaveBlock3Ptr->trainerFlags, 0, sizeof(gSaveBlock3Ptr->trainerFlags));
    PrepareTrainerFlagsForSave();
#endif
}

#if !IS_FRLG
static u16 TrainerFlagsChecksum(const u8 *flags, u32 size)
{
    u16 crc = 0xFFFF;
    u32 i, bit;

    // SaveBlock3's reserved sector bytes are not covered by the normal sector
    // checksum, so protect the trainer bitset separately (CRC-16/CCITT-FALSE).
    for (i = 0; i < size; i++)
    {
        crc ^= flags[i] << 8;
        for (bit = 0; bit < 8; bit++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}
#endif

void PrepareTrainerFlagsForSave(void)
{
#if !IS_FRLG
    gSaveBlock3Ptr->trainerFlagsVersion = TRAINER_FLAGS_SAVE_VERSION;
    gSaveBlock3Ptr->trainerFlagsChecksum = TrainerFlagsChecksum(gSaveBlock3Ptr->trainerFlags, sizeof(gSaveBlock3Ptr->trainerFlags));
#endif
}

bool32 IsTrainerFlagsSaveValid(const struct SaveBlock3 *save)
{
#if !IS_FRLG
    if (save->trainerFlagsVersion == TRAINER_FLAGS_SAVE_VERSION)
        return save->trainerFlagsChecksum == TrainerFlagsChecksum(save->trainerFlags, sizeof(save->trainerFlags));
    if (save->trainerFlagsVersion == TRAINER_FLAGS_SAVE_VERSION_V1)
        return save->trainerFlagsChecksum == TrainerFlagsChecksum(save->trainerFlags, LEGACY_TRAINER_FLAG_BYTES);

    return save->trainerFlagsVersion == 0 && save->trainerFlagsChecksum == 0;
#else
    return TRUE;
#endif
}

bool32 UpgradeTrainerFlagsSave(void)
{
#if !IS_FRLG
    if (!IsTrainerFlagsSaveValid(gSaveBlock3Ptr))
        return FALSE;
    if (gSaveBlock3Ptr->trainerFlagsVersion == TRAINER_FLAGS_SAVE_VERSION)
        return TRUE;

    // Legacy saves wrote zeros after their four-byte SaveBlock3. Do not interpret
    // an unknown version or a damaged header as legacy and silently replace data.
    // Preserve the ambiguous legacy bits in BOTH stores. Never clear story flags
    // or guess which event originally set a bit. Subsequent changes are independent.
    if (gSaveBlock3Ptr->trainerFlagsVersion == 0)
        memcpy(gSaveBlock3Ptr->trainerFlags,
               &gSaveBlock1Ptr->flags[LEGACY_TRAINER_FLAGS_START / 8],
               LEGACY_TRAINER_FLAG_BYTES);
    // V0/v1 shared trainer IDs across regions. Preserve the recorded defeats in
    // each region once, rather than resetting either region or guessing history.
    memcpy(gSaveBlock3Ptr->trainerFlags + LEGACY_TRAINER_FLAG_BYTES,
           gSaveBlock3Ptr->trainerFlags, KANTO_TRAINER_FLAG_BYTES);
    // Previously Kanto rivals read Hoenn's starter. Keep that behavior for old
    // saves until a new Oak gift explicitly records a different Kanto choice.
    VarSet(VAR_KANTO_STARTER, VarGet(VAR_STARTER_MON));
    PrepareTrainerFlagsForSave();
#endif
    return TRUE;
}

void ClearTempFieldEventData(void)
{
    memset(&gSaveBlock1Ptr->flags[TEMP_FLAGS_START / 8], 0, TEMP_FLAGS_SIZE);
    memset(&gSaveBlock1Ptr->vars[TEMP_VARS_START - VARS_START], 0, TEMP_VARS_SIZE);
    FlagClear(FLAG_SYS_ENC_UP_ITEM);
    FlagClear(FLAG_SYS_ENC_DOWN_ITEM);
    FlagClear(FLAG_SYS_USE_STRENGTH);
    FlagClear(FLAG_SYS_CTRL_OBJ_DELETE);
    FlagClear(FLAG_NURSE_UNION_ROOM_REMINDER);
}

void ClearDailyFlags(void)
{
    memset(&gSaveBlock1Ptr->flags[DAILY_FLAGS_START / 8], 0, DAILY_FLAGS_SIZE);
}

void DisableNationalPokedex(void)
{
    u16 *nationalDexVar = GetVarPointer(VAR_NATIONAL_DEX);
    gSaveBlock2Ptr->pokedex.nationalMagic = 0;
    *nationalDexVar = 0;
    FlagClear(FLAG_SYS_NATIONAL_DEX);
}

void EnableNationalPokedex(void)
{
    u16 *nationalDexVar = GetVarPointer(VAR_NATIONAL_DEX);
    gSaveBlock2Ptr->pokedex.nationalMagic = 0xDA;
    *nationalDexVar = 0x302;
    FlagSet(FLAG_SYS_NATIONAL_DEX);
    gSaveBlock2Ptr->pokedex.mode = DEX_MODE_NATIONAL;
    gSaveBlock2Ptr->pokedex.order = 0;
    ResetPokedexScrollPositions();
}

bool32 IsNationalPokedexEnabled(void)
{
    if (gSaveBlock2Ptr->pokedex.nationalMagic == 0xDA && VarGet(VAR_NATIONAL_DEX) == 0x302 && FlagGet(FLAG_SYS_NATIONAL_DEX))
        return TRUE;
    else
        return FALSE;
}

bool32 IsMysteryEventEnabled(void)
{
    return FlagGet(FLAG_SYS_MYSTERY_EVENT_ENABLE);
}

bool32 IsMysteryGiftEnabled(void)
{
    return FlagGet(FLAG_SYS_MYSTERY_GIFT_ENABLE);
}

void ClearMysteryGiftFlags(void)
{
    FlagClear(FLAG_MYSTERY_GIFT_DONE);
    FlagClear(FLAG_MYSTERY_GIFT_1);
    FlagClear(FLAG_MYSTERY_GIFT_2);
    FlagClear(FLAG_MYSTERY_GIFT_3);
    FlagClear(FLAG_MYSTERY_GIFT_4);
    FlagClear(FLAG_MYSTERY_GIFT_5);
    FlagClear(FLAG_MYSTERY_GIFT_6);
    FlagClear(FLAG_MYSTERY_GIFT_7);
    FlagClear(FLAG_MYSTERY_GIFT_8);
    FlagClear(FLAG_MYSTERY_GIFT_9);
    FlagClear(FLAG_MYSTERY_GIFT_10);
    FlagClear(FLAG_MYSTERY_GIFT_11);
    FlagClear(FLAG_MYSTERY_GIFT_12);
    FlagClear(FLAG_MYSTERY_GIFT_13);
    FlagClear(FLAG_MYSTERY_GIFT_14);
    FlagClear(FLAG_MYSTERY_GIFT_15);
}

void ClearMysteryGiftVars(void)
{
    VarSet(VAR_GIFT_PICHU_SLOT, 0);
    VarSet(VAR_GIFT_UNUSED_1, 0);
    VarSet(VAR_GIFT_UNUSED_2, 0);
    VarSet(VAR_GIFT_UNUSED_3, 0);
    VarSet(VAR_GIFT_UNUSED_4, 0);
    VarSet(VAR_GIFT_UNUSED_5, 0);
    VarSet(VAR_GIFT_UNUSED_6, 0);
    VarSet(VAR_GIFT_UNUSED_7, 0);
}

void DisableResetRTC(void)
{
    VarSet(VAR_RESET_RTC_ENABLE, 0);
    FlagClear(FLAG_SYS_RESET_RTC_ENABLE);
}

void EnableResetRTC(void)
{
    VarSet(VAR_RESET_RTC_ENABLE, 0x920);
    FlagSet(FLAG_SYS_RESET_RTC_ENABLE);
}

bool32 CanResetRTC(void)
{
    if (FlagGet(FLAG_SYS_RESET_RTC_ENABLE) && VarGet(VAR_RESET_RTC_ENABLE) == 0x920)
        return TRUE;
    else
        return FALSE;
}

u16 *GetVarPointer(u16 id)
{
    if (id < VARS_START)
        return NULL;
    else if (id <= VARS_END)
        return &gSaveBlock1Ptr->vars[id - VARS_START];
#if TESTING
    else if (id >= TESTING_VARS_START && id < TESTING_VARS_START + TEST_VARS_SIZE)
        return &sTestVars[id - TESTING_VARS_START];
#endif // TESTING
    else if (id >= SPECIAL_VARS_START && id <= SPECIAL_VARS_END)
        return gSpecialVars[id - SPECIAL_VARS_START];
    return NULL;
}

u16 VarGet(u16 id)
{
    u16 *ptr = GetVarPointer(id);
    if (!ptr)
        return id;
    return *ptr;
}

u16 VarGetIfExist(u16 id)
{
    u16 *ptr = GetVarPointer(id);
    if (!ptr)
        return 65535;
    return *ptr;
}

bool8 VarSet(u16 id, u16 value)
{
    u16 *ptr = GetVarPointer(id);
    if (!ptr)
        return FALSE;
    *ptr = value;
    return TRUE;
}

u16 VarGetObjectEventGraphicsId(u8 id)
{
    return VarGet(VAR_OBJ_GFX_ID_0 + id);
}

u8 *GetFlagPointer(u16 id)
{
    if (id == 0)
        return NULL;
#if !IS_FRLG
    else if (id >= TRAINER_FLAGS_START && id <= TRAINER_FLAGS_END)
        return &gSaveBlock3Ptr->trainerFlags[(id - TRAINER_FLAGS_START) / 8];
#endif
    else if (id < FLAGS_COUNT)
        return &gSaveBlock1Ptr->flags[id / 8];
#if TESTING
    else if (id >= TESTING_FLAGS_START && id < TESTING_FLAGS_START + TEST_FLAGS_SIZE * 8)
        return &sTestFlags[(id - TESTING_FLAGS_START) / 8];
#endif // TESTING
    else if (id >= SPECIAL_FLAGS_START && id <= SPECIAL_FLAGS_END)
        return &sSpecialFlags[(id - SPECIAL_FLAGS_START) / 8];
    return NULL;
}

u8 FlagSet(u16 id)
{
    u8 *ptr = GetFlagPointer(id);
    if (ptr)
        *ptr |= 1 << (id & 7);
    return 0;
}

u8 FlagToggle(u16 id)
{
    u8 *ptr = GetFlagPointer(id);
    if (ptr)
        *ptr ^= 1 << (id & 7);
    return 0;
}

u8 FlagClear(u16 id)
{
    u8 *ptr = GetFlagPointer(id);
    if (ptr)
        *ptr &= ~(1 << (id & 7));
    return 0;
}

bool8 FlagGet(u16 id)
{
    u8 *ptr = GetFlagPointer(id);

    if (!ptr)
        return FALSE;

    if (!(((*ptr) >> (id & 7)) & 1))
        return FALSE;

    return TRUE;
}
