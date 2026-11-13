# Sockets and the skeleton (weapons)

A weapon that inherits a vanilla prefab is driven by the vanilla animation graph. The graph's
clips move bones by name and write bone-local transforms, and the prefab chain names sockets
(`PivotID`) for the muzzle, the sights and the magazine. So the mesh is fitted around the vanilla
skeleton, never the other way round.

## What the tool does

- The skeleton is the vanilla M9's, read from `Assets/Weapons/Handguns/M9/M9_body.xob` with
  `tools/assets/pipeline/xob.mjs` (game 1.9, 2026-11-12) and stored in `kinds.mjs`. Bones:
  `w_root`, `w_hammer`, `w_trigger`, `w_slide`, `w_fire_mode`, `w_slide_release`. Sockets: `eye`,
  `barrel_muzzle`, `barrel_chamber`, `slot_magazine`.
- Every bone keeps its vanilla rest position and has no rest rotation. `clean` fails when one
  moved by more than 0.1 mm, and `import` checks the compiled `.xob` again (0.5 mm), because an
  export axis mistake shows up only there.
- The whole mesh is skinned to `w_root`. Faces inside a `parts` box are split off and skinned to
  that part's bone.
- `barrel_muzzle` is moved to the mesh's front face, at the mean height of what is there. The
  other sockets keep their vanilla positions unless the spec's `sockets` overrides them.
- The mesh is scaled to the M9's height and its rear bottom corner is put on the M9's, because
  the hand pose and the reload clips assume that grip.

## What to check in `quarter.png`

| Marker | Socket | Must be |
|---|---|---|
| red | `barrel_muzzle` | On the front face, where the projectile leaves. The muzzle flash and the shot start here |
| blue | `eye` | Behind and above the weapon: the first-person aiming camera. In front of the rear sight means the player aims through the body |
| orange | `barrel_chamber` | Inside the body, behind the muzzle. It keeps its vanilla position |
| green | `slot_magazine` | At the bottom of the grip. The magazine entity attaches here, and the reload clip moves the hand to it |
| white | bones | Inside or on the mesh. A `w_trigger` marker in empty air means the mesh's trigger is somewhere else: adjust `fit.offset` or accept that the finger rests beside it |

The tool fails a muzzle more than 1 cm from the front face or off the centre line, an eye point
below the middle of the weapon or in front of the muzzle, and a magazine slot above the middle.

## What only the game shows

- **The hand.** The right hand takes the vanilla grip pose. A grip that is deeper or thicker than
  the M9's leaves the fingers lying on its side (the taser does this). Shoot the held weapon from
  another client; the holder's own first-person view does not show a lowered pistol.
- **The sights.** The scaffold puts `SightsPointFront` and `SightsPointRear` on the mesh's top
  edge. Aim down them in play mode; if the model has a raised rear sight, lift both points.
- **Parts that should not exist.** The vanilla graph still plays the slide and the hammer. On a
  model with neither, nothing is skinned to those bones and nothing moves, which is right.
- **The magazine.** The scaffolded magazine inherits the vanilla magazine and its mesh. A weapon
  whose cartridge looks different needs its own magazine model: run the pipeline again for it as
  a `gadget` and set the magazine prefab's `MeshObject`.

## Reading a vanilla weapon

```
tools\cli call game_read '{"path":"Prefabs/Weapons/Handguns/M9/Handgun_M9_base.et"}'
tools\cli call asset_search '{"query":"M9_body.xob"}'
```

`game_read` gives the prefab text (the `PivotID` names). The `.xob` is binary: extract it and
parse it with `xob.mjs` `parse()`, which returns the bounds, the materials and the node table.
