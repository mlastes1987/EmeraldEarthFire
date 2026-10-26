@ Separate selection menus for before the Rainbow Pass is obtained
EventScript_ChooseDestFromOneIsland::
	goto_if_ge VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F, 5, EventScript_SeviiDestinationsPage1
	goto_if_ge VAR_MAP_SCENE_CINNABAR_ISLAND, 4, EventScript_ChooseDestFromOneIslandVermilionAllowed
	multichoice 19, 6, MULTI_ISLAND_23, FALSE
	switch VAR_RESULT
	case 0, EventScript_SailToTwoIsland2
	case 1, EventScript_SailToThreeIsland2
	case 2, EventScript_CancelSail
	case MULTI_B_PRESSED, EventScript_CancelSail
	end

EventScript_ChooseDestFromOneIslandVermilionAllowed::
	multichoice 19, 5, MULTI_SEAGALLOP_V23, FALSE
	switch VAR_RESULT
	case 0, EventScript_SailToVermilion2
	case 1, EventScript_SailToTwoIsland2
	case 2, EventScript_SailToThreeIsland2
	case 3, EventScript_CancelSail
	case MULTI_B_PRESSED, EventScript_CancelSail
	end

EventScript_ChooseDestFromTwoIsland::
	goto_if_ge VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F, 5, EventScript_SeviiDestinationsPage1
	goto_if_ge VAR_MAP_SCENE_CINNABAR_ISLAND, 4, EventScript_ChooseDestFromTwoIslandVermilionAllowed
	multichoice 19, 6, MULTI_ISLAND_13, FALSE
	switch VAR_RESULT
	case 0, EventScript_SailToOneIsland2
	case 1, EventScript_SailToThreeIsland2
	case 2, EventScript_CancelSail
	case MULTI_B_PRESSED, EventScript_CancelSail
	end

EventScript_ChooseDestFromTwoIslandVermilionAllowed::
	multichoice 19, 5, MULTI_SEAGALLOP_V13, FALSE
	switch VAR_RESULT
	case 0, EventScript_SailToVermilion2
	case 1, EventScript_SailToOneIsland2
	case 2, EventScript_SailToThreeIsland2
	case 3, EventScript_CancelSail
	case MULTI_B_PRESSED, EventScript_CancelSail
	end

EventScript_SailToVermilion2::
	setvar VAR_0x8006, SEAGALLOP_VERMILION_CITY
	goto EventScript_SailToDest
	end

EventScript_SailToOneIsland2::
	setvar VAR_0x8006, SEAGALLOP_ONE_ISLAND
	goto EventScript_SailToDest
	end

EventScript_SailToTwoIsland2::
	setvar VAR_0x8006, SEAGALLOP_TWO_ISLAND
	goto EventScript_SailToDest
	end

EventScript_SailToThreeIsland2::
	setvar VAR_0x8006, SEAGALLOP_THREE_ISLAND
	goto EventScript_SailToDest
	end

EventScript_ChooseDestFromIsland::
	goto_if_ge VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F, 5, EventScript_SeviiDestinationsPage1
	goto_if_ge VAR_MAP_SCENE_CINNABAR_ISLAND, 4, EventScript_ChooseDestFromIslandVermilionAllowed
	multichoice 19, 6, MULTI_ISLAND_12, FALSE
	switch VAR_RESULT
	case 0, EventScript_SailToOneIsland2
	case 1, EventScript_SailToTwoIsland2
	case 2, EventScript_CancelSail
	case MULTI_B_PRESSED, EventScript_CancelSail
	end

EventScript_ChooseDestFromIslandVermilionAllowed::
	multichoice 19, 5, MULTI_SEAGALLOP_V12, FALSE
	switch VAR_RESULT
	case 0, EventScript_SailToVermilion2
	case 1, EventScript_SailToOneIsland2
	case 2, EventScript_SailToTwoIsland2
	case 3, EventScript_CancelSail
	case MULTI_B_PRESSED, EventScript_CancelSail
	end

EventScript_SailToDest::
	specialvar VAR_RESULT, GetSeagallopNumber
	buffernumberstring STR_VAR_1, VAR_RESULT
#if !IS_FRLG
@ The HOENN crossing is its own route, so its departure line names the destination
@ rather than using the VERMILION or SEVII wording. Only a ROM containing both
@ regions can reach either of these, so an FRLG build never enters them and keeps the
@ symbol table it always had.
	goto_if_eq VAR_0x8006, SEAGALLOP_LILYCOVE_CITY, EventScript_DepartingVermilionToHoenn
	goto_if_eq VAR_0x8004, SEAGALLOP_LILYCOVE_CITY, EventScript_DepartingFromLilycove
