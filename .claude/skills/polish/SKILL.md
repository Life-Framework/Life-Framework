---
name: polish
description: "Use before shipping anything a player can see in Life Framework: a placed prop, a sign, an NPC, an item, a sheet or HUD element, a layer or prefab edit. The definition of done for visible work: look before and after, the polish checklist, and the pictures the PR must embed. Trigger on 'does it look right', 'polish', 'janky', 'screenshots for the PR', or any diff touching Worlds, Prefabs, Assets, UI or Configs/Signs."
---

# Polish: done for anything a player sees

A feature that passes its tests and looks like a test fixture is not done. Tests and the
placement audit measure; only a picture shows "a desk standing in a field". The maintainer found
most of the town's jank by looking, after merge. Look first, so they do not have to.

`docs/style-guide.md` says what right looks like. This is the procedure.

## The loop

1. **Before.** `tools\cli look <area>` (about 25 s, no session) before the first edit. Read
   `tmp/look/<area>/contact.jpg`. `tools\cli evidence <issue> before <area>` keeps the wide;
   add `--shot c07` for the close-up of the spot you will change.
2. **Place with the tool.** `tools\cli place on|against|ground|arrange`, never typed coordinates.
   `tools\cli audit placement --area <area> --strict` must be clean.
3. **After.** `tools\cli look <area>` again. Open the close-up PNG of every new or moved thing.
   Walk the checklist below against the pictures, not against your memory of the edit.
4. **Fix and repeat** until every line holds. Each pass costs 25 s.
5. **Keep the pair.** `tools\cli evidence <issue> after <area>` (same `--shot`), then
   `tools\cli evidence <issue> table` prints the before | after table for the PR body.
6. **These are the PR pictures.** `look`, `look item <prefab>` (a dropped item and its inventory
   camera view) and `wb_preview` (one prefab, material or mesh in an open Workbench, about 2 s;
   `tools\cli call wb_preview '{"resource":"{GUID}path"}'`) cover placement, props, signs, items,
   prefabs, materials and layers. Do not book a session for a picture of those.
7. **A session only for what Workbench play cannot capture**: a sheet, HUD or other UI widget, a
   second player or late joiner, motion, animation, sound, filming (a held item's hand pose is
   motion). One `session up --profile full`, the proof and the shots in it, `session down`. Keep those
   shots with `tools\cli evidence <issue> after <png> --name <what>`.

## Checklist

Tick each line from the pictures. A line that cannot hold gets one sentence in the PR saying why.

**Place**
- [ ] It sits on its surface: nothing floats, sinks, tilts, or hangs off an edge.
- [ ] It is where that kind of thing belongs (style guide "Where things go"): boards on walls,
      furniture under a roof, illegal things out of sight.
- [ ] It touches its group: a wall, a counter, a kerb, a neighbour. Not alone on a lawn.
- [ ] It does not block a door, a path, a road, or the view of a sign.
- [ ] Interactables face the side players come from and can be reached on foot.

**Thing**
- [ ] It is what it claims to be, at its real size beside its neighbours (the bounds are under
      each close-up). A stand-in is in `docs/asset-inventory.md` and named in the PR.
- [ ] No white, magenta or mirror-chrome surface; no blank sign.
- [ ] Nothing debug-looking: no `*_Debug` entity or `/Debug/` prefab in a village layer.
- [ ] It fits 1989 Everon: period, material, wear.

**Sign or text**
- [ ] The right sign type and paint for the job; capitals; two or three words.
- [ ] Readable from where the player stands; not clipped by a roof, a wall or another sign.
- [ ] Every string is a key in the string table and reads like printed matter.

**Sheet, HUD, face** (needs a session shot: Workbench captures no UI)
- [ ] Built from UI kit parts; no colour, font or size set by the feature.
- [ ] One job per sheet; it closes on death, on losing the character and on walking away.
- [ ] Nothing overflows or is cut off at 1280x720; long text wraps.
- [ ] A face on a world prop reads at the distance a player stands, by day and at night.

**Item** (`look item` for the ground and inventory views; a session only for the hand)
- [ ] Held (session): the hand closes on it, it is the right size, it is not inside the arm.
- [ ] Inventory preview (`look item`) shows the item, lit and filling the cell; the slot UI frame needs a session.

**Scene**
- [ ] The wide still reads at a glance as what the place is, and the new thing does not make it
      busier than it has to be.
- [ ] Nothing you did not mean to change moved: compare the before and after wides.

## What the PR carries

- The before | after table, embedded. `wt ship` refuses a player-visible change whose body
  embeds no picture, and a path in backticks is not a picture. If there is truly nothing to
  see, the body says `No screenshots: <reason>`.
- One line per checklist exception.
- Under `## Manual testing`, what only a human can judge: how it sounds, how it feels to use,
  a physical gamepad.

## When the picture is wrong and the numbers are right

The audit passes "on a surface" when it is the wrong surface; `look` raises no flag for a desk in
a field. The picture wins. Fix the scene, and if the same thing could be caught by a number next
time, add the check to `tools/validation/` in the same PR.
