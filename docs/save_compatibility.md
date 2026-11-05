# Save compatibility

The supported legacy baseline is the **Emerald target's 16-badge build**, commit
`792f59470d`. SaveBlock1, SaveBlock2, Pokémon storage, existing story/badge/variable IDs,
and SaveBlock3's original four-byte prefix retain their layouts. Trainer defeat flags
now use a versioned, append-only SaveBlock3 extension, upgraded automatically on load.

Saves from before the Kanto integration are a different format. They are not converted
automatically. The two layouts have no reliable version marker, so loading code must
not guess which one an unmarked save uses. FireRed and LeafGreen save compatibility
is outside this baseline.

## Measured layout

ARM GCC 14.2.1 measured these values with the build's `-mabi=apcs-gnu`, `-mthumb`,
and `-march=armv4t` settings:

| Structure or field | Size or byte offset |
| --- | --- |
| `SaveBlock1` size | `0x3CD0` (15,568 bytes) |
| `SaveBlock1.berryBlenderRecords` | `0x988` |
| `SaveBlock1.objectEvents` | `0x9FC` |
| `SaveBlock1.flags` | `0x123C` |
| `SaveBlock1.vars` | `0x1368`, size `0x234` (282 variables) |
| `SaveBlock1.gameStats` | `0x159C` |
| `SaveBlock2` size | `0xF2C` (3,884 bytes) |
| Legacy `SaveBlock3` prefix | `0x4` bytes, unchanged |
| New `SaveBlock3` size | `0xD4` (212 bytes) |
| `SaveBlock3.trainerFlagsVersion` | `0x4`, size 2 |
| `SaveBlock3.trainerFlagsChecksum` | `0x6`, size 2 |
| `SaveBlock3.trainerFlags` | `0x8`, size `0xCC` (204 bytes) |
| `PokemonStorage` size | `0x8560` (34,144 bytes) |
| `SaveSector` size | `0x1000` (4,096 bytes) |

The earlier integration removed a 52-byte filler before the event data and added
52 bytes to the variable array later. This preserved Emerald's total save-block size
while shifting the intervening fields. Restoring that filler now would move them
again and break the current 16-badge format.

## Safeguards

[save_layout.h](../include/save_layout.h) locks the five structures' total sizes,
122 top-level non-bitfield offsets and field sizes, the 41 relocated Kanto variable
IDs, the DexNav and Kanto starter variable IDs, all 16 badge flag IDs, and the Kanto League marker.
[save.c](../src/save.c) includes it in normal ROM builds, so the guards
do not depend on running the test ROM. A layout change fails compilation with the
affected field's name.

These guards do not fully describe nested structures, bitfield packing, or the
meaning of every flag, variable, item, species, and map ID. Reassigning saved IDs or
changing nested fields can still break compatibility without changing a top-level
offset. Review those changes separately. Adding new top-level fields also requires
review even if existing fields and total sizes do not move.

Do not update expected values merely to make a failing build pass. First decide how
existing saves will be supported. A future incompatible format needs an explicit
version and conversion strategy.

## Headless testing

With the build dependencies installed, run from the repository root:

```sh
make -j4 check TESTS=test/save.c
```

This uses the project's headless mGBA test runner. The save tests check the structure
sizes, read a synthetic byte fixture in the current layout through the Hoenn and
Kanto accessors, write the extended Kanto variable range, and verify that event-data
initialization preserves adjacent fields.

A fast compile-only check, without building a ROM or starting an emulator, is:

```sh
arm-none-eabi-gcc -fsyntax-only -x c -iquote include \
  -DMODERN=1 -DTESTING=0 -DEMERALD -std=gnu17 \
  -mthumb -mabi=apcs-gnu -march=armv4t include/save_layout.h
```

Negative checks during implementation confirmed that the guards reject both the
pre-Kanto field arrangement (even with the same total size) and a reassigned Kanto
variable ID. No personal save file was supplied or modified; the byte fixture is
synthetic, and does not replace testing a real save from the supported build.

The compile-only guard check passed in normal, test, and release configurations.
The event-data implementation also compiles for FireRed and LeafGreen; their trainer
storage and SaveBlock3 layout are unchanged. Their ROM builds also pass; their gameplay was not played through.

On November 5, 2026, `make -j4 check TESTS=test/save.c` built and linked the Emerald
test ROM with the shipped configuration, and all focused save/migration tests passed in headless mGBA.
The earlier DexNav configuration, missing FRLG tilesets, and Navel Rock label blockers
are fixed. This successful run uses no test-only DexNav override. Its local log is
`build/sweep-save-final.log`.

