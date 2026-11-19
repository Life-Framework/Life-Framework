# Black Lake: location dossier

Generated 2026-11-19 by `tools\cli scout`. Do not edit by hand; run `tools\cli scout "Black Lake"` to refresh. The machine-readable twin is `dossier.json`.

- **Place:** Black Lake (aquatic), from the map's own label
- **Centre:** x 9482.455, z 3047.666, ground 207.163 m; **radius** 120 m
- **World:** Worlds/CoreOnEveron/CoreOnEveron.ent, Arma Reforger build 26266485 (installed 2026-11-08)
- **Other labels nearby:** Mill Pond (aquatic, 675 m W)

## At a glance

0 buildings (0 homes, 0 civic, 0 ruins), 0 enterable (a door entity below them), 0 entrances from outside among 0 door entities; 3 road(s), 1 junction(s); 0 notable props; 0 trees, bushes and other generated vegetation; 8 free plot(s).

_none_

## Overview

![from above, north up](img/overview-top.jpg)

![wide, from the south](img/overview-wide.jpg)

![from the west](img/overview-ob270.jpg)

![from the north](img/overview-ob000.jpg)

![from the east](img/overview-ob090.jpg)

## Terrain

- ground 207.3 to 228.9 m (21.5 m of relief) within 120 m; mean 214.3 m
- 4% of the ground is under 3 degrees, 28% 3 to 8, 17% over 15
- overall the ground falls 1.4 degrees toward the S
- steepest ground 32.1 degrees at 9372.5, 3092.7
- open water (the sea or a lake) is 0 m away to the N (21% of the circle is water)

## Water

| Id | Name | Kind | Centre x | Centre z | Area (m2) | Surface y (m) | From centre |
| --- | --- | --- | --- | --- | --- | --- | --- |
| w1 | Lake_Durras | lake | 9478.9 | 3023.1 | 9351 | 207.4 | 24.8 m |

Shore points (dry ground a step from the outline, facing the water):

| Water | x | z | Ground y (m) | Faces | Yaw | From centre |
| --- | --- | --- | --- | --- | --- | --- |
| w1 | 9517.9 | 3042 | 208.2 | SW | 244 | 35.9 m |
| w1 | 9512.1 | 3077.4 | 208 | SW | 211 | 41.9 m |
| w1 | 9463.3 | 3085.2 | 207.9 | S | 166 | 42.1 m |
| w1 | 9434.7 | 3049.8 | 211.2 | SE | 121 | 47.9 m |
| w1 | 9519 | 2997 | 208.5 | NW | 303 | 62.5 m |
| w1 | 9457.6 | 2971.3 | 208.4 | N | 22 | 80.3 m |

lakes and ponds are the closed outlines of the dump's water shapes (the sea probe cannot see them); the grid, the terrain share and the free plots count the cells inside them as water. Shore points are dry ground 2.5 m outside an outline, not checked against trees or props

## Roads and junctions

**Main street:** NW-SE road r04, 0 buildings within 25 m of it; its busiest stretch is at x 9441.6, z 2965.4.

| Id | Road | Length in radius (m) | Points |
| --- | --- | --- | --- |
| r01 | NW-SE road r01 | 138.8 | 7 |
| r04 | NW-SE road r04 | 162.8 | 7 |
| r05 | NE-SW road r05 | 20.5 | 2 |

| x | z | Kind | Arms | Roads |
| --- | --- | --- | --- | --- |
| 9399.7 | 2979 | t-junction | 3 | r04, r01 |

## Civic candidates

Public-facing buildings a town plan would use (church, chapel, government, police, fire, pub, shop, fuel station, hotel, recreation), nearest first. Each links its pictures.

_none_

## All buildings

_none_

Footprint: `catalog` and `measure` are the model's own extents turned by the yaw; `world-bounds` is the engine's axis-aligned box. Yaw is Enfusion's (0 faces +z north, 90 faces +x east). Interior `yes` means door entities and a walkable floor (a heuristic, not walked); `unknown` means door entities but no walkable floor. Entrances are the doors on the outline; `?` means the building was not found in the engine dump. Furniture is the count of furnishing props inside the footprint where the place file has it.

## Notable props

_none_

## Free plots

| Id | x | z | Size (m) | Area (m2) | Ground (m) | Nearest road | Nearest building | From centre |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| p1 | 9552.5 | 3075.7 | 64 x 40 | 2560 | 208.3 to 216.8 | NW-SE road r04 144.4 m | - | 75.4 m |
| p2 | 9527.5 | 3125.7 | 46 x 40 | 1840 | 210.4 to 217.5 | NW-SE road r04 181.8 m | - | 90 m |
| p3 | 9402.5 | 3069.7 | 36 x 40 | 1440 | 216.5 to 220.1 | NW-SE road r01 54.2 m | - | 83 m |
| p4 | 9527.5 | 2989.7 | 14 x 80 | 1120 | 208.8 to 212.4 | NW-SE road r04 54.9 m | - | 73.4 m |
| p5 | 9427.5 | 3105.7 | 46 x 20 | 920 | 212.5 to 219.7 | NW-SE road r01 97.2 m | - | 79.9 m |
| p6 | 9485.5 | 3110.7 | 30 x 30 | 900 | 208.1 to 211.9 | NW-SE road r01 145.5 m | - | 63.1 m |
| p7 | 9387.5 | 3036.7 | 26 x 22 | 572 | 213.3 to 217.4 | NW-SE road r01 22.7 m | - | 95.6 m |
| p8 | 9450.5 | 2942.7 | 28 x 14 | 392 | 209.1 to 210.6 | NW-SE road r04 16.3 m | - | 109.8 m |

plots are open rectangles at least 10 m on the short side, kept 2.5 m off buildings, 4.5 m off a road's centre line, 2 m off a tree, on ground under 12 degrees; axis aligned, not the best use of the land

## Limits of this dossier

- buildings (types, footprints, floors, interiors) are read from the Everon catalog, docs/everon/catalog-buildings.json; 0 of 0 matched an entity of the engine dump, which adds the door positions

## Prefab references

Every distinct building prefab, as the world file refers to it.

_none_
