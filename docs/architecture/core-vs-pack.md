# Core vs pack

**The core decides what the game can do. The pack decides what this server is.**

Life Framework ships as two addons in one repo (decision 2026-11-08, "The
framework ships as two addons in one repo"; `docs/roadmap/m3-outline.md`
section 1). The pack depends on the core. The core never references the pack.
Every new resource is classified core or pack with this rule before it is
committed. The current tree is classified in
[`core-pack-inventory.md`](core-pack-inventory.md).

| Addon | Folder | Project |
| --- | --- | --- |
| Core | `addons/LifeFramework/` | `LifeFramework.gproj`, GUID `79636E668EA37AC9` |
| Pack | `addons/LF_Everon1989/` | `LF_Everon1989.gproj`, GUID `B1E84D5687E11724`, depends on the core |

**Until the move.** The pack is a skeleton and nothing loads it: `build`,
`test`, `serve`, and `playtest` pass `-addons LifeFramework` only. A new
resource the rule calls pack still lands in the core for now, and gets a pack
row in the inventory so the M3 move takes it along.

**What a pack may rely on** is the declared public surface, [`public-surface.md`](public-surface.md); a pack also ships a manifest naming the core versions it supports.

## Core

The core gets anything that:

- has script logic or components;
- a server in a different setting (modern Chernarus, 1990s Arland) would need
  unchanged;
- a test needs;
- is a base or generic prefab that a system spawns or depends on (cash, ID
  card, generic sign, ATM, pickaxe, handcuffs), with a neutral look;
- is a neutral working default for a system, such as one example job, shop,
  and crime, so the core runs alone.

## Pack

The pack gets the choices:

- **Where:** placement, world layers, the village, the MainWorld town.
- **Who and what it's called:** place names, sign titles, NPC names, lore,
  flavour copy, dialogue lines.
- **How much:** prices, wages, fines, and timers, as `.conf` values.
- **What 1989 looks like:** period-dressed prefab variants, uniforms, the
  vehicle catalogue and liveries, period props.
- The flagship mission header and faction naming.

## Map independence

