VermilionCity_Frlg_MapScripts::
	map_script MAP_SCRIPT_ON_TRANSITION, VermilionCity_OnTransition
	map_script MAP_SCRIPT_ON_FRAME_TABLE, VermilionCity_OnFrame
	.byte 0

VermilionCity_OnFrame::
	map_script_2 VAR_MAP_SCENE_VERMILION_CITY, 2, VermilionCity_EventScript_ExitSSAnne
	.2byte 0

VermilionCity_EventScript_ExitSSAnne::
	lockall
	applymovement LOCALID_PLAYER, VermilionCity_Movement_ExitSSAnne
	waitmovement 0
	setvar VAR_MAP_SCENE_VERMILION_CITY, 3
	releaseall
	end

VermilionCity_Movement_ExitSSAnne::
	walk_up
	walk_up
	step_end

VermilionCity_OnTransition::
	setworldmapflag FLAG_WORLD_MAP_VERMILION_CITY
	call_if_set FLAG_TALKED_TO_OAKS_AIDE_IN_VERMILION, VermilionCity_EventScript_HideOaksAide
@ PROF. OAK waits at the end of the pier for the CHAMPION the letter in the player's house
@ invited over, so whether he is standing there turns on the same flag the SEAGALLOP and
@ the LILYCOVE crossing turn on. The object event itself is in the shared map.json, which
@ both builds read, so the FireRed build has to park it rather than not have it.
#if IS_FRLG
	setflag FLAG_HIDE_VERMILION_PROF_OAK
#else
	call_if_set FLAG_SYS_GAME_CLEAR, VermilionCity_EventScript_ShowProfOak
	call_if_unset FLAG_SYS_GAME_CLEAR, VermilionCity_EventScript_HideProfOak
#endif
	end

VermilionCity_EventScript_HideOaksAide::
	setflag FLAG_HIDE_VERMILION_CITY_OAKS_AIDE
	return

#if !IS_FRLG
VermilionCity_EventScript_ShowProfOak::
	clearflag FLAG_HIDE_VERMILION_PROF_OAK
	return

VermilionCity_EventScript_HideProfOak::
	setflag FLAG_HIDE_VERMILION_PROF_OAK
	return
#endif // !IS_FRLG

VermilionCity_EventScript_Woman::
	msgbox VermilionCity_Text_GrimerMultipliesInSludge, MSGBOX_NPC
	end

VermilionCity_EventScript_OldMan1::
	lock
	faceplayer
	goto_if_eq VAR_MAP_SCENE_VERMILION_CITY, 3, VermilionCity_EventScript_OldMan1SSAnneLeft
	msgbox VermilionCity_Text_DidYouSeeSSAnneInHarbor
	release
	end

VermilionCity_EventScript_OldMan1SSAnneLeft::
	msgbox VermilionCity_Text_SSAnneHasDepartedForYear
	release
	end

VermilionCity_EventScript_FerrySailor::
	lock
	faceplayer
@ A ROM that holds both regions turns this pier into the cross-region gateway: the
@ SEAGALLOP to the SEVII ISLES once the player is Champion, or the liner to HOENN.
@ A FireRed build keeps the original harbour exactly as it was.
#if !IS_FRLG
	goto VermilionCity_EventScript_ChooseSailDest
#else
	goto_if_eq VAR_MAP_SCENE_VERMILION_CITY, 3, VermilionCity_EventScript_CheckSeagallopPresent
	msgbox VermilionCity_Text_WelcomeToTheSSAnne
	release
	end
#endif // !IS_FRLG

#if !IS_FRLG
VermilionCity_EventScript_ChooseSailDest::
	message VermilionCity_Text_WhereShallISailYou
	waitmessage
	multichoice 13, 6, MULTI_SAIL_DEST, FALSE
	switch VAR_RESULT
	case 0, VermilionCity_EventScript_SailToSeviiIsles
	case 1, EventScript_SailToHoenn
	case 2, EventScript_CancelSail
	case MULTI_B_PRESSED, EventScript_CancelSail
	end

