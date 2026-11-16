# scout: a location dossier for any named place on vanilla Everon

`tools\cli scout <place>` is the first tool of the town pipeline (survey, map, kits, validate). It answers "what is
actually there?" for a place on the vanilla map, so a plan is cut from facts: which buildings stand there and how big
they are, where the roads and junctions run, what is free to build on, how the ground lies, and what it looks like.

```
tools\cli scout "Saint Philippe"            docs/everon/places/saint-philippe/{dossier.md, dossier.json, img/}
tools\cli scout 4502,10771 --radius 90      a place by coordinates (slug xz-4502-10771)
tools\cli scout list [filter]               the map's named places and where they are
tools\cli scout types                       the building and prop type names a dossier uses
tools\cli scout "Gorey" --quick             dump and overview pictures only (about 3 minutes)
tools\cli scout "Gorey" --from-dump f.txt   analyse a dump already taken; no Workbench
```

Flags: `--radius m` (the survey radius, default 120), `--view m` (the overview camera's reach, 60), `--civic n` (how
many civic buildings get pictures, 6), `--interiors n` (how many get an inside look, 2), `--slug`, `--out dir`,
`--machine`, `--wait`. It runs in a worktree like `look`; the Workbench part goes to a pool machine.

## What a dossier holds

| Section | From |
|---|---|
| Place: name, kind, coordinates | the map's own label prefabs, `Prefabs/World/Locations/Eden/**` (Eden is the engine's name for Everon), names from the game's string table |
| Buildings: `{GUID}path`, type, position, yaw, footprint, interior, entrances | the engine's entities through the `dump` directive of the look plugin; footprint from `tools\cli measure` (the model's bounds turned by the yaw), the engine's world bounds when the model does not cover the building |
| Roads and junctions, the main street | road splines from the same dump; crossings and ends within 3 m of another road are junctions (merged within 8 m), the main street is the road with the most buildings within 25 m |
| Notable props: piers and docks, fuel pumps, benches, bus shelters, monuments ... | the dump, classified by prefab path (`tools/scout/types.mjs`) |
| Free plots | open rectangles of at least 10 m on the short side, 2.5 m off buildings, 4.5 m off a road's centre line, 2 m off trees, on ground under 12 degrees, not in the sea |
| Terrain | ground heights on a 5 m grid: relief, how much is flat, the overall fall of the ground, steepest point, nearest sea |
| Pictures | `look` on `CoreOnEveron`: overhead, wide and three obliques; the main street; each civic candidate from the front and the east; `look --interior` inside the first few that have a door |

`dossier.json` is the same data for tools (schema 1); `dossier.md` is rendered from it and carries every picture.

## How it reads the world

Vanilla Everon's world file (`worlds/Eden/Eden.ent`) is a binary the repo tools do not decode, and a guessed decoder
would be wrong in ways nothing checks. So the engine is the reader: the look plugin
(`tools/visual/wb/LF_VisualShotsPlugin.c`, directive `dump <name> <x> <z> <radius> [step]`) asks the editor world for
every entity near the place and writes one text row each (`tools/scout/dump.mjs` lists the rows and parses them). The
pictures and the dump come from one hidden Workbench run, routed through the pool like any `look`
(`look <x,z[,r]> --world Worlds/CoreOnEveron/CoreOnEveron.ent --dump <radius>`).

## Interiors and entrances

A vanilla building is enterable through door entities (`BaseDoorComponent`) that hang below it, so the dump lists
them: each is an entrance with the side of the building it stands on and the way it faces. `interior: yes` means at least
one door entity; `no` means none and a type that is not built to be entered (a shed), or a ruin; `unknown` means a
type that is built to be entered (a church, a pub) but no door entity was found: look at it. The inside pictures
are `look --interior x,y,z` from the middle of the footprint at the floor height; read them before relying on a room's size.

## Several at once

Each run writes only `docs/everon/places/<slug>/` and `tmp/scout/<slug>/`, and the Workbench runs queue on the pool,
so one agent per place can run side by side (`tools\cli scout A --quick &`, `scout B`, ...). There is no index file:
list the places with `ls docs/everon/places`.

## Building type names

`tools\cli scout types` prints the vocabulary: `church`, `chapel`, `municipal-office`, `school`, `fire-station`, `pub`,
`shop-food` ..., `hotel`, `fuel-station`, `house-village`, `house-town`, `farmhouse`, `villa`, `apartment-block`,
`shed`, `garage`, `farm-building`, `industrial-building`, `ruin` and more. They are read off the vanilla prefab path
(`Prefabs/Structures/<Category>/<Sub>/<Name>.et`), so they are stable across game builds. A catalogue of vanilla
buildings should use the same names so a dossier and a catalogue never name one thing twice.

## Limits

- Roads come from the splines the engine exposes; if a game build hides them the dossier says so and the roads,
  junctions, main street and free plots are marked incomplete (the `RV` rows in the dump list what was readable).
- A footprint from `world-bounds` is axis aligned: a turned building reads larger than it is. The `source` column says which.
- The ground is the terrain, not the buildings: an interior floor height is a guess (ground plus 0.3 m) until the picture confirms it.
- Free plots are axis-aligned rectangles, not the best use of the land.
