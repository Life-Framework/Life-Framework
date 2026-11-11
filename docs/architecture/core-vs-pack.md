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

## The four tests

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
  `validate-annex-fixtures` stops a new test naming a village entity (baseline in
  `tools/validation/annex-fixtures-baseline.json`).
  The street around them moves to the pack.

A resource that moves keeps its `.meta`, so its GUID stays the same and every
`{GUID}path` reference still resolves. A new pack resource gets its `.meta`
from Workbench with the pack project loaded, never by hand.