@ The SEVII excursion is the Champion's reward, so it is gated on the HOENN league
@ rather than on the KANTO story; a player who reaches VERMILION before that is told
@ to come back. Once it opens, control falls through to the harbour's own
@ RAINBOW PASS branch, so the MYSTIC and AURORA ticket trips and the whole
@ destination-page machinery behave exactly as they do in a FireRed build.
VermilionCity_EventScript_SailToSeviiIsles::
	goto_if_unset FLAG_SYS_GAME_CLEAR, VermilionCity_EventScript_SeagallopChampionsOnly
	call VermilionCity_EventScript_OpenSeviiIsles
	goto_if_eq VAR_RESULT, FALSE, VermilionCity_EventScript_NoRoomForRainbowPass
	setvar VAR_0x8004, SEAGALLOP_VERMILION_CITY
	goto VermilionCity_EventScript_ChooseSeagallopDestRainbowPass
	end

VermilionCity_EventScript_SeagallopChampionsOnly::
	msgbox VermilionCity_Text_SeagallopChampionsOnly, MSGBOX_DEFAULT
	release
	end

VermilionCity_EventScript_NoRoomForRainbowPass::
	msgbox VermilionCity_Text_NoRoomForRainbowPass, MSGBOX_DEFAULT
	release
	end

@ Puts the world in the state CELIO's errand would have left it in: the RAINBOW PASS
@ in the bag, the SEVII pages on the TOWN MAP, the pass variable at the value that
@ makes every island's harbour list every island, and therefore every island able to
@ sail back to VERMILION. A player who already ran the errand keeps what they earned.
VermilionCity_EventScript_OpenSeviiIsles::
	call VermilionCity_EventScript_GiveEventTickets
	checkitem ITEM_RAINBOW_PASS
	goto_if_eq VAR_RESULT, TRUE, EventScript_SetResultTrue
	msgbox VermilionCity_Text_ChampionGetsRainbowPass, MSGBOX_DEFAULT
	giveitem ITEM_RAINBOW_PASS
	goto_if_eq VAR_RESULT, FALSE, EventScript_SetResultFalse
	setvar VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F, 5
	setflag FLAG_SYS_SEVII_MAP_123
	setflag FLAG_SYS_SEVII_MAP_4567
	@ ONE ISLAND harbor's walk-out scene is how the story's crossing from CINNABAR
	@ puts the player onto the island; arm it for the first arrival only.
	goto_if_ne VAR_MAP_SCENE_ONE_ISLAND_HARBOR, 0, EventScript_SetResultTrue
	setvar VAR_MAP_SCENE_ONE_ISLAND_HARBOR, 1
	goto EventScript_SetResultTrue
	end

VermilionCity_Text_WhereShallISailYou:
	.string "Where shall I sail you today?$"

@ The MYSTIC and AURORA TICKETS were Wonder Card distributions, and this ROM can receive
@ no Wonder Card -- so NAVEL ROCK and BIRTH ISLAND were closed through *every* harbour,
@ because VERMILION's SEAGALLOP, the KANTO liner at LILYCOVE and HOENN's own S.S. TIDAL
@ all test the distribution flag as well as the item. The CHAMPION's welcome is where they
@ come from now, and the flag and the item are set together, exactly as the Mystery Gift
@ scripts set them. A full KEY ITEMS pocket is not treated as failure: the RAINBOW PASS is
@ what this harbour promises, so a ticket that will not fit simply waits for a later visit.
VermilionCity_EventScript_GiveEventTickets::
	setvar VAR_TEMP_0, FALSE
	checkitem ITEM_MYSTIC_TICKET
	goto_if_eq VAR_RESULT, TRUE, VermilionCity_EventScript_EnableNavelRockShip
	checkitemspace ITEM_MYSTIC_TICKET
	goto_if_eq VAR_RESULT, FALSE, VermilionCity_EventScript_EnableNavelRockShip
	giveitem ITEM_MYSTIC_TICKET
	setflag FLAG_RECEIVED_MYSTIC_TICKET
	setvar VAR_TEMP_0, TRUE
VermilionCity_EventScript_EnableNavelRockShip::
	setflag FLAG_ENABLE_SHIP_NAVEL_ROCK
	checkitem ITEM_AURORA_TICKET
	goto_if_eq VAR_RESULT, TRUE, VermilionCity_EventScript_EnableBirthIslandShip
	checkitemspace ITEM_AURORA_TICKET
	goto_if_eq VAR_RESULT, FALSE, VermilionCity_EventScript_EnableBirthIslandShip
	giveitem ITEM_AURORA_TICKET
	setflag FLAG_RECEIVED_AURORA_TICKET
	setvar VAR_TEMP_0, TRUE
