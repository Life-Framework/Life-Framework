# Town kits

A kit is a finished fit-out lifted from the playtest town (or a grid cell): prefab rows with a role and a place-tool
verb, in the building's own frame, with no world coordinates. `town fit` (T3) applies one to a building from the
dossier. Design: `docs/architecture/everon-world.md` "Fit-out: kits".

```
node tools/town/kit.mjs extract <source> <group> --kit <name> [--system s] [--origin spec] [--exclude globs]
                                [--building layer::Name --with layer::globs] [--drop id,id] [--role id=role] [--verb id=verb]
node tools/town/kit.mjs check [kit.json ...]
node tools/town/kit.mjs apply <kit.json> <x> <z> <yawDeg> [floorY]
```

`extract-first-kits.sh` rewrites the committed kits. `tools\cli validate` runs `town-kit-selftest`, which checks
them. Kits are pack data: they name pack and vanilla prefabs, never core-only coordinates.

## Format (schema 1)

| Field | Meaning |
|---|---|
| `scope` | `counter` (a till and its counter), `room` (a layer group around an anchor entity), `building` (a whole-building fit-out: more than one room or floor, rows spread through the footprint) |
| `source` | `file` it was read from, `also` (parent prefabs and layers it read), `origin` (`prefab` = the prefab's own origin, or the anchor entity), `frame` (`building-local` or `origin-entity`) |
| `building` | `mode: override` (the kit is a pack prefab that overrides a vanilla building: place `store.prefab` at the building) with `via` (the prefab it inherits) and `vanilla` (the base it ends on), or `mode: inside` (rows go into a vanilla building as they are; `--inside {GUID}path` names it) |
| `store` | Prefab kits only: the `{GUID}path` of the store prefab and its component config (see "Config is referenced") |
| `anchoring` | How the kit sits: `floorY` (interior floor above the origin), `floors` (`f0`, `f1` ... with `y`), `footprint` (min/max x, z in the kit frame; the store's own boundary when it has one), `front` (the street side, from the sign), `counters` (which wall each counter hugs and the gap) and `adapt` (how to fit another building shape) |
| `rows[]` | `id`, `prefab` (`{GUID}path`), `role`, `verb`, `offset` [x, y, z] in the kit frame, `yaw`, optional `pitch`, `roll`, `scale`, `floor` (`f0`... or `all`), `wall` (`-x +x -z +z` or `free`), `class`, `from` (which source file it came from), `config`, `children` |

Roles: `counter till npc board dressing sign light alarm interactable furniture`. Verbs are the place tool's:
`on floor`, `against wall`, `on <row id>`. Offsets are rotated into the building frame (yaw taken out), so a kit
turns with the building it is fitted to. Layer kits are rebased so the lowest row is y 0; prefab kits keep the
building origin as y 0 and say where the interior floor is in `anchoring.floorY`.

### Config is referenced, not copied

A row keeps the scalar properties it sets itself (`m_sOutletId`, `m_sStoreKey`, `m_vOffset`, `m_fReach`), because
they are the row's identity. Everything nested (stock lines, shop items, slots, action contexts) is named in
`config[].blocks` and stays in `source.file`. A fit applied in Saint-Philippe that places the store prefab
(`building.mode: override`) gets the stock, till and interactables from the prefab itself, so it behaves like the
playtest shop. A kit that places rows alone reads the named blocks from `source`.

## A whole-building kit (for the police station)

Write the building as one `.et` store-style prefab inside the vanilla police villa, then extract it:

```
node tools/town/kit.mjs extract <Police_Store.et> '*' --kit police-station --system police --scope building
```

Rows hold every room's props; `floor` says which storey (`f0` ground, `f1` upper, ...); `anchoring.footprint`
is the building's usable box; `building` records the vanilla villa it overrides. Per-room grouping can be
done with row id prefixes (`front-desk-*`, `cell-*`, `armoury-*`) and `--role`/`--verb` overrides. Anything the
format cannot say yet goes in a new optional field of `anchoring` (not a new top-level key), and
`checkKit` in `kit.mjs` gets the matching check.
