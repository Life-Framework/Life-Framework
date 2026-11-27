---
name: polish
description: "Use before shipping anything a player can see in Life Framework: a placed prop, a sign, an NPC, an item, a sheet or HUD element, a layer or prefab edit. The definition of done for visible work: look before and after, the polish checklist, and the pictures the PR must embed. Trigger on 'does it look right', 'polish', 'janky', 'screenshots for the PR', or any diff touching Worlds, Prefabs, Assets, UI or Configs/Signs."
---

# Polish: done for anything a player sees

A feature that passes its tests and looks like a test fixture is not done. Tests and the
placement audit measure; only a picture shows "a desk standing in a field". The maintainer found
most of the town's jank by looking, after merge. Look first, so they do not have to.

`docs/style-guide.md` says what right looks like. This is the procedure.

## Which bar

DebugWorld has two zones (`docs/architecture/world-zones.md`):

- **Town** (village layers and the pack town): the whole checklist below, every line. Only a
  town-placement pass (worktree slug `town-...`) edits town layers.
- **Debug grid** (a feature's `Grid_<slug>.layer` cell, and the legacy annex): only "It sits on its
  surface" and "can be reached on foot" hold. Debug-looking props, stand-ins and the grid sign are fine.
  One `tools\cli look grid:<slug>` picture goes in the PR; there is no before picture.

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
   shots with `tools\cli evidence <issue> after <png> --name <what>`. **`session down` deletes the client
   screenshot folders: `session.mjs pull <dir>` (or `evidence`) before it.** A sheet has a proof that opens it and
   shoots it already (`lot-sheet-proof` through `lotsheet open|rows|close`); look in `agent-box/README.md` first.

## Checklist

Tick each line from the pictures. A line that cannot hold gets one sentence in the PR saying why.

**Place**
- [ ] It sits on its surface: nothing floats, sinks (a few cm into the ground counts), tilts, or stands on a
      slope. The audit passes a car on a tilted lawn; the picture shows the lean.
- [ ] It is where that kind of thing belongs (style guide "Where things go"): boards on walls,
      furniture under a roof, illegal things out of sight.
- [ ] It touches its group: a wall, a counter, a kerb, a neighbour. Not alone on a lawn.
- [ ] It does not block a door, a path, a road, or the view of a sign, and no sign stands behind a door, a
      wall or a hedge from where a player faces it.
- [ ] Interactables face the side players come from and can be reached on foot.

**Thing**
- [ ] It is what it claims to be, at its real size beside its neighbours (the bounds are under
      each close-up). A stand-in is in `docs/asset-inventory.md` and named in the PR.
- [ ] No white, magenta or mirror-chrome surface (a blank sign face is expected in `look`: see below).
- [ ] Nothing debug-looking: no `*_Debug` entity or `/Debug/` prefab in a village layer, no placeholder-bright
      box (flat white or saturated colour), no vanilla kitchen unit or indoor fixture standing outdoors, no traffic cones.
- [ ] A shop or counter is stocked: no empty shelf run, and goods stand singly on the shelf, never in a pile
      (11-25 sweep: gun store, hardware, general store, Chotain bar).
- [ ] A light has a visible source (lamp, window, flame, fixture); no glow hangs in mid-air.
- [ ] It fits 1989 Everon: period, material, wear.

**Sign or text**
- [ ] The right sign type and paint for the job; capitals; two or three words.
- [ ] Readable from where the player stands; not clipped by a roof, a wall or another sign. The text is read in a
      session shot, by day and at 22:00 (below).
- [ ] Every string is a key in the string table and reads like printed matter: US English, no raw id, repo
      path, stray placeholder or `...` cut-off (#4993).

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

## What `look` cannot show

- **Sign faces.** A face is a render target painted at runtime; a `look` picture shows a blank plate and prints a
  `NOTE`. That is not a defect (the 11-25 sweep graded 21 working signs BELOW for it). Placement, size and what
  blocks the sign are judged from `look`; the text only in a session (`harness stand` + `shot world`).
- **The right room.** `--interior auto` stands at the slot's counter or desk row. When the camera lands in a
  stairwell, on a roof or in a neighbour's flat, `--interior <kit row>` (a `Fit_...` name or a unique suffix such as
  `lf_medic_duty_desk`) anchors on that row; an unknown name lists the rows.
- **Lights at night, UI, motion, sound, a second player:** a session (next section, and step 7).

## Night (22:00)

Every player-facing site is graded at 22:00 as well as by day. The 11-25 night sweep found 17 of 42 stops BELOW
(dark fascias, interiors with no lamp, fronts with no street light); the 11-27 east sweep found 4 of 6 work sites
pitch black that were all OK by day. Pass: from where a player stands, the door, the counter or ore or furnace,
and the sign plate can be found without a torch. Light a new work site in the same PR; do not wait for a sweep.

| The thing | Use |
|---|---|
| A used building's windows, a street-lamp head, a dark front | `node tools/town/nightlights.mjs <place>` writes `Lights_<place>.layer` (generated: rerun, never hand-edit; a dark front is a row in its `FRONTS`) |
| A lamp, lantern or flood-lamp mesh with no light | `LF_LanternGlow.et` on it (warm, 7 m, no shadow, 18:00 to 06:00); lift one lying in grass onto a stool so its pool falls on the ground and the ore |
| A camp or fire | `LF_CampFlame.et` (flame plus orange light) |
| A shop sign | `LF_Sign_Fascia` lights itself (spot 2.2 m in front); a pole plate needs a lamp beside it |
| A room with no lamp | a ceiling lamp or pack `LF_LightInteriorFill` row in its kit (vanilla ceiling lamps are dead meshes) |

All run on `LF_NightLightComponent` (`Feature/Atmosphere/Feature.md`). Each is a dynamic light: one per spot, short
range, no shadow (`docs/architecture/performance.md`). A kit change goes in the kit and its slot layer together.
Grade it in one full-profile island session: pin the clock with the harness `time` verb (22; pin again every few
shots, the lights need about 35 s), `stand` at the site, `capture world` at eye height, `session down` straight
after. `agent-box/east-night-shots.mjs` is the model; its `before` and `after` modes give the PR pair.

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
