# North outpost site: Schooner's End camp

The site card for the north rebel outpost (#4218, plan `docs/roadmap/next-release-0.3.0.md`, decision N29). The machine
data is the `outpost` block of [dossier.json](dossier.json); the scout's own pages are [dossier.md](dossier.md).

![from above, north up](img/overview-top.jpg)

## Why here

- **4.01 km from Saint-Philippe** (centre 7728, 8368; Saint-Philippe 4480, 10715), east-north-east, past the minimum of 4 km (the compound's nearest corner, 7698, 8398, is 3.97 km).
- **Off the roads.** The survey has no road inside 120 m. The nearest mapped road is the training area's yard road r02 about
  240 m south; the main road r01 is about 285 m south-west. A car reaches the gate over the pine floor along the free strip
  p2 (x 7688, z 8237 to 8355).
- **A clearing the camp can fill.** The meadow is about 70 m by 70 m between pines (west, south) and a pebble beach (east),
  ground 3.6 to 17 m with the plots at 14 to 17 m on the plateau. The overall fall is 6.7 degrees toward the sea.
  The cove gives the smugglers' landing a reason to exist, and the pines hide the camp from the road.
- **Not the scope file's spot.** The gangs scope names "about 7000 to 8000, 11000" east of the airbase. That ground is sea
  (Maiden's Bay); the north-east bulge near the old training area is the only land of the north half at least 4 km out.

## Layout (first values; the dressing card seats them)

| Part | x, z | Note |
|---|---|---|
| Camp bounds | 7698 to 7758, 8338 to 8398 | the meadow floor |
| Safe zone | centre 7728, 8368, radius 60 m | the `LF_Outposts.conf` default; the east third is beach and sea |
| Gate | 7700, 8340, facing S | guard posts at 7694, 8336 and 7706, 8336; the approach is the p2 strip |
| Quartermaster ("Sarge") | 7738, 8372, faces W | table and crate under canvas, 40 m in from the gate |
| Chalk board | 7704, 8346 | beside the gate, read before the quartermaster |
| Contraband buyer | 7742, 8352 | |
| Mechanic canopy | 7716, 8392 | |
| Campfire | 7724, 8364 | |
| Radio mast | 7712, 8394 | |
| Flag pole | 7752, 8380 | |

The centre, the plots, the terrain, the water and the picture are measured by the scout. The part spots are read off the
overhead picture and the free plots; nothing here is a placement. Cards O9 and the town pass seat them with `town fit
--terrain` and `audit placement`.

## Dressing kit (#4458, #4555)

The camp's 1989 smuggler look is the kit `tools/town/kits/outpost-north.json` (29 rows, vanilla assets only, open ground:
every row `"mount": "terrain"`), authored in the debug grid and archived as `tools/town/kits/sources/outpost-north.layer`.
It holds the sandbag gate (4 m opening), the FIA tent for the quartermaster, a canvas cover over the crate spot, a camo net
shrunk to the two pallets, the fire with three stools and a crate bench, the mechanic canopy over two workbenches, the generator,
the R161 mast, the table, the barrel, and the flag pole with the vanilla green cloth (`Flag_1_2` with `Flag_1_2_Green.emat`,
the vanilla green flag prefab's GUID is not in the game's reference index). Six prefab-less rows (`outpost-spot-crate`,
`-chalk-board`, `-buyer`, `-quartermaster`, `-guard-s`, `-guard-n`) mark where cards #4223, GA-O3 and #4519 put their
interactables and NPCs. `town fit --terrain` seats it on the site; `town author --reopen outpost-north` rebuilds the grid bay.