VermilionCity_EventScript_EnableBirthIslandShip::
	setflag FLAG_ENABLE_SHIP_BIRTH_ISLAND
	goto_if_eq VAR_TEMP_0, FALSE, Common_EventScript_NopReturn
	msgbox VermilionCity_Text_ChampionGetsEventTickets, MSGBOX_DEFAULT
	return

VermilionCity_Text_ChampionGetsEventTickets:
	.string "And these, for the two islands\n"
	.string "the sailors talk about.\p"
	.string "A MYSTIC TICKET for NAVEL ROCK,\n"
	.string "an AURORA TICKET for BIRTH ISLAND.\p"
	.string "My SEAGALLOP calls at both -- and\n"
	.string "so does the LILYCOVE liner.$"

VermilionCity_Text_SeagallopChampionsOnly:
	.string "The SEAGALLOP to the SEVII ISLES\n"
	.string "carries CHAMPIONS only.\p"
	.string "Come back once you've won HOENN's\n"
	.string "league, and the archipelago is yours.$"

VermilionCity_Text_ChampionGetsRainbowPass:
	.string "So you're the CHAMPION of HOENN!\p"
	.string "Then no isle of SEVII is off\n"
	.string "limits to you.\p"
	.string "Here, take this RAINBOW PASS as\n"
	.string "a welcome from VERMILION!$"

VermilionCity_Text_NoRoomForRainbowPass:
	.string "Hm? There's no room in your BAG\n"
	.string "for this pass.\p"
	.string "Come back once you've made some.$"
#endif // !IS_FRLG

VermilionCity_EventScript_CheckHasMysticTicket::
	goto_if_unset FLAG_ENABLE_SHIP_NAVEL_ROCK, EventScript_SetResultFalse
	checkitem ITEM_MYSTIC_TICKET
	goto_if_eq VAR_RESULT, FALSE, EventScript_SetResultFalse
	goto EventScript_SetResultTrue
	end

VermilionCity_EventScript_CheckHasAuroraTicket::
	goto_if_unset FLAG_ENABLE_SHIP_BIRTH_ISLAND, EventScript_SetResultFalse
	checkitem ITEM_AURORA_TICKET
	goto_if_eq VAR_RESULT, FALSE, EventScript_SetResultFalse
	goto EventScript_SetResultTrue
	end

VermilionCity_EventScript_CheckSeagallopPresent::
	setvar VAR_0x8004, SEAGALLOP_VERMILION_CITY
	goto_if_ge VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F, 5, VermilionCity_EventScript_ChooseSeagallopDestRainbowPass
	goto_if_ge VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F, 1, VermilionCity_EventScript_ChooseSeagallopDestTriPass
	msgbox VermilionCity_Text_TheShipSetSail
	release
	end

VermilionCity_EventScript_ChooseSeagallopDestRainbowPass::
	call VermilionCity_EventScript_CheckHasMysticTicket
	goto_if_eq VAR_RESULT, TRUE, VermilionCity_EventScript_HasMysticTicket
	call VermilionCity_EventScript_CheckHasAuroraTicket
	goto_if_eq VAR_RESULT, TRUE, VermilionCity_EventScript_HasAuroraTicket
	setvar VAR_0x8004, SEAGALLOP_VERMILION_CITY
	message VermilionCity_Text_BoardSeagallopRainbowPass
	waitmessage
	goto EventScript_SeviiDestinationsPage1
	end

VermilionCity_EventScript_HasMysticTicket::
	call VermilionCity_EventScript_CheckHasAuroraTicket
	goto_if_eq VAR_RESULT, TRUE, VermilionCity_EventScript_HasMysticAndAuroraTickets
	call_if_unset FLAG_SHOWN_MYSTIC_TICKET, VermilionCity_EventScript_ShowMysticTicket
	message VermilionCity_Text_BoardSeagallopFerry
	waitmessage
	multichoice 17, 6, MULTI_SEVII_NAVEL, FALSE
	switch VAR_RESULT
	case 0, EventScript_SeviiDestinationsPage1
	case 1, EventScript_SailToNavelRock
	case 2, EventScript_CancelSail
	case 127, EventScript_CancelSail
	end