The regular `make -j4 all` build also succeeded, producing `Emerald.gba`.
Its local build log is `build/sweep-rom-final.log`. This verifies the build and targeted tests,
not a full gameplay walkthrough or loading a personal save. The full headless suite
also completed successfully: 5,177 passed with no unexpected failures; existing known,
TODO, expected-failure, and configuration-assumption exclusions are listed in the
[review](project_review.md).

## DexNav allocation

DexNav now uses the previously unreferenced slots `0x409B` and `0x409D` for its
registered species/environment and hidden-encounter step counter. Neither the variable
table nor any save block grows. Existing Kanto variable IDs remain unchanged.

The active-search flag is `0x4006` in the RAM-only special-flag array. It is not
serialized: a search must not survive a restart without its allocated search data.
DexNav menu access and detector mode use the existing Pokédex unlock flag, so both
new and current saves gain access when the Pokédex is available. Turning off
`DEXNAV_ENABLED` also hides its start-menu action.

The added regression test verifies unlock behavior, RAM-only search state, independent
variable storage, and initialization. It does not exercise the rendered DexNav UI or
a complete encounter in either region.

## Regional trainers and migration (format v2)

Hoenn trainer IDs retain their original values. Kanto IDs in the Emerald target are
`864 + native FRLG ID`; both party tables are compiled into the game. Native FireRed
and LeafGreen IDs stay unchanged. The 864 Hoenn and 768 Kanto defeat bits use logical
flag IDs `0x1000`–`0x165F`, separate from story flags. Kanto story IDs stay at
`0x500`–`0x7F9`, and system/badge flags remain anchored at `0x860`.

The current 212-byte SaveBlock3 uses existing reserved bytes in logical sectors 0
and 1. The original four-byte prefix, version field at offset 4, checksum at 6, and
first 108 trainer bytes at 8 retain their v1 positions. V2 appends 96 Kanto trainer
bytes. SaveBlock1, SaveBlock2, storage, main sector sizes, footer positions, and normal
primary checksum rules do not change.

The current marker is `0x5402`, with CRC-16/CCITT-FALSE over all 204 trainer bytes.
The loader assembles the extension across both sectors before choosing a save slot.
It rejects mixed save counters, invalid sector IDs, unknown versions, and bad CRCs.
A valid older slot is recovered with the existing backup-recovery status.

Supported upgrades happen in RAM on load and are persisted on the next normal save:

- **Original 16-badge format:** a zero version/checksum header identifies the old
  zero-filled reserved tail. Its 108 shared story/trainer bytes seed Hoenn defeats.
- **Format v1 (`0x5401`):** its CRC is checked over the original 108 trainer bytes,
  which retain their positions and values.
- **Both legacy formats:** the first 96 old trainer bytes also seed Kanto defeats.
  The old Hoenn starter choice seeds the previously-unused Kanto starter variable,
  preserving the previous rival selection. The result is stamped as v2.
- **Format v2:** no reimport from story bits or the other region occurs.

**Legacy ambiguity cannot be reconstructed.** Previously shared story/defeat bits
and shared regional trainer IDs do not reveal which event originally set them.
Migration preserves their recorded values in both interpretations, then separates
future updates. It does not reset Kanto or infer missing past events. Existing saves
also keep their recorded NPC flags; only new games run both regions' initial flag scripts.

Kanto starter gifts now record their own selection at `0x40A1`; Kanto's League
completion uses the previously-unused `0x8F8` flag. Neither changes Hoenn's starter
or completion state. An old save's historical Kanto League victory cannot be inferred
from Hoenn's completion flag: completing Kanto's League once records the new marker.
The exact species of an old Oak gift is likewise not recoverable in every save;
legacy rival behavior is retained until a new gift records its choice.

Recorded battles reuse one formerly-zero padding bit to distinguish the new trainer
namespace. Old partner IDs are translated on playback; new recordings store the new
namespace marker before their existing checksum is calculated. The recording structure
and checksum positions do not move.

Keep using the updated ROM after saving in v2. Older ROMs do not understand the new
trainer store and can overwrite its reserved bytes. Pre-Kanto save layouts still
require a separate conversion and are not supported by this migration.

Headless regressions cover original/v1 migration, both regional defeat stores,
second-sector persistence and corruption, backup recovery, malformed IDs, starter
selection, real trainer parties, and new-game flag initialization. No personal save
was modified. Full adventure progression still needs a gameplay walkthrough.
