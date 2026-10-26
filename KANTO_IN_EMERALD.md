# Traveling Hoenn → Kanto on an Emerald build

Status: the build layer, the flag/variable relocation, the event-script assembly gate, the
Lilycove↔Vermilion sea route, the **post-game SEVII excursion** (section 6) and the
**two-region sixteen-badge scheme** (section 7) are implemented. The build's own scripting
tools were compiled and the real `preproc | cpp | preproc -ie` script pipeline replayed —
and the touched C files re-run through the build's own `cpp | preproc -i` path — so the
symbol-level claims below are measured rather than counted by hand — but **no ARM
assembler, linker or ROM build has been run**; see [Limitations](#limitations).

---

## TL;DR

The `how_to_frlg.md` tutorial says Kanto maps "merely have to be changed to Hoenn region".
That is **not** sufficient, and doing only that breaks the build. Four separate gates kept
Kanto out of an Emerald ROM; all four are now handled:

1. `tools/mapjson/mapjson.cpp` dropped every Kanto map and every FRLG layout from a
   non-FRLG build, so the maps were **absent from the ROM**, not merely unreachable.
2. Kanto map *scripts* were wrapped in `.if IS_FRLG` in `data/event_scripts.s` while
   `data/maps/events.inc` (which registers every map's object/coord/bg events) is
   assembled unconditionally. As soon as gate 1 was lifted this became **1731 undefined
   script labels** in an Emerald build — the ROM could not link.
3. Every Kanto flag was declared as the literal `0`, which `GetFlagPointer(0)` turns into
   **a silent no-op** — so Kanto hide flags and story flags never latched.
4. 41 Kanto map variables shared an id with a **live Hoenn variable**, so Kanto and Hoenn
   map state would overwrite each other.

Plus the actual journey is now scripted: a sailor in Lilycove City Harbor sails to
Vermilion City, and Vermilion's ferry sailor offers the crossing back — or, once the
player is Champion, the **post-game excursion to the SEVII ISLES** (section 6). Both
crossings ride the SEAGALLOP cut-scene rather than warping between regions (section 5).

Kanto's eight gym badges are now a **real second set** rather than eight aliases of
Hoenn's, so a single save can hold sixteen badges (section 7).

---

## Already wired for cross-region (do not rebuild this)

| Piece | Evidence |
| --- | --- |
| All 421 Kanto maps + 344 FRLG layouts exist as data | `data/maps/*_Frlg`, `data/layouts/layouts.json` |
| Kanto region map + Sevii subregion maps | `src/data/region_map/region_map_layout_kanto.h`, `..._sevii123/45/67.h` |
| Region-aware Town Map | `GetRegionMapType()`, `GetMapSecIdAt()` in [region_map.c](src/region_map.c) |
| FRLG tilesets | `src/data/tilesets/headers.h` — 44 `*_Frlg` tilesets, **not** version-gated |
| FRLG music | `sound/song_table.inc` — every `mus_rg_*` song, incl. `mus_rg_pallet` |
| FRLG trainers | `src/data/trainers_frlg.h` (auto-generated, built in all versions) |
| Every special usable in every build | `data/specials.inc` defines `SPECIAL_*` and builds `gSpecials` unconditionally, so every `special`/`specialvar` target has to exist in an Emerald build too |
| `IS_FRLG` byte on a layout | `MapLayout.isFrlg` — drives 640 vs 512 primary metatiles, 7 vs 6 primary palettes, and the FRLG metatile attribute format |

Only three headers version-gate anything script-facing, and none of them gate an id a
script uses: `flags.h` (which branch of the flag list), `vars.h` (the variable range),
`opponents.h` (`TRAINERS_COUNT`), plus `global.h` (SaveBlock1 fields).

---

## 1. The build gates (fixed)

Both live in `tools/mapjson/mapjson.cpp` and are selected by `MAP_VERSION`.

### Gate 1 — maps dropped by region (`process_groups`)

```cpp
if ((version == "emerald" && region != "REGION_HOENN") || ...)   // before
```

`invalid_maps` removes a map from `headers.inc`, `events.inc`, `connections.inc` and
`groups.inc`; its group becomes `.4byte NULL` in `gMapGroups`. That NULL is exactly the
tutorial's *"you cannot properly access map inside a vanilla map group from a different
game"* — you cannot warp to a NULL map group. Measured on the unmodified tool:

```
headers includes: 522 (518 Hoenn)   PalletTown included: 0   groups NULL: 41
```

### Gate 2 — FRLG layouts dropped by version (`generate_layout_headers_text`, `generate_layouts_table_text`)

Measured on the unmodified tool: `layout labels: 441   isFrlg TRUE: 0   table NULLs: 344`.

### Why "change the region to Hoenn" alone fails

1. Set `"region": "REGION_HOENN"` → Gate 1 passes and the header emits `.4byte PalletTown_Layout`.
2. Gate 2 still skips every FRLG layout → `PalletTown_Layout` is never defined →
   **undefined symbol at assembly.**
3. "Fix" it by also setting `"layout_version": "emerald"` and it links but renders wrong:
   that field is the only thing that sets `isFrlg`, which selects
   `NUM_METATILES_IN_PRIMARY_FRLG` (640) over 512, `NUM_PALS_IN_PRIMARY_FRLG` (7) over 6,
   and the FRLG attribute format (`ExtractMetatileAttribute`). FRLG layouts use
   `gTileset_General_Frlg` / `BuildingFrlg` / `PalletTown` with metatile ids above 512.

**Fix applied:** an Emerald build now accepts maps of either region and layouts of either
version, keeping `layout_version: "frlg"` intact so `isFrlg` stays TRUE. FireRed behaviour
is unchanged. `generate_layouts_constants_text` needed no change — it already assigns a
`LAYOUT_*` id to every layout with an existing border file.

| Check | Before | After |
| --- | --- | --- |
| Emerald layout labels / `isFrlg` TRUE / NULL table entries | 441 / 0 / 344 | 784 / 344 / **0** |
| Emerald map headers / NULL map groups | 518 / 41 | **935** / **0** |
| `MAP_GROUPS_COUNT` | 75 | 75 (unchanged) |
| Duplicate map ids / dangling layout refs | — | none |
| `include/constants/map_groups.h` vs unpatched | — | **byte-identical** (md5 `9e1e7583…`) |
| FireRed: Hoenn maps included | 0 | 0 (unchanged) |

The byte-identical `map_groups.h` result matters: `generate_map_constants_text` always
assigns ids for every group in `group_order`, so Kanto reuses already-reserved group indices
(`MAP_PALLET_TOWN = group 37, map 0`) and no existing Hoenn id moves. Existing saves stay
valid. ROM cost is small (~0.46 MiB of FRLG layout blockdata; tilesets/music/trainers were
already in the ROM).

---

## 2. The event-script assembly gate (fixed)

This is the gate that gate 1 exposed, and the reason a naive "make Kanto reachable" patch
cannot link.

`data/map_events.s` includes `data/maps/events.inc` **unconditionally**, and that aggregate
includes one `events.inc` per map — now including all 417 Kanto maps. A map's `events.inc`
is generated from `map.json` and names the scripts its objects, coord events and bg events
run:

```
object_event 1, OBJ_EVENT_GFX_WOMAN_1_FRLG, 3, 10, 3, ..., PalletTown_EventScript_SignLady, 0
```

Those labels live in `data/maps/<map>/scripts.inc`, and in `data/event_scripts.s` every one
of them sat inside a single `.if IS_FRLG` … `.endif` block, so an Emerald build never
assembled them.

Measured with [audit_kanto_script_labels.py](migration_scripts/audit_kanto_script_labels.py):

| | before | after |
| --- | --- | --- |
| Kanto maps | 421 | 421 |
| distinct script labels referenced by their events | 1741 | 1741 |
| **not defined anywhere an Emerald build assembles** | **1731** | **0** |

### Fix

The `.if IS_FRLG` / `.endif` pair around the FRLG script block in
[data/event_scripts.s](data/event_scripts.s) is removed (442 includes: 418 map scripts plus
18 `data/scripts` and 6 `data/text` files). Nothing else changed; in a FireRed build the
same 442 files were already assembled, so only the Emerald build is affected.

This is safe because:

- **No duplicate labels.** [audit_duplicate_labels.py](migration_scripts/audit_duplicate_labels.py)
  walks the whole 1025-file include tree: 0 labels are defined in more than one file.
- **All specials exist in every build.** `data/specials.inc` builds `gSpecials` with
  `.4byte <function>` for all 640 entries in every build, so any FRLG-only C function it
  names must already be compiled into an Emerald build.
- **No constant is version-gated.** Only four headers contain `#if … IS_FRLG`, and none of
  them defines a script-visible constant.
- No `.include` appears twice and no include target is missing (1003 includes checked).

Verified end to end by
[audit_event_scripts_assembly.py](migration_scripts/audit_event_scripts_assembly.py), which
replays the real pipeline (`preproc -s` inlines every `.include`, then the C preprocessor
runs, then the assembler):

```
emerald: distinct labels=18560  duplicate names=1 (real 0, assembler .if/.else variants 1)
frlg:    distinct labels=18559  duplicate names=1 (real 0, assembler .if/.else variants 1)
```

The one "duplicate" is `MtChimney_EventScript_BagIsFull`, defined twice in
`data/maps/MtChimney/scripts.inc` inside mutually exclusive assembler
`.if OW_SHOW_ITEM_DESCRIPTIONS` branches. That is a deliberate variant, it is Hoenn, and it
predates this work.

### Measured at assembler level, not file level

The label count above reasons about which *files* are assembled. That is only a proxy: the
build's script rule is

```
preproc -s $< charmap.txt | cpp -D<GAME> ... - | preproc -ie $< charmap.txt | as
```

and the first `preproc` pass inlines every `.include` **before** any conditional is
evaluated, so the FRLG block is present as *text* even in the pre-change Emerald build. What
actually removes it is the assembler: cpp substitutes the `IS_FRLG` macro, leaving
`.if 0`, and `as` drops the block.

[audit_script_symbols.py](migration_scripts/audit_script_symbols.py) removes the guesswork.
It runs the real pipeline (`tools/preproc`, the C preprocessor, and `preproc -ie`, which is
what expands the headers' C `enum` bodies into `.global X; .equiv X, ...` definitions), then
evaluates the `.if`/`.elseif`/`.else`/`.endif` directives the way `as` would, and finally
reports names that are referenced but never defined. Run on the working tree and on
`HEAD:data/event_scripts.s` with everything else identical:

| Emerald build, `data/event_scripts.s` | `HEAD` | now |
| --- | --- | --- |
| symbols defined in the unit | 25,958 | **34,896** |
| script labels named by map event data with no live definition | **1,732** | **0** |
| constant-like names referenced but not defined | 235 | 235 (identical set) |
| FireRed build: symbols defined in the unit | 34,893 | 34,893 (identical) |

The 235 are the analyser's own ceiling, not a build error: they are names that appear only
as *macro-invocation arguments* (`.byte SCR_OP_ADDCOINS`, `msgbox ..., MSGBOX_NPC`,
`bg_event ..., BG_EVENT_HIDDEN_ITEM`), which `as` resolves — or discards — when it expands
the macro. They are byte-identical in both trees and in both builds, which is the point:
**un-gating the FRLG block adds 8,938 definitions and zero new unresolved names.** The
FireRed column matters too — 34,893 symbols with the same definitions in both revisions
means the FireRed script unit is untouched by this change, which is now measured rather
than argued.

---

## 3. Kanto flags (fixed)

`include/constants/flags.h` gates on `IS_FRLG`. In the non-FRLG branch a `// FRLG flags`
section declared 762 Kanto/Sevii flags as literal `0`. That is a deliberate-looking
placeholder, but it is a trap:

```c
u8 *GetFlagPointer(u16 id)
{
    if (id == 0)
        return NULL;          // ← every Kanto flag was silently inert
```

`FlagSet(0)` does nothing and `FlagGet(0)` is always FALSE, so Kanto hide flags never
latched and Kanto story flags never persisted. (It is *not* memory corruption and it does
not hide every NPC — the calls are no-ops.)

### Fix

[migration_scripts/relocate_kanto_defines.py](migration_scripts/relocate_kanto_defines.py)
assigns every flag in that section a unique id from the free Emerald flag range, in FireRed
id order so the numbering stays traceable:

```
#define FLAG_HIDE_BULBASAUR_BALL      0x500 // FRLG 0x28
#define FLAG_HIDE_OAK_IN_HIS_LAB      0x503 // FRLG 0x2B
```

Flag id layout, for reference:

| Range | Contents |
| --- | --- |
| `0x000`–`0x01F` | temporary flags |
| `0x050`–`0x4FE` | Emerald hide flags |
| **`0x500`–`0x85F`** | **free — 864 ids; the Kanto flags now occupy 0x500–0x7F9 (762)** |
| `0x860`–`0x91F` | `SYSTEM_FLAGS` |
| `0x920`–`0x95F` | daily flags (`FLAGS_COUNT` = `0x960`) |
| `0x4000`+ | special flags (separate array) |

Verification: 762 names → 762 unique ids, **0 Kanto flag left at `0`**, 0 name shared by two
ids, 0 real id collisions among all 1813 flat `FLAG_*` definitions in the file, and every
id inside `0x500`–`0x7F9` so nothing reaches `SYSTEM_FLAGS` at `0x860`. No save-block size
change was needed.

---

## 4. Kanto variables (fixed)

`vars.h` includes `vars_frlg.h` unconditionally, so Kanto variables always had real values —
but 41 of them collided with a variable that Hoenn code actually uses. Examples:

| id | Hoenn variable | Kanto variable |
| --- | --- | --- |
| `0x4050` | `VAR_LITTLEROOT_TOWN_STATE` | `VAR_MAP_SCENE_PALLET_TOWN_OAK` |
| `0x405E` | `VAR_SOOTOPOLIS_CITY_STATE` | `VAR_MAP_SCENE_ROUTE16` |
| `0x403A` | `VAR_FARAWAY_ISLAND_STEP_COUNTER` | `VAR_ELEVATOR_FLOOR` |

The last one is a pre-existing Emerald bug the fix also repairs: `src/field_specials.c`
sets `VAR_ELEVATOR_FLOOR` for the department-store elevator, which by coincidence also
incremented Faraway Island's step counter.

### Sizes

Counting only names that are actually **referenced** (comments and the definition headers
excluded): 254 names need 254 ids, 213 were occupied, so the table genuinely had to grow.

### Fix

- `include/constants/vars.h`: extend `VARS_END` from `0x40FF` to `0x4119` (282 slots) **for
  non-FRLG builds only**; FRLG keeps `0x40FF` so its save layout is untouched.
- `include/config/save.h`: `FREE_EXTRA_SEEN_FLAGS_SAVEBLOCK1 TRUE`, which frees exactly 52
  bytes of a field `global.h` itself labels *"Previously Dex Flags, feel free to remove."*
- 26 extra slots × 2 bytes = 52 bytes. **Net `sizeof(struct SaveBlock1)` change: 0**, so
  `RomHeader::saveBlock1Size` is unchanged and existing saves still load. The 26 is chosen
  for that arithmetic, not for the number of ids in use — if you move one endpoint, move the
  other to match.
- The script emits an `#if !IS_FRLG` block in `vars.h` that `#undef`s and re-points the 41
  colliding Kanto variables into free slots, taken from the top of the range so they stay
  clear of Emerald's low ids and never land in the temporary range (0x4000–0x400F, cleared
  on every map load).

Verification: **254 referenced names → 254 distinct ids, 0 shared**, nothing in the
temporary range, highest id `0x4119` = `VARS_END`. Fifteen of the assigned slots had a
`VAR_UNUSED_0x40xx` placeholder defined at that id; those placeholders are never referenced
(one comment in `include/config/battle.h` mentions the convention), so reusing the slot is
the intended use.

---

## 5. The Lilycove ↔ Vermilion sea route

A ferry rather than a map connection, because Hoenn and Kanto do not share a border.

**Outbound** — new sailor in [LilycoveCity_Harbor](data/maps/LilycoveCity_Harbor/map.json)
at `(4, 13)`, `LOCALID_LILYCOVE_HARBOR_KANTO_SAILOR`, script
`LilycoveCity_Harbor_EventScript_KantoSailor`
([scripts.inc](data/maps/LilycoveCity_Harbor/scripts.inc)). Yes/No prompt, then the
SEAGALLOP cut-scene.

`(4, 13)` sits on `LAYOUT_HARBOR` row 13, which is uniformly metatile 514 — the same floor
the existing harbor objects stand on — and it is neither a warp tile nor an occupied tile.

**Return** — Vermilion's `VermilionCity_EventScript_FerrySailor`
(`LOCALID_VERMILION_FERRY_SAILOR`, at `(24, 33)`, script field unchanged) offers the
destination menu of section 6, whose `LILyCOVE CITY` entry takes the same route
([scripts.inc](data/maps/VermilionCity_Frlg/scripts.inc)).

### The crossing sails with the SEAGALLOP scene

Two shared labels own the route — `EventScript_SailToHoenn` and `EventScript_SailToKanto`
in [seagallop.inc](data/scripts/seagallop.inc) — and each does nothing but set
`VAR_0x8004` (origin) and `VAR_0x8006` (destination) before `goto EventScript_SailToDest`,
the same instructions every Sevii trip uses. So the player gets the ferry cut-scene, the
“all aboard” line and the fade instead of blinking into another region, and the route has
one definition instead of one per pier. Three places call them: the Vermilion pier's menu,
the Lilycove sailor, and every SEVII harbour (below). A SEVII departure files the same
*pair* as the VERMILION one — the KANTO end and LILYCOVE — rather than the pier it actually
left from, because that pair is what the tables are keyed on.
[src/seagallop.c](src/seagallop.c) carries it:

| Table | Entry | Effect |
| --- | --- | --- |
| `sSeag` | `[SEAGALLOP_LILYCOVE_CITY]` = Lilycove Harbor at `(8, 11)` | where `Task_Seagallop_3` warps the player |
| `sTravelDirectionMatrix` | bit `SEAGALLOP_LILYCOVE_CITY` **set** in `VERMILION`'s row, **clear** in `LILYCOVE`'s own row | `GetDirectionOfTravel()` gives eastbound out, westbound back — which flips the water tilemap, the BG scroll and the ferry sprite |
| `GetSeagallopNumber` | the pair returns **8** | HOENN gets a boat of its own instead of inheriting KANTO's HI-SPEED 7 |

`SEAGALLOP_LILYCOVE_CITY` is `11`, placed *after* the Sevii range on purpose:
`DrawSeagallopDestinationMenu` builds its pages by cycling `SEAGALLOP_VERMILION_CITY` …
`SEAGALLOP_SEVEN_ISLAND`, so an id beyond `SEVEN_ISLAND` can never be offered as a Sevii
destination and the islands' own menus are untouched. Without the direction bit the
feature would still work, but both legs would sail the same way: `GetDirectionOfTravel()`
reads bit 11 of an 11-bit row, gets 0, and calls everything westbound.

Two departure lines were added for the route (`Text_AllAboardSeagallopToHoenn`,
`Text_AllAboardSeagallopToKanto`), because `EventScript_SailToDest` picks its wording from
the origin and “SEAGALLOP HI-SPEED 7 departing for the SEVII ISLES” is wrong for a trip to
LILYCOVE. Both sit inside `#if !IS_FRLG`.

### Landing tiles were measured, not guessed

Both coordinates come from the layout blockdata named by `blockdata_filepath` in
`data/layouts/layouts.json`, which stores a 10-bit metatile id, a 2-bit collision value and
a 4-bit elevation per block (see `include/global.fieldmap.h`):

| tile | layout | metatile | collision | elevation | warp event | NPC |
| --- | --- | --- | --- | --- | --- | --- |
| Vermilion `(23, 32)` | `LAYOUT_VERMILION_CITY` 48×40 | `0x1C6` | 0 passable | 3 | no | none |
| Lilycove `(8, 11)` | `LAYOUT_HARBOR` 24×15 | `0x202` | 0 passable | 3 | no | none |

`(8, 11)` is the tile **every** other Hoenn ferry arrival uses — `SSTidalCorridor`,
`SouthernIsland_Exterior`, `NavelRock_Harbor`, `BirthIsland_Harbor`,
`FarawayIsland_Entrance` and `BattleFrontier_OutsideWest` all warp there.

`(23, 32)` is the tile the vanilla `sSeag` table already names for a VERMILION arrival, which
is why adopting the SEAGALLOP scene needed no new coordinate. It is not one of the S.S. Anne
dock tiles `(22,34) (23,34) (24,34)`: those are real warp events to `MAP_SSANNE_EXTERIOR`
(`MB_SOUTH_ARROW_WARP`), and `VermilionCity_EventScript_FerrySailor` itself stands on
`(24, 33)`. The bare-warp revision of this feature used `(23, 33)` (metatile `0x309`, also
passable, measured the same way); nothing references it now that the SEAGALLOP table owns
the arrival.

### Hoenn is reachable from the islands, not just from Vermilion

All seven SEVII harbour sailors end their “where do you want to sail?” prologue with a
`goto` into a vanilla island-destination entry point, so one shared label in
[seagallop.inc](data/scripts/seagallop.inc) is all the change needs:

```asm
	multichoice 13, 6, MULTI_SAIL_DEST, FALSE
	switch VAR_RESULT
	case 0, EventScript_SeviiSailDestIslands
	case 1, EventScript_SailToHoenn
	case 2, EventScript_CancelSail
```

`EventScript_SeviiSailDestIslands` then dispatches back into the vanilla menus — which one
depends on the port, and the sailor has already put its id in `VAR_0x8004`, so it branches
on that: `SEAGALLOP_ONE_ISLAND` → `EventScript_ChooseDestFromOneIsland`,
`SEAGALLOP_TWO_ISLAND` → `EventScript_ChooseDestFromTwoIsland`, everything else →
`EventScript_ChooseDestFromIsland`. The seven sailor scripts each gained three lines and a
`#if !IS_FRLG` around their `goto`.

**NAVEL ROCK and BIRTH ISLAND.** These two piers are SEVII-family harbours as well — a
MYSTIC or AURORA ticket trip lands on `NavelRock_Harbor_Frlg` / `BirthIsland_Harbor_Frlg`
(the `sSeag` table names them) — but their vanilla sailor offers exactly **one**
destination, back to VERMILION, so the island pre-menu's SEVII ISLES entry would dispatch
into an island menu a player standing there cannot legitimately reach. They get the same
crossing with their own wording instead: `EventScript_NavelBirthSailDest` offers VERMILION,
LILYCOVE CITY or EXIT, and the two scripts swap their `multichoice 20, 8,
MULTI_SEAGALLOP_VERMILION` for a `goto` behind the same `#if !IS_FRLG` guard.

That needs a second three-item list, `MULTI_SAIL_DEST_VERMILION`, because the opening
entry has to name VERMILION and every existing VERMILION-first list carries four items.
The menu also moves from `(20, 8)` to `13, 6` — the position the other three-item
KANTO/SEVII menus use. The window is sized from its widest entry and clamped left by
`ScriptMenu_AdjustLeftCoordFromWidth`, so a 13-character line like LILYCOVE CITY would have
been pushed to `(15, 8)` at the old coordinate regardless; matching the sibling menus is
both tidier and the only way the two piers look alike.

The Hoenn-flavoured `NavelRock_Harbor` / `BirthIsland_Harbor` are deliberately left alone:
those twin maps are reached from [LilycoveCity_Harbor](data/maps/LilycoveCity_Harbor/scripts.inc)
by the Hoenn S.S. Tidal and already sail back to LILYCOVE, so they own a crossing to the
other region already. It is the KANTO-side piers that had nowhere else to go.

**Why a menu in front of the island menus rather than inside them.**
`DrawSeagallopDestinationMenu` builds its page by cycling the eight ids
`SEAGALLOP_VERMILION_CITY` … `SEAGALLOP_SEVEN_ISLAND` and skipping the port you are standing
in, so there is no free row to add a ninth destination to without changing the menu for
every port in every build. The pre-menu keeps the pages — and a FireRed build's menus —
byte-identical, at the cost of one extra button press per island hop for the player.

**A consequence worth knowing:** a player who is mid-KANTO-story on SEVII can now leave for
HOENN (and so, via LILYCOVE and back, reach VERMILION) without finishing Celio's errand,
where vanilla FRLG funnels them through it because the island menus offer VERMILION only
from pass value 4 up. Nothing in the save state changes — the funnel is simply no longer
compulsory — and the HOENN liner is what makes “the islands can always sail home” true at
both ends of the route.

The dock warp is only triggered from `ProcessPlayerFieldInput` when the player moves south
onto the tile (`TryArrowWarp` in [field_control_avatar.c](src/field_control_avatar.c)), never
on a scripted placement, so arriving on a warp tile would not re-trigger it — but the free
tile is still the better choice.

The Lilycove side is deliberately **not** gated on `FLAG_SYS_GAME_CLEAR`, so Kanto is
reachable immediately for testing. To make it post-game, add one line at the top of the
Lilycove sailor:

```asm
	goto_if_unset FLAG_SYS_GAME_CLEAR, LilycoveCity_Harbor_EventScript_KantoSailorDeclined
```

The Vermilion side is no longer a bare Yes/No prompt: it is a destination menu, and the
SEVII half of it *is* gated (section 6). The Lilycove half stays ungated on purpose — a
player who reaches Kanto early must always be able to sail home.

### Hoenn reaches the KANTO event islands too

The Lilycove KANTO sailor now also sells the two ticket berths, so the KANTO-side NAVEL
ROCK and BIRTH ISLAND — the `_Frlg` harbour family the Seagallop from VERMILION lands on —
no longer need VERMILION as a stopover. The tickets are tested by **calling VERMILION's own
checks**, `VermilionCity_EventScript_CheckHasMysticTicket` / `...CheckHasAuroraTicket`
(flag *and* item), so a port that opens at one end of the crossing opens at the other, and
there is one definition of what “holds a MYSTIC TICKET” means.

As at VERMILION, the menu is built for the tickets you actually hold, since a destination
you cannot sail to is never offered:

| tickets held | menu |
| --- | --- |
| neither | the original Yes/No prompt, VERMILION only |
| MYSTIC | VERMILION / NAVEL ROCK / EXIT |
| AURORA | VERMILION / BIRTH ISLAND / EXIT |
| both | VERMILION / NAVEL ROCK / BIRTH ISLAND / EXIT |

The three lists are `MULTI_SAIL_DEST_NAVEL`, `..._BIRTH` and `..._NAVEL_BIRTH`, with
`gText_Vermilion` leading where VERMILION's harbour has `gText_SeviiIslands` leading.
Each island leg files the pair `(LILYCOVE, NAVEL ROCK)` / `(LILYCOVE, BIRTH ISLAND)` and
falls into `EventScript_SailToDest`, so the `sSeag` rows, the HI-SPEED number
(`GetSeagallopNumber` answers 8 for anything touching LILYCOVE) and the screen direction
(`sTravelDirectionMatrix[LILYCOVE]` is 0, i.e. westbound, the same as the run to
VERMILION) all come from tables that already exist. The arrivals are the vanilla `(8, 5)`
of each `_Frlg` harbour, the same tiles the VERMILION trips use.

`EventScript_SailToDest` gained one level of indirection for this: an origin of LILYCOVE
now goes to `EventScript_DepartingFromLilycove`, which names the island it is bound for or
falls through to the existing VERMILION wording. Without it the dest-first check would have
printed “this crossing calls at VERMILION CITY” for a NAVEL ROCK trip, because the KANTO
end of every HOENN sailing is VERMILION by construction.

**Two things followed from that, and both are now handled** — the duplicate island sets in
the next subsection, and the fact that no ticket could be bought in the first place in the
one after it.

### One island, one dungeon

HOENN already had a ticket trip to *its own* NAVEL ROCK and BIRTH ISLAND — the vanilla
Emerald SS Tidal menu in the same harbour (`LilycoveCity_Harbor_EventScript_FerryRegularLocationSelect`
→ `GoToNavelRock` / `GoToBirthIsland`), and it did **not** reach the same place as the new
liner: the two sets are complete parallel dungeons. `NavelRock_Bottom`/`Top` against
`NavelRock_Base_Frlg`/`Summit_Frlg`, `BirthIsland_Exterior` against
`BirthIsland_Exterior_Frlg` — same legendaries, and crucially **different flags**
(`FLAG_CAUGHT_LUGIA` + `FLAG_DEFEATED_LUGIA` against `FLAG_FOUGHT_LUGIA` +
`FLAG_LUGIA_FLEW_AWAY`, and so on). So one save could take LUGIA, HO-OH and DEOXYS twice
over, once per cave, and LILYCOVE offered two berths both named NAVEL ROCK that were not the
same place.

The fix is consolidation onto the **KANTO** set, which is not a preference but a build
constraint: `sSeag` is shared C with no version guard, and a FireRed build NULLs the HOENN map
groups, so the FRLG maps are the only ones a Seagallop can name in every build. All **four**
warps that reached the HOENN pair — `GoToNavelRock`, `GoToNavelRockFirstTime`,
`GoToBirthIsland`, `GoToBirthIslandFirstTime` — now land on the `_Frlg` harbours at `(8, 5)`,
the arrival tile `sSeag` already gives those maps (not the `(8, 4)` they used: the two harbour
layouts differ). One island, one dungeon, one name: LILYCOVE's two sailors now agree, and the
legendaries are unique per save again.

The HOENN maps were then **deleted** rather than left dead: "The HOENN island set is gone"
below records what went and what the revert is. The respawn path is unaffected: a fainted
LUGIA/OH-OH/DEOXYS comes back on the *KANTO* Hall of Fame (`hall_of_fame_frlg.inc` clears the
FRLG “flew away” flags, and `PokemonLeague_HallOfFame_Frlg` calls it), which is the same route
a player who reached the island from VERMILION always had.

`migration_scripts/audit_harbour_routes.py` is what found the second pair of warps — I had
missed the two `...FirstTime` variants — and what now states the result: every island the
tree still has is reached by a harbour, its list of island sections being the KANTO
`MAPSEC_NAVEL_ROCK_FRLG` / `MAPSEC_BIRTH_ISLAND_FRLG` rather than the HOENN pair the
consolidation retired.

### The HOENN island set is gone

The consolidation left the HOENN dungeons built but unreachable, which was never a resting
place: 24 maps of content no build could reach, two of the same three legendaries, and a
`mapjson` group holding both halves of a name pair. They are now **deleted outright** — 24
map dirs (`data/maps/{NavelRock_*,BirthIsland_*}`, the `_Frlg` twins untouched), the 10
layouts only they used including their `data/layouts/*/map.bin`, the 2 tilesets only they used
(`gTileset_NavelRock`, `gTileset_IslandHarbor`, whose FRLG namesakes stay), their 24
`.include` lines in [data/event_scripts.s](data/event_scripts.s), their entries in
`data/maps/map_groups.json` and `data/layouts/layouts.json` — and with them the two HOENN
region-map sections `MAPSEC_NAVEL_ROCK` / `MAPSEC_BIRTH_ISLAND`, which is why
[src/region_map.c](src/region_map.c) no longer lists them in `sMapSecIdsOffMap` and
[src/map_name_popup.c](src/map_name_popup.c) drops their four theme rows.

The only C that actively branched on the two sets was the BIRTH ISLAND rock: `ChangeDeoxysRockLevel`
used to pick a map group/number by which BIRTH ISLAND the player stood on and
`IncrementBirthIslandRockStepCount` tested both. Both now name `MAP_BIRTH_ISLAND_EXTERIOR_FRLG`
only ([src/field_specials.c](src/field_specials.c)).

**The revert is the same four warps plus the map files.** LILYCOVE's `GoToNavelRock` /
`GoToBirthIsland` and their `...FirstTime` twins are still the only thing that chose between
the sets and still name the `_Frlg` harbours, and the 24 map dirs, 10 layout dirs and 2
tileset dirs are tracked files, so `git checkout` of those paths brings the HOENN dungeons
back. The C and JSON edits above are what would have to be undone with them.

It was checked the way the build checks itself. `mapjson groups`, `mapjson layouts` and
`mapjson event_constants` (emerald) all exit 0 against the edited JSON and regenerate every
affected header; `jsonproc` regenerates `region_map_entries.h` and
`region_map_sections.h` after the two sections go; and the new
[audit_map_tree_consistency.py](migration_scripts/audit_map_tree_consistency.py) reports
**0** JSON findings and **0** surviving references to any of the 36 retired names, where a
retired name means a map dir, a layout id or a tileset symbol. The rest of the suite is
unchanged: 0 duplicate labels, 0 unresolved Kanto labels, 0 gated-include collisions, and the
symbol audit's **235** unresolved constants are the same set as its baseline (they are C enum
values its preprocessor cannot see, `SCR_OP_*` and `NO_MUSIC`), with **0** map-event script
labels without a live definition. What is *not* established here is a compiled ROM: the tree
has no toolchain on this host, so the C edits above are checked by reading and by name
resolution, not by a compiler.

### The tickets have a source now

Every route to those islands — VERMILION's SEAGALLOP, the KANTO liner at LILYCOVE, and
HOENN's own S.S. TIDAL — tests `FLAG_ENABLE_SHIP_NAVEL_ROCK` /
`FLAG_ENABLE_SHIP_BIRTH_ISLAND` **as well as** the ticket item, and the only code that set
those flags was the Mystery Gift pair ([gift_mystic_ticket.inc](data/scripts/gift_mystic_ticket.inc),
[gift_aurora_ticket.inc](data/scripts/gift_aurora_ticket.inc)) — a distributed Wonder Card this
ROM has no way to receive. The debug menu's “Fill Pocket Key Items” would hand over the items
(it adds every `POCKET_KEY_ITEMS` entry in the game) but not the flags, so the berth was still
refused. All three routes were closed in a normal playthrough.

`VermilionCity_EventScript_GiveEventTickets` is now the source. It hangs off
`VermilionCity_EventScript_OpenSeviiIsles` — the CHAMPION hand-off that already gives the
RAINBOW PASS — so it inherits that gate, and the same sailor that says “so you're the
CHAMPION of HOENN” hands over both tickets. Flag and item are set together, exactly as the
Mystery Gift scripts set them, and the ship flag is set even when the item is already held, so
a ticket that arrived some other way still opens the berth. A full KEY ITEMS pocket is not an
error: the pass is what that harbour promises, so the tickets simply wait for a later visit.

One consequence worth stating plainly: a Champion who has never spoken to VERMILION's ferry
sailor still has no ticket, which is why the berths are *announced* rather than merely offered
— see below.

### A closed berth says so

Both ticket menus are built for the tickets you hold, so a player without them sees no
event-island option at all — the berth is silent rather than closed. `Lilycove`'s KANTO sailor
is the one place that has to describe what it cannot sell, because it is the sailor a HOENN
player meets first: holding neither ticket, it runs a second prompt,
`LilycoveCity_Harbor_Text_KantoSailorWelcomeNoTickets`, in place of its usual welcome. It names
both berths, the ticket each needs, and where those come from — VERMILION's harbour, to a
CHAMPION of HOENN. A player who is not Champion yet therefore learns what the post-game reward
is before they can claim it, and a player who has claimed it never sees the text again because
the ticket branch intercepts them first.

HOENN's own S.S. TIDAL menu is built in C (`ScriptMenu_CreateLilycoveSSTidalMultichoice`) and
still lists nothing it cannot sell; giving *it* a hint would mean changing that C menu for both
regions, so it is left as vanilla Emerald has it. The FRLG island piers need no hint either:
standing on the island is proof the berth was already bought.

---

## 6. The post-game SEVII excursion

The SEVII ship with FRLG is intact in this tree — `src/seagallop.c` (the ferry cut-scene
and its `sSeag` destination table), `src/script_menu.c` (`DrawSeagallopDestinationMenu` /
`GetSelectedSeagallopDestination`), the harbour sailor in every Sevii harbour, and all of
the Sevii maps. What was missing was an entrance a *Champion* could use: Vermilion's
ferry sailor returned on the Hoenn crossing before it could ever reach the Seagallop, so
in an Emerald build the Seagallop itself was dead code. (The islands were still reachable
the long way round — Bill's boat at CINNABAR — but that is the story's trip, not a
post-game one.)

### Route

`VermilionCity_EventScript_FerrySailor` now opens a destination menu
([scripts.inc](data/maps/VermilionCity_Frlg/scripts.inc)):

| Choice | Result |
| --- | --- |
| `SEVII ISLANDS` | the harbour's own RAINBOW PASS branch, `VermilionCity_EventScript_ChooseSeagallopDestRainbowPass` |
| `LILYCOVE CITY` | the SEAGALLOP cut-scene to Hoenn (section 5) |
| `EXIT` / B | `EventScript_CancelSail` |

The menu is `MULTI_VERMILION_SAIL_DEST`, added to
[include/constants/script_menu.h](include/constants/script_menu.h) — appended **last**, so
no existing `MULTI_*` id shifts — and to `sMultichoiceLists` in
[src/data/script_menu.h](src/data/script_menu.h). Both strings it needs,
`gText_SeviiIslands` and `gText_LilycoveCity`, already existed, so the menu needed no new
text constant.

### Why the branch rejoins the FRLG code instead of bypassing it

Choosing the islands calls `VermilionCity_EventScript_OpenSeviiIsles` and then continues
into `VermilionCity_EventScript_ChooseSeagallopDestRainbowPass` — the same label a FireRed
player reaches. The MYSTIC/AURORA ticket prompts, the two-page destination menu and every
island's own return menu therefore keep working instead of being reimplemented.

`OpenSeviiIsles` puts the save into the state Celio's errand would have left it in:

| Written | Why |
| --- | --- |
| `giveitem ITEM_RAINBOW_PASS` | the pass the menus and the player's BAG expect |
| `setvar VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F, 5` | the value every harbour uses to decide whether it may offer all seven islands *and* VERMILION |
| `setflag FLAG_SYS_SEVII_MAP_123` / `_4567` | the Sevii pages on the TOWN MAP, exactly as the story sets them |
| `setvar VAR_MAP_SCENE_ONE_ISLAND_HARBOR, 1` (only when still 0) | arms ONE ISLAND harbour's own walk-out scene for the first arrival, the way the story crossing from CINNABAR does |

The pass variable is what makes the round trip work. Below 4 the harbour menus
(`EventScript_ChooseDestFromIsland` and friends in
[data/scripts/seagallop.inc](data/scripts/seagallop.inc)) deliberately list only the
neighbouring islands and **no** VERMILION — that is the vanilla funnel through Celio's
quest, and it would strand a post-game visitor on SEVII with no way home. At 5 the
`SeviiDestinationsPage1/2` menus apply, whose first page starts at
`SEAGALLOP_VERMILION_CITY` and skips only the port you are standing in, so every island
can sail back to VERMILION and Vermilion can reach every island.

A player who already ran Celio's errand keeps exactly what they earned: `OpenSeviiIsles`
returns TRUE immediately when the RAINBOW PASS is already in the BAG.

One vanilla quirk to know about, read out of `DrawSeagallopDestinationMenu` rather than
observed in a running game: the page is built by skipping the port you are standing in,
and the row is left blank rather than reflowed. From VERMILION — the departure this
feature adds — that produces exactly ONE / TWO / THREE / FOUR / OTHER / EXIT, so the outbound
trip is clean. From a Sevii harbour the blank row moves into the list and FOUR ISLAND can
fall off the first page entirely, which is why the *return* leg is the one that matters
for this feature and why every harbour lists VERMILION in row 0.

### What it deliberately does not touch

- **Celio's RUBY/SAPPHIRE errand.** The excursion short-circuits it by moving the pass
  variable to 5. That is what makes a free post-game visit possible, but it does mean the
  quest's dialogue will not play out for a player who takes this route first. Design
  choice, recorded here rather than hidden.
- **The CINNABAR crossing.** Bill's boat and `CinnabarIsland_EventScript_SailToOneIsland`
  are untouched, so the story trip still happens on its own terms.
- **FireRed/LeafGreen builds.** Every new label and text is inside `#if !IS_FRLG`, so an
  FRLG build assembles the harbour script it always did (measured in the audit table in
  section 2's verification notes).
- **A refusal rather than silence before the league.** The gate is `FLAG_SYS_GAME_CLEAR`,
  set by [post_battle_event_funcs.c](src/post_battle_event_funcs.c) on the first Hall of
  Fame entry. Pre-league the SEVII choice answers that the SEAGALLOP carries Champions
  only, and the HOENN crossing stays available so nobody can be stranded in Kanto.

## 7. Sixteen badges across two regions

The Kanto gym scripts set `FLAG_BADGE01_GET` … `FLAG_BADGE08_GET`. In an Emerald build
those are *Hoenn's* badges — beating Brock lit Roxanne's STONE BADGE — while in a FireRed
build they are Kanto's. A ROM holding both regions needs two independent sets.

### Flags

`include/constants/flags.h` (non-FRLG branch) gains eight region-scoped flags in the
`SYSTEM_FLAGS` area, `FLAG_KANTO_BADGE01_GET` … `08`, at `0x8F0`–`0x8F7`;
`include/constants/flags_frlg.h` aliases the same eight names onto that build's own badge
ids, so exactly one set exists per build and the script text is version-independent.

### Accessors instead of arithmetic

[src/event_data.c](src/event_data.c) / [include/event_data.h](include/event_data.h) add
`gKantoBadgeFlags[NUM_KANTO_BADGES]` plus `GetBadgeFlag`, `HasBadge`,
`HasEveryBadgeInRegion`, `CountBadges(region)` and `CountAllBadges()`. `NUM_BADGES` is
both a loop bound and an array size in at least eight places, so it stays 8 and the region
is passed in instead.

### Consumers made region-aware

| Site | Change |
| --- | --- |
| Kanto gym and badge scripts (8 gym maps, Route 23 checkpoints, Viridian gate) | set/check the Kanto flags |
| Catch-rate badge malus ([battle_script_commands.c](src/battle_script_commands.c)) | counts the badges of the region the battle is in |
| Field moves / HM gates ([field_move.c](src/field_move.c)) | requirement chosen through the accessor, so one build can gate a Hoenn HM on Hoenn badges and a Kanto obstacle on Kanto badges |
| Save menu badge count ([menu.c](src/menu.c)) | `CountAllBadges()`, printed with **two** digits; it was written as a single digit and would have capped at 9 |
| Main menu “Continue” badge count ([main_menu.c](src/main_menu.c)) | same, left-aligned two digits |
| Shop badge criteria ([shop_criteria.c](src/shop_criteria.c)) | the shop's own region |
| Rival-trainer TV show ([tv.c](src/tv.c)) | the region the show is recorded in |
| Debug badge toggles ([debug.c](src/debug.c)) | sets/clears both sets |
| Trainer card ([trainer_card.c](src/trainer_card.c)) | second badge row: 8 Hoenn badges on the first row, 8 Kanto badges on the second |

One approximation worth knowing: the Gen-9 catch malus walks
`sBadgeLevel[] = {25, 30, 35, 40, 45, 50, 55, 60, 100}`, which is Hoenn's badge-to-level
ladder, and it is now applied to a count that may have come from Kanto. Kanto's badges are
earned in a different order against different levels, so a second ladder would be a
balance change rather than a correctness fix; it is left as-is on purpose.

## 8. The journey to KANTO

Everything above builds the road. This is what sends the player down it: an invitation read
in the house, a pier OAK waits on at the far end, and the gate both ends of the crossing now
share. It is also what closes the “Still open” design question.

### The invitation rides in the S.S. TICKET scene

`PlayersHouse_1F_EventScript_GetSSTicketAndSeeLatiTV`
([players_house.inc](data/scripts/players_house.inc)) already runs exactly once per save: the
LEAGUE sets `VAR_LITTLEROOT_HOUSES_STATE_MAY`/`_BRENDAN` to 3
([hall_of_fame.inc](data/scripts/hall_of_fame.inc)), the `ON_FRAME` table of both houses fires
it on the way in, and the ticket itself is behind `call_if_unset FLAG_RECEIVED_SS_TICKET`. So
the letter goes into the same scene and the same envelope — MR. BRINEY's TICKET arrives, DAD
reads out what came with it, and the one thing the player needs to know is said out loud:
**the KANTO liner sails from LILYCOVE**. Nothing new is triggered, nothing new is gated, a
player who already holds the ticket is not shown it twice, and VERMILION CITY is named here
before they ever see it.

### Two flags, converted rather than added

| Flag | Emerald | FireRed | What it does |
| --- | --- | --- | --- |
| `FLAG_HIDE_VERMILION_PROF_OAK` | `0x493` (was `FLAG_UNUSED_0x493`) | `0x4A7` (was `FLAG_UNUSED_0x4A7`) | keeps OAK off the pier until the player is Champion |
| `FLAG_RECEIVED_KANTO_STARTER` | `0x494` (was `FLAG_UNUSED_0x494`) | `0x4A8` (was `FLAG_UNUSED_0x4A8`) | books the one-off gift |

Section 3's rule applies: an id is taken from a slot whose own `#define` was its only
reference anywhere in the tree, not from a fresh number, so nothing can shift. Both names are
defined in **both** headers so a Kanto map script never has to know which ROM it is compiled
into — in a FireRed build the received flag is merely unused, and each of the four numeric ids
was checked to appear exactly once in its own list.

### OAK's tile was measured, the same way the landing tiles were

From the blockdata behind `LAYOUT_VERMILION_CITY` (section 5's method): **`(23, 31)`** is
metatile `0x1C6`, collision 0, elevation 3 — the dock the SEAGALLOP's own `(23, 32)` arrival
sits one tile south of, with no warp, coord event or object event on it. OAK stands there
facing down the pier, so a Champion steps off the ship and into him; the pier is three tiles
wide at that point (`(22,31)`, `(24,31)` are both passable), so he blocks nobody's way.

### One flag, read at both ends

`FLAG_SYS_GAME_CLEAR` is what the rest of the post-game already uses, and it is now the whole
of the gate:

| Site | How it reads the flag |
| --- | --- |
| `VermilionCity_OnTransition` ([VermilionCity_Frlg](data/maps/VermilionCity_Frlg/scripts.inc)) | `call_if_set` → clear the hide flag, `call_if_unset` → set it, so OAK appears and disappears with the game's clear state. A FireRed build takes the other branch and sets the hide flag on every load, because the object event lives in the shared `map.json` and a FireRed build has to *park* him rather than not have him |
| `LilycoveCity_Harbor_EventScript_KantoSailor` ([LilycoveCity_Harbor](data/maps/LilycoveCity_Harbor/scripts.inc)) | `goto_if_unset` → a refusal that names the CHAMPION rule, the PROFESSOR waiting on the far side and the LEAGUE, so the closed door is discoverable from the pier |
| Lilycove's S.S. TIDAL attendant | already gated (vanilla harbour behaviour), so the whole building now opens together |

The label `VermilionCity_EventScript_ProfOak` exists in **both** builds — the object table
names it unconditionally — so the FireRed half of it is an inert `release`/`end`, and every
HOENN-side piece of the event (`ShowProfOak`, `HideProfOak`, the three starter branches, its
seven texts) sits inside `#if !IS_FRLG`. The KANTO-side return berth is deliberately left
ungated: a save that reaches VERMILION before the LEAGUE — or one made before this event
existed — can always sail home, so no save can be stranded by the gate.

### The gift

The three starters are a menu here (`MULTI_VERMILION_KANTO_STARTER`, id **164**, the last
member of the multichoice enum, backed by `sMultichoiceList_KantoStarter` in
[src/data/script_menu.h](src/data/script_menu.h)), because a pier cannot do what the FRLG lab
scene does — put a ball on the floor for each of them. `givemon` takes its species as an
immediate, so the menu's three cases reach three near-identical branches; everything after the
`givemon` is shared, exactly as the JOHTO starter gift in BIRCH's lab shares it: the fanfare,
the `{STR_VAR_1}` name in `STR_VAR_1` and `VAR_TEMP_TRANSFERRED_SPECIES`, the nickname prompt,
and the party-or-PC handoff. `FLAG_RECEIVED_KANTO_STARTER` is set only on the path that
actually delivers the POKéMON, so a full party **and** a full PC falls into
`Common_EventScript_NoMoreRoomForPokemon` and leaves OAK's offer standing for the next visit,
while a taken one gets a different line on every later conversation.

## Verification summary

The build's own preprocessing tools were built and run on this machine
(`tools/preproc`, `tools/mapjson`, `tools/jsonproc` — all gitignored binaries), so the
script pipeline could be replayed for real. **No ARM assembler or linker was run**: there is
no `devkitARM` and no `make` here, so nothing below covers assembly, linking, or the ROM.

- [audit_script_symbols.py](migration_scripts/audit_script_symbols.py): the real
  `preproc -s | cpp | preproc -ie` pipeline with the assembler conditionals evaluated, both
  builds, working tree vs `HEAD` → **1,732 → 0** map-event script labels without a live
  definition in Emerald, **0** newly unresolved names, and an unchanged FireRed symbol
  table (section 2).
- The three generated headers the pipeline needs
  (`include/constants/{map_event_ids,heal_locations,script_commands}.h`) were produced with
  those tools rather than stubbed, which is how the tool audit in section 1 was confirmed to
  emit **742** `LOCALID_*` defines including the Kanto and Sevii maps and the new
  `LOCALID_LILYCOVE_HARBOR_KANTO_SAILOR 6`.
- `mapjson` rebuilt clean under `-Wall -Wextra`; real tool runs produced the before/after
  table in section 1.
- All **911** grouped maps (**915** dirs, four of them ungrouped by design and dropped from
  the ROM): no duplicate ids, no missing layout id, **0 dangling layout references**; FireRed
  regression run still excludes Hoenn and produces 34 NULL groups.
- [audit_map_tree_consistency.py](migration_scripts/audit_map_tree_consistency.py): **0**
  findings — group list against map dirs, every map's layout against `layouts.json` and its
  blockdata/border on disk, `event_scripts.s`'s map includes against the files behind them,
  and the 36 retired HOENN island names against the whole tree (section 5). The general "does
  every `MAP_*` name resolve" sweep is in the same script behind `--names` and is **not** a
  gate: those prefixes also carry names that are not map names (the region map's
  `MAP_INPUT_*`/`MAP_WIDTH`, Makefile variables, ids an assembler file defines for itself).
  **It is now a build step**: the ELF takes `make map-tree-check` as an order-only
  prerequisite, so a finding stops the ROM before it is linked and a passing check never
  forces a relink, and `build.yml` runs the same target as its own job (in the `build` gate
  job's `needs:`) so the failure is quick and names the map. Four debris cases were checked by
  breaking each on purpose: a group entry with no map dir, a map dir in no group, an include
  with no file, and a `scripts.inc` no include names — each exits 1 with the file named. The
  check takes ~3.5 s and is stamped on the map and layout *directories* as well as the files,
  because a just-deleted map dir is exactly what a list of existing files cannot see.
- `map_groups.h` byte-identical to the unpatched Emerald build → no Hoenn id moved.
- [audit_kanto_script_labels.py](migration_scripts/audit_kanto_script_labels.py):
  1731 → **0** Kanto event-script labels unresolved (file-level count; the assembler-level
  measurement in section 2 is the one to quote).
- [audit_duplicate_labels.py](migration_scripts/audit_duplicate_labels.py):
  0 labels defined in more than one of the 1025 included files; the only same-file repeat is
  a pre-existing assembler `.if/.else` variant.
- [audit_event_scripts_assembly.py](migration_scripts/audit_event_scripts_assembly.py):
  whole-tree preprocessor run in both modes → 0 real duplicate labels,
  `VermilionCity_EventScript_FerrySailor` defined exactly once in each.
- [audit_frlg_block_ungating.py](migration_scripts/audit_frlg_block_ungating.py): 442 gated
  includes, 0 collisions with the unconditional tree.
- Flag id uniqueness/range and variable id uniqueness/range checks (sections 3 and 4).
- `relocate_kanto_defines.py --check` reports "nothing to do" → idempotent.
- New Lilycove object event regenerated correctly by the tool
  (`object_event 6, OBJ_EVENT_GFX_SAILOR, 4, 13, …`), and the tool's only side effect
  (`src/data/heal_locations.json`) was left unmodified.

### Added with the badges and the SEVII excursion

- **The C build's real pipeline, not a bare compiler.** Each file was run the way the
  Makefile runs it — `cpp … | preproc -i -g graphics <src> charmap.txt | clang
  -fsyntax-only` — so `_()`, `COMPOUND_STRING` and `INCGFX_*` are expanded as they are in
  a build. **Ten of the eleven** touched C files report 0 errors, `src/data/script_menu.h`
  (the multichoice list, re-checked after it was renamed `sMultichoiceList_SailDest`, exit
  status 0) among them. That header was re-run for each pass that added a list to it — the
  NAVEL ROCK / BIRTH ISLAND menu, then the three LILYCOVE ticket menus — and the last run
  reports `cpp` 0, `preproc -i` 0 and `clang -fsyntax-only` **exit 0 with 0 errors** under
  both `-DEMERALD` and `-DFIRERED`, the only diagnostic being the pre-existing
  pointer-to-int cast at `src/script_menu.c:422`. Five lists in one header is also where
  `[MULTI_*]` indices could silently skew, and they cannot here: the id→table mapping needs
  no arithmetic to trust, because both the script constant and the `[MULTI_*]` initialiser
  come from the same enum, so the numeric value the script emits indexes the entry *named*
  for it. `MULTI_SAIL_DEST` is 159 and the last of its four siblings 163, so all five fit
  the `u8` multichoice id and the highest member is the highest table index — no trailing
  hole. Three caveats, none of them these changes':
  the `STATIC_ASSERT`s in `include/move.h` compare ARM field sizes and cannot hold on a
  64-bit host; `src/debug.c` **did not compile** because it includes the build-generated
  `data/debug_trainers.h`, which no build has produced here; and one pre-existing
  pointer-to-int-cast warning sits at `src/script_menu.c:422`. For `src/debug.c` the
  changed lines were cleared a different way — every diagnostic in a raw compiler run of
  that file lands at lines 445–463 or in `include/move.h`, i.e. none inside the two edited
  ranges (1570–1590, 3030–3060).
- `audit_script_symbols.py`, Emerald working tree: **34,931** definitions, 1,349 referenced
  constants, **235** unresolved — the *same set* as its baseline's 235 — and **0** map-event
  script labels without a live definition (1,732 in the baseline). Delta: newly unresolved
  constants **0**. Every `MULTI_SAIL_DEST*` id and every `SEAGALLOP_*` id resolves. The count
  is exactly accounted for across the three passes: four crossing labels
  (`EventScript_SailToHoenn`, `EventScript_SailToKanto`, `EventScript_SeviiSailDest`,
  `EventScript_SeviiSailDestIslands`), two with the event-island piers
  (`EventScript_NavelBirthSailDest` and one enumerator), and **14** here — six labels and
  strings in the Lilycove harbour (three menu entry points, two sail labels, one prompt),
  five in `seagallop.inc` (the LILYCOVE dispatcher, two departure labels, two strings) and
  the three new `MULTI_SAIL_DEST_*` enumerators. One label was retired
  (`VermilionCity_EventScript_SailToLilycove`) in the first pass. The ticket hand-off and the
  harbour hint added **5** more, also exactly accounted for: three subroutines and one string
  in the VERMILION script, one string in LILYCOVE's. The referenced-constant count rose by
  **2** with them, `FLAG_RECEIVED_MYSTIC_TICKET` and `FLAG_RECEIVED_AURORA_TICKET` — new to
  `event_scripts.s` because `data/mystery_gift.s`, where they were already used, is a separate
  assembly unit.
  Note when comparing runs: the audit's baseline column is `HEAD:data/event_scripts.s` plus
  the *working tree's* other files, so it legitimately drifts by a label when an
  unconditional file changes. The assertions worth quoting are the two delta lines and the
  “0 uncovered labels”, not the absolute baseline.
- `audit_script_symbols.py --version frlg`: **34,897** definitions in *both* the working
  tree and `HEAD`, and an identical constant set. That equality is the measurement that
  matters for the guards: nine harbour sailor scripts (the seven SEVII piers, NAVEL ROCK and
  BIRTH ISLAND), the Vermilion pier script, the Lilycove sailor and `seagallop.inc` all
  changed across these passes, and every label added inside a `#if !IS_FRLG` block is gone
  from a FireRed build's live script text.
  **Read that number with its caveat:** the audit's `HEAD` column is
  `HEAD:data/event_scripts.s` **plus this working tree's other files**, so the two columns
  share `data/maps/*/scripts.inc` and an unguarded map-script addition cancels out. It
  proves the `event_scripts.s` side stayed put and that the guarded labels are absent; it does
  *not* prove a FireRed build gains no symbol. The unguarded HOENN-map additions — the
  Lilycove KANTO sailor and its two strings — **are** in the FireRed text, because
  `data/maps/LilycoveCity_Harbor/scripts.inc` is assembled in both builds like every other
  Hoenn map in this tree. They are unreachable rather than absent: gate 1 of section 1 drops
  that map from an FRLG ROM, and with it the object event that names the sailor. The
  text-level read below is the check that actually distinguishes the two.
- **`src/seagallop.c` through the same path, in all three versions:** the HOENN
  destination row, direction bit and HI-SPEED number compile with **0 errors** under
  `-DEMERALD`, `-DFIRERED` and `-DLEAFGREEN`. That matters because those three additions
  are deliberately *not* version-guarded: they rely on `MAP_LILYCOVE_CITY_HARBOR` existing
  in every build's generated map constants, which `generate_map_constants_text` guarantees
  by emitting an id for every map in `group_order` regardless of version (only the groups
  and headers are NULLed for a FireRed build).
- **That pipeline was checked stage by stage, not just grepped for errors**, because a
  `pipefail`ed run of it exits **1** for a reason that has nothing to do with the code:
  `cpp` exits 0, then `preproc` exits 1 because it cannot open the build-generated
  `graphics/seagallop/*.4bpp` intermediates (a real build makes those with gbagfx), and
  `clang -fsyntax-only` on the result exits **0 with 0 errors**. So the non-zero pipeline
  status is the missing asset intermediate only.
- **`goto`/`case` targets resolved against the assembled text, which the audit above does
  not do.** `audit_script_symbols.py` checks constants and the labels *map event data* names;
  it does not follow a script's own `goto`. So the same preprocessed text was read directly
  for the twelve files this feature touches (the seven SEVII piers, VERMILION, LILYCOVE, the
  two event-island piers and `seagallop.inc`), and then for **every** `goto`,
  `goto_if_*`, `call`, `call_if_*`, `case` and `vgoto` line in the live text of both builds —
  **11,000 branch instructions in the Emerald text** (10,947 in the FireRed one). Every
  target resolves *except* ten names, and those ten are macro **parameters** (`\taughtJmp`,
  `\declineTradeJump`, `\wrongMonJump`, `\declinedJmp`, `f`, `dest` …): they sit inside
  `.macro` bodies, where `as` substitutes them on expansion, and the identical ten (17
  occurrences) appear in the FireRed text, so they are unchanged noise rather than a failure
  this change could have introduced. All **twenty** labels the three passes added (8 + 1 +
  11) have exactly one definition in the Emerald text and **zero** in the FireRed one
  (`EventScript_SeviiSailDest` 7 references, once per
  harbour; `EventScript_NavelBirthSailDest` 2, once per event-island pier;
  `EventScript_DepartingFromLilycove` reached from `EventScript_SailToDest`), and the
  dispatch it sits in assembles as
  `dest == 11 → DepartingVermilionToHoenn`, `origin == 11 → DepartingFromLilycove`,
  `origin == 0 → DepartingVermilion` — the Lilycove branch ahead of the Vermilion one, which
  is what stops a NAVEL ROCK trip being announced as a VERMILION one.
  The same read on the FireRed text is the guard proof at text level rather than symbol
  level: the seven sailors fall back to their original entries (5 × `ChooseDestFromIsland`,
  1 × `FromOneIsland`, 1 × `FromTwoIsland`), NAVEL ROCK and BIRTH ISLAND keep their
  `multichoice 20, 8, MULTI_SEAGALLOP_VERMILION` (2 lines in the FireRed text, 0 in Emerald,
  where `MULTI_SAIL_DEST_VERMILION` is used instead), the Lilycove sailor keeps its bare
  Yes/No prompt with no ticket dispatch, all **11** new Lilycove and `seagallop.inc` labels
  report **defs 0, refs 0**, and no reference to any HOENN-route symbol survives.
- [audit_harbour_routes.py](migration_scripts/audit_harbour_routes.py): the new route check
  (section 5). Emerald: **12 harbours, 18 berth-selling scripts, 12 `sSeag` rows**, every pier
  with a berth in its own region, **0** duplicate islands. FireRed: **17** berth-selling
  scripts against Emerald's 18, because LILYCOVE's KANTO sailor loses its guarded `goto` and
  stops selling anything, and **0** SEAGALLOP berths to a HOENN destination against Emerald's
  10 — the guard holding, measured at route level rather than symbol level. `--strict` exits
  **0** in both builds now; it exited 1 while the retired HOENN sets existed, which is why the
  retired set was deleted rather than left as an expected finding.
- `audit_duplicate_labels.py`: **0** labels defined in more than one of the 1,025 files
  `event_scripts.s` includes; the only same-file repeat is the pre-existing assembler
  `.if/.else` variant of `MtChimney_EventScript_BagIsFull`. Against `HEAD` the Vermilion
  harbour script gains **6** labels and **5** text labels and loses none, and none of them
  collides; the NAVEL ROCK and BIRTH ISLAND scripts add no label of their own — they reuse
  the sailor label their `map.json` already names and the one shared crossing label.

### Added with the KANTO journey

- `mapjson` was re-run the way [map_data_rules.mk](map_data_rules.mk) runs it (`map`,
  `groups`, `event_constants`, `layouts`) → exit 0 throughout, `LOCALID_VERMILION_PROF_OAK 9`
  generated from the object event's position in the list, and the object itself generated as
  `object_event 9, OBJ_EVENT_GFX_PROF_OAK, 23, 31, 3, MOVEMENT_TYPE_FACE_DOWN, 1, 1,
  TRAINER_TYPE_NONE, 0, VermilionCity_EventScript_ProfOak, FLAG_HIDE_VERMILION_PROF_OAK`
  (all of these are gitignored build outputs).
- The whole script pipeline (`preproc -s | cpp | preproc -ie`) was replayed over
  `data/event_scripts.s` under **both** `-DEMERALD` and `-DFIRERED`: three stages, exit 0 each
  time. In the Emerald text the new labels are all present and defined once; in the FireRed
  text the same label is present with its inert body, **0** occurrences of
  `ProfOakStarterTaken`, `ProfOakBulbasaur` or Lilycove's `KantoSailorNoPassage` survive, and
  `VermilionCity_OnTransition` assembles down to `setflag 0x4A7` — the guard holding at text
  level, not symbol level. The new strings' non-ASCII characters (`…`, `“`, `”`, `é`) convert
  in the charmap pass (`0xAD`, `0xB1`, `0xB2`, `0x1B`), so the letter renders as written.
- The C pipeline (`cpp | preproc -i -g graphics | clang -fsyntax-only`) on
  [src/data/script_menu.h](src/data/script_menu.h) exits 0 under both `-DEMERALD` and
  `-DFIRERED`, the only diagnostic being the pre-existing pointer-to-int cast at
  `src/script_menu.c:422`. Because the table is indexed by the script's own `MULTI_*` constant,
  the enum and the `[MULTI_*]` initialisers were also compared directly: **165** members,
  **165** entries, no member without an entry, no entry naming an unknown id, no duplicate,
  and the new id is the last of both.
- Flag ids: each of the four new numeric ids appears exactly once in its list, and none of the
  four names is referenced anywhere else in the tree (the `FLAG_UNUSED_*` names it replaced
  are gone with them).
- The repo's audits, all green: [audit_script_symbols.py](migration_scripts/audit_script_symbols.py)
  (0 script labels without a live definition, both versions — its `HEAD`-baseline half cannot
  run at all, because the committed `event_scripts.s` still names the Hoenn
  `BirthIsland_Exterior` map this working tree deleted, which is pre-existing and unrelated),
  [audit_duplicate_labels.py](migration_scripts/audit_duplicate_labels.py) (0 real duplicates),
  [audit_kanto_script_labels.py](migration_scripts/audit_kanto_script_labels.py) (0 unresolved),
  [audit_event_scripts_assembly.py](migration_scripts/audit_event_scripts_assembly.py) (0),
  [audit_map_tree_consistency.py](migration_scripts/audit_map_tree_consistency.py) (0 findings),
  [audit_frlg_block_ungating.py](migration_scripts/audit_frlg_block_ungating.py) (0 collisions),
  [audit_harbour_routes.py](migration_scripts/audit_harbour_routes.py) (0 findings in both
  builds — the Emerald berth count is unchanged, because the KANTO sailor was already a berth
  seller and only gained a refusal in front of it).

## Limitations

- **No ROM was assembled or linked.** This machine has no `devkitARM` and no `make`
  (devkitPro is not installed and the WSL distros are stopped), so `as`, `ld` and `gcc` for
  ARM never ran. Everything above is static: it proves symbol *definitions* and *references*
  line up, that the map/layout data is consistent, and now also that the assembler-level
  conditionals resolve the way a build needs them to — which is where all four blockers were.
  It does **not** prove the ROM assembles, links, boots, or that the ferry changes maps
  correctly. Run `make clean && make -j$(nproc)` and then `make firered` / `make leafgreen`
  in your devkitPro environment.
- The symbol audit does not model macro *expansion*. Names that appear only as arguments to
  an assembler macro (`.byte SCR_OP_ADDCOINS`, `msgbox ..., MSGBOX_NPC`) stay "unresolved" in
  its report even though `as` accepts them; that fixed residual of 235 names is why the
  report is useful only as a *delta*. C symbols that scripts reach indirectly
  (`special`/`callnative` arguments) are lowercase and outside the UPPER_SNAKE filter, so
  whether a C function exists for every script `special` is still argued from the
  unconditional `gSpecials` table rather than measured.
- `sizeof(struct SaveBlock1) <= 4 × 3968` is enforced by a `STATIC_ASSERT` in
  [save.c](src/save.c). The change is arithmetically neutral (section 4), but that is
  arithmetic, not a measurement.
- Wild encounters, trainer flags and Kanto side-quests have not been play-tested.

## Still open

1. FireRed and LeafGreen should still be rebuilt. Their *script symbol table* is now
   measured to be identical (section 2), but the flags, variables and C-level headers are
   only argued to be unchanged, and only a build can confirm that.
2. Kanto/Sevii subregion Town Map pages are wired in `region_map.c` but untested in-game.
3. No crossing has been played. The routing, the flags it sets and the menu geometry are
   read from the shipping FRLG scripts, but nobody has yet watched a Champion sail from
   VERMILION to ONE ISLAND and back, which is the only way to confirm the pass variable
   really opens every return menu **and** that the HOENN legs' eastbound/westbound choice
   looks right on screen. The SEAGALLOP cut-scene itself was never exercised with a HOENN
   destination, so the first thing to watch is that `SetWarpDestination` lands on Lilycove
   Harbor `(8, 11)` and Vermilion `(23, 32)` rather than on an unexpected map. Three things
   are new since and are equally unobserved: the three ways into the HOENN liner (Vermilion's
   menu, Lilycove's sailor, and the seven SEVII harbours plus NAVEL ROCK and BIRTH ISLAND),
   the `VAR_0x8004` dispatch that sends an island sailor back into the right vanilla menu, the
   two-menus-in-a-row feel of the harbour flow, and the NAVEL ROCK / BIRTH ISLAND menu's new
   `(13, 6)` position, which is the one visual change here a static check cannot see. The
   LILYCOVE berths are in the same position: the three-way ticket dispatch, the menus behind
   it, the departure line and the `(8, 5)` arrival on a `_Frlg` island harbour from a LILYCOVE
   origin have been read, not seen. Both LILYCOVE legs take their screen direction from
   `sTravelDirectionMatrix[LILYCOVE]`, which is 0 — westbound, the same as the existing run to
   VERMILION — so nothing new is assumed there, but nobody has watched one. Four things added
   later are unobserved in the same way: the CHAMPION ticket hand-off (two messages, a
   `checkitemspace` branch and the flags it sets), the harbour hint a ticketless player gets at
   LILYCOVE, the four redirected S.S. TIDAL warps — in particular whether a `warp` into a
   `_Frlg` harbour lands as cleanly as the SEAGALLOP's own placement does, and whether a HOENN
   departure arriving in a KANTO-region map trips anything the region reader does not expect —
   and the return leg from those islands, which is the FRLG sailor's LILYCOVE berth and has
   never carried an S.S. TIDAL passenger. The same goes for the multi-digit badge counts on the
   trainer card and both save menus, which are laid out to the widths the single-digit
   versions used.
4. ~~Whether Kanto itself should be gated behind `FLAG_SYS_GAME_CLEAR` is still an open
   design question: the Lilycove crossing is deliberately ungated today, so a pre-league
   player can walk into Kanto, earn Kanto badges, and only then find the SEVII gate closed
   to them.~~ **Closed: both ends of the crossing are gated now** (section 8). The KANTO
   mainland is no longer reachable before the LEAGUE — the KANTO sailor at LILYCOVE refuses
   on the same flag, the invitation that names the route arrives with the S.S. TICKET after
   the LEAGUE, and the piers of the SEVII ISLES were already gated that way. The return berth
   is left ungated on purpose, so a save made before this change and standing in KANTO can
   still sail back to HOENN, and so can one that somehow arrives early.
5. ~~**The retired HOENN island set is dead content.**~~ **Closed: the 24 HOENN NAVEL ROCK /
   BIRTH ISLAND maps are deleted**, with their 10 layouts, 2 tilesets, two region-map sections
   and their `event_scripts.s` includes — "The HOENN island set is gone" in section 5 records
   what went and what the revert is. The judgement call was between deleting them and giving
   them distinguishing content of their own; deletion won because the two sets held the same
   three legendaries, and because a second dungeon for a legendary that is already unique is
   the duplication this work exists to remove.
6. ~~The two ticket hand-off paths still do not agree on the flags.~~ **Closed: the debug menu
   sets them.** The Mystery Gift scripts
   ([gift_mystic_ticket.inc](data/scripts/gift_mystic_ticket.inc),
   [gift_aurora_ticket.inc](data/scripts/gift_aurora_ticket.inc)) set the flag with the item,
   the CHAMPION hand-off does too, and so does [src/debug.c](src/debug.c) now: one table pairs
   each event ticket with its `FLAG_ENABLE_SHIP_*` (EON/SOUTHERN ISLAND, MYSTIC/NAVEL ROCK,
   AURORA/BIRTH ISLAND, OLD SEA MAP/FARAWAY ISLAND), and both “Give item XYZ…” and “Fill Pocket
   Key Items” run it, so a cheated-in ticket opens its berth. Islands that belong to the other
   region are flag 0 in this build and their table rows are skipped rather than written to flag
   0, the same split `sLocationFlags` handles.
7. Two further badge consumers were found and left alone because both are switched off by
   the shipped configuration, so changing them would be unverifiable churn:
   `GetCurrentLevelCap` ([caps.c](src/caps.c)) keys its ladder on Hoenn's badge flags and
   is inert while `B_LEVEL_CAP_TYPE` is `LEVEL_CAP_NONE`; the Gen-3 badge boosts
   (`B_FLAG_BADGE_BOOST_*` in [battle.h](include/config/battle.h)) name Hoenn flags and are
   inert while `B_BADGE_BOOST` is `GEN_LATEST`. Whoever turns either feature on has to
   make it region-aware first, or Kanto badges will not count.