VermilionCity_EventScript_ShowMysticTicket::
	setflag FLAG_SHOWN_MYSTIC_TICKET
	msgbox VermilionCity_Text_OhMysticTicketTakeYouToNavelRock
	return

VermilionCity_EventScript_HasAuroraTicket::
	call_if_unset FLAG_SHOWN_AURORA_TICKET, VermilionCity_EventScript_ShowAuroraTicket
	message VermilionCity_Text_BoardSeagallopFerry
	waitmessage
	multichoice 13, 6, MULTI_SEVII_BIRTH, FALSE
	switch VAR_RESULT
	case 0, EventScript_SeviiDestinationsPage1
	case 1, EventScript_SailToBirthIsland
	case 2, EventScript_CancelSail
	case 127, EventScript_CancelSail
	end

VermilionCity_EventScript_ShowAuroraTicket::
	setflag FLAG_SHOWN_AURORA_TICKET
	msgbox VermilionCity_Text_OhAuroraTicketTakeYouToBirthIsland
	return

VermilionCity_EventScript_HasMysticAndAuroraTickets::
	call_if_unset FLAG_SHOWN_MYSTIC_TICKET, VermilionCity_EventScript_ShowMysticTicket
	call_if_unset FLAG_SHOWN_AURORA_TICKET, VermilionCity_EventScript_ShowAuroraTicket
	message VermilionCity_Text_BoardSeagallopFerry
	waitmessage
	multichoice 13, 5, MULTI_SEVII_NAVEL_BIRTH, FALSE
	switch VAR_RESULT
	case 0, EventScript_SeviiDestinationsPage1
	case 1, EventScript_SailToNavelRock
	case 2, EventScript_SailToBirthIsland
	case 3, EventScript_CancelSail
	case 127, EventScript_CancelSail
	end

EventScript_SailToNavelRock::
	msgbox VermilionCity_Text_Seagallop10Departing
	setvar VAR_0x8004, SEAGALLOP_VERMILION_CITY
	setvar VAR_0x8006, SEAGALLOP_NAVEL_ROCK
	goto EventScript_SetSail
	end

EventScript_SailToBirthIsland::
	msgbox VermilionCity_Text_Seagallop12Departing
	setvar VAR_0x8004, SEAGALLOP_VERMILION_CITY
	setvar VAR_0x8006, SEAGALLOP_BIRTH_ISLAND
	goto EventScript_SetSail
	end

VermilionCity_EventScript_ChooseSeagallopDestTriPass::
	message VermilionCity_Text_BoardSeagallopTriPass
	waitmessage
	setvar VAR_0x8004, SEAGALLOP_VERMILION_CITY
	multichoice 19, 5, MULTI_SEAGALLOP_123, FALSE
	switch VAR_RESULT
	case 0, EventScript_SailToOneIsland2
	case 1, EventScript_SailToTwoIsland2
	case 2, EventScript_SailToThreeIsland2
	case 3, EventScript_CancelSail
	case 127, EventScript_CancelSail
	end

@ Unreachable
Vermilion_EventScript_Unused::
	release
	end

VermilionCity_EventScript_CheckTicketLeft::
	lockall
	goto VermilionCity_EventScript_CheckTicket
	end

VermilionCity_EventScript_CheckTicketRight::
	lockall
	goto VermilionCity_EventScript_CheckTicket
	end

VermilionCity_EventScript_ExitedTicketCheck::
	lockall
	setvar VAR_VERMILION_CITY_TICKET_CHECK_TRIGGER, 0
	releaseall
	end

