# Reviewing the clean stage

`clean` fails by itself on the numbers (`tools/assets/pipeline/checks.mjs`): triangle budget, LOD
names and order, size against the spec, UVs inside the atlas, a base colour map, the collider's
name and layer, sockets and bones, and a moving part that caught no faces. This page is for what
the numbers cannot see. The renders are in `tmp/assets/<slug>/clean/review/`.

| File | Look for |
|---|---|
| `side_left.png`, `side_right.png`, `top.png`, `front.png` | Orientation: the front of the object points right in `side_left.png`, up is up. The yellow wire box is the vanilla reference's bounds; a weapon's grip should sit in its rear lower corner |
| `quarter.png` | The socket markers (see [socket-checks.md](socket-checks.md)) |
| `clean_side.png` | LOD0 with its texture and no markers: holes, flipped faces (black patches), smeared texture where decimation collapsed a UV seam |
| `lod_last.png` | The lowest LOD: the silhouette must still read. If parts are missing, raise the last entry of `budget.lod_ratios` |
| `icon.png` | A transparent render for docs and PRs. It is not the inventory picture; the engine renders that |

## Fixing what you see

Every fix is a spec edit followed by `tools\cli asset run <slug> --from clean`. Nothing is paid.

| Seen | Edit |
|---|---|
| Upside down, or the muzzle points backwards | `fit.flip_up` / `fit.flip_forward`: `true` inverts what the tool guessed |
| Lying on its side | `fit.up_axis` to the source axis that is really up |
| Nose up or nose down | `fit.pitch_deg` |
| Too big or small in the hand although the height is right | `fit.scale`, and say why in the spec's `concept` |
| The grip is not where the hand is | `fit.offset` in metres (x right, y up, z forward) |
| A hole after decimation | Raise `budget.lod0_tris`, up to the kind's cap |
| The trigger does not move alone, or takes part of the guard with it | Adjust the part's `box`; `report.json` `parts[].faces` says how many faces it caught |
| `report.json` `notes` says the collider fell back to the bounding box | Accept it for a boxy object; otherwise report it, the convex hull kept sliver faces |

## Textures

`Taser_BCR.tif` is base colour with roughness in alpha; `Taser_NMO.tif` is normal X, normal Y
(flipped to DirectX), metallic, and ambient occlusion in alpha. `report.json` `textures` lists
which source maps were found and their means. A mean metallic near 1 on a plastic object, or a
mean roughness under 0.2, means the generator's maps are wrong: the object will look like chrome.
Meshy's maps do that to everything, so the clean stage remaps them per material class
([README "Materials"](README.md)); `report.json` `materials` has the before and after means and each
region's share of the atlas. If the object still looks wrong, change its classes in the spec's
`materials` block (a region too, if one part is another material) and rerun `--from clean`. Do not
write a one-off script.

## What a pass means

The model has the right size, sits right way up on the reference, has LODs that read, one
material on one atlas, a collider, and for a weapon the vanilla skeleton. It does not mean the
model is good; that was the generation review.
