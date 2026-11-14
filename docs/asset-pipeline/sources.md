# Sources: look for the asset before generating it

The `source` stage runs before `concept` and `generate`. A real model is better than a generated one,
so the stage tries four rungs in order and shows every candidate on one contact sheet:

| Rung | What | Licence we ship | Cost | What happens next |
|---|---|---|---|---|
| 1 Vanilla | The Arma Reforger base game: props, furniture, clutter, interiors, vehicle parts | Bohemia Interactive, in the engine | 0 | No import. The prefab points at the vanilla `.xob` or inherits the vanilla prefab (`game_duplicate`); we add states and `LF_` components |
| 2 Open source | Poly Haven (CC0), Smithsonian Open Access (CC0), Sketchfab (CC0 or CC-BY) | CC0, or CC-BY with a credit row | 0 | The full pipeline after the source: `clean` (real size, LODs, material classes), `import` |
| 2b Free download | Whatever [3d-asset-server](#3d-asset-server-search-and-the-paid-rung) finds with a direct file: 3dassets.dev, Kenney, Quaternius, BlenderKit and more | CC0, CC-BY (credit row) | 0 | The same: `source.provider "download"`, then `clean`, `import` |
| 2c Paid | CGTrader (Royalty Free), also Fab and TurboSquid as links. A human buys; the pipeline never does | Royalty Free, with the extraction caveat below | the model's price | The human drops the download in `tmp/assets/<slug>/purchased/`; `source.provider "purchased"`, then `clean`, `import` |
| 3 Generate | `concept` then Meshy | our Meshy plan | 70 credits | the rest of the pipeline |

Textures come from ambientCG (CC0), which the search lists beside the models. **Sounds are not a ladder**: they stay on
ElevenLabs ([sounds.md](sounds.md)). **Animations**: reuse vanilla anims and graphs first (gadget hold and use poses, item
animation attributes), then our own Blender anim stage for object motion ([animation.md](animation.md)). A motion-capture
library (Mixamo, CMU) was deferred: retargeting to the Enfusion skeleton is not worth it yet.

Nothing skips a quality check because it came from a library: the clean stage's budget, size and UV checks, the Workbench
import checks and the before and after `look` pictures all apply.

## Commands

```
tools\cli asset-search install                       clone 3d-asset-server at the pinned commit into tools/mcp/3d-asset-server/ and build it
tools\cli asset-search "<words>" [--slug <spec>]       search the sites, gate the licences, score against the spec, write tmp/assets/<slug>/shortlist.md
```

```
tools\cli asset source search "<words>" [--size <metres>]    all rungs for a few words, plus a contact sheet
tools\cli asset source search <slug>                         the same for a spec's name, concept and "search" field
tools\cli asset source pick <slug> <n | id | Assets/...xob>  writes spec.source (a sheet number, a Poly Haven id, a vanilla path)
tools\cli asset source info <id>                             one Poly Haven asset: size, files, licence
```

In a spec:

```json
"source": { "provider": "polyhaven", "id": "cement_bag", "resolution": "2k" }
"source": { "provider": "vanilla", "path": "Assets/Props/Furniture/Safe_01/Safe_01.xob", "prefab": "Prefabs/Props/Furniture/Safe_01.et" }
"source": "search"
"search": "cement bag"            optional extra words; "search_vanilla": ["safe", "strongbox"] extra base-game queries
```

`"source": "search"` makes the stage print the ladder and wait for a pick, like the concept sheet. A spec with a Poly Haven or
vanilla source needs no `prompt` and no credits; the stages it makes pointless (`concept`, `generate`, `remesh`; for vanilla also
`clean` and `import`) are skipped and marked so. A spec that runs `concept` gets the Poly Haven and library matches as extra
cells on the concept sheet, numbered after the ten concepts, so an existing model can win: `asset pick <slug> 12`.

Set `materials.default` to `scanned` for a library model: Poly Haven's roughness and metallic are authored values and are kept.
`budget.lod0_tris` is the model's own triangle count when it is under the kind's floor; the cap and the LOD steps still apply.
Pick `size_m` from the model's native size (`source` prints it) unless the game needs another.

## What the stage writes

- `tmp/assets/<slug>/polyhaven/` the downloaded glTF, `.bin` and maps (not committed; checksums verified against the API).
- `tmp/assets/<slug>/raw/` `model.glb` and `tex_base_color|normal|roughness|metallic.png`, the files Meshy would have left.
  `blender/polyhaven_prep.py` splits Poly Haven's packed `arm` map (R ambient occlusion, G roughness, B metallic).
- `tools/assets/<slug>/source.json` the committed record: provider, id, URL, author, licence, resolution, file hash, date.
- A row in `docs/credits.md` under "Poly Haven models". CC0 needs no credit; we record it anyway.

A model with more than one texture set, or an alpha-cut or glass material, is refused with the reason: the clean stage packs one
opaque atlas.

## 3d-asset-server: search and the paid rung

[arielshad/3d-asset-server](https://github.com/arielshad/3d-asset-server) (Apache-2.0, TypeScript) searches about 27 sites behind one
result shape: licence, free or paid, formats, polygon count, preview and whether a direct file exists. `asset-search install` clones
it at a pinned commit (`PIN` in `tools/assets/pipeline/assetserver.mjs`) into the gitignored `tools/mcp/3d-asset-server/` and builds
the server only (`npm ci --ignore-scripts`, `tsc`; no web build). `source search` and `asset-search` load its library
(`dist/index.js`) in our own process.

**What it does on this PC** (source read at the pin, 2026-11-14): it sends HTTP GETs to the provider sites it searches (with its own
User-Agent) and writes files only when a download is asked for, under the folder we give it (`tmp/assets/<slug>/download/`; zips are
extracted with entry-count, size and zip-slip caps, and every download hop re-checks the host is public). It spawns no process and
runs no package script. **Telemetry:** its `cli.js` (`serve`, `mcp`) sends anonymous usage events to `stats.shep.bot` unless
`ASSET_SERVER_TELEMETRY=0` or `DO_NOT_TRACK=1`; we call the library, which sends none, and the MCP snippet below sets the variable. With
`METRICS_PORT` set it would also fetch network ranges (we never set it). Bump the pin only after reading the diff.

**Which providers.** `api` and `link` providers only. `scrape` providers parse HTML and their robots.txt was not audited, so they are
opt-in (`--scrape`). CGTrader is never queried by us even though the tool can: its provider requests `/search?...`, and
`cgtrader.com/robots.txt` says `Disallow: /search*` for every agent. A CGTrader, Fab, Poliigon and TurboSquid search is a link for the
human, printed on the shortlist. Poly Haven stays on our own adapter (`source.mjs`): it checks the API's md5s, splits the packed
`arm` map and filters by real size, which the tool's Poly Haven provider does not; the tool is better for everything else it
reaches. The tool's Sketchfab and ambientCG results duplicate our adapters and are shown beside them.

**Licence gate** (`licenceGate`):

| Licence | Verdict | What happens |
|---|---|---|
| CC0, public domain | ok | no credit needed, a row in `docs/credits.md` anyway |
| CC-BY | credit | author and URL in `docs/credits.md` before it ships |
| Royalty Free (CGTrader) | paid | a human buys it; `source.risk_accepted: true` is their yes to the caveat |
| CC-BY-SA, no-redistribution, custom, unrecognised | review | a human reads the terms, then `source.licence_checked: true`; the fit score is capped at 50 |
| Editorial, non-commercial, no-derivatives, none stated | refuse | never a candidate |

**CGTrader terms** (<https://www.cgtrader.com/pages/terms-and-conditions>, read 2026-11-14). Royalty Free, section 21A.2: the licence "is strictly
limited to Incorporated Product", defined as "Product that cannot be extracted from an application or other product" and "used as a
stand-alone object without the use of reverse engineering tools or techniques"; games are covered "if the Product is contained inside a
proprietary format and displays inside the game during play", "but not for users to re-package as goods distributed or sold inside a virtual
world". 21A.3: "you must take all commercially reasonable measures to prevent the end user from gaining access to the Product". 21A.1 and
21A.6 forbid selling or giving the Product on "in the form it is downloaded from the Site". Editorial, section 22.2: such products may be used
only "for legitimate, editorial purposes on some issue of journalistic, editorial, cultural or otherwise newsworthy value", and the Royalty Free
restrictions still apply. **Consequence for a mod:** a compiled `.xob` in a published addon can be unpacked by a player, which is the grey
area of "cannot be extracted". We ship only compiled assets, never the purchased files (`tmp/` is gitignored), but the risk is the owner's:
the shortlist repeats this, and a purchased source does not validate without `risk_accepted: true`. For a hero prop where the risk matters,
ask the seller in writing first.

**Purchase is human-only.** The pipeline stops at `tmp/assets/<slug>/shortlist.md` (link, price, licence, formats, polygons, preview, fit
score and why). It never logs in, never opens a cart and never enters payment details. After the human buys:

```json
"source": { "provider": "purchased", "url": "<store page>", "licence": "Royalty Free", "author": "<seller>", "risk_accepted": true }
```

unzip the download into `tmp/assets/<slug>/purchased/` and run `asset run <slug>`: `blender/incoming_prep.py` imports the glTF, FBX, OBJ
or .blend, follows the Principled BSDF inputs to find the base colour, normal, roughness and metallic maps (a roughness or metallic read
through a Separate node takes its channel) and writes the same `raw/` files as for Poly Haven; `clean` and `import` follow. A free one is
`"source": { "provider": "download", "id": "<provider>:<id>" }` (`asset source pick <slug> <n>` writes it) and `asset run` fetches it first. Both
write `tools/assets/<slug>/source.json` and a row under "Library and purchased models" in `docs/credits.md`. A model with several base colour
textures is refused, as for Poly Haven.

**For agents.** `tools\cli asset-search "<words>" --json` is the route (it needs no MCP restart, like `tools\cli call`). The tool also runs as
an MCP server; to add it to an agent, in `opencode.json`:
`"3d-asset-server": { "type": "local", "command": ["node", "tools/mcp/3d-asset-server/dist/cli.js", "mcp"], "environment": { "ASSET_SERVER_TELEMETRY": "0", "ASSET_DOWNLOAD_DIR": "tmp/downloads" } }`.
Its `download_asset` writes where the agent asks, so prefer the pipeline's own download, which gates the licence first.

## Poly Haven

- **API:** `https://api.polyhaven.com`: `/assets?t=models` (the catalogue, one call, cached a day), `/info/<id>`,
  `/files/<id>`. Terms: <https://github.com/Poly-Haven/Public-API/blob/main/ToS.md> (read 2026-11-14).
- **Rules:** free for any use including commercial; no key; no stated rate limit, so we make one catalogue call a day and fetch
  files only for the asset picked. Every call sends a user agent that names the software (`LifeFrameworkAssetPipeline/1.0`, no
  personal address). A tool that surfaces Poly Haven content on screen must say where it comes from: the printed lists and the
  contact sheet say "Poly Haven". Scraping the website is forbidden; the API is the sanctioned route.
- **Licence:** every model is CC0 1.0 (<https://polyhaven.com/license>): commercial use and redistribution need no credit.
- **Formats:** glTF (with `.bin`), FBX, Blender and USD; maps as JPG, PNG or EXR at 1k, 2k, 4k and often 8k. The glTF carries
  `diff` (base colour), `nor_gl` (OpenGL normal) and `arm`. We take glTF at 2k by default, scaled to the budget by the clean stage.
  Dimensions in `/info` are millimetres in Blender axes (x, y, z up).
- **Not used:** HDRIs and Poly Haven's own texture sets could feed a material later; ambientCG already does.

## Other adapters

`tools/assets/pipeline/sources.mjs` holds one small adapter per provider; add a provider by adding an entry. A licence that is
not CC0 or CC-BY (NC, ND, share-alike, unclear) never becomes a candidate (`licenceVerdict`). A CC-BY model needs its author and
URL written to `docs/credits.md` before it ships.

| Provider | Licence filter | Key | State |
|---|---|---|---|
| Smithsonian Open Access 3D | CC0 only | api.data.gov key in `SMITHSONIAN_API_KEY` or `~/.life-framework/smithsonian-key`; `DEMO_KEY` works at a low rate | search only |
| Sketchfab | `license=cc0` and `license=by`, downloadable | search is open; a download needs a token in `SKETCHFAB_TOKEN` or `~/.life-framework/sketchfab-token` (sketchfab.com/settings#password) | search only; fetch the glTF by hand into `raw/` until a token exists |
| ambientCG | CC0 | none | textures; the Blender-source path already downloads them (`spec.source.textures`) |

Checked and not added: BBC Sound Effects (non-commercial), and any library without a plain machine-readable licence.

## Catalogue fit for the 26 stand-in audit rows

Searched 2026-11-14: about ten base-game queries per row (synonyms, parts of a larger prop) and the 522 Poly Haven models. "Good" means
the object is right for 1989 Everon; "part" means a piece or a near miss. Scale is the Poly Haven model's real size in cm (x y z).
Sketchfab and Smithsonian were searched only for the proof asset.

| # | Row | 1 Vanilla | 2 Open source (Poly Haven) | Verdict |
|---|---|---|---|---|
| 1 | Robbery drill | `BenchDrill_01` is the current wrong object | `Drill_01` 2.9k tris, 18x5x19: a hand power drill, good | good (already generated; Meshy stays) |
| 2 | Bank vault door | `Safe_01`: a safe, not a door | none (`large_iron_gate` is a gate) | part; kit built in Blender |
| 3 | Money bag | `SackUniversal_01` is the current | none (`compost_bag_02`, `trashbag` are wrong) | none; generated |
| 4 | Moonshine still | `Boiler_01`, `BoilerTank_02/03` (current) | `brass_pot_01` 3.8k, 30x30x29: the pot only | part |
| 5 | Apple juicer | `CoffeeGrinderShop_01` (current); `Lathe_01`, `Anvil_01` wrong | none | none; generated |
| 6 | Metal ingots | `Brick_dbr_02` (current) | none | none; Blender |
| 7 | Cash van strongbox | `Safe_01` fits well | `metal_tool_chest` 14k, 69x41x65, rigged lid | good (vanilla wins) |
| 8 | Lockpick | none | none (`screwdrivers_02` is a screwdriver) | none; generated |
| 9 | Evidence marker | none (`ConeTraffic_01` is a cone) | none | none; generated |
| 10 | Pager | none (`PhoneTable_01` is a desk phone) | none | none; generated |
| 11 | Evidence bag | none | none | none |
| 12 | Taser cartridge | none (vanilla cartridges are bullets) | none | none |
| 13 | Police scanner | `Radio_R107M_01`, `RadioStation_R123M_01` (tried: reads as a grey box) | `vintage_radio_transceiver` 42k, 62x42x43 | good (Poly Haven; heavy, needs a 4k-6k budget) |
| 14 | Parcel | `CardboardBox_01` to `_09` (current is `_03`) | `cardboard_box_01` 17k, 39x52x34 | part (a plain box, not a string-tied parcel) |
| 15 | Apple juice bottle | `SodaBottle_01` glass bottle | none | part (vanilla) |
| 16 | Cabbage head | `CabbageCrop_01` is the whole plant | none | none |
| 17 | Hospital stretcher | none | `wheelchair_01` 40k, 82x109x110; `vintage_crutches_01` | part |
| 18 | Smelter furnace | `StoveIndustrial_01`, `OvenIndustrial_01`, `Stove_02` | `barrel_stove` 11k, 60x60x86 | good (vanilla wins) |
| 19 | Engine block, stripped shell | `EngineCrane_01`, `TirePile_01`, `Tire_*`, `Wheel_*` | `old_tyre`, `rusted_wheel_rim_01/02`, `portable_generator` | good for yard dressing; no engine block |
| 20 | Hideout press | `Workbench_Vice_01`, `Lathe_01`, `Anvil_01` | `bench_vice_01`, `old_drill_press` | part |
| 21 | Dispatch console | `SwitchboardPhone_01` | `vintage_radio_transceiver`, `television_02` | good (vanilla wins) |
| 22 | Nuggets | `c_pebbles_01` is a pebble | `rock_09`, `stone_01` (rock, 7-15 cm) | part; Blender nugget stays |
| 23 | Cement and gravel bag | `Sandbag_01_*` (current) | **`cement_bag` 844 tris, 46x70x18: exact** (Sketchfab "Cement Bag", CC-BY, also listed) | good, **proven** (below); gravel bag none |
| 24 | Fence keycard | none | none | none |
| 25 | Bank teller cage | `CounterPostOffice_01_window`, `Door_ShopModern_E_bars_*` | none | part (vanilla) |
| 26 | Armory locker | `Wardrobe_01/02` | `worn_metal_rack`, `steel_frame_shelves_01`, `drawer_cabinet` | part |

**Count: 7 good (1, 7, 13, 18, 19, 21, 23), 9 part (2, 4, 14, 15, 17, 20, 22, 25, 26), 10 none (3, 5, 6, 8, 9, 10, 11, 12, 16, 24).**
Vanilla is the best hit for rows 7, 18 and 21; Poly Haven for 1, 13 and 23; the other rows still need the generator.
Rows already generated (1, 2, 3, 5, 6, 8, 9, 10, 22) are not redone. Check every "good" visually before wiring it: the
`RadioStation_R123M_01` swap for row 21 was tried in the dispatch console and rejected on sight.

## Rows tried in batch 2 (2026-11-14), judged in a `look`

| Row | Tried | Result |
|---|---|---|
| 1 Drill | nothing | Already the generated `Drill.xob` (`LF_Drill.et`); the table's "good" was for a hand drill we already have |
| 7 Strongbox | `Safe_01` | Not needed: `LF_CashVanBox.et` already carries the generated `CashStrongbox.xob` (f9e9c57a), better than a vanilla safe |
| 13 Scanner | Poly Haven `vintage_radio_transceiver` | Rejected. Two texture sets (the stage refuses it); with the accessories set cut away the body is an olive military tank radio with heat-sink fins and no control panel: the same "army radio" read the audit complains about |
| 18 Furnace | `StoveIndustrial_01`, `Stove_01`, `OvenIndustrial_01`, Poly Haven `barrel_stove` | Rejected. All three vanilla stoves are white domestic kitchen ranges or a chest of drawers; `barrel_stove` imported cleanly (9,216 tris) but is a plain rusted oil drum with no door or chimney. The vanilla boiler stays |
| 21 Dispatch console | `SwitchboardPhone_01` | Rejected. A 2 m blue wall cabinet; at the 0.5 scale limit it still towers over the desk |
| 23 Cement bag | Poly Haven `cement_bag` | Done, then retextured (below) |

## The proof: cement bag (row 23)

`tools/assets/cement-bag/spec.json` runs `source` (Poly Haven `cement_bag`, 2k), `clean` (844 / 506 / 252 triangles, 0.70 x 0.18 x 0.46 m,
one 1024 px atlas), `import` (Workbench, checks green) with no prompt and no credit spent. `Prefabs/Items/Resources/CementBag.et` keeps
its GUID and every component; only the `MeshObject` changed, from the vanilla sandbag. The two bags on the cement stand
(`CementChain.layer`) were re-seated for the new size. Where to see it: [playtest.md](../playtest.md), "Cement bag".

### Retexturing a library model for the period

Poly Haven's bag carried a modern red "CEMENT / HIGH STRENGTH / CEM 42.5 R" print, a company logo and a barcode. A library model is checked
for era like any other: `tools/assets/<slug>/base_color.png`, when it sits beside the spec, replaces `raw/tex_base_color.png` right after
the source stage, and the clean stage packs it. The cement bag's is made by `tools/assets/cement-bag/make-label.ps1` (kraft paper, one dark
ink, stencil Cyrillic "ЦЕМЕНТ / ПОРТЛАНДЦЕМЕНТ М-400 / 50 кг / ГОСТ 10178-85", the Soviet cement standard; no logo, no icons, no barcode). Rerun
the script, then `toolscli asset run cement-bag --force`.