VermilionCity_EventScript_CheckTicket::
	textcolor NPC_TEXT_COLOR_MALE
	applymovement LOCALID_VERMILION_FERRY_SAILOR, Common_Movement_WalkInPlaceFasterLeft
	applymovement LOCALID_PLAYER, Common_Movement_WalkInPlaceFasterRight
	waitmovement 0
	goto_if_eq VAR_MAP_SCENE_VERMILION_CITY, 3, VermilionCity_EventScript_CheckSeagallopPresentTrigger
	msgbox VermilionCity_Text_DoYouHaveATicket
	goto_if_unset FLAG_GOT_SS_TICKET, VermilionCity_EventScript_DontHaveSSTicket
	msgbox VermilionCity_Text_FlashedSSTicket
	setvar VAR_VERMILION_CITY_TICKET_CHECK_TRIGGER, 1
	releaseall
	end

VermilionCity_EventScript_DontHaveSSTicket::
	msgbox VermilionCity_Text_DontHaveNeededSSTicket
	closemessage
	goto VermilionCity_EventScript_WalkUpPier
	end

VermilionCity_EventScript_CheckSeagallopPresentTrigger::
	setvar VAR_0x8004, SEAGALLOP_VERMILION_CITY
	goto_if_ge VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F, 5, VermilionCity_EventScript_ChooseSeagallopDestRainbowPass
	goto_if_ge VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F, 1, VermilionCity_EventScript_ChooseSeagallopDestTriPass
	msgbox VermilionCity_Text_TheShipSetSail
	closemessage
	goto VermilionCity_EventScript_WalkUpPier
	end

VermilionCity_EventScript_Sailor::
	msgbox VermilionCity_Text_SSAnneVisitsOnceAYear, MSGBOX_NPC
	end

VermilionCity_EventScript_OaksAide::
	lock
	faceplayer
	setflag FLAG_TALKED_TO_OAKS_AIDE_IN_VERMILION
	msgbox VermilionCity_Text_Route2AideHasPackageForYou
	release
	end

VermilionCity_EventScript_OldMan2::
	msgbox VermilionCity_Text_BuildingOnThisLand, MSGBOX_NPC
	end

VermilionCity_EventScript_Machop::
	lock
	faceplayer
	waitse
	playmoncry SPECIES_MACHOP, CRY_MODE_NORMAL
	msgbox VermilionCity_Text_Machop
	waitmoncry
	msgbox VermilionCity_Text_MachopStompingLandFlat
	release
	end

VermilionCity_EventScript_CitySign::
	msgbox VermilionCity_Text_CitySign, MSGBOX_SIGN
	end

VermilionCity_EventScript_SnorlaxNotice::
	msgbox VermilionCity_Text_SnorlaxBlockingRoute12, MSGBOX_SIGN
	end

VermilionCity_EventScript_PokemonFanClubSign::
	msgbox VermilionCity_Text_PokemonFanClubSign, MSGBOX_SIGN
	end

VermilionCity_EventScript_GymSign::
	lockall
	famechecker FAMECHECKER_LTSURGE, 0
	msgbox VermilionCity_Text_GymSign
	releaseall
	end

VermilionCity_EventScript_HarborSign::
	msgbox VermilionCity_Text_VermilionHarbor, MSGBOX_SIGN
	end

VermilionCity_Text_GrimerMultipliesInSludge::
	.string "We're careful about pollution here.\p"
	.string "We've heard GRIMER multiplies in\n"
	.string "toxic sludge.$"

VermilionCity_Text_DidYouSeeSSAnneInHarbor::
	.string "Did you see the S.S. ANNE moored\n"
	.string "in the harbor?$"

VermilionCity_Text_SSAnneHasDepartedForYear::
	.string "So, the S.S. ANNE has departed?\p"
	.string "She'll be back in VERMILION about\n"
	.string "this time next year.$"

VermilionCity_Text_BuildingOnThisLand::
	.string "I'm putting up a building on this\n"
	.string "plot of land. I own it all.\p"
	.string "My POKéMON is tamping the land\n"
	.string "to lay the foundation.$"

VermilionCity_Text_Machop::
	.string "MACHOP: Guoh! Gogogoh!$"

VermilionCity_Text_MachopStompingLandFlat::
	.string "A MACHOP is stomping the land flat.$"

VermilionCity_Text_SSAnneVisitsOnceAYear::
	.string "The S.S. ANNE is a famous luxury\n"
	.string "cruise ship.\p"
	.string "It visits VERMILION once a year.$"

VermilionCity_Text_CitySign::
	.string "VERMILION CITY\n"
	.string "The Port of Exquisite Sunsets$"

