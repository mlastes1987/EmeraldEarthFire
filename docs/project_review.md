# Emerald Earth Fire integration sweep

Reviewed November 4–5, 2026. This is a source, build, and headless regression review,
not a complete gameplay walkthrough. The intended adventure is Hoenn's badges and
League followed by Kanto's badges and League.

## Resolved integration defects

- **Trainer/story flag collision:** trainers formerly shared `0x500`–`0x85F` with
  Kanto story flags. Trainer defeats now use independent, versioned SaveBlock3 data.
- **Regional trainer collision and wrong parties:** Kanto trainer IDs previously
  selected Hoenn parties. Kanto IDs are now offset by 864 in Emerald, both actual
  party tables are included, and each region has its own defeat bits.
- **Save compatibility claims:** equal block sizes did not preserve old field
  offsets. The supported 16-badge SaveBlock1/2/storage layout is locked by assertions;
  the append-only trainer extension supports both original and v1 saves.
- **Missing FRLG tilesets:** Emerald now includes both regions' graphics, metatiles,
  and tileset headers. The previously missing 62 definitions link successfully.
- **Broken map-script references:** Navel Rock's transition label matches its table;
  Rocket Hideout's trainer-condition call now has its required argument separator.
- **DexNav's zero flag/variable IDs:** unused slots `0x409B`/`0x409D` hold its persistent
  state; search state is RAM-only, and availability follows the Pokédex unlock.
- **Shared League progression:** Kanto's first challenge and rematch teams/dialogue
  use Kanto completion, not Hoenn's Champion/network flag. Its Hall of Fame entry
  records the separate completion marker.
- **Shared starter choice:** Kanto rivals read a dedicated starter variable. Oak's
  gift records its choice on successful receipt, before nickname helpers reuse temp
  variables. The script special now calls a proper no-argument Kanto species wrapper.
- **Missing initial Kanto state:** new Emerald games initialize both regions' NPC
  flags. Legacy story flags are preserved rather than reset during migration.
- **Out-of-bounds accesses:** variable IDs outside declared ranges are rejected;
  starter index 3 correctly falls back instead of reading past a three-entry array.
- **Hall of Fame disagreement:** both screens share the existing 50-team capacity,
  so either region can display the same saved record set.
- **Recorded partner compatibility:** legacy partner IDs translate to the expanded
  trainer namespace; new recordings mark their namespace in a former padding bit.

## Validation and practical limits

The focused headless suites exercise real Hoenn/Kanto party data, all 623 populated
Kanto trainers, regional defeat isolation, original/v1 save upgrades, normal flash
save/reload, CRC failures across both extension sectors, backup recovery, invalid
sector and variable IDs, starter handling, and story initialization. The regular
Emerald ROM also builds with ARM GCC in WSL. The complete headless engine suite is
run as the final sweep. The final run exited successfully with **5,177 passed**,
16 existing known-failing tests, 586 TODO tests, 9 expected-failure checks, and one
unmet assumption. There were no unexpected failures. The skipped map-popup test
requires Gen 3 popups, while this project's popup configuration uses another style.
Local logs: `build/sweep-full-final.log` and the supplementary save run in
`build/sweep-save-final.log`.

The map-tree and script-label audits supplement actual assembly/linking. Their
textual scans alone do not prove runtime behavior. The symbol audit's unresolved
macro-token report is an analyzer limitation, not a successful assembly result.

Emerald, FireRed, LeafGreen, and release ROM builds pass, as do the five compile-only
configuration checks. The build matrix restores the shared generated data to Emerald
at the end. A zero-size SaveBlock3 copy now returns before forming an out-of-bounds
pointer; optimized native builds enforce this with warnings treated as errors.
Logs: `build/sweep-build-matrix.log` and `build/sweep-rom-final.log`. FireRed/LeafGreen
retain their native trainer IDs and trainer storage; their gameplay was not played through.

## Save upgrade constraints

See [save compatibility](save_compatibility.md) for the exact v2 layout, CRC,
original/v1 migration rules, and the restriction on saving with older ROMs after
upgrading. Already-ambiguous bits cannot reveal which historical event set them.
Legacy Kanto League completion and the exact old gift choice cannot always be
reconstructed. No personal save was opened or modified.

## Remaining manual walkthrough

1. Complete Hoenn, receive the invitation, sail from Lilycove, and return from Kanto.
2. Check Oak's gift with party space, a full party, and a full PC; inspect nickname flows.
3. Earn each region's eight badges, check HM gates, obedience and badge displays,
   and save/reload in both regions.
4. Challenge Kanto's League for the first time, continue after credits, and rematch.
5. Visit every Sevii/event island and return; test missing tickets and cancellation.
6. Check Town Map pages, map rendering, trainer slides, respawns, and both Hall of
   Fame screens. Also exercise actual recorded-battle playback with a partner.

The sweep establishes tested integration behavior and buildability. It does not
claim that every map, side quest, or two-region story route has been played through.
