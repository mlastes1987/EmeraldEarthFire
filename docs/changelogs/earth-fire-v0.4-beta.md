# Emerald Earth Fire v0.4-beta

November 20, 2026

The Emerald build brings Hoenn and Kanto into one adventure. Play through Hoenn's eight badges and League, then sail to Kanto for another eight badges and its own League. This beta includes the new title screen and the integration fixes made since the 16-badge build.

## Title screen

- The title now shows **Emerald** and **EarthFire** on two centered lines beneath the Pokémon logo.
- Rayquaza, the scrolling clouds, and the pulsing markings remain animated. The sky and clouds use a red palette, and the logo shine flashes coral.
- The complete `pk87 v0.4-beta` label appears halfway between `PRESS START` and the copyright. Its font fits the entire label on one line, including the initial lowercase `p`.

## Hoenn and Kanto progression

- The Seagallop connects Lilycove Harbor with Vermilion City after the Hoenn League. Vermilion offers the return crossing and routes into the Sevii Islands as the relevant unlocks become available.
- The two regions track their eight badges independently. Kanto's first League challenge and rematches now use Kanto's own completion flag and teams. Both Hall of Fame screens read the same 50-team record set.
- Emerald loads the actual parties for Hoenn and all 623 populated Kanto trainer entries. Trainer defeat records are separate for each region and no longer collide with Kanto story flags.
- Professor Oak's Kanto gift records the starter when the Pokémon is received. Kanto rival scripts use that choice separately from the Hoenn starter.
- A new Emerald game initializes story and NPC flags for both regions.

## Maps and field features

- The Emerald build includes Kanto maps, layouts, event scripts, and FRLG tilesets alongside Hoenn's. Kanto layouts retain their original metatile format.
- Fixed the Navel Rock transition script label and the trainer condition in Rocket Hideout B1F.
- DexNav becomes available with the Pokédex. It now uses dedicated variables for the selected species and steps, plus a temporary flag for an active search.
- The project configuration also enables following Pokémon, the HGSS-style Pokédex, reusable TMs, and a summary view that can show IVs and EVs.

## Saves and recorded battles

- Saves from the supported 16-badge Emerald build upgrade on load. The existing story flags, badge IDs, and primary save layouts are preserved. A versioned and checksummed save extension stores trainer defeats.
- The upgrade preserves ambiguous historical defeat bits. It can neither identify which region originally set a shared bit nor reliably reconstruct an old Kanto League clear or starter choice.
- Save loading validates the complete trainer extension across both sectors and can recover from the backup slot when the active slot is corrupt. Out-of-range variable and starter IDs are handled safely.
- Recorded battles with legacy partner IDs translate those IDs into the expanded trainer table.
- Back up your save before upgrading. Once you save in this beta, continue using the updated ROM. Saves from before the Kanto variable relocation require a fresh game. See [save compatibility](../save_compatibility.md) for the supported baseline and upgrade details.

## Build and verification

- The standalone map-tree CI job now installs the tools its Makefile needs. The strict audit checks map, layout, and script references before the ROM links.
- The integration sweep passed 5,177 headless tests with no unexpected failures. The updated title was verified separately in headless mGBA, including its animation and the transition to the main menu.
- The full Hoenn-to-Kanto story still needs a continuous playthrough. See the [integration review](../project_review.md) for the remaining checks.
