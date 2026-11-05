#include "global.h"
#include "data.h"
#include "event_data.h"
#include "starter_choose.h"
#include "script.h"
#include "battle_setup.h"
#include "recorded_battle.h"
#include "test/test.h"

extern const struct Trainer gActualTrainers[DIFFICULTY_COUNT][TRAINERS_COUNT];
extern const u8 EventScript_ResetAllMapFlagsFrlg[];

TEST("Both regions retain their real gym leader parties")
{
    const struct Trainer *brock = &gActualTrainers[DIFFICULTY_NORMAL][TRAINER_LEADER_BROCK];
#if !IS_FRLG
    const struct Trainer *roxanne = &gActualTrainers[DIFFICULTY_NORMAL][TRAINER_ROXANNE_1];
    EXPECT_EQ(TRAINER_ROXANNE_1, 265);
    EXPECT_EQ(TRAINER_LEADER_BROCK, 864 + 314);
    EXPECT_EQ((u32)roxanne->partySize, 3);
    EXPECT_EQ(roxanne->party[0].species, SPECIES_GEODUDE);
#endif
    EXPECT_EQ((u32)brock->partySize, 2);
    EXPECT_EQ(brock->party[0].species, SPECIES_GEODUDE);
    EXPECT_EQ(brock->party[0].lvl, 12);
    EXPECT_EQ(brock->party[1].species, SPECIES_ONIX);
    EXPECT_EQ(brock->party[1].lvl, 14);
}

TEST("Every normal Kanto trainer has a populated party")
{
    for (u32 i = 1; i < TRAINERS_COUNT_FRLG; i++)
    {
        const struct Trainer *trainer = &gActualTrainers[DIFFICULTY_NORMAL][FRLG_TRAINER_ID(i)];
        EXPECT(trainer->partySize > 0);
        EXPECT(trainer->party != NULL);
    }
}

TEST("Kanto starter reads and league completion are independent from Hoenn")
{
    static const u16 species[] = {SPECIES_BULBASAUR, SPECIES_SQUIRTLE, SPECIES_CHARMANDER};
#if !IS_FRLG
    VarSet(VAR_STARTER_MON, 2);
    FlagSet(FLAG_SYS_GAME_CLEAR);
    FlagSet(FLAG_IS_CHAMPION);
    EXPECT(!FlagGet(FLAG_KANTO_LEAGUE_CLEAR));
    EXPECT(!FlagGet(FLAG_KANTO_LEAGUE_REMATCH));
#endif
    for (u32 i = 0; i < ARRAY_COUNT(species); i++)
    {
        VarSet(VAR_KANTO_STARTER, i);
        EXPECT_EQ(Script_GetKantoStarterPokemon(), species[i]);
#if !IS_FRLG
        EXPECT_EQ(VarGet(VAR_STARTER_MON), 2);
#endif
    }
    EXPECT_EQ(GetStarterPokemon(3), GetStarterPokemon(0));
    EXPECT_EQ(GetStarterPokemon(0xFFFF), GetStarterPokemon(0));
}

TEST("Kanto initial story flags do not mark Hoenn trainers defeated")
{
    InitEventData();
    RunScriptImmediately(EventScript_ResetAllMapFlagsFrlg);
    EXPECT(FlagGet(FLAG_HIDE_OAK_IN_HIS_LAB));
    EXPECT(FlagGet(FLAG_HIDE_CERULEAN_RIVAL));
    EXPECT_EQ(VarGet(VAR_MASSAGE_COOLDOWN_STEP_COUNTER), 500);
#if !IS_FRLG
    for (u32 i = 0; i < MAX_TRAINERS_COUNT_EMERALD; i++)
        EXPECT(!HasTrainerBeenFought(i));
#endif
}

#if !IS_FRLG
TEST("Recorded battles translate legacy partner IDs without changing new IDs")
{
    static const struct RecordedBattleSave legacy = {
        .partnerId = MAX_TRAINERS_COUNT_EMERALD + PARTNER_STEVEN,
    };
    static const struct RecordedBattleSave current = {
        .partnerId = TRAINER_PARTNER(PARTNER_STEVEN),
        .usesRegionalTrainers = TRUE,
    };
    EXPECT_EQ(RecordedBattle_GetPartnerTrainerId(&legacy), TRAINER_PARTNER(PARTNER_STEVEN));
    EXPECT_EQ(RecordedBattle_GetPartnerTrainerId(&current), TRAINER_PARTNER(PARTNER_STEVEN));
}
#endif