VermilionCity_Text_SnorlaxBlockingRoute12::
	.string "NOTICE!\p"
	.string "ROUTE 12 may be blocked off by a\n"
	.string "sleeping POKéMON.\p"
	.string "Detour through ROCK TUNNEL to\n"
	.string "LAVENDER TOWN.\p"
	.string "VERMILION POLICE$"

VermilionCity_Text_PokemonFanClubSign::
	.string "POKéMON FAN CLUB\n"
	.string "All POKéMON fans welcome!$"

VermilionCity_Text_GymSign::
	.string "VERMILION CITY POKéMON GYM\n"
	.string "LEADER: LT. SURGE\l"
	.string "The Lightning American!$"

VermilionCity_Text_VermilionHarbor::
	.string "VERMILION HARBOR$"

VermilionCity_Text_WelcomeToTheSSAnne::
	.string "Welcome to the S.S. ANNE!$"

VermilionCity_Text_DoYouHaveATicket::
	.string "Welcome to the S.S. ANNE!\p"
	.string "Excuse me, do you have a ticket?$"

VermilionCity_Text_FlashedSSTicket::
	.string "{FONT_NORMAL}{PLAYER} flashed the S.S. TICKET!\p"
	.string "{FONT_MALE}Great!\n"
	.string "Welcome to the S.S. ANNE!$"

VermilionCity_Text_DontHaveNeededSSTicket::
	.string "{FONT_NORMAL}{PLAYER} doesn't have the needed\n"
	.string "S.S. TICKET.\p"
	.string "{FONT_MALE}Sorry!\p"
	.string "You need a ticket to get aboard.$"

VermilionCity_Text_TheShipSetSail::
	.string "The ship set sail.$"

VermilionCity_Text_BoardSeagallopTriPass::
	.string "Ah, you have a TRI-PASS.\p"
	.string "Would you like to board\n"
	.string "a SEAGALLOP ferry?$"

VermilionCity_Text_Seagallop7Departing::
	.string "Okay, everything's in order.\p"
	.string "SEAGALLOP HI-SPEED 7 will be\n"
	.string "departing immediately.$"

VermilionCity_Text_BoardSeagallopRainbowPass::
	.string "Ah, you have a RAINBOW PASS.\p"
	.string "Would you like to board\n"
	.string "a SEAGALLOP ferry?$"

VermilionCity_Text_OhMysticTicketTakeYouToNavelRock::
	.string "Oh! That's a MYSTICTICKET!\n"
	.string "Now that is rare.\p"
	.string "We'll be happy to take you to\n"
	.string "NAVEL ROCK anytime.$"

VermilionCity_Text_OhAuroraTicketTakeYouToBirthIsland::
	.string "Oh! That's an AURORATICKET!\n"
	.string "Now that is rare.\p"
	.string "We'll be happy to take you to\n"
	.string "BIRTH ISLAND anytime.$"

VermilionCity_Text_BoardSeagallopFerry::
	.string "Would you like to board\n"
	.string "a SEAGALLOP ferry?$"

VermilionCity_Text_Seagallop10Departing::
	.string "Okay, everything's in order for you\n"
	.string "to board a special ferry.\p"
	.string "SEAGALLOP HI-SPEED 10 will be\n"
	.string "departing immediately.$"

VermilionCity_Text_Seagallop12Departing::
	.string "Okay, everything's in order for you\n"
	.string "to board a special ferry.\p"
	.string "SEAGALLOP HI-SPEED 12 will be\n"
	.string "departing immediately.$"

VermilionCity_Text_Route2AideHasPackageForYou::
	.string "Oh, hello, {PLAYER}!\n"
	.string "How are you doing?\p"
	.string "It's me, one of PROF. OAK's AIDES.\p"
	.string "Did you meet the other AIDE?\p"
	.string "He had a package from PROF. OAK\n"
	.string "for you, {PLAYER}.\p"
	.string "He said he'd look for you around\n"
	.string "ROUTE 2, {PLAYER}.\p"
	.string "If you're in the ROUTE 2 area,\n"
	.string "please look for him.$"

