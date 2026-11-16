# Agent box

### `LF_AgentHarness` — `Feature/AgentHarness/`
- ✅ (2026-11-11, placement tool) `tools\cli place on|against|ground|arrange` places world props without typed coordinates: mesh bounds from the `.xob`, the surface top, wall face and ground from probe rays on the test server (`LF_PlacementAudit.RayLine`, answered in the same boot as the audit), offline refusals (does not fit, overlap, carriageway, door swing and walk-up, scale, tall simulated body), the layer entry written by the tool, then the audit on what it wrote with a rollback when flagged. `place bounds|probe|verify` inspect. Probed tops are cached in `tools/place/surface-cache.json`. Tests: `place-tool-selftest` (validate), `agent-harness/placement-probe`. Docs: `docs/placement-tool.md`.
- ⚠️ (2026-11-14, look on the pool) `tools\cli look` and `look item` run on the machine with the most Workbench room (this PC, lf-testbox, lf-cloud; the `ci` Workbench ranking in `tools/lib/wb-gate.mjs`) and copy the pictures back to `tmp/look/<area>/`; with no room anywhere they queue instead of failing, and a machine that turns out short (RAM, build lock, a starved Workbench) hands the look to the next. `--local`, `--machine <name>`, `--force` pin it. A local `test` under the 5 GB disk floor goes to the pool machines instead of being refused. Tests: `look-route-selftest`. Docs: `docs/visual-testing.md` "Where it runs", `docs/worktrees.md` "Disk hygiene".
- ⚠️ (2026-11-14, measure) `toolscli measure <prefab|{GUID}path|name|.xob> [--surfaces] [--all] [--json]` prints a model's bounds and every upward face of its colliders (box, convex, trimesh, cylinder) in prefab space, from the `.xob` alone (`tools/lib/xob.mjs`), no game or Workbench: about 2 s cold, under 1 s cached per game build. A bare game path or file name resolves to its `{GUID}path` (`tools/lib/game-prefab.mjs`). Proof: `measure-selftest` (a synthetic model, plus the supermarket fixtures #1143 traced in game: ShelfShop_01 and CounterShop_01 within 1 mm, CoolingBoxShop_01 within 14 mm). Docs: `docs/placement-tool.md`.
- ✅ (2026-11-11, placement audit) `tools\cli audit placement` ranks every badly placed entity in the playtest town: floating, sunk, hanging off a surface edge, clipping walls, interpenetrating other props, in a door's swing, on a carriageway, odd scale, tilt, reach. `LF_PlacementAudit` (traces, oriented boxes, doors) measures on the dedicated test server through `LF_Test_PlacementAuditTown`, so it needs no session (about 10 s); `agent-box/place-audit.mjs` pairs, exempts (`agent-box/place-audit-allow.json`) and ranks. The harness `probe ent` verb uses the same class. Red-proof `agent-harness/placement-audit` (WORLD: grounded, stacked, floating, scaled and half-off-the-table crates). `shots area --eye-only` shoots an area's eye-level and close-up shots without the wide (about 75 s). `docs/placement-audit.md`; a pre-filming step in `docs/marketing/trailer/quality-gate.md`.
- ✅ (2026-11-10, M2 QA) Harness verbs `pager` (opens the carried pager's card, as `LF_ReadPagerAction` does) and `report` (the `/report` chat command's ask). `use LF_PayphoneAction__3` picks the fourth action of that class on the host (a payphone carries one per service preset; the plain name finds the first). `agent-box/qa-m2.mjs` drives the comms loop (payphone, board, take, caller hears back, pager), `/report`, the square payphone, and `normalize` (clock officers and medics out between proofs).
- ✅ (2026-11-10, M2 QA) `LF_LockpickAction.CanBeShownScript` reads `LF_Lockpick.ShowsPick` again, so a locked car lists the pick action (a merge had put the door-only check back). Test: `e2e/lockpick-vehicle`.
- ✅ (2026-11-10, #348) The `state` line also prints `jail=` (seconds left on a sentence), `garage=` (cars stored), `parked=` (plate of the car in the world) and `stash=` (items in `Roadmap_Hideout_Stash`, -1 with none placed). The verbs `stash` (moves an apple from the character into the stash: the member check in `CanStoreItem` decides) and `park` (sets the owner's car down beside `Roadmap_Garage`) stand in for an inventory drag and for driving; with `apples` they pass the harness gate of a `LF_HARNESS` build, so the playtest journey needs no debug menu. `agent-box/journey.mjs` runs the police and org-C steps and a wider restart check on 3 clients.
- ✅ Dev-only. A rendering play session polls `$profile:agent-box/command.txt`.
  `press F` calls `SCR_InteractionHandlerComponent.ActionPerform`. Wheel scroll
  calls `ActionScroll`. `click` invokes the same button invoker or `OnClick` a
  mouse click does. `use` still checks `CanBeShownScript` and
  `CanBePerformedScript` before `PerformAction`. Menu keys are an allowlist
  (`MenuUp`, `MenuDown`, `MenuLeft`, `MenuRight`, `MenuSelect`, `MenuBack`,
  `LF_OpenDebugMenu`); movement actions are refused.
- ✅ `stand` is the placement exception: the server moves the local character
  beside a named entity. The poll does not start on a dedicated server
  (`RplMode.Dedicated`). The server checks the `stand`, `pose`, and `use` RPCs
  with `LF_DevGate`, the same gate as the debug menu. It refuses and logs
  (`[LFDebug:AgentHarness] <request> refused: ...`) unless the game mode sets
  `m_bDevToolsEnabled` or the sender is an administrator. On a production or
  listen-hosted server a client cannot teleport or reach past action range
  (2026-11-07). On a client it retries until a local controller exists,
  including while faction and spawn menus are up.
- ✅ `previews show <prefab,...>|hide` draws up to 24 prefabs through `ItemPreviewWidget` (`LF_ItemPreviewSheet`), the widget the inventory slots use, hiding the HUD and notification toasts; `toolscli shots previews` pages every mod item onto contact sheets (#612).
- ✅ Screenshots go through `LF_ScreenshotCapture`. UI shots leave the camera
  alone. A world shot teleports the character to the camera offset (default
  `10 3 10`, or `ox oy oz` for the row), faces the target, and screenshots the
  player view. That view faces -Z, so a picture stands on the +Z side of
  its target. The pose is applied again 700 ms later so gravity does not drop
  the body before the picture. `SetCamera` does not move that view. (verified 2026-10-11) On a
  dedicated server the server also sends the pose to the owning client
  (`RpcDo_LF_HarnessPose`), which would otherwise overwrite it; session
  clients take world shots this way (verified 2026-11-08, M1-25). A filtered row's wide shot
  stands just north of that row when the street is further north. If the new PNG matches the
  previous PNG, `report roadmap` records `camera_stuck` once and stops.
  `Roadmap_Jail_Door` and `Roadmap_Jail_Cell` are painted-light when look
  returns an action. Any other white mesh fails. Menu shots use `shot ui`; the
  report judges `classify` and the widget list. Names after `roadmap` walk
  only that row. `node agent-box/harness.mjs kit` photographs the UI kit
  sample in the current play session when play is already up, and enters play
  only when it is not. `node agent-box/harness.mjs report roadmap` writes
  `agent-box/reports/roadmap.html` from a play session. The Node harness, MCP
  tools, layers, and named flows live in `agent-box/`. See `agent-box/README.md`.
  (verified 2026-10-11)
- ✅ A world-shot pose turns off damage handling on the posed character for
  8 s, so the fall from the camera height no longer kills it (a 4.5 m pose
  ended on the death screen; verified 2026-11-08 with an 18 m row shot).
  `shot world <name> x y z ox oy oz` takes a close camera offset. The poll
  restarts when a new game mode appears, so a second Play in the same
  Workbench no longer answers `no_session`. The command file is shared by
  every default-profile session on the machine; the `agent-box-play` lock
  ties it to one checkout's Workbench or game process.
- (2026-11-08, #108) Screens: `click` accepts every UI kit button by the name
  the screen gave it (`LF_UIButtonRelay`: ATM keys, `IdClose`, the sign card's
  `Close`, the garage's `Buy`). `ui text` reads a sheet as `kind:name=text`
  (button labels, typed fields, printed lines, `#keys` translated);
  `wait-text` polls it; `ui-shot` is `shot ui`. `stand` turns the character to
  face the entity. `session.mjs up --mission playtest` runs `LF_PlaytestTown`
  with `-scrDefine LF_HARNESS` on server and clients: harness RPCs pass
  `LF_DevGate.CheckHarnessRequest` in that build only, the debug menu stays
  closed, a client without the define is refused by the script checksum, and
  `host start` never passes it. `session.mjs restart` (playtest) stops with the
  host stop file (SHUTDOWN save), loads, and rejoins the same profiles.
- (2026-11-11, #571) The stand-in verbs `drill`, `keycard`, `charge`, `shelf`, `crate`, `keys`,
  `scanner`, `shoot`, `shootnpc`, `siren` and `die` pass `LF_DevGate.CheckHarnessRequest` too, so a
  playtest-mission session can film the heists and the CPR; without `LF_HARNESS` they still need an
  administrator or dev tools. The launch trailer is shot and cut with `video/trailer-v1/shoot.mjs` and
  `video/trailer-v1/cut.mjs` (takes and cuts in the git-ignored `tmp/trailer-v1/`).
- (2026-11-08, #108) `node agent-box/journey.mjs` plays the M1 journey on a
  two-client session from the player's view (job, parcel run, ATM keypad, price
  list, car, give cash, till robbery and alert, org desk, garage, and on the
  playtest mission a restart), judges `state` before and after each step, and
  writes `agent-box/reports/journey.html` with the client screenshots.
  `tools/validation/agent-box-selftest.mjs` runs the harness, session, and
  journey selftests in `validate`.
  Every step ends with a picture from each client it touched. Clients default to 1280x720 (`session.mjs up --res 1920x1080` for evidence) on the hidden desktop. `journey.mjs --film` records each step inside the game client (`LF_FilmRecorder`), one mp4 per step plus the joined run, linked from the report; it works on the hidden desktop.

### `LF_AgentHarness` — `Feature/AgentHarness/`
- ✅ Dev-only. A rendering play session polls `$profile:agent-box/command.txt`.
  `press F` calls `SCR_InteractionHandlerComponent.ActionPerform`. Wheel scroll
  calls `ActionScroll`. `click` invokes the same button invoker or `OnClick` a
  mouse click does. `use` still checks `CanBeShownScript` and
  `CanBePerformedScript` before `PerformAction`. Menu keys are an allowlist
  (`MenuUp`, `MenuDown`, `MenuLeft`, `MenuRight`, `MenuSelect`, `MenuBack`,
  `LF_OpenDebugMenu`); movement actions are refused.
- ✅ `stand` is the placement exception: the server moves the local character
  beside a named entity. The poll does not start on a dedicated server
  (`RplMode.Dedicated`). The server checks the `stand`, `pose`, and `use` RPCs
  with `LF_DevGate`, the same gate as the debug menu. It refuses and logs
  (`[LFDebug:AgentHarness] <request> refused: ...`) unless the game mode sets
  `m_bDevToolsEnabled` or the sender is an administrator. On a production or
  listen-hosted server a client cannot teleport or reach past action range
  (2026-11-07). On a client it retries until a local controller exists,
  including while faction and spawn menus are up.
- ✅ Screenshots go through `LF_ScreenshotCapture`. UI shots leave the camera
  alone. A world shot teleports the character to the camera offset (default
  `10 3 10`, or `ox oy oz` for the row), faces the target, and screenshots the
  player view. That view faces -Z, so a picture stands on the +Z side of
  its target. The pose is applied again 700 ms later so gravity does not drop
  the body before the picture. `SetCamera` does not move that view. (verified 2026-10-11) A filtered row's wide shot
  stands just north of that row when the street is further north. If the new PNG matches the
  previous PNG, `report roadmap` records `camera_stuck` once and stops.
  `Roadmap_Jail_Door` and `Roadmap_Jail_Cell` are painted-light when look
  returns an action. Any other white mesh fails. Menu shots use `shot ui`; the
  report judges `classify` and the widget list. Names after `roadmap` walk
  only that row. `node agent-box/harness.mjs kit` photographs the UI kit
  sample in the current play session when play is already up, and enters play
  only when it is not. `node agent-box/harness.mjs report roadmap` writes
  `agent-box/reports/roadmap.html` from a play session. The Node harness, MCP
  tools, layers, and named flows live in `agent-box/`. See `agent-box/README.md`.
  (verified 2026-10-11)
- ✅ Town manifest (`agent-box/town.mjs`): every M1 site by entity name (or
  prefab, for the unnamed spawn points), the actions look must offer, its sign
  id, and its journey step. No positions: sites and signs resolve against the
  layers, so a site can move layers without a manifest edit.
  `node agent-box/harness.mjs town` and `validate-town-manifest.mjs` check it
  offline; `report town [site-id...]` walks it in play mode and writes
  `agent-box/reports/town.html`. Sign problems (none placed, blank Read
  card) fail the row: `requireSigns` is true since M1-09 placed the signs.
  Distances are the maintainer's layout and only warn, on every branch: a sign
  farther than its radius, a site more than 30 m from the rest. A site whose
  entity was renamed resolves by the actions it carries, with a warning.
  The spawn site is the named civilian town spawn `Spawn_Town`; the clerk
  (`QuestGiver_Start`, prefab mode) is a site so the square meets the 30 m
  neighbour rule.
  Play-mode run not yet recorded.

### Directed camera rig and storyboards — `Feature/Film/LF_FilmCameraPath.c`, `tools/film/storyboard.mjs` (dev tooling, #392, #395)
- ✅ Harness verb `camera orbit x y z radius seconds [azimuth arc height]` (an arc round a point, centred on any azimuth: 0 = +z, 90 = +x, 180 = -z, 270 = -x; the old +z sweep is the default), `camera follow x y z distance seconds`, `camera static px py pz lx ly lz seconds [fov]`, `camera drone p l q m seconds [fov]` (eased move between two poses), `camera hud off|on` (HUD and hint pop-ups hidden for a first-person shot; `LF_FilmClean` keeps hints down every 400 ms), `camera stop`. An orbit or follow (many keys) eases in and out once for the whole pass and runs a Catmull-Rom curve through its keys at a steady speed; it used to ease every segment and so stopped at each key, a pulse once a second (fixed 2026-11-11, #571 trailer). Covered by `film/camera-path` (LOGIC). Proven 2026-11-11 on a 1920x1080 session: five 4 s shots of one scene (first person, follow, orbit, drone, static), no HUD (`video/camera-test.json`, internal).
- ✅ Scene presets (#394): light-only scenes for the trailer locations (`scene start trailer-square`, `trailer-bank`, `trailer-quarry` ... : fixed clock, clear weather, day frozen, no actors; `LF_FilmSceneEnv.EnvOnlyHour`) beside `street`. Covered by `film/scene-env`.
- ✅ Storyboard format `lf-storyboard/1` (`video/storyboard.schema.json`, checks in `tools/film/storyboard.mjs`): output size, fps and aspect (16:9 and 9:16), scenes, shots with camera, duration, captions and voice. `toolscli film-make video/<name>.json` validates (a bad file fails naming the field, e.g. `shots[2].camera.radius`), shoots each shot on the session through the engine recorder, deletes the frames after encoding, and hands the takes to post-production. Flags `--shot ids`, `--group name`, `--preview` (half size, 10 fps), `--client n`, `--no-shoot`, `--shoot-only`. The trailer storyboard is `video/trailer-571.json`; its `spawn-square` group (drone, orbit, static) shoots today, the rest are marked `pending` and skipped. `validate-storyboards` checks every `video/*.json`.
- ⚠️ A first-person shot films the client as it stands: the actor setup for each trailer beat (`shot.setup`) is not written yet, and a title card between shots is not supported by post-production.

### Asset pipeline — `tools/assets/pipeline/` (dev tooling, #339, #341 spike)
- ✅ `tools\cli asset new <slug> --kind weapon|gadget|deployable --prompt "..."` and `asset run <slug>` take a roadmap item to an asset in five resumable stages cached in `tmp/assets/<slug>/`: `concept` (spec checks), `generate` (Meshy text-to-3D with PBR maps, credits logged per call, a 300-credit cap per asset), `clean` (headless Blender: orientation, metres, LODs, one atlas packed to `_BCR`/`_NMO`, collider, and for a weapon the vanilla pistol skeleton, sockets and moving parts), `import` (`.meta` files with fresh GUIDs, `.emat`, a hidden Workbench rebuild, then a read-back of the `.xob`), `prefab` (weapon, magazine, projectile, config). `asset status | doctor | selftest`. Playbook for the judgement steps and the review checklist: `docs/asset-pipeline/`. First asset: the taser. `generate` writes the Meshy preview and refine task ids to `tools/assets/<slug>/meshy.json` (the provenance record; credit in `docs/credits.md`, decision in `docs/decisions.md`). Offline checks: `tools/validation/asset-pipeline-selftest.mjs` (part of `validate`).
- ⚠️ Only the `weapon` kind has run end to end. `gadget` and `deployable` have profiles and no prefab scaffold. Verification (look, previews, the session proof) is a checklist, not a stage. Findings and the issue breakdown: `docs/architecture/asset-pipeline-spike.md`.
- ✅ **Source stage** (2026-11-14): `toolscli asset source search "<words>"` looks for the model before anything is generated: the Arma Reforger base game first, then Poly Haven (CC0) and Smithsonian / Sketchfab (CC0 or CC-BY only), then the concept and Meshy stages. A spec with `source: { provider: "polyhaven", id }` downloads the glTF and maps, skips `concept`, `generate` and `remesh`, and runs `clean` and `import`; a `vanilla` source points the prefab at the base-game model. The vendored cement bag (`tools/assets/cement-bag`) is the first one. `docs/asset-pipeline/sources.md` has the API terms and the catalogue fit of the 26 stand-in props.
- ✅ **The cement bag is a 1989 sack** (#997, 2026-11-14): the Poly Haven sack carried a modern red
  print, a logo and a barcode; `tools/assets/cement-bag/make-label.ps1` repaints the base colour map
  in one dark ink with Soviet stencil type (GOST 10178-85). Same `CementBag.et`, GUID and placement.
  A spec's `base_color.png` replaces a library model's colour map after the source stage.
- ✅ **LOD0 comes from a Meshy remesh** (#664, 2026-11-14): the clean stage used to collapse the raw
  17k to 30k triangle mesh to the LOD0 budget in one decimate step, smearing UVs and denting panels,
  over budgets below vanilla (handcuffs 3,000 against the compass 7,046). The new `remesh` stage
  asks Meshy for exactly `budget.lod0_tris` and Blender only builds LOD1 to LOD3; per kind a floor
  and cap (weapon 12,000, gadget 8,000, deployable 14,000), with `meshy.remesh: false` and
  `fit.bake: false` to opt out. Taser, handcuffs and spike strip rebuilt, GUIDs unchanged.
- ✅ **Five stand-in props** (#1050, 2026-11-14): the police scanner, post parcel, moonshine still,
  smelter furnace and dispatch console went through concept, Meshy and clean and are wired into
  `LF_Scanner.et`, `LF_Parcel.et`, `LF_ChainStill.et`, `Furnace.et` and `LF_DispatchConsole.et`;
  GUIDs kept. The furnace is the clear win; the still is a drum with a copper coil and the console
  a cream desk set.
- ✅ **Sound stage** (2026-11-13): \`tools\cli asset sound <slug>\` gives an asset generated sound effects (`docs/asset-pipeline/sounds.md`). `sounds.json` lists the events of the item kind (a weapon: shot, dry fire, magazine out and in) with a prompt each; `generate` calls the ElevenLabs sound-effects API (a row per call, a cap of 20 generations, the key never logged); `process` normalises, trims, fades, seams loops and can lay one take over another, and writes 48 kHz mono 16-bit WAVs plus a waveform and spectrogram picture per take for the agent's review; `wire` puts the picked takes and their `.meta` files in the addon and writes the `.acp` sound projects on the vanilla M9 handling graph. `asset-sound-selftest` (in `validate`, ten red-proofs) checks the format of every committed sample and each committed project against the generator. Harness verb `sound [EVENT]` reports and plays a held weapon's events on a client.
- ⚠️ Proven with the taser only. The prefab's `Filenames` override and the script that plays a non-weapon sound are written by hand. Cost and quality: `docs/architecture/asset-sound-spike.md`.
- ✅ (#664, 2026-11-13) **The `gadget` kind runs end to end**, proven with the handcuffs: the `prefab` stage dispatches every kind but `weapon` to `prefab-<kind>.mjs`, and `prefab-gadget.mjs` writes one held item on the vanilla `Item_Base.et` (inventory name and volume, the vanilla gauze hand attachment bound as `Gadget`, `SCR_GadgetComponent`, preview camera, the spec's extra components). A gadget is scaled on its length, so the size check holds the length to 5 % and the other two to 40 %. The import judged each Workbench run on `console.log` alone; joining `script.log` and `error.log` after it counted the start-up `metafile without corresponding resource` lines and failed a clean import. Offline checks: `tools/validation/asset-gadget-selftest.mjs`.
- ✅ (2026-11-14) **Kits and reference concepts.** `tools\cli asset kit <slug>` builds several models that share materials with the kit's own headless Blender script and imports them all (`.emat`, one `MaterialAssign` per material, `UBX_` colliders per model, bones checked); a concept round with a `reference` picture goes through Meshy image-to-image so it matches a piece already chosen (`docs/asset-pipeline/README.md` "Kits", `concepts.md`). First kit: the 1989 bank vault (`tools/assets/bank-vault/`), the maintainer's door #1 generated and split from its frame on a `door` hinge bone (pivot 1.271 1.35 0.162 in the front's space), a concrete wall block with a round steel tunnel and step, a 3 x 3 x 2.5 m brushed-steel room, two riveted deposit box walls and a flat cash trolley (`Prefabs/Props/Roleplay/BankVault/`, closed and open). Placed in the DebugWorld bank (see "Bank vault in the bank" below).
- ✅ **Bank vault in the bank** (#925, 2026-11-14). `Roadmap_Bank_Vault` is the vault kit itself (`LF_BankVault.et`: front, round door, steel room, two deposit box walls, cash trolley, two ceiling lights `LF_BankVault_Light.et`): a concrete strong room against the bank's east wall at 194.3 0.007 259.6, front facing the street, cash bag on its floor. The lock, rob and repair actions, `LF_BreachTargetComponent` and `LF_BankVaultComponent` sit on the kit root (it carries an `RplComponent`), so the robbery, fuse, jam, drill and evidence paths are unchanged. `LF_BreachTargetComponent.m_sSwingChild` / `m_fSwingYaw` swing the `VaultDoor` mesh 70 degrees when the breach state says open and back on repair, on the server and every client (`m_bRplOpen` has an `onRplName` callback; a prefab's nested entities are not hierarchy children, so the door is found by its mesh within 4 m). The door's collider turns with it. The two annex display vaults are gone. Not done: a walk-through door from the bank's own floor. The swing on a second client and a late joiner is proven on a dedicated server by `session.mjs mp-robbery-proof` (2026-11-14: every client and a joiner after the breach draw the door at the server's yaw, and shut again on repair).
- ✅ **A chosen addon** (#971, 2026-11-14): every stage of `tools\cli asset` (spec, source, concept, generate, remesh, clean, import, prefab, anim, states, and `sound`, `dialogue`, `kit`, `ambience`) writes into the addon the spec's `addon` field names, or `--addon <name>` on the command; the default is `LifeFramework`. A pack such as `LF_Everon1989` gets its models, `.meta` files, prefabs, clips, sounds and strings in its own folder, and the hidden Workbench opens that project (the core is its dependency). Fresh GUIDs are drawn against every repo addon. `asset plan <slug> [--addon]` prints every destination and writes nothing. A core asset that names the pack (a GUID, a path, the addon name) is refused before any stage runs, and each writing stage ends with the `validate-addon-boundary` check. A pack asset may not sit at a core path (it would shadow it). A changed target reruns `import`, `prefab`, `anim` and `states` and keeps the generator and clean results. Offline checks: `tools/validation/asset-addon-selftest.mjs`; playbook `docs/asset-pipeline/README.md` "Target addon".

### Building pipeline — `tools/buildings/pipeline/` (dev tooling)
- ✅ (2026-11-14) `tools\cli building run <slug>` turns `tools/buildings/<slug>/spec.json` into a building in the addon by driving [ArchBuilder](https://arribas22.github.io/archbuilder/) (Arribas's Blender add-on, the user's own licensed copy) in headless Blender 5.3: `spec`, `generate`, `review` (four renders and triangle, storey, footprint and height stats against the spec's budget), `export` (ArchBuilder's Reforger export into `tmp/buildings/<slug>/export/`), `import` (refuses any file with the licence watermark, moves models to `Assets/Buildings/<Name>/` and prefabs to `Prefabs/Buildings/<Name>/` with GUIDs of ours that a re-export keeps, compiles in a hidden Workbench kept open until every collider surface is written) and `prefab` (every reference resolves; base-game ones by path and GUID in the game's own resource database). Stages that need ArchBuilder skip on a machine without it. `docs/building-pipeline/README.md`, spike `docs/architecture/building-pipeline-spike.md`, selftest `tools/validation/building-pipeline-selftest.mjs`.
- ✅ (2026-11-14) First building: the 1989 Everon supermarket (`Prefabs/Buildings/Supermarket/Supermarket.et`, ArchBuilder `SUPERMARKET`, 18 x 14 m, one storey, plate-glass front with a vanilla glazed double door, stockroom, office and WCs behind vanilla interior doors). Placed in the DebugWorld test annex as `Annex_Supermarket` (76 0.1 96.45), audited clean.
- ✅ (2026-11-14) Blender parity: the supermarket is exported furnished, every piece a vanilla prop child of `Prefabs/Buildings/Supermarket/Furniture/Supermarket_furniture_01.et` (68: 16 `ShelfShop_01` gondola shelves, 4 `CoolingBoxShop_01`, 3 counters with `CashierShop_01` tills, back-room shelving, lamps, WCs, the `SignCommercial_01_GroceryStore` fascia); the shop fixtures arrive empty. The export stage fails on any ArchBuilder warning or dropped furniture, and rewrites every `AB_*.emat` `Color` from the material's Blender colour, calibrated on the base game's own plaster and divided by the texture's measured mean (`tools/buildings/pipeline/materials.mjs`): beige walls and a dark roof instead of white.
- ✅ (2026-11-14, #323 batch 2) The supermarket moved to the pack: its prefabs, furniture and model now live in `addons/LF_Everon1989/` at the same paths and GUIDs, and its placement left the core `Annex.layer` for the pack layer `Village_Buildings` as `Village_Supermarket` (same coordinates). `spec.json` sets `"addon": "LF_Everon1989"`, so a rebuild writes into the pack.
- ✅ (2026-11-14) Hand polish, per building (`docs/building-pipeline/polish.md`): a `polish` stage renders a plan section, the four sides, the street corner and the hall from inside; `tools\cli building adopt <slug>` keeps a scene edited by hand as `tools/buildings/<slug>/polished.blend` (watermark-checked), which the export then ships with ArchBuilder's scene export; `spec.polish.entities` nudges placed game entities on every export; three or more doors in a row on one wall print a warning. No rule gates. The doc holds the Everon reference sizes (vanilla shops and houses, from the game's `.xob` data). Supermarket polished: 3.8 m storey (was 5.75 m; parapet top 4.9 m, the vanilla grocery is 5.8 m), office and staff WC opening off the stockroom behind one service door (an ArchBuilder change, listed in the README), GroceryStore sign moved over the shop window clear of the canopy, a tiled dado in the hall, interior materials calibrated on an in-game picture (half the albedo; still brighter than vanilla rooms).
- ✅ `building-proof` (agent-box, full-quality session): the F key offers the vanilla door and the server opens and closes it.
- ✅ (2026-11-14, #1143) **The supermarket is a working store.** `Prefabs/Buildings/Supermarket/Supermarket_Store.et` (pack) inherits the generated `Supermarket.et` and adds `LF_ShopComponent` (prices from `ShopPrices.conf`), `LF_StoreComponent` (key `everon_supermarket`, the whole building as the shoplifting boundary) and a "Pay for goods" context with `LF_StoreTillAction` at each of the three `CashierShop_01` registers (the F key pays through it). The shop sells only the store's own lines (`m_aShopItems`). It is written by a new `store` stage of `toolscli building` (`tools/buildings/pipeline/store-slots.mjs`) from the spec's `store` block: each line names a vanilla fixture, which of them (`pick`, by the index the stage prints), which boards and how many per board; shelf points are the fixture's position plus a board point turned by its yaw, with the board heights measured once from the vanilla models (`tools/buildings/pipeline/fixtures.json`: `ShelfShop_01` 0.10/0.43/0.82/1.18, `CoolingBoxShop_01` deck 0.66, `CounterShop_01` 1.04). A rerun rewrites the prefab from the spec. First line, to prove the path: apples on both sides of a gondola, apple juice on both sides of another, water in a cooler. Proof on a full-quality session: shelf takes from both gondola sides and the cooler, paid at two tills ("paid 20 for 3 item(s)", "paid 14 for 2 item(s)"), walked out clean with no alarm, walked out with an unpaid water: "walked out with 1 unpaid item(s) worth 6; alarm, dispatch, wanted level and name".
- ✅ (2026-11-14, #1143) Store goods land on the board under their shelf point: `LF_StoreComponent.SnapToSurfaceAt` traces from 0.15 m above the point (under the next board up) against any static surface, looking past loose items and characters. It traced only the store entity: a building's fixtures come from its furniture prefab and are not runtime child entities, so goods hung at the written point. The till measures reach to the nearest of its own contexts, not the owner's origin (a building carries one per register). The harness `shelf` takes the copy nearest the character. Test `store/snap-to-board`.
- ✅ (2026-11-15, #1143) Shelf points and stock layout come from measured data, not tuned numbers. `tools\cli fixtures` regenerates `tools/buildings/pipeline/fixtures.json` from `tools\cli measure` (the boards of `ShelfShop_01`, `CoolingBoxShop_01` and `CounterShop_01` with their extents and the clearance to the surface above, and the footprint of every item a spec's store sells; `fixtures --check` compares it to a fresh measure). The store stage fills each board from it: one or two rows by depth, as many copies per row as the width takes, evenly spread, at most 20 a board (`per_board` and `rows` lower it), refusing goods too tall for the gap above; `max` is one copy per shelf point and `restock` a sixth of it unless the line gives them, so the supermarket spec lines are only item, fixture, pick and boards. The supermarket went from about 250 to about 580 shelf points. `store-fixtures-selftest` checks `LF_StoreComponent`'s `SNAP_ABOVE` and `STRAY_HEIGHT` against the smallest gap between boards and the heights in `LF_Test_StoreSnapToBoard` against the measured boards; `measure-selftest` compares the file to the models where the game is installed. Saved stock is keyed by store and item prefab, not slot, so old saves load unchanged. Fixed the stale model path in `RowanBerries.et` (`rowan_berries.xob` for `RowanBerries.xob`). Proof pictures: `session.mjs store-shots --name <prefix>`.
- ✅ (2026-11-14, #1143) Supermarket polish: a shop-front canopy along the whole glazed front (a slab on the wall, no posts, a red painted fascia, the GroceryStore sign above it; the sign nudge is gone), one staff WC (a toilet and a basin, was three cubicles), the stockroom furnished as a stockroom (four `ShelfUniversal_01_wide` racks and two pallet boxes, were living-room `ShelfDecorative` dividers), all ArchBuilder changes listed in the README. `spec.polish.props` places vanilla props by hand on every export (26: cartons on the racks and pallets, a crate, a bucket, the punch clock and an extinguisher by the service door, a filing cabinet and files in the office, a basket pile inside the entrance). The shared `AB_*` materials moved to the pack with their GUIDs (only pack buildings use them).
- ✅ (2026-11-14, #1143) **The supermarket is fully stocked.** Nine lines, 248 shelf copies on the eye-level boards (every line 36 points or fewer): apples and plums on the first gondola, rowan berries on the second, crackers and dried sausage on the third, field dressings and torches on the fourth (by the tills), water and apple juice in the four chilled counters; restock every 120 s, stock saved by the existing `storestock` rows (no new persistence). New core food items over vanilla meshes, edible like the apple: `Prefabs/Items/Food/LF_Crackers.et` (`ArmyCrackers_Soviet_01`) and `LF_DriedMeat.et` (`Meat_01_Dry`), priced by the food rule (8); new price rules: `FieldDressing_US_01` 15, `Flashlight_Soviet_01` 40.
- ✅ (2026-11-14, #1143) **A shopkeeper and an entrance alarm.** `Prefabs/Characters/Npcs/LF_Shopkeeper.et` (core) is a dialogue NPC on the fuel attendant's pattern with the store clerk's clothes and the text-only `shopkeeper` dialogue (`tools/assets/dialogue/shopkeeper/`: greets, how to pay at the till, what the store sells, goodbye); `Village_Supermarket_Shopkeeper` stands at the end of the tills. When a shopper walks out unpaid, `LF_Stores.Shoplift` makes every NPC with a dialogue inside the store's boundary say its line for `#LF-Store_ClerkThief` to everyone near (`LF_Stores.StaffFor`/`StaffReact`, found by component; `LF_Dialogue.Announce`). The store prefab's new `store.alarm` child, a bell box on the shop front under the canopy, left of the door, with `LF_AlarmMountComponent` (reach 10 m), carries the walk-out's site alarm (beacon, light and siren from `LF_Dispatch`), so it sounds at the entrance and stops when the alarm time runs out. Test `store/staff-react`; session proof `toolscli building proof supermarket` (its `tools/buildings/supermarket/proof.json`; was `session.mjs supermarket-proof`); after `session.mjs restart` the store came back with `Flashlight_Soviet_01.et 22/30` (8 taken before the save).
- ✅ (2026-11-14, #1143 review) A shop refuses to buy an item the seller carries while a store still holds a copy of it apart (a shelf copy or one tagged unpaid), so stolen shelf goods cannot be sold back (`LF_Stores.CarriesHeldApart`). Restart reconciliation matches a restored shelf copy to a slot within 0.15 m flat **and** 0.2 m in height, so the copies on two boards of one gondola no longer swap.
- ⚠️ Cut for now (roadmap): cabbage (the LF cabbage is a whole crop plant at scale 0.5; on a gondola it overlaps its neighbours and floats on the bottom board), so the front of the second gondola is empty; more 1989 products need item prefabs.
- ✅ (2026-11-15, #1143) The harness takes goods the way a player does: verb `pickup [item]` (`LF_HarnessPickup`) checks and performs the vanilla `SCR_PickUpItemAction` of the nearest loose copy on the client, as the F key does, and replies once the item is in the inventory (or names the refusing check: animation, room, reason). The supermarket proof picks the torch, crackers, water and dressing with it; `shelf` (a server-side insert) stays for older proofs.
- ✅ (2026-11-15, #1143) **`toolscli building proof <slug>`**: one command per building. The placed prefab resolves to its model with colliders, its height against the Everon buildings within 150 m, `audit placement --strict` on its layer, then one full-profile session: every door set offered by the F key, opened, a doorway a character wide clear at 0.5/1.0/1.6 m (traces; shut, every lane stops at the leaf), closed, with pictures shut and open; the four sides, the street corner and the hall from inside; then the building's own scenario `tools/buildings/<slug>/proof.json` (shots, harness verbs, `pickup`, kept `state` values, checks and server-log rules in building space; `tools/buildings/pipeline/scenario.mjs`). Pictures (PNG) in `tmp/buildings/<slug>/proof/`. The supermarket's scenario is the former `supermarket-proof`: talk to the shopkeeper, pick three items, pay, leave clean, walk out with a dressing: alarm on the shop-front box. Selftest `building-proof-plan-selftest`.
- ✅ (2026-11-15, #1143) **`toolscli building plan <slug>`** prints what an agent brief needs, as markdown (and `tmp/buildings/<slug>/plan.md`): prefabs as `{GUID}path`, every furniture and fixture prefab with count, `measure` size and stand-on heights, hand props, rooms inferred from their furniture with the doors near each, doors and windows by wall, store fixtures by `pick` index with lines, tills and alarm, the parts that can run as parallel agents and the ones that cannot (`polished.blend`), and the proof list.
- `tools\cli` hands `tools/commands/*.mjs` the Workbench runner (`main(args, { root, workbench })`), and `runWorkbenchPlugin` takes `doneWhen` to keep Workbench open until a build is really finished.
- ✅ (2026-11-15) **Fast polish iterations.** One `building run` starts Blender once (`archbuilder_job.py --serve`) instead of once per stage, with `PYTHONHASHSEED=0` so the same scene exports a byte-identical `.fbx` apart from its time stamp. The import writes only changed files and compiles only a model whose `.fbx` or `.xob.meta` changed, judged against `tmp/buildings/<slug>/import-manifest.json`; with nothing changed it skips Workbench and says so in one line, and `--full` (or `--force`) compiles every model. On a PC without the workbench cap the compile goes to a pool machine (`building compile`, in its desktop session) and the `.xob`, `.xob.meta` and `.txo` come back. Supermarket: an unchanged rerun from `polish` takes 14 s (was 32 to 67 s of Blender plus a 72 to 136 s Workbench import), one changed mesh 103 s. The Enfusion Blender Tools start-up errors stay in the log and out of the output. Selftest `tools/validation/building-import-delta-selftest.mjs`.

- ✅ **Blender bridge** (2026-11-14, `docs/blender-bridge.md`): `tools\cli blender status|install|run|exec|shot|show` drives the user's open Blender through the `LF Bridge` extension (`tools/blender/lf_bridge`: localhost only, token in `~/.life-framework/blender-bridge-token`, a sidebar and status-bar line with a Start/Stop toggle) and falls back to a headless Blender. `show <slug|model file|addon .fbx>` puts a model in an `LF_Show` collection, selected and framed (compiled `.xob` cannot be opened; the source models are shown). `asset run <slug> --only clean --live` runs the clean stage in the visible Blender; `asset shot <slug>` takes viewport pictures of the cleaned model. Finding Blender and running scripts live in `tools/lib/blender.mjs` (`blenderExe({ needs })`, `runHeadless`, `bridge`), re-exported by `asset.mjs`. Offline checks: `tools/validation/blender-bridge-selftest.mjs` (locator, client framing, token required, loopback only, committed files).

### `film-make` post-production — `tools/film/post.mjs` (dev tooling, #397)
- ✅ `tools\cli film-make video/<name>.json` joins clips into one mp4: logo title and end cards, crossfades, burned-in captions from cue files plus an `.srt`, offline Piper voice-over, and a licensed music bed ducked under game sound and voice (about -14 LUFS). Covered by `node tools/film/film.mjs selftest`; proven on `video/tooling-test.json` (not for posting).

### `LF_FilmCameraPath` — `Feature/Film/` (filming harness)
- ✅ Dev-only, like the agent harness. `LF_FilmCameraPath` holds timed camera
  keys (position, look-at, FOV); `Sample(t)` eases between keys with
  smoothstep and clamps at both ends. Out-of-order keys are rejected and
  logged (`[LFDebug:Film]`). Covered by `film/camera-path` (LOGIC).
- ✅ `LF_FilmCameraRunner.Play(path)` spawns a manual camera with input off,
  hides the HUD, moves the camera each frame, then restores the previous
  camera and HUD. Harness verb `camera` plays it (directed camera rig, #392,
  next section).
- ✅ `tools/film/film.mjs` records a window or region to mp4 (ddagrab +
  NVENC; gdigrab when ddagrab delivers no frame in 5 s), makes a GIF preview,
  films an agent-box flow, and publishes a clip to the `film-evidence`
  prerelease for a PR comment. Proven in game 2026-11-08: a game client on
  DebugWorld, flow `film-debugworld`, 20.7 s at 1280x720, 2.8 MB (PR #111).
  Screen capture records a frozen frame when the window is unfocused and
  cannot see the hidden `LF-Agents` desktop (#91).
- ✅ `LF_FilmRecorder` records inside the game client: a per-frame callqueue
  tick calls `System.MakeScreenshot` at a fixed rate (default 15 fps) into
  `$logs:el-film/<name>/`, with HUD and menus, and writes `index.txt` with
  each shot's real time and frame-time stats (`[LFDebug:Film]`). Harness verb
  `film start <name> <fps> [max s]` / `film stop`; a dedicated server refuses
  (`error no_renderer`). `tools/film/engine.mjs` encodes the BMPs at true speed
  (concat with per-frame durations, NVENC) and deletes them. Wired as
  `film.mjs engine|encode`, `film.mjs flow <flow> --engine`,
  `journey.mjs --film`, and `session.mjs film` / `proof --film`. Covered by
  `film/recorder` (LOGIC). Proven 2026-11-08 on the hidden desktop, 1280x720,
  client capped at 30 fps: a 64.5 s journey clip (arrive, job, earn, fund, atm,
  shop) and a 27.9 s `world-atm` clip, capture 14.7-15.1 fps, game 29.5-30
  fps, frame after a shot 33.3-33.5 ms vs 33.2-34.5 ms otherwise, one 243 ms
  stall in 64 s (shop menu, not a shot frame).

- ✅ Game audio (#396): the engine cannot record sound, so `tools/film/lf-procaudio.cs`
  (Windows process loopback, `AUDCLNT_PROCESS_LOOPBACK`) captures only the game client's
  process into a 48 kHz float WAV (other host sounds never enter it; the WAV is the separate
  audio track). Harness verb `film slate` plays a UI beep and logs `# slate=<ms>` in
  `index.txt`; `tools/film/audio.mjs` finds the beep in the WAV, lines audio up with the
  frames and muxes an mp4 (`street-clip.mjs --audio`, session started with `--sound`). A start
  and an end slate give the offset and the drift (the mux uses their mean; one unrecorded warm-up beep plays first because the first play is heard about 0.5 s late). The recorder refuses a silent capture
  (`mean_volume` under -60 dB, or no slate heard) and warns when the default output is muted
  (#482). Covered by `node tools/film/film.mjs selftest` (pure parts). Proven on this PC 2026-11-10: three takes read start-to-end beep drift of -96, +119 and +52 ms; the laptop run waits on #482 and a free lane.

### `LF_FilmActor` / `LF_FilmDirector` — `Feature/Film/` (scripted actors, #393)
- ✅ Dev-only, harness verbs `scene start <name> [hour|-] [weather]`, `scene stop|reset|status` (server, through the
  character RPC and `LF_DevGate`) and `camera orbit|follow x y z size seconds`
  (client). An actor is a spawned `Character_Roleplay` on a timed step list:
  `walk`, `run`, `enter`, `drive`, `exit`, `use <action class>`, `wait`. Waypoints
  are authored (no navmesh). No progress for 6 s logs `[LFDebug:FilmActor] ... stuck`
  and stops that actor. A `camera follow` low-passes the followed entity's position (time constant 0.25 s, `LF_FilmCameraPath.SmoothAlpha`), because a car driven in 100 ms velocity steps made the camera shake. Pure maths (steer, pedals, stuck detection, follow smoothing) is covered by
  `film/actor-math` (LOGIC).
- ✅ Scene setup and reset (#394, `LF_FilmScene.c`): a scene is a clock hour, a weather state, the day frozen,
  its actors and vehicles and its camera cues. `scene start street` snapshots the world's local clock hour, weather state, weather looping and day advance, sets the scene's defaults (street: 16:00, Clear, frozen), then
  spawns; `scene start street 21.5 Overcast` overrides one take (`-` keeps a value; an hour outside 0..24 returns
  `error:bad_hour`). `scene stop` (alias `reset`) despawns every actor and vehicle the scene made and writes the snapshot
  back, so every take starts from the same light. Logs `[LFDebug:FilmScene]`. A world with no time and weather manager
  logs an error and runs the scene without a clock. Camera cues stay in the take script (`tools/film/street-clip.mjs`).
  Covered by `film/scene-env` (LOGIC: defaults, override, bad hours, clock conversion); the world write is proved
  in game (PR). Not done: scenes other than `street`.
- ✅ Walk, run and `use` proven 2026-11-10 (the store till action ran for an actor,
  `canPerform=1 uid='film-walker'`). Actors read a stand-in player uid from
  `LF_FilmIdentity` in a `LF_HARNESS` build only (`LF_Utils.GetPlayerUID`).
- ⚠️ Driving: an actor gets into a base-game car (`MoveInVehicle` + `StartEngine`
  worked). Throttle, steering and brake set on `VehicleWheeledSimulation` did NOT move
  it (speed stayed 0 for 9 s, also with clutch 0, gear set and `ForceEnableSimulation`).
  What works: setting the physics body's velocity and angular velocity each tick
  (`Physics.SetVelocity`) along authored waypoints at a steady 30 km/h, wheels
  turning, driver visible; a final taper and zero velocity stop it. No real
  throttle, braking or gear model, and a steering model that only turns the body.
  Clip 2026-11-10 (`agent-box/reports/film/`, PR for #393): walker uses the store till
  while a car drives down the main street.
- ⚠️ Actors in DebugWorld are underwear characters (no loadout). `camera follow`
  tracks the vehicle nearest the given point, client side.

- ✅ (2026-11-15, drug store) **The 1989 Everon drug store is a working store on the main street.** `Village_Drugstore` (Everon1989Town `Village_Buildings.layer`, 140 0 266.05, `angleY 180`, north row facing the street) places `Prefabs/Buildings/Drugstore/Drugstore_Store.et` (pack), written by the building pipeline's vanilla-base store stage from `tools/buildings/drugstore/spec.json`. It inherits our `ShopHouse_E_2I01t_drugstore_lf.et` (the vanilla `ShopHouse_E_2I01t_Base.et` with the vanilla drugstore variant's doors, windows and `SignCommercial_01_Drugstore` "DROGERIE" fascia) and its furniture copy `Furniture/ShopHouse_E_2I01t_furniture_drugstore_lf_01.et` (the vanilla drugstore set flattened one child per prop; newspaper stands, a pub A-board and the sales-room crate stack left out, a `ShelfUniversal_01_wide` where the crates stood). Self-serve shelves (store key `everon_drugstore`, restock 300 s, 20 points a line): aspirin 10, gauze (two kinds) 8, tourniquets US and USSR 25, paid at the `CashierShop_01` till; prices in the pack overlay `Configs/Shop/LF_ShopPrices_Drugstore.conf`. Beside the till, `Prefabs/Props/Commercial/LF_PharmacyCounter.et` ("Dispensary") sells the catalogue outlet `pharmacy_counter`: morphine 80 (licensed to carry) and saline 60, each offer gated on the existing `MEDIC_ACCESS` licence, so the slip names the "Ambulance service ID" when it is missing (data only, `Configs/Catalogue/LF_Catalogue_Drugstore.conf`; both overlays in the pack mission headers). `Village_Drugstore_Pharmacist` (`Prefabs/Characters/Npcs/LF_Npc_Pharmacist.et`: `LF_Shopkeeper.et` in the core `DoctorCoat`) stands behind the dispensary and calls out a walk-out; a bell box on the shop front, left of the window, carries the walk-out alarm. Field dressings stay with the general store. Proof: `tools\cli building proof drugstore` (aspirin and gauze taken and paid at the till; morphine refused without the licence, `/licence MEDIC_ACCESS`, then bought); `audit placement --strict` clean. The building proof verb now finds a vanilla-based building through `buildingFiles` and skips the door walk for it (its doors are the vanilla sockets'). Not yet: a window display and the `Signboard_03_Drugstore` blade sign, an exterior night picture, 1989 pharmacy packages for the bare wall shelves (supermarket card's package pipeline).

- ✅ (2026-11-16, drug store polish) **The drug store reads as a lived-in shop, day and night.** `ShopHouse_E_2I01t_furniture_drugstore_lf_01.et` gained about 240 hand-placed base-game children (decor only, no store-slot items): five ceiling fluorescents each with an inline spot `LightEntity` (cool white, `LV 7.5` to 9), two warm spots over a window display (two coffee tables with boxes, bottles, a plant, a tape recorder) so the window glows from the street at night, boxes, bottles, clocks, radios, tape recorders and cleaning buckets on every wall-shelf board and on the shelf tops, bottom boards of the gondolas, a ledger, phone, radio and photo frame on the till counter, a scale, prescription pad and plant on the dispensary counter, a medical cabinet, bin, stool and delivery boxes behind the counters, a first-aid box, clocks and three health posters on the walls, a waiting chair and a weighing scale, and outside the `Signboard_03_Drugstore` blade sign with a wall lamp and spot, a bench and a crate display on the pavement. Sold shelves keep their slots; the only lights are eight spots. Not yet: 1989 pharmacy packages (pill boxes, plasters) for the shelves, a green pharmacy cross, a mop and bucket by the door.
- ✅ **The grid is guarded and pictured (#1814, 2026-11-17).** `audit placement --grid --strict` has 0 failures after the annex move (#2133): `wt gate`/`ci` (`tools/lib/placement-gate.mjs`) run it when a GridWorld layer or a prefab changes, the main guard (`grid placement audit` step) after every merge; any finding fails. `look <area> --interior x,y,z [--radius m]` shoots a room from inside (eye level on the room's floor; `look selftest` covers the plan). One picture per claimed cell is kept in `website/public/docs-images/1802/` and indexed by `docs/architecture/grid-cells.md` (`node tools/grid/cell-index.mjs` regenerates it). Merged, in-dev.
