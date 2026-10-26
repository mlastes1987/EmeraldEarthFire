#ifndef GUARD_CONSTANTS_SEAGALLOP_H
#define GUARD_CONSTANTS_SEAGALLOP_H

#define SEAGALLOP_VERMILION_CITY   0
#define SEAGALLOP_ONE_ISLAND       1
#define SEAGALLOP_TWO_ISLAND       2
#define SEAGALLOP_THREE_ISLAND     3
#define SEAGALLOP_FOUR_ISLAND      4
#define SEAGALLOP_FIVE_ISLAND      5
#define SEAGALLOP_SIX_ISLAND       6
#define SEAGALLOP_SEVEN_ISLAND     7
#define SEAGALLOP_CINNABAR_ISLAND  8
#define SEAGALLOP_NAVEL_ROCK       9
#define SEAGALLOP_BIRTH_ISLAND    10

// The HOENN crossing. It sits after the SEVII range on purpose: the destination menu
// (DrawSeagallopDestinationMenu) builds its pages by cycling SEAGALLOP_VERMILION_CITY ..
// SEAGALLOP_SEVEN_ISLAND, so an id past SEVEN_ISLAND is never offered as a Sevii
// destination and the pages stay exactly as they are.
#define SEAGALLOP_LILYCOVE_CITY   11

#define SEAGALLOP_MORE           254

#endif //GUARD_CONSTANTS_SEAGALLOP_H