@ PROF. OAK, at the end of the VERMILION pier, for the CHAMPION of HOENN that the letter in
@ the player's house invited over. The map's ON_TRANSITION holds him behind
@ FLAG_HIDE_VERMILION_PROF_OAK until FLAG_SYS_GAME_CLEAR is set, so only a completed game
@ ever meets him -- but the label has to exist in *both* builds, because the object event
@ that names it is in the shared map.json, so a FireRed build gets an inert label instead
@ of the scene. The three starters are offered as a menu here; the FRLG lab scene puts a
@ ball on the floor for each of them, which is not something a pier can do.
VermilionCity_EventScript_ProfOak::
#if !IS_FRLG
	lock
	faceplayer
	goto_if_set FLAG_RECEIVED_KANTO_STARTER, VermilionCity_EventScript_ProfOakStarterTaken
	msgbox VermilionCity_Text_OakGreetsTheChampion, MSGBOX_DEFAULT
	message VermilionCity_Text_OakOffersThreeStarters
	waitmessage
	multichoice 13, 6, MULTI_VERMILION_KANTO_STARTER, FALSE
	switch VAR_RESULT
	case 0, VermilionCity_EventScript_ProfOakBulbasaur
	case 1, VermilionCity_EventScript_ProfOakCharmander
	case 2, VermilionCity_EventScript_ProfOakSquirtle
	case MULTI_B_PRESSED, VermilionCity_EventScript_ProfOakNoRush
	end
#else
@ A FireRed build keeps its OAK in PALLET TOWN; nothing calls this label there.
	release
	end
#endif // !IS_FRLG

#if !IS_FRLG
VermilionCity_EventScript_ProfOakNoRush::
	hidemonpic
	msgbox VermilionCity_Text_OakNoRush, MSGBOX_DEFAULT
	release
	end

@ One branch per starter: givemon takes its species as an immediate, so the three cannot
@ share a script the way the menu above is shared. Everything after the givemon is shared --
@ the fanfare, the nickname and the party-or-PC handoff are the same for all three, and they
@ are keyed on the species name in STR_VAR_1 and VAR_TEMP_TRANSFERRED_SPECIES.
VermilionCity_EventScript_ProfOakBulbasaur::
	showmonpic SPECIES_BULBASAUR, 10, 3
	msgbox VermilionCity_Text_OakBulbasaur, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, VermilionCity_EventScript_ProfOakNoRush
	bufferspeciesname STR_VAR_1, SPECIES_BULBASAUR
	setvar VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_BULBASAUR
	givemon SPECIES_BULBASAUR, 5
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PARTY, VermilionCity_EventScript_ProfOakStarterToParty
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PC, VermilionCity_EventScript_ProfOakStarterToPC
	hidemonpic
	goto Common_EventScript_NoMoreRoomForPokemon
	end

VermilionCity_EventScript_ProfOakCharmander::
	showmonpic SPECIES_CHARMANDER, 10, 3
	msgbox VermilionCity_Text_OakCharmander, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, VermilionCity_EventScript_ProfOakNoRush
	bufferspeciesname STR_VAR_1, SPECIES_CHARMANDER
	setvar VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_CHARMANDER
	givemon SPECIES_CHARMANDER, 5
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PARTY, VermilionCity_EventScript_ProfOakStarterToParty
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PC, VermilionCity_EventScript_ProfOakStarterToPC
	hidemonpic
	goto Common_EventScript_NoMoreRoomForPokemon
	end

VermilionCity_EventScript_ProfOakSquirtle::
	showmonpic SPECIES_SQUIRTLE, 10, 3
	msgbox VermilionCity_Text_OakSquirtle, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, VermilionCity_EventScript_ProfOakNoRush
	bufferspeciesname STR_VAR_1, SPECIES_SQUIRTLE
	setvar VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_SQUIRTLE
	givemon SPECIES_SQUIRTLE, 5
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PARTY, VermilionCity_EventScript_ProfOakStarterToParty
	goto_if_eq VAR_RESULT, MON_GIVEN_TO_PC, VermilionCity_EventScript_ProfOakStarterToPC
	hidemonpic
	goto Common_EventScript_NoMoreRoomForPokemon
	end

