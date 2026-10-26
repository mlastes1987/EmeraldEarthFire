ThreeIsland_Harbor_Frlg_MapScripts::
	.byte 0

ThreeIsland_Harbor_EventScript_Sailor::
	lock
	faceplayer
	message Text_WhereDoYouWantToSail
	waitmessage
	setvar VAR_0x8004, SEAGALLOP_THREE_ISLAND
#if !IS_FRLG
@ A ROM holding both regions offers the HOENN liner next to the island
@ sailings; see EventScript_SeviiSailDest.
	goto EventScript_SeviiSailDest
#else
	goto EventScript_ChooseDestFromIsland
#endif // !IS_FRLG
	end
