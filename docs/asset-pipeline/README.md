# Asset pipeline: roadmap item to Reforger asset

`tools\cli asset` turns a line in the roadmap into a model, textures, and prefabs in the addon.
The tool does the steps a script can do. An agent does the steps that need eyes, and this folder
is the playbook for those. What the first run taught us is in
[`docs/architecture/asset-pipeline-spike.md`](../architecture/asset-pipeline-spike.md).

```
tools\cli asset new <slug> --kind weapon|gadget|deployable --prompt "..." [--name PascalName] [--roadmap #N] [--no-run]
tools\cli asset run <slug> [--from <stage>] [--only <stage>] [--force] [--pick]
tools\cli asset score <slug> <n> <silhouette,era,scale_grip,background,no_text,image_to_3d> [note]
tools\cli asset pick <slug> <n> [--why "..."]     the human's choice from the concept sheet
tools\cli asset status [<slug>]
tools\cli asset sound <slug>  generated sound effects, after the prefab stage: sounds.md
tools\cli asset doctor        Blender, the Meshy key file, the credit balance
tools\cli asset selftest      the offline checks (also part of tools\cli validate)
```

Voiced NPC conversations are a separate stage with its own data file: `tools\cli asset dialogue <npc>`,
[dialogue.md](dialogue.md).

Run it in a worktree. The spec is `tools/assets/<slug>/spec.json` and is committed. Everything a
stage produces is cached in `tmp/assets/<slug>/`, which is not.

## Stages

| Stage | Does | Who judges the result |
|---|---|---|
| `spec` | Checks the spec: prompt length, size, triangle and credit budgets, part bones | The agent writes the spec: [prompt-writing.md](prompt-writing.md) |
| `concept` | Optional. 10 prompts become 10 Meshy text-to-image pictures and a numbered 5x2 contact sheet; each is scored against a rubric; one is picked. No `concepts.json`, no stage. [concepts.md](concepts.md) | A human picks (`asset pick`), or the agent with `--pick` |
| `generate` | With a picked concept: Meshy image-to-3D from that picture (one task, PBR maps). Otherwise text-to-3D preview, then refine. Downloads the GLB, the maps and a thumbnail; logs credits | The agent reads `raw/refined.png`: [review-generation.md](review-generation.md) |
| `clean` | Headless Blender: orientation, scale in metres, origin, floaters, decimation to LODs, one UV atlas packed as `_BCR` and `_NMO`, collider, and for a weapon the vanilla pistol skeleton, sockets and moving parts | The tool checks the numbers; the agent reads `clean/review/*.png`: [review-clean.md](review-clean.md), [socket-checks.md](socket-checks.md) |
| `import` | Copies the sources into the addon, writes every `.meta` with fresh GUIDs and the `.emat`, rebuilds them in a hidden Workbench, then reads the `.xob` back and checks bones, sockets, material and collider | The tool |
| `prefab` | Scaffolds the prefabs once (a weapon: weapon, magazine, projectile, magazine config, ammunition list entry; a gadget: one held item; a deployable: the deployed prop and the carried item). Every kind but `weapon` has its own `prefab-<kind>.mjs` | The agent finishes them: strings, feature component, placement |

After `prefab` comes verification, which is not a stage yet: [review-checklist.md](review-checklist.md).
Sound is its own command with its own record, because a review sits in the middle of it:
[sounds.md](sounds.md).

A finished stage is skipped on the next `run`. `--only clean` reruns one stage, `--from clean`
reruns it and everything after it, and `--force` reruns from the start. A stage that failed is
rerun. The `generate` stage saves each Meshy task id before it waits, so a run that died while
waiting resumes the same task and pays nothing twice.

## Credits

Read this before any `generate`.

- A concept batch is 10 images: 4 credits each on `nano-banana-2-1`, 40 in all (`concepts.mjs`; `nano-banana` is 3,
  `nano-banana-2` 6, GPT Image 9). Image-to-3D from the picked picture is one task: 30 credits on `latest` / `meshy-6`,
  15 on `meshy-6-lite`. Both are in `credits.jsonl` and count against `budget.credits`.
- One text-to-3D attempt is a preview plus a refine: 15 credits on `meshy-6-lite`, 30 on `latest`
  (`meshy.mjs` `COST`, read from the Meshy pricing page on 2026-11-12; a task's own
  `consumed_credits` wins when it reports one).
- Every generation's task ids and prompt (for image-to-3D: the image task, the concept number, its task id and prompt) are written to `tools/assets/<slug>/meshy.json` (committed; the asset's credit in `docs/credits.md` points at it).
- Every paid call is appended to `tmp/assets/<slug>/credits.jsonl`. `asset status` prints the sum.
- `budget.credits` in the spec is the cap for the asset and cannot be set above 300. `generate`
  refuses to start an attempt that would cross it. When that happens, stop and report: the prompt
  or the concept is wrong, and more attempts will not fix it.
- **`run --force` and `run --only generate --force` start a new paid attempt.** The old download
  moves to `raw-attempt-<n>/`. Never force `generate` to "refresh" a model you already have.
- A new attempt is justified when the review in [review-generation.md](review-generation.md)
  fails on shape. Two failed attempts mean the prompt needs a different idea, not a third try.
- The key is read at call time from `~/.life-framework/meshy-key` (`LF_MESHY_KEY_FILE` overrides).
  It is never printed, logged, or put in an error message; the selftest proves the last one.