Core script never holds a world coordinate, a map name, an entity name or a
layer name (#961, decided with the maintainer 2026-11-14). A pack for Arland,
Chernarus or a custom map works by placing components, with no script change.

- A system finds its places by component. The component registers its owner in
  `LF_Places` (`Core/LF_Places.c`) under a kind and the stable id the feature
  saves; the system asks `LF_Places.Find(kind, id)`, `FindOrigin` or `FindAll`.
  Kinds in use: `BreachTarget` (`LF_BreachTargetComponent`, and every bank
  vault), `BankVault` (`LF_BankVaultComponent`, `LF_HeistVaultComponent`),
  `HospitalSite` (hospital till and cabinet), `HideoutDoor`
  (`LF_HideoutDoorComponent`, under the hideout id).
- An id defaults to the entity name only so placed layers keep their saved
  keys; nothing looks an entity up by that name.
- A place that is not on the map is a null lookup: the system logs and that
  one feature stays off. A system that cannot work without a place lists its
  kinds in its catalog entry (`m_aPlaces`, `LF_SystemCatalog.conf`); at game
  start it switches off with one line when none is placed (#1047). The jail
  cell registers as `JailCell`. `Worlds/CoreOnEveron` boots the core alone on
  vanilla Everon to prove it (`tools\cli test --tier fast --world ...`).
- Tests keep their annex fixtures and may look them up by name.


1. **Delete the pack.** The core still boots DebugWorld, every test passes,
   and nothing references the pack.
2. **The swap test.** Would a modern-setting server use it as is? If yes, it
   belongs in the core.
3. **Script in the pack is a smell.** Add an extension point to the core (a
   config field, an attribute, a prefab slot) instead.
4. **Override, never fork.** Pack values override core defaults through `.conf`
   or config. A pack never copies a system to change it.

`tools/validation/validate-addon-boundary.mjs` enforces test 1's "nothing
references the pack" and test 3: it fails when a core file names a pack GUID,
path, class, or the pack addon ID, and when the pack holds a `.c` file that is
not on its allowlist (empty today).

## Grey cases, decided

| Case | Core | Pack |
| --- | --- | --- |
| A new gameplay item (a bowling ball, a taser) | The item prefab and a neutral mesh, if a system depends on them | Purely decorative or period props |
| Vehicles | Ownership, garage, impound, and the base vehicle prefabs with LF components | The catalogue: which cars, their prices, liveries |
| Police | The system and the rank mechanics | Faction names, uniforms, rank titles |
| Strings | Functional UI and prompt keys | Flavour copy, in the pack's own string table |
| Terms (Police, town, currency) | The placeholders (`{police}`) and neutral default words | The pack's own words for a term, in its table, or the owner's (#958, [owner guide](../owner/rename-terms.md)) |
| DebugWorld | The test annex with neutral fixtures | The village |
| Voice, dialogue, animation | The systems and generic animations | Voice lines and story content |

## How a split works

Most files that fail the rule are not wrong, they are two things in one file.
Split them along these seams:

- **A prefab with a period look.** The core keeps a neutral base prefab with
  every component. The pack adds a child prefab that only overrides the mesh,
  clothing, or name. Systems and tests spawn the core base.
- **A `.conf` with flagship values.** The core keeps the file as the neutral
  default. The pack ships an override with the flagship numbers. Until the
  config layer exists (`m3-outline.md` section 2), every `LF_*Config` loads one
  hard-coded core path, so this split waits for it.
- **A string.** A key that names a function (`Read`, `Pay fine`) stays in the
  core table. A key that names a place or tells a story moves to the pack
  table, and the core gets a neutral default under its own key.
- **A world layer.** Fixtures a test looks up by name stay in the core annex.
  The street around them moves to the pack. `validate-annex-fixtures` stops a new
  test naming a village entity (baseline in
  `tools/validation/annex-fixtures-baseline.json`).

A resource that moves keeps its `.meta`, so its GUID stays the same and every
`{GUID}path` reference still resolves. A new pack resource gets its `.meta`
from Workbench with the pack project loaded, never by hand. The one exception
is `tools\cli asset`, which writes the `.meta` files of what it imports (a
headless Workbench cannot register a new `.fbx`) with GUIDs unique across both
addons, and imports a pack asset with `--addon LF_Everon1989`
([`asset-pipeline/README.md`](../asset-pipeline/README.md) "Target addon").

## Starting a pack

`tools\cli pack new <Name>` scaffolds `addons/LF_<Name>/` (#968): the `.gproj` on the base
game and the core, the folder tree, an empty string table with its language confs, a mission
header that names the manifest and lists one empty overlay, the manifest holding the running
core's range, and a README of what to fill in. The `.meta` GUIDs are random and checked
against every GUID in `addons/`, so none repeats. It refuses a name the core already uses (`Test`:
`LF_Test` is the test base class). `validate-pack-manifest` and `validate-addon-boundary` hold every
`LF_*` addon, so a fresh pack passes with no edits (`pack-new-selftest.mjs`).

## Checking a pack

`tools\cli pack check <addon>` (#969) reads a pack offline and reports each finding with the
pack file and the id: the version range and public surface, the boundary, a mission header that
names the manifest and lists overlays that resolve, overlay ids against the merged layers (an `add`
of an id that exists, a `patch`, `replace` or `remove` of one that does not, an unknown verb, an
offer for an item the catalogue lacks), a `{GUID}path` that names another path or a stale GUID,
a `#Key` no string table defines, and a second pack changing an id an earlier pack changed (a
warning). Base-game GUIDs cannot be resolved offline and are counted, not listed. Dialogue (#957),
terms (#958), map independence (#961) and profit loops over pack prices (#686) are not in it yet.
Exit 1 on any error. `pack explain` is its other half: `docs/config/owner-config.md` "Where a value came from".
The list sections both read are one table, `SECTIONS` in `tools/pack/layers.mjs`; a section moved onto the
override model adds its row there (`pack-check-selftest.mjs` fails until it does).
