# Agent-driven static asset pipeline

How an agent should take a new static inventory item (first target: a bowling
ball) from nothing to a held, dropped, inventoried object in game, and what
blocks it. Researched 2026-11-08 on this PC. Nothing here is built yet.
The work is tracked in the Development epic
[#339](https://github.com/Life-Framework/Life-Framework/issues/339). Paid
prompt-to-3D services are compared in
[Paid prompt-to-3D services](#paid-prompt-to-3d-services) (2026-11-09).

Sources are numbered `[n]` and listed at the end. Facts tagged **(local)**
were read on this machine, in the installed Arma Reforger Tools, the
packed game data (`tools\cli call game_read`), or this repo. The Bohemia wiki
refused direct fetches (Cloudflare 403). Wiki facts come from the
`wiki_read` MCP tool's cached pages or search excerpts and are marked
*(wiki)*.

## Recommendation in one paragraph

Generate simple and medium props as **Python scripts run by headless Blender
4.5 LTS**, with Cycles bakes on the CPU, packed into Enfusion's `_BCR`/`_NMO`
layout by the same script. Commit the generator script as the asset's source.
Import through **Workbench plus Enfusion Blender Tools (EBT)**, which ships
with Arma Reforger Tools. The one step that is not yet scriptable is the
first registration of a new `.fbx`, because Workbench opens a modal dialog;
every later reimport runs headless. Verify with a Blender render, a Workbench
import report, and an agent-box world shot. Use CC0 libraries (Poly Haven,
ambientCG) for texture sets and for props too complex to model in code. Do
not use AI text-to-3D for shipped art yet: the one free local model that fits
in 10 GB of VRAM (Hunyuan3D) is not licensed in the EU, and the paid APIs
produce triangle soup that needs retopology anyway.

## What this PC has today

| Item | State | Evidence |
|---|---|---|
| Blender | **Not installed.** `steamapps/common/Blender` holds only a leftover `4.3` config folder, no `blender.exe`; `winget` finds none | (local) |
| EBT | **Installed** with Arma Reforger Tools at `Arma Reforger Tools/Blender/scripts/addons/EnfusionBlenderTools`, build 37, core version `2026-08` | `build_ver.json` (local) |
| EBT's Blender version | Exactly **4.5** (`SUPPORTED_BLENDER_VERSION = (4, 5, 0)`, checked on major.minor) | `ebt/ebt_configuration.py` (local) |
| EBT licence | GPL-3.0-or-later with Section 7 terms. Fine to use as a tool; never copy its code into this MIT repo | `LICENSE.txt` (local) |
| GPU | RTX 3080, 10 GB | `nvidia-smi` (local) |
| Workbench MCP | `enfusion-mcp-bk` has `wb_resources` (`register`, `rebuild`, `getInfo`, `open`) through its own handler `EMCP_WB_Resources.c`, which calls `ResourceManager.RegisterResourceFile` and `RebuildResourceFile`. It needs a live Workbench. `api_search`, `game_read`, `asset_search`, `game_browse`, `wiki_read` work offline | (local) |
| Headless Workbench | Already used by `tools\cli build` (`-wbModule=ResourceManager -buildData`) and `validate-scripts.ps1`, on the hidden `LF-Agents` desktop | `tools/cli.mjs`, `tools/validation/validate-scripts.ps1` (local) |

So this PC can run every stage except Blender itself. Installing Blender 4.5
LTS (portable zip from blender.org, or the Steam beta branch pinned to 4.5)
plus EBT's bundled installer is the first task.

## Spike: can Workbench import without a click?

A throwaway `WorkbenchPlugin` with `RunCommandline()` was launched as
`ArmaReforgerWorkbenchSteamDiag.exe -gproj ... -wbModule=ResourceManager -plugin=LF_SpikeImportPlugin -lfFbx=<abs path>`
in this worktree on the hidden desktop. The plugin was not committed. Results:

| Action | Result |
|---|---|
| Plugin `RunCommandline` with `-plugin=` | Runs. `ResourceManager.GetCmdLine("-lfFbx", v)` reads one value per flag; a `;`-joined list arrived empty |
| `RegisterResourceFile(<new>_BCR.tif, false)` | **Works headless.** Writes `.edds.meta` with `TIFFResourceClass PC : TextureColorMap.conf` picked from the suffix; log `Resource file successfully registered` |
| `RegisterResourceFile(<new>.fbx, false)` | **Blocks.** Workbench opens a modal window titled `Register Resource File(s)` (enumerated with `EnumDesktopWindows`). It has no Win32 child controls, and a posted Enter did not dismiss it. Workbench logs `Application hangs` after 65 s |
| `RegisterResourceFile(<new>.xob, ...)` | Returns false; the source path must be the `.fbx` |
| `RebuildResourceFiles([Apple.fbx], "PC")` on an already registered FBX | **Works headless.** `Rebuilding @"Assets/Items/Food/Apple/Apple.fbx"` then `Build successful`; `.txo` rewritten |

What follows from it:

- First registration of a new FBX needs one human click, or a GUI
  Workbench where the same dialog may still appear. EBT's `ExportFBXResource`
  net API handler calls the same `RegisterResourceFile(path, false)`
  (`scripts/WorkbenchGameCommon/ResourceInfo.c`), so EBT does not avoid it.
  Whether that dialog shows when the call comes through the net API into a
  GUI Workbench is **the main open unknown**. It is the next spike.
- After that, the import settings (`MaterialAssigns`, `GeometryParams`,
  `ExportSceneHierarchy`) are fields on the `MetaFile` that a plugin can set
  and `Save()`, then `RebuildResourceFile`. `ExportFBXResource` does exactly
  this, so we can copy its approach in our own `LF_` plugin.
- A VME inside a `RunCommandline` plugin also hangs Workbench (VME dialog).
  The real plugin must write a result file and keep a watchdog timeout.

## Pipeline stages

A = automated, S = semi-automated (a human click or a judgement), M = manual.

| # | Stage | Mode | Tool |
|---|---|---|---|
| 1 | Write an asset spec (`tools/assets/<name>/spec.json`): real dimensions, mass, materials, texture size, collider shape, inventory slot | A | agent |
| 2 | Build the mesh with a generator script: `blender -b --factory-startup --python tools/assets/<name>/build.py` | A | Blender 4.5 `bpy`/`bmesh` |
| 3 | UV unwrap in the script with pinned settings | A | `bpy.ops.uv.smart_project` under `temp_override`, or seams plus `unwrap` |
| 4 | Material: procedural node tree baked with Cycles CPU to base colour, roughness, normal, AO, metallic (via Emit) | A | Cycles bake |
| 5 | Pack channels into `_BCR.tif` (RGB base colour, A roughness) and `_NMO.tif` (R normal X, G normal Y DirectX, B metallic, A AO) | A | script (numpy) or EBT `ebt.texture_tools_channel_pack_texture` |
| 6 | Add colliders (`USP_`, `UBX_`, `UCX_`...) with a `usage` custom property for the layer preset; name materials so they match the `.emat` | A | script, or EBT collider operators |
| 7 | Render a turntable and a scale shot beside a 1.8 m reference figure; agent reads the PNGs | A | Blender Workbench/EEVEE or Cycles render |
| 8 | Run EBT's model QA headless (`ebt.mqa_report_background`, the operator Workbench's Validate FBX plugin calls) | A | EBT |
| 9 | Export FBX (Blender exporter with EBT settings, or `ebt.export_fbx` with Workbench up) | A | Blender / EBT |
| 10 | Register textures headless | A | `LF_` CLI plugin (spike proved it) |
| 11 | Register the new FBX the first time | **S** (one click today) | Workbench Resource Manager |
| 12 | Create `.emat` (`MatPBRBasic` with `BCRMap`, `NMOMap`) and set `MaterialAssigns`, `GeometryParams` (layer preset, `.gamemat`) on the meta; reimport | A | `LF_` CLI plugin, `RebuildResourceFile` |
| 13 | Check the import: log lines (`Invalid collider`, missing material), the meta has a `GeometryParam` per collider, every `SourceMaterial` maps to our `.emat` | A | log grep plus meta parse |
| 14 | Create the item prefab inheriting a base item | A | `game_duplicate` / prefab tools, `{GUID}path` from `asset_search` |
| 15 | Add a DebugWorld fixture and a `flow` file; `shot world` at 1.5 m and 0.5 m; pick up, hold, drop, open inventory | A | agent-box harness, `journey.mjs --film` |
| 16 | Judge look and feel (texture, hand pose) | **M** | maintainer, from the filmed clip |
| 17 | Ship: validate, PR with manual steps | A | `tools\cli wt ship` |

## Tool choices

**Headless Blender over Blender MCP.** `ahujasid/blender-mcp` and the
official Blender Lab MCP both drive an open GUI session over a localhost
socket and run arbitrary LLM-written Python [7][8]. They are good for
exploring. A committed script run with `-b` is reproducible, diffable,
reviewable, and runs on the hidden desktop or on `lf-testbox`. The agent
iterates by editing the script and reading renders, which is the same loop
an MCP would give it. Blender Lab MCP also needs Blender 5.1, and EBT is
pinned to 4.5.

**EBT for Enfusion specifics.** It already knows the collider prefixes, layer
presets (read from Workbench via `LayerPresets`), game materials
(`GetGameMaterials`), channel packing, and model QA, and it talks to
Workbench over the net API with `RegisterResource`, `ExportFBXResource`, and
`ExportTextureResource` (local, `extern/enfusion/workbench.py`,
`core/resource_info.py`). Its operators are callable from Python
(`bpy.ops.ebt.*`). Workbench itself launches Blender with
`--python-expr "bpy.ops.ebt.mqa_report_background(...)"` for validation
(local, `ValidateFBXPlugin.c`). Most EBT operators require a running
Workbench; the plain Blender FBX exporter does not.

**Our own `LF_` Workbench CLI plugin** for register, meta edits, and rebuild,
modelled on `ExportFBXResource`. It needs no GUI for anything but the first
FBX registration.

### Mesh route per asset class

| Class | Route | Why |
|---|---|---|
| Simple primitives (bowling ball, bar, box, can, bottle, sign) | Procedural script | Exact real dimensions, clean topology, 100% our own MIT work, no licence trail |
| Props with detail (radio, toolbox, chair) | CC0 model from Poly Haven, or procedural with a hand-tuned script | Poly Haven is CC0 with a keyless API (User-Agent required) [9]. Their models are realistic and fit Reforger's look |
| Organic items (fruit, food, bread) | CC0 scan (Poly Haven), or Sketchfab CC0/CC-BY with attribution | Scans beat code here. Sketchfab downloads need OAuth and come as glTF only [11] |
| Anything AI text/image-to-3D | Concept or blockout only, not shipped | See below |

**Licences** follow `docs/ecosystem.md`. CC0 copies freely with provenance
recorded. CC-BY needs title, author, source URL, licence link, and the
changes made, in a credits file [12]. CC-BY-SA, NC, and ND are out. Kenney is
CC0 but stylised low-poly [13], a poor fit for this game's art.

**AI generators, checked 2026-11-08:**

| Tool | Runs | Output | Licence of outputs | Fit |
|---|---|---|---|---|
| Hunyuan3D-2 / 2.1 | Local, 6 to 29 GB VRAM | GLB, PBR in 2.1 | Tencent Community Licence **excludes the EU, UK, and South Korea** [14] | Blocked; the maintainer is in the Netherlands |
| TRELLIS.2 | Local, 24 GB+, Linux | GLB with base colour, roughness, metallic | MIT (NVIDIA deps under their own terms) [15] | Does not fit a 10 GB card |
| Meshy | API (paid plan) | glb/fbx, optional quad remesh, PBR maps | Free plan CC BY and public; paid plans customer-owned [16] | Usable on a paid plan, about USD 20/month |
| Tripo | API (paid plan) | quad option, PBR flag | Free outputs retained by Tripo; paid owned by user [17] | Same |
| Rodin (Hyper3D) | API on the Business plan only | quad presets 4k to 50k | Not stated on the pricing page [18] | Expensive for a test |

All of them return dense triangle meshes with auto-generated atlases that
need retopology, UV cleanup, real-world scaling, and a collider before
Enfusion. For a bowling ball that is more work than writing the sphere.
For props that are hard to script, a paid service can be worth it; see
[Paid prompt-to-3D services](#paid-prompt-to-3d-services).

## Texturing

1. **Procedural in Blender, baked.** Build the look from nodes (noise for
   marbling, a roughness value, a normal from bump), bake with Cycles on the
   CPU, which works without a display [19]. Bake types needed: Diffuse with
   only Color, Roughness, Normal (tangent space), AO. Metallic has no bake
   type; route it through an Emission shader and bake Emit.
2. **CC0 PBR sets** from ambientCG or Poly Haven [9][10] for wood, metal,
   fabric. Reproject onto our UVs in Blender and bake to our maps.
3. **AI texture generation**: Hunyuan3D-Paint shares Hunyuan's EU exclusion;
   StableMaterials is under OpenRAIL [20]. Not recommended now.

**Packing for Enfusion** (texture suffix rules read from
`TextureImportTool.c` [3], channel layout from [5]):

| File | R | G | B | A | Import |
|---|---|---|---|---|---|
| `<Name>_BCR.tif` | base colour R | G | B | roughness | BC7, sRGB |
| `<Name>_NMO.tif` | normal X | normal Y, **DirectX** (flip Blender's OpenGL green) | metallic | AO | BC7, linear |

Base colour is baked in sRGB; roughness, metallic, AO, and normal are
linear. Save 8-bit TIFF with alpha. The `.emat` is `MatPBRBasic` with
`BCRMap` and `NMOMap` set, as `Assets/Crafting/Charcoal/Data/Coal_material.emat`
already does (local).

**Size.** A hand item needs 1024² at most; 512² for a plain ball.
`Assets/Tools/Pickaxe/Data/T_Pickaxe_BCR.tif` is 67 MB and its `.edds`
22 MB (local); the repo pack is already 289 MB. Cap new textures at 1024²
and add a validation check for it.

## Import conventions

- **Units and orientation.** Metres. Apply rotation and scale, not location,
  before export *(wiki: FBX Import)*. Front faces +Y in Blender, which the
  FBX export turns into Enfusion's +Z [6].
- **Pivot.** The origin is what the hand holds and what the drop rests on.
  For a ball put it at the centre of the sphere; `USP_`, `UCS_`, and `UCL_`
  colliders also need their origin at their own centre *(wiki: FBX Import)*.
- **Colliders** *(wiki: FBX Import, Prop Creation)*: `UBX_` box, `USP_`
  sphere, `UCS_` capsule, `UCL_` cylinder, `UCX_` convex, `UTM_` trimesh
  (not for general collision). The layer preset is a custom property `usage`
  on each collider, so the FBX must export custom properties. A bad collider
  logs `RESOURCES (W): Invalid collider '<name>'. Export skipped`. The game
  material comes from the collider's material name or the meta. FBX values
  are only imported while the meta's `GeometryParams` list is empty.
- **Item colliders use `LayerPreset "ItemFireView"`**, as our
  `Apple.xob.meta` does with `USP_Main` and `default.gamemat` (local).
- **LODs.** `<name>_LOD0`, `_LOD1`, ...; gaps compact; unsuffixed meshes go
  to LOD0 *(wiki: FBX Import)*. A ball needs LOD0 plus one LOD at about 25%.
- **Material mapping.** The meta maps each FBX `SourceMaterial` name to an
  `AssignedMaterial` `.emat` (local, Coal and Apple metas). If the name does
  not match, Workbench generates a placeholder `.emat` in `Data/`
  (`Sulfur/Data/DefaultGeneratedMaterial.emat` is one, local).

### What the inventory item prefab needs

Read from base-game prefabs with `game_read` (local):

| Field | `Item_Base.et` | Grenade M67 | Radio PRC-68 | Canteen US |
|---|---|---|---|---|
| Parent | none | `FragGrenade_Base.et` | `Radio_base.et` | `Canteen_base.et` |
| `ItemPhysicalAttributes` | Weight 0.01 | 0.39, Manual | 1.3, Manual, Volume 300 | 0.4, Manual, Volume 1000 |
| `PreviewRenderAttributes` | none | Left, distance 0.7, FOV 10 | Top, orbit 20 0 0, distance 1, FOV 10 | Back, orbit 25 -25 180, distance 1.5, FOV 10 |
| `RigidBody` | none | Mass 0.397 | CenterOfMass 0 0.1 0 | Mass 3 |
| Holding | none | `BindingName "Grenade"` | `BindingName "Gadget"` + `.agr`/`.asi` | `BindingName "Gadget"` + `.agr`/`.asi` |
| `m_Size` | `SLOT_1x1` | | | `SLOT_1x1` (inherited) |

`Item_Base.et` (`{C95E11C60810F432}`) brings `InventoryItemComponent`,
`ActionsManagerComponent` with `SCR_PickUpItemAction`, pickup/drop sounds,
`RplComponent`, `Hierarchy`, and `Persistence`. The inventory icon is a live
render driven by `PreviewRenderAttributes`; no icon file is drawn. A plain
held item sets `ItemAnimationAttributes { HandAdditiveRotation;
HandAdditiveTranslation }`, as our `Prefabs/Items/Food/Apple.et` does. Gadget
holding (radio, canteen) needs an anim graph and instance and is out of scope
for a ball.

## Bowling ball checklist

Spec: diameter 0.216 m (the 8.5 in maximum), mass 7.0 kg (about 15 lb),
three holes (two fingers, about 22 mm wide; one thumb, about 25 mm; 60 mm
deep), glossy urethane, dark blue marbled.

1. `tools\cli wt new asset-bowling-ball`.
2. Write `tools/assets/bowling-ball/build.py`: UV sphere or quad sphere
   r = 0.108, boolean the three holes, bevel the rims, apply transforms,
   origin at the centre.
3. Collection `BowlingBall`: `BowlingBall_LOD0` (about 2k tris),
   `BowlingBall_LOD1` (about 500), collider `USP_Main` r = 0.108 with
   `usage = "ItemFireView"`.
4. Material `BowlingBall` (this exact name becomes `SourceMaterial`).
   Bake 512² `BowlingBall_BCR.tif` (roughness about 0.15) and
   `BowlingBall_NMO.tif` (metallic 0, AO from the holes).
5. Render front, top, and a shot beside a 1.8 m capsule to
   `tmp/asset-bowling-ball/`. Read them: round, holes on one hemisphere,
   about one-eighth of the figure's height.
6. Run EBT model QA; zero errors.
7. Export `addons/LifeFramework/Assets/Items/Roleplay/BowlingBall/BowlingBall.fbx`
   with the textures in `.../BowlingBall/Data/`.
8. Register the textures with the `LF_` plugin; confirm both `.edds.meta`
   files name `TextureColorMap.conf` and the NMO profile.
9. Register the FBX in Workbench (the one click), then let the plugin write
   `Data/BowlingBall.emat`, set `MaterialAssigns` (`BowlingBall` to that
   `.emat`) and `GeometryParam USP_Main { LayerPreset "ItemFireView";
   SurfaceProperties { plastic.gamemat } }` (`Common/Materials/Game/plastic.gamemat`
   exists, local), save, and rebuild.
10. Grep the import log for `Invalid collider` and `DefaultGeneratedMaterial`;
    expect neither.
11. `Prefabs/Items/Roleplay/BowlingBall.et` inheriting `Item_Base.et`:
    `MeshObject` to the `.xob`; `RigidBody { Mass 7 }`; `UIInfo` with
    `#LF-Item_BowlingBall_Name`/`_Desc` keys appended to the string table
    plus `regen-localization`; `ItemPhysicalAttributes { Weight 7;
    SizeSetupStrategy Volume; ItemVolume 5300 }` (sphere volume in cm³;
    confirm the unit against the Apple's 113); `m_Size SLOT_2x2` (unverified,
    test in the inventory); `PreviewRenderAttributes { CameraDistanceToItem
    0.8; FOV 10 }`; `HandAdditiveTranslation` tuned from the film.
12. DebugWorld fixture `BowlingBall_Debug` in the test annex layer, and
    `agent-box/flows/item-bowling-ball.txt`: stand, look, pick up, open
    inventory (`shot ui`), equip, drop, `shot world` close and at 2 m.
13. `node agent-box/journey.mjs ... --film` on a game client at playable
    resolution. Check: textured (not white or grey checker), sits on the
    ground without sinking or floating, rolls a little, not hovering in the
    hand, preview icon framed, size next to the character.
14. `docs/features.md` row, `tools\cli validate`, ship. The PR lists the
    manual check: look at the filmed clip, then pick it up yourself once.

## Verification and failure modes

| Failure | How it shows | Automated check |
|---|---|---|
| White or untextured material | Prefab override over a placeholder bake renders white (AGENTS.md) | Meta: every `SourceMaterial` maps to our `.emat`; no new `DefaultGeneratedMaterial.emat`; `.emat` `BCRMap`/`NMOMap` resolve via `asset_search`; then the world shot |
| Wrong scale | Ball the size of a car, or a marble | Blender bounding box against the spec within 2%; scale shot beside the figure; in-game shot beside the character |
| Inverted normals | Inside-out or black faces | Blender: recalculate outside, then count faces whose normal points toward the centre (expect 0); EBT model QA |
| Missing or invalid collider | Falls through the floor, cannot be picked up | Log `Invalid collider`; meta has `GeometryParam USP_Main` with `ItemFireView`; in game the dropped ball stays above ground |
| Pivot off | Held off to the side, spins oddly, drops offset | Blender: origin at bounding-box centre; film the hold |
| Normal map green flipped | Lighting looks embossed the wrong way | Pack step flips G; one close shot under noon light |
| Texture too big | Repo bloat | Validation rule: texture source above 1024² fails for items |
| Script VME in the CLI plugin | Workbench hangs on a dialog | Watchdog timeout; plugin writes a result file |

Textures can only be judged on a rendering instance; the dedicated server
and `tools\cli test` have no pixels (agent-box README). Workbench play
screenshots omit UI, so the inventory preview must be shot from a game client.

## Repo conventions

- **Layout.** `Assets/Items/<Category>/<Name>/<Name>.fbx` plus `.xob`, with
  `Data/` for `.emat` and textures, as Apple, Coal, and WaterBottle do (local).
  Prefab in `Prefabs/Items/<Category>/<Name>.et`.
- **GUIDs.** Workbench writes every `.meta`. Never write one by hand or copy
  one from another asset. `tools\cli validate` catches duplicates.
- **Binary blobs.** `.fbx`, `.tif`, `.xob`, `.edds` are already `-text` in
  `.gitattributes`. LFS is not enabled. `.txo` is not listed but is committed
  for all 30 models; add it to `.gitattributes` when the pipeline lands.
- **Sources.** Commit the generator script and the `.fbx` (Workbench needs
  the FBX to reimport, and 27 FBX are already tracked). Do not commit
  `.blend` files: the script regenerates them, and they are large binary
  blobs. A sourced CC0 or CC-BY asset keeps its original download URL and
  licence in `tools/assets/<name>/SOURCE.md` and in the credits.

## Paid prompt-to-3D services

Researched 2026-11-09 for props that are too detailed to script (payphone,
medkit, vending machine). No account was created and nothing was paid.
Prices and terms change often, so recheck them on the day you sign up.
Quality notes are reviews and vendor claims, not our own tests; the trial
below is how we get our own. This section is research, not legal advice.

### Who is still in business

| Service | State on 2026-11-09 | Source |
|---|---|---|
| CSM (csm.ai) | Google acquired the company around 2026-01-24 and the team moved to DeepMind [29]. Whether the Cube service still runs is unconfirmed; do not build on it | [29] |
| Luma Genie | Luma sunset Flythroughs on 2026-01-01 [30]. No source confirms Genie itself shut down, and no Genie API was found; status unclear, so skip it | [30] |
| Kaedim | Human-in-the-loop artists, quote-only pricing, no self-serve API | [22] |
| Sloyd | Parametric templates; redistribution allowed on Pro only | [22] |
| Stability AI 3D (Stable Fast 3D, SPAR3D) | Cheap API; the Community Licence carries a USD 1M revenue cap and a "Powered by Stability AI" attribution rule that needs a legal read before shipping | [22] |
| 3D AI Studio | Reseller of Meshy, Tripo, Rodin, and Hunyuan behind one API, about USD 0.34 a model; terms allow redistribution but it trains on outputs | [22] |
| Meshy, Tripo, Rodin, Hunyuan (Tencent Cloud) | Active; compared below | |

### The four serious candidates

| | Meshy | Tripo | Rodin (Hyper3D) | Hunyuan 3D on Tencent Cloud |
|---|---|---|---|---|
| Hard-surface quality | Best-rated API in reviews; good silhouettes, soft edges on small detail (vendor and review claims) | Strong geometry, runner-up to Meshy in reviews | Highest-detail outputs in reviews; quad presets aimed at production | Hunyuan 3.x quality is competitive in public comparisons (unverified for props) |
| Topology | `topology: quad` or `triangle` with remesh; `target_polycount` 100 to 300k [16] | `face_limit` (default 10k), `smart_low_poly` 500 to 20k, quad up to 10k [17] | Quad presets 4k, 8k, 18k, 50k faces [18] | `FaceCount` option; Smart Topology post-process (50 credits) [23] |
| UVs and PBR | Auto UV atlas; `enable_pbr` adds metallic, roughness, normal [16] | `pbr` flag; texture task 10 to 30 credits [24] | PBR maps (base colour, roughness, metallic, normal) per vendor | `EnablePBR` (+10 credits); UV Unfold post-process (10) [23] |
| Image and multi-view input | Image-to-3D and multi-image | Image and multi-view (20 to 30 credits) [24] | Image and multi-image | `MultiViewImages` option (+10) [23] |
| API and unattended use | Bearer key, async tasks; best documented. Free plan has no API [16] | Bearer key, webhook callbacks, per-task concurrency quotas [24] | API on the Business plan only (USD 120/month, 120 RPM) [18] | Tencent Cloud API key (signed requests), Frankfurt endpoint `hunyuan.eu-frankfurt.tencentcloudapi.com` [25] |
| Price per generation | About 35 to 45 credits with refine and remesh; Pro about USD 20 for 1,000 credits, so about USD 0.70 to 0.90 [16] | USD 0.01 a credit; generation 10 to 30, texture 10 to 30, retopology 10 to 40, so about USD 0.40 to 1.00 [24] | Creator USD 30 (about 60 models, no API); Business USD 120 (about 416 models) [18] | USD 0.02 a credit postpaid, 0.015 prepaid; Normal 25 + PBR 10 = about USD 0.50 to 0.70; Smart Topology adds USD 0.75 to 1.00 [23] |
| Output ownership (paid) | "Private and customer owned" on Pro and above [16] | Paid users hold all rights (ToS 5.2.2) [17] | Plan-dependent; ToS section 5: "we will not limit your use of such Output, subject to any restrictions" [26] | Tencent assigns all its rights in Output to the customer (TOS 6.3(c)) [27] |
| Attribution or labels | None on paid plans | None on paid plans | None stated | **Published Output must be labelled AI-generated** (6.4(f)); Tencent may add watermarks (6.3(d)) [27] |
| Revenue cap | None found | None found | None found | None in the cloud TOS (the 1M MAU clause is in the open-weights licence only) |
| EU/UK | Available | Available | Available; Delaware law | **Available.** The cloud TOS excludes only sanctioned territories (11.1(b)); the EU/UK exclusion is in the open-weights licence, not the paid API [14][27] |
| Data retention and training | API assets kept at most 3 days on most tiers [31]; reported to train on data unless you opt out (unverified) [22] | Paid inputs and outputs not used for training [17] | May use prompts and outputs to "improve our services" (section 5) [26] | Generated content kept up to 7 days [28]; Tencent keeps a perpetual right to use Content to improve the Services (6.3(c)) [27] |
| Free tier for a trial | 100 credits, web only, outputs CC BY and public [16] | Free credits, outputs owned by Tripo [17] | 10 private assets, credits USD 1.50 each [18] | **200 credits once, through the API** (about 5 to 8 models) [23] |

**What this means for redistribution.** On the paid tiers of Meshy, Tripo,
and Tencent, the provider gives us whatever rights exist in the output, so
the files can sit in the MIT repo and in the APL Workshop build. Two caveats
apply to all of them. Pure AI output may not be copyrightable, so our MIT or
APL licence may not bind others who copy those meshes; that does not stop us
shipping them. And every provider forbids using outputs to build a competing
AI model, which does not affect a game mod. Free-tier outputs are out: Meshy
free is CC BY and public, Tripo free belongs to Tripo. Record the provider,
plan, task id, prompt, and date for every shipped model in
`tools/assets/<name>/SOURCE.md`, and mark it AI-generated in the credits.
Tencent requires that label; it is good practice for the others.

### How a paid service slots into the pipeline

It replaces stages 2 to 4 and keeps everything after them.

1. **Spec** (stage 1) adds a reference image when we have one and a
   triangle budget.
2. **Generate.** `tools/assets/providers/<service>.mjs` submits the job with
   the API key from the environment, polls or takes the webhook, and
   downloads GLB plus PBR maps into `tmp/asset-<name>/raw/` before the
   provider deletes them (Meshy 3 days, Tencent 7). The raw download never
   enters git.
3. **Clean in headless Blender.** Import GLB, scale to the spec's real
   dimensions, set the origin, delete loose parts and interior faces,
   recalculate normals. If the provider's quad remesh hits the budget, keep
   it; otherwise decimate or remesh in Blender. Re-unwrap when the provider's
   atlas is fragmented.
4. **Bake high to low.** Cycles "Selected to Active" from the raw mesh onto
   the cleaned one: base colour, roughness, metallic (via Emit), normal, AO.
   Pack to `_BCR`/`_NMO` as in [Texturing](#texturing). Rebaking also fixes
   AI textures with baked-in lighting only partly; the trial measures it.
5. **Stages 6 to 17 unchanged.** Colliders, renders, EBT QA, import, prefab,
   agent-box film.

### Cost for about 50 props

Assume three attempts per prop before one passes, so about 150 generations
with PBR and remesh.

| Service | Cash for 150 generations | Notes |
|---|---|---|
| 3D AI Studio | about USD 50 | Cheapest; trains on outputs |
| Tripo | about USD 60 to 150 | Pay as you go |
| Tencent Hunyuan | about USD 75 to 105, or 190 to 260 with Smart Topology | Prepaid packs cheaper |
| Meshy | about USD 105 to 135 | Pro covers about 25 generations a month, so a higher tier or credit packs for a burst |
| Rodin | USD 120 for one Business month | Only plan with an API |

The cash is small next to the cleanup. At 30 to 60 agent-minutes per prop
for steps 3 to 5 and a short maintainer look at each film, 50 props cost
roughly 25 to 50 agent-hours. That number decides whether the service pays
off, which is why the trial times it.

### Trial plan

**First: Meshy** (one month of Pro, about USD 20, paid by the maintainer
when ready). It has the most complete API for unattended use: quad remesh,
polycount target, PBR flag, image input, and customer-owned output on
paid plans. Check its training opt-out on day one.

One Tencent clause needs a read before Hunyuan goes past the trial. TOS
5.4(e) prohibits "automated systems to extract data or Output from the
Services". The documented API is presumably exempt, but confirm with
Tencent support before an agent drives it in bulk [27].

**Free comparisons, run before paying.** Tencent Hunyuan's 200 API credits
(the only free tier that exercises the real API, from the Frankfurt
endpoint) and Tripo's free credits. Free outputs are for judging only and
are deleted after the trial.

**Props and budgets.**

| Prop | Real size | LOD0 triangles | What it tests |
|---|---|---|---|
| Bowling ball | 0.216 m diameter, 7 kg | 2,000 | Baseline against the procedural ball; finger holes; glossy material |
| Payphone | about 0.25 x 0.20 x 0.55 m housing (wall unit) | 8,000 | Hard surfaces, keypad, cord, printed signage |
| Medkit | about 0.30 x 0.20 x 0.10 m case | 3,000 | Clean box with rounded edges, a cross decal, latches |

For each prop: one text prompt and one image prompt, three attempts each,
the same prompts on every service.

**Pass (all must hold):**

- Silhouette reads as the object in an agent-box world shot at 2 m, judged
  by the maintainer from the film.
- After cleanup: dimensions within 5% of the spec, LOD0 within the
  triangle budget, no non-manifold edges, no loose interior parts,
  zero inverted normals.
- The rebaked `_BCR` has no baked-in light or shadow (no strong
  one-directional brightness gradient on a flat-coloured part) and no
  visible seams at 1 m.
- Text is legible or absent. Garbled signage on the payphone fails unless
  it can be replaced with a decal.
- Cleanup and bake take at most 60 agent-minutes per prop, measured.
- Import passes stage 13 with no placeholder material and a working
  collider.
- The paid plan's terms on the day still say customer-owned output with no
  attribution beyond our own credits line.

**Fail fast:** a service that fails the bowling ball against the
procedural baseline or garbles the medkit's box is dropped. If no service
passes the payphone, paid generation stays a blockout tool and detailed
props come from CC0 sources or a commissioned artist.

**Output of the trial:** a table per service and prop with the metrics
above, the cleanup minutes, the cash spent, and the filmed clips, appended
to this doc and linked from epic #339.

## Risks and unknowns

1. **First FBX registration needs a click** (spike). Next spike: call
   `ExportFBXResource` or `wb_resources register` against a GUI Workbench on
   the `LF-Agents` desktop and see whether the dialog appears there. If it
   does, options are UI automation on that desktop, or a human click per new
   model (once per asset, about 5 s).
2. **Blender is not installed**, and EBT is pinned to 4.5 exactly.
3. **EBT's headless behaviour is untested.** It installs Python packages
   (PySide6, vhacdx) on first load and many operators require Workbench.
4. **Hand pose.** `HandAdditive*` is tuned by eye; there is no metric check
   yet beyond filming.
5. **Inventory size units** (`ItemVolume`, `m_Size`) for a large item are
   unverified.
6. **Cycles CPU bake time** for 512² to 1024² maps is unmeasured (expected
   seconds to a minute).
7. **Wiki pages unreadable from tools.** Some convention details (texel
   density, forward axis wording) rest on excerpts.

## Effort for a first implementation

| Piece | Estimate |
|---|---|
| Install Blender 4.5 LTS and EBT; a `tools\cli asset doctor` check | 0.5 day |
| Spike 2: FBX registration through a GUI Workbench | 0.5 day |
| `LF_` Workbench CLI plugin (register, meta edit, emat create, rebuild, result file, watchdog) and `tools\cli asset import` | 1.5 days |
| Blender library: spec reader, bake and pack, collider helper, render and checks | 2 days |
| Bowling ball script, prefab, fixture, flow, film | 1 day |
| Validation rules (texture size, material mapping, collider present) | 0.5 day |
| **Total** | **about 6 agent-days**, plus one maintainer look at the film |

A second simple prop after that should take an agent under half a day.

## Later: animations, weapons, vehicles

- **Animations.** EBT exports `.anm` through Workbench's net API
  (`AnimExportProfiles`, `AnimExportTrackExport`; local) and has skeleton
  tools. Keep Blender at the EBT-pinned version so animation export works
  from the same install. Animation needs authored motion; procedural scripts
  do not cover it. ACE-Anvil animations are GPL (`docs/ecosystem.md`).
- **Melee weapons and a taser.** Same mesh path, plus sockets and memory
  points from empties (`ExportSceneHierarchy`), and the `Weapon` layer preset
  for equip actions [6]. Base the prefab on a vanilla weapon or gadget so the
  hand IK comes from its anim graph. The taser needs an emissive map and
  later a projectile; keep `EmissiveMap` support in the pack step now.
- **Vehicles.** Up to 8 LODs, many colliders, sockets for wheels and doors,
  `UTM_` for some parts. Procedural scripts do not scale to that; plan on a
  CC0 or commissioned model and use this pipeline for import, validation,
  and filming. The decision that helps now: keep the import, meta edit, and
  verification steps separate from mesh creation, so they work for any FBX.

## Sources

Local evidence (read 2026-11-08 on this PC):
`Arma Reforger Tools/Blender/scripts/addons/EnfusionBlenderTools/` (`build_ver.json`,
`LICENSE.txt`, `ebt/ebt_configuration.py`, `fbx/operators.py`,
`core/fbx/fbx_io.py`, `core/resource_info.py`, `extern/enfusion/workbench.py`,
`bake/callbacks.py`); packed game scripts `scripts/WorkbenchGameCommon/ResourceInfo.c`,
`ValidateFBXPlugin.c`, `scripts/WorkbenchCommon/AddonBuildInfoTool.c`;
prefabs `Prefabs/Items/Core/Item_Base.et`, `Canteen_base.et`,
`Prefabs/Weapons/Grenades/Grenade_M67.et`, `Prefabs/Items/Equipment/Radios/Radio_ANPRC68.et`,
`Prefabs/Items/Equipment/Canteens/Canteen_US_01.et`; `api_search` for
`ResourceManager`, `MetaFile`, `Workbench`; `wiki_read` for Workbench Plugin,
Startup Parameters, FBX Import, Prop Creation.

1. Bohemia, Arma Reforger Samples, `box_1m.xob.meta`: https://github.com/BohemiaInteractive/Arma-Reforger-Samples/blob/main/SampleMod_Main/Assets/TestSite/box_1m.xob.meta
2. Bohemia, Script Diff, `ResourceManager.c`: https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/main/scripts/GameLib/generated/WorkbenchAPI/Modules/ResourceManager.c
3. Bohemia, Script Diff, `TextureImportTool.c`: https://github.com/BohemiaInteractive/Arma-Reforger-Script-Diff/blob/main/scripts/WorkbenchGameCommon/TextureImportTool.c
4. Bohemia wiki, FBX Import: https://community.bistudio.com/wiki/Arma_Reforger:FBX_Import
5. Bohemia wiki, Textures: https://community.bistudio.com/wiki/Arma_Reforger:Textures, and Modding Boot Camp 3: https://reforger.armaplatform.com/news/modding-boot-camp-3-modeling
6. Bohemia wiki, Weapon Creation, Asset Preparation: https://community.bistudio.com/wiki/Arma_Reforger:Weapon_Creation/Asset_Preparation; Prop Creation: https://community.bistudio.com/wiki/Arma_Reforger:Prop_Creation; Enfusion Blender Tools: https://community.bistudio.com/wiki/Arma_Reforger:Enfusion_Blender_Tools
7. ahujasid/blender-mcp: https://github.com/ahujasid/blender-mcp
8. Blender Lab MCP server: https://www.blender.org/lab/mcp-server/
9. Poly Haven API and licence: https://polyhaven.com/llms.txt
10. ambientCG licence: https://docs.ambientcg.com/license/ and API: https://docs.ambientcg.com/api/
11. Sketchfab Download API: https://sketchfab.com/developers/download-api
12. Creative Commons attribution practice: https://wiki.creativecommons.org/wiki/Recommended_practices_for_attribution
13. Kenney licence: https://kenney.nl/support
14. Hunyuan3D-2 licence: https://github.com/Tencent-Hunyuan/Hunyuan3D-2/blob/main/LICENSE; 2.1: https://huggingface.co/tencent/Hunyuan3D-2.1/blob/main/LICENSE
15. TRELLIS.2: https://github.com/microsoft/TRELLIS.2; TRELLIS: https://github.com/microsoft/TRELLIS
16. Meshy pricing: https://www.meshy.ai/pricing; API: https://docs.meshy.ai/en/api/text-to-3d, https://docs.meshy.ai/api/pricing.md
17. Tripo commercial use: https://www.tripo3d.ai/help/privacy-policy/how-to-use-tripo-models-commercially; API: https://developers.tripo3d.com/en/models/v3-1
18. Hyper3D Rodin pricing: https://hyper3d.ai/pricing?lang=en
19. Blender manual, Cycles baking: https://docs.blender.org/manual/en/latest/render/cycles/baking.html
20. StableMaterials: https://huggingface.co/gvecchio/StableMaterials
21. EveronLife Axe meta (community example of `UTM_` + `ItemFireView`): https://github.com/EveronLife/EveronLife/blob/main/src/Assets/Tools/Axe/Axe.xob.meta
22. Vendor survey of 2026-11-09 by a research helper whose page URLs were not kept. Treat these rows as unverified until rechecked: Sloyd https://www.sloyd.ai/pricing, Kaedim https://www.kaedim3d.com, Stability AI https://platform.stability.ai/pricing and https://stability.ai/community-license-agreement, 3D AI Studio https://www.3daistudio.com, Meshy https://www.meshy.ai/terms-of-use
23. Tencent Cloud, HY 3D Global billing: https://www.tencentcloud.com/document/product/1284/75281
24. Tripo API pricing and auth: https://developers.tripo3d.ai/
25. Tencent Cloud Hunyuan API endpoints (Frankfurt): https://www.tencentcloud.com/document/api/1284/75533
26. Hyper3D terms of service: https://hyper3d.ai/legal/terms
27. Tencent Cloud HY 3D Global API Terms of Service (updated 2026-03-10): https://intl.cloud.tencent.com/document/product/301/78149
28. Tencent Cloud HY 3D data processing terms (7-day retention): https://www-sg.tencentcloud.com/document/product/1284/75301
29. Gunderson Dettmer, Common Sense Machines acquired by Google: https://www.gunder.com/en/news-insights/client-news/gunderson-dettmer-represents-common-sense-machines-in-acquisition-by-google
30. Radiance Fields, Luma AI sunsets Flythroughs: https://radiancefields.com/tags/luma-ai
31. Meshy API asset retention: https://docs.meshy.ai/api/asset-retention