VermilionCity_EventScript_ProfOakStarterToParty::
	call VermilionCity_EventScript_ProfOakReceivedStarter
	msgbox gText_NicknameThisPokemon, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, VermilionCity_EventScript_ProfOakStarterHandedOver
	call Common_EventScript_GetGiftMonPartySlot
	call Common_EventScript_NameReceivedPartyMon
	goto VermilionCity_EventScript_ProfOakStarterHandedOver
	end

VermilionCity_EventScript_ProfOakStarterToPC::
	call VermilionCity_EventScript_ProfOakReceivedStarter
	msgbox gText_NicknameThisPokemon, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, VermilionCity_EventScript_ProfOakStarterSentToPC
	call Common_EventScript_NameReceivedBoxMon
	goto VermilionCity_EventScript_ProfOakStarterSentToPC
	end

VermilionCity_EventScript_ProfOakStarterSentToPC::
	call Common_EventScript_TransferredToPC
	goto VermilionCity_EventScript_ProfOakStarterHandedOver
	end

VermilionCity_EventScript_ProfOakReceivedStarter::
	setvar VAR_KANTO_STARTER, 0
	call_if_eq VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_SQUIRTLE, VermilionCity_EventScript_RecordSquirtleStarter
	call_if_eq VAR_TEMP_TRANSFERRED_SPECIES, SPECIES_CHARMANDER, VermilionCity_EventScript_RecordCharmanderStarter
	playfanfare MUS_OBTAIN_ITEM
	message VermilionCity_Text_OakStarterReceived
	waitmessage
	waitfanfare
	return

@ The gift is booked only once the POKéMON is actually in the player's hands: a full party
@ with a full PC falls through to Common_EventScript_NoMoreRoomForPokemon above without
@ reaching here, so OAK keeps his offer for the next visit.
VermilionCity_EventScript_ProfOakStarterHandedOver::
	hidemonpic
	setflag FLAG_RECEIVED_KANTO_STARTER
	msgbox VermilionCity_Text_OakRaiseItWell, MSGBOX_DEFAULT
	release
	end

VermilionCity_EventScript_RecordSquirtleStarter::
	setvar VAR_KANTO_STARTER, 1
	return

VermilionCity_EventScript_RecordCharmanderStarter::
	setvar VAR_KANTO_STARTER, 2
	return

VermilionCity_EventScript_ProfOakStarterTaken::
	msgbox VermilionCity_Text_OakHowIsYourStarter, MSGBOX_DEFAULT
	release
	end

VermilionCity_Text_OakGreetsTheChampion:
	.string "You are the CHAMPION of HOENN,\n"
	.string "or so I hear.\p"
	.string "My name is OAK. People call me\n"
	.string "the POKéMON PROFESSOR.\p"
	.string "I came down from PALLET TOWN to\n"
	.string "be here when your ship arrived.\p"
	.string "Now then… I have three POKéMON\n"
	.string "here that want a TRAINER.\p"
	.string "Will you take one of them?$"

VermilionCity_Text_OakOffersThreeStarters:
	.string "Which one will you take?$"

VermilionCity_Text_OakBulbasaur:
	.string "OAK: BULBASAUR, the GRASS\n"
	.string "POKéMON? You'll take it?$"

VermilionCity_Text_OakCharmander:
	.string "OAK: CHARMANDER, the FIRE\n"
	.string "POKéMON? You'll take it?$"

VermilionCity_Text_OakSquirtle:
	.string "OAK: SQUIRTLE, the WATER\n"
	.string "POKéMON? You'll take it?$"

VermilionCity_Text_OakNoRush:
	.string "OAK: No rush. Have a good look\n"
	.string "at the other two first.$"

VermilionCity_Text_OakStarterReceived:
	.string "{PLAYER} received the {STR_VAR_1}\n"
	.string "from PROF. OAK!$"

VermilionCity_Text_OakRaiseItWell:
	.string "OAK: POKéMON and people grow\n"
	.string "by being together.\p"
	.string "That one will grow with you.\n"
	.string "I know it.\p"
	.string "Enjoy KANTO, CHAMPION.$"

VermilionCity_Text_OakHowIsYourStarter:
	.string "OAK: How is the POKéMON I gave\n"
	.string "you doing?\p"
	.string "A starter is a partner for life.\n"
	.string "Look after each other.$"
#endif // !IS_FRLG