#endif // !IS_FRLG
	goto_if_eq VAR_0x8004, SEAGALLOP_VERMILION_CITY, EventScript_DepartingVermilion
	goto_if_ne VAR_0x8004, SEAGALLOP_VERMILION_CITY, EventScript_DepartingNotVermilion
	end

EventScript_DepartingVermilion::
	msgbox VermilionCity_Text_Seagallop7Departing
	goto EventScript_SetSail
	end

EventScript_DepartingNotVermilion::
	msgbox Text_AllAboardSeagallopNum
	goto EventScript_SetSail
	end

#if !IS_FRLG
EventScript_DepartingVermilionToHoenn::
	msgbox Text_AllAboardSeagallopToHoenn
	goto EventScript_SetSail
	end

EventScript_DepartingLilycoveToKanto::
	msgbox Text_AllAboardSeagallopToKanto
	goto EventScript_SetSail
	end

@ A LILYCOVE departure is named by where it is going: the KANTO mainland, or one of the
@ two event islands when the sailor has just been asked which. The islands are the KANTO
@ harbour family's own piers, reached through the same sSeag rows the VERMILION trips
@ use, so they add no new destination to anything.
EventScript_DepartingFromLilycove::
	goto_if_eq VAR_0x8006, SEAGALLOP_NAVEL_ROCK, EventScript_DepartingLilycoveToNavelRock
	goto_if_eq VAR_0x8006, SEAGALLOP_BIRTH_ISLAND, EventScript_DepartingLilycoveToBirthIsland
	goto EventScript_DepartingLilycoveToKanto
	end

EventScript_DepartingLilycoveToNavelRock::
	msgbox Text_AllAboardSeagallopToNavelRock
	goto EventScript_SetSail
	end

EventScript_DepartingLilycoveToBirthIsland::
	msgbox Text_AllAboardSeagallopToBirthIsland
	goto EventScript_SetSail
	end

@ The HOENN liner, from either end. Both crossings file the same two ids -- the KANTO
@ end and LILYCOVE -- rather than the pier the player actually boarded at, because
@ that pair is what the SEAGALLOP tables are keyed on: the direction bit, the
@ departure wording and the HI-SPEED number all come from it. A SEVII departure for
@ HOENN therefore sails, and sounds, exactly like the VERMILION one.
EventScript_SailToHoenn::
	setvar VAR_0x8004, SEAGALLOP_VERMILION_CITY
	setvar VAR_0x8006, SEAGALLOP_LILYCOVE_CITY
	goto EventScript_SailToDest
	end

EventScript_SailToKanto::
	setvar VAR_0x8004, SEAGALLOP_LILYCOVE_CITY
	setvar VAR_0x8006, SEAGALLOP_VERMILION_CITY
	goto EventScript_SailToDest
	end

@ What every SEVII harbour sailor reaches after asking where to sail: the island
@ sailings, or the HOENN liner. It sits in front of the vanilla island menus rather
@ than inside them, so the pages' geometry and the FRLG build's menus are untouched,
@ at the cost of one extra button press per island hop.
EventScript_SeviiSailDest::
	multichoice 13, 6, MULTI_SAIL_DEST, FALSE
	switch VAR_RESULT
	case 0, EventScript_SeviiSailDestIslands
	case 1, EventScript_SailToHoenn
	case 2, EventScript_CancelSail
	case MULTI_B_PRESSED, EventScript_CancelSail
	end

@ Which island menu a harbour shows depends on the port, and the sailor has already put
@ its id in VAR_0x8004, so dispatch on that: ONE and TWO ISLAND have menus of their own,
@ the other five share one.
EventScript_SeviiSailDestIslands::
	goto_if_eq VAR_0x8004, SEAGALLOP_ONE_ISLAND, EventScript_ChooseDestFromOneIsland
	goto_if_eq VAR_0x8004, SEAGALLOP_TWO_ISLAND, EventScript_ChooseDestFromTwoIsland
	goto EventScript_ChooseDestFromIsland
	end

@ NAVEL ROCK and BIRTH ISLAND each have a single KANTO sailing -- back to VERMILION --
@ so their menu names that destination outright rather than leading with the SEVII ISLES
@ entry the island harbours use to reach their own menus. The HOENN liner sits beside
@ it, exactly as it does at the island harbours.
EventScript_NavelBirthSailDest::
	multichoice 13, 6, MULTI_SAIL_DEST_VERMILION, FALSE
	switch VAR_RESULT
	case 0, EventScript_SailToVermilionCity
	case 1, EventScript_SailToHoenn
	case 2, EventScript_CancelSail
	case MULTI_B_PRESSED, EventScript_CancelSail
	end

Text_AllAboardSeagallopToHoenn:
	.string "All aboard SEAGALLOP HI-SPEED {STR_VAR_1}!\p"
	.string "This crossing calls at LILYCOVE\n"
	.string "CITY in HOENN.$"