## Setup

- Blender 4.5 LTS at `C:\Users\jaspe\Tools\blender-4.5.9-windows-x64\blender.exe`, or set
  `LF_BLENDER`. The clean stage needs no add-on.
- Arma Reforger Tools (Workbench), which `tools\cli build` already uses.
- `tools\cli asset doctor` checks both and prints the Meshy balance.

## Kinds

| Kind | State | Fit | Collider | Prefab scaffold |
|---|---|---|---|---|
| `weapon` | proven with the taser | Scaled to the vanilla M9's height and put on its grip; skinned to the M9 skeleton with its sockets | convex `UCX_`, `WeaponFire` | weapon on `Handgun_M9_base.et`, magazine, projectile, config |
| `gadget` | proven with the handcuffs (#664) | Centred on the origin, scaled to `size_m.length`; the origin is where the hand holds it | box `UBX_`, `ItemFireView` | one held item on the vanilla `Item_Base.et` (`prefab-gadget.mjs`): name, volume, the vanilla gauze hand attachment, `SCR_GadgetComponent`, preview camera, `gadget.components` |
| `deployable` | proven with the spike strip | Standing on the origin, scaled to `size_m.length`; `fit.repeat` tiles one generated link along the length, `fit.fold` stacks the links into a second model | thin box `UBX_` (3 cm), `ItemFireView` | the deployed prop and the carried item (`prefab-deployable.mjs`) |

The profiles are in `tools/assets/pipeline/kinds.mjs`. A new weapon family (a rifle, a melee
weapon) is a new kind with its own reference skeleton: read it from the vanilla `.xob` with
`xob.mjs`, as the M9's was.

## What the spec holds

```json
{
  "slug": "taser", "name": "Taser", "kind": "weapon", "roadmap": "#341",
  "concept": "one paragraph: what it is in 1989 Everon and what it does",
  "prompt": "...", "texture_prompt": "...",
  "meshy": { "ai_model": "latest", "target_polycount": 20000 },
  "size_m": { "length": 0.2, "height": 0.14, "width": 0.036 },
  "budget": { "lod0_tris": 8000, "lod_ratios": [0.5, 0.2], "texture_px": 1024, "credits": 300 },
  "fit": { "offset": [0, 0, 0] },
  "parts": [{ "name": "trigger", "bone": "w_trigger", "box": [x0, y0, z0, x1, y1, z1] }],
  "asset_dir": "Assets/Weapons/Handguns/Taser", "prefab_dir": "Prefabs/Weapons/Handguns/Taser",
  "weapon": { "name_key": "#LF-Weapon_Taser_Name", "...": "..." }
}
```

- `fit` corrects what the clean stage guessed: `up_axis` (`"x"`, `"y"`, `"z"`), `flip_up`,
  `flip_forward`, `pitch_deg`, `scale`, `offset` (metres, Enfusion axes: x right, y up, z forward).
- `parts` are the moving parts. Every face whose centre lies in the box is split off and skinned
  to the bone. Boxes are in Enfusion model space after the fit.
- `sockets` (optional) overrides a socket position by name.
- `weapon.components` are extra components written into the weapon prefab, with their values.
- A gadget has a `gadget` block instead: `name_key`, `description_key`, `weight`, `volume`, `components`, and `hand`
  (`rotation`, `translation`: the `HandAdditive*` values; the default is the lockpick's). Vanilla held gadgets have no
  bones or sockets (`Gauze_01_packed.xob`): the hand holds the model at its origin, so move the grip there with `fit.offset`.
- A gadget is scaled on its length, so the size check holds the length to 5 % and the height and width to 40 %.
- `fit.stretch` (not for a weapon) fits each axis to `size_m` on its own instead of scaling by the
  length: right for a flat prop whose height the generator guesses.
- `fit.repeat: n` (deployable) makes the generated model one of n links end to end; `size_m` stays
  the size of the whole thing and the triangle budget is shared by the links. `fit.fold: true` also
  exports `<Name>_Folded`: the n links stacked, with its own LODs and box, imported beside the first
  model with the same material. The spike strip is 7 links of 0.5 m.
- `deployable` holds what the scaffold writes: `name_key`, `description_key`, `weight`,
  `item_volume`, the deployed prop's `components` and `actions`, and the carried item's
  `item_components` and `item_actions`. A string prop value `"$deployed"` or `"$item"` becomes that
  prefab's `{GUID}path`, so the item names what it deploys into and the prop what it folds back
  into. Nothing else is written: the deploy, the pick-up and what the prop does are feature code
  (`Feature/Police/LF_SpikeStrip.c` for the strip).

## What the tool does not do

- It does not decide whether the model looks like the thing. That is the generation review.
- It does not make an icon. Enfusion renders inventory pictures from the prefab at run time;
  check them with the harness `previews` verb or `tools\cli shots previews`.
- It does not add string table keys, feature code, world placement or tests.
- The model stages make no sound. `asset sound` does ([sounds.md](sounds.md)); until it has run, a
  weapon keeps the vanilla M9's sounds.
- It does not rewrite a prefab that exists. After the scaffold the prefabs are ordinary files.
- It does not classify the asset core or pack. Do that by the red line
  (`docs/architecture/core-vs-pack.md`) and add the inventory rows.