Text_AllAboardSeagallopToKanto:
	.string "All aboard SEAGALLOP HI-SPEED {STR_VAR_1}!\p"
	.string "This crossing calls at VERMILION\n"
	.string "CITY in KANTO.$"

Text_AllAboardSeagallopToNavelRock:
	.string "All aboard SEAGALLOP HI-SPEED {STR_VAR_1}!\p"
	.string "This crossing calls at NAVEL\n"
	.string "ROCK in KANTO.$"

Text_AllAboardSeagallopToBirthIsland:
	.string "All aboard SEAGALLOP HI-SPEED {STR_VAR_1}!\p"
	.string "This crossing calls at BIRTH\n"
	.string "ISLAND in KANTO.$"
#endif // !IS_FRLG

EventScript_SetSail::
	closemessage
	delay 20
	fadescreen FADE_TO_BLACK
	special DoSeagallopFerryScene
	waitstate
	end

EventScript_CancelSail::
	specialvar VAR_RESULT, IsPlayerLeftOfVermilionSailor
	goto_if_eq VAR_RESULT, TRUE, VermilionCity_EventScript_WalkUpPier
	release
	end

VermilionCity_EventScript_WalkUpPier::
	closemessage
	applymovement LOCALID_VERMILION_FERRY_SAILOR, Common_Movement_FaceOriginalDirection
	applymovement LOCALID_PLAYER, VermilionCity_Movement_WalkUp
	waitmovement 0
	releaseall
	end

VermilionCity_Movement_WalkUp::
	walk_up
	step_end

EventScript_SeviiDestinationsPage1::
	setvar VAR_0x8005, 0
	special DrawSeagallopDestinationMenu
	waitstate
	specialvar VAR_0x8006, GetSelectedSeagallopDestination
	switch VAR_0x8006
	case SEAGALLOP_VERMILION_CITY, EventScript_SailToVermilionCity
	case SEAGALLOP_ONE_ISLAND,     EventScript_SailToOneIsland
	case SEAGALLOP_TWO_ISLAND,     EventScript_SailToTwoIsland
	case SEAGALLOP_THREE_ISLAND,   EventScript_SailToThreeIsland
	case SEAGALLOP_FOUR_ISLAND,    EventScript_SailToFourIsland
	case SEAGALLOP_MORE,           EventScript_SeviiDestinationsPage2
	case MULTI_B_PRESSED,          EventScript_CancelSail
	end

EventScript_SeviiDestinationsPage2::
	setvar VAR_0x8005, 1
	special DrawSeagallopDestinationMenu
	waitstate
	specialvar VAR_0x8006, GetSelectedSeagallopDestination
	switch VAR_0x8006
	case SEAGALLOP_FOUR_ISLAND,  EventScript_SailToFourIsland
	case SEAGALLOP_FIVE_ISLAND,  EventScript_SailToFiveIsland
	case SEAGALLOP_SIX_ISLAND,   EventScript_SailToSixIsland
	case SEAGALLOP_SEVEN_ISLAND, EventScript_SailToSevenIsland
	case SEAGALLOP_MORE,         EventScript_SeviiDestinationsPage1
	case MULTI_B_PRESSED,        EventScript_CancelSail
	end

EventScript_SailToVermilionCity::
	setvar VAR_0x8006, SEAGALLOP_VERMILION_CITY
	goto EventScript_SailToDest
	end

EventScript_SailToOneIsland::
	setvar VAR_0x8006, SEAGALLOP_ONE_ISLAND
	goto EventScript_SailToDest
	end

EventScript_SailToTwoIsland::
	setvar VAR_0x8006, SEAGALLOP_TWO_ISLAND
	goto EventScript_SailToDest
	end

EventScript_SailToThreeIsland::
	setvar VAR_0x8006, SEAGALLOP_THREE_ISLAND
	goto EventScript_SailToDest
	end

EventScript_SailToFourIsland::
	setvar VAR_0x8006, SEAGALLOP_FOUR_ISLAND
	goto EventScript_SailToDest
	end

EventScript_SailToFiveIsland::
	setvar VAR_0x8006, SEAGALLOP_FIVE_ISLAND
	goto EventScript_SailToDest
	end

EventScript_SailToSixIsland::
	setvar VAR_0x8006, SEAGALLOP_SIX_ISLAND
	goto EventScript_SailToDest
	end

EventScript_SailToSevenIsland::
	setvar VAR_0x8006, SEAGALLOP_SEVEN_ISLAND
	goto EventScript_SailToDest
	end

Text_AllAboardSeagallopNum::
	.string "All right!\n"
	.string "All aboard SEAGALLOP HI-SPEED {STR_VAR_1}!$"
