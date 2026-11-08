# Agent box

Portable setup for a GPU (or any) machine that should build, play, and inspect
Life Framework the way a player sees it. Clone the repo, run one script, and
the Workbench MCP servers plus a small player harness are available.
Player-equivalent actions are `EL_AgentHarness` under
`addons/LifeFramework/Scripts/Game/Feature/AgentHarness/`.

```powershell
powershell -ExecutionPolicy Bypass -File agent-box\setup.ps1
```

That clones the Enfusion MCP servers (`tools\cli mcp install`), checks they
boot, and writes Cursor MCP config:

- `.cursor/mcp.json` in this checkout
- `%USERPROFILE%\.cursor\mcp.json` (existing servers are kept)

Restart the agent session after setup. MCP servers load at session start.

`.cursor/mcp.json` contains absolute paths for this machine. `.cursor/.gitignore`
keeps that file untracked. Do not commit it.

## Fresh machine

1. Install Git, Node 18+, and the Reforger game, tools, and dedicated server.
2. Clone and `git config core.hooksPath .githooks`.
3. Run `agent-box\setup.ps1`.
4. Open `addons/LifeFramework/LifeFramework.gproj`, build, and play DebugWorld
   once so the harness script is compiled into the running session.

`node agent-box/harness.mjs selftest` checks the command formatter and the
report judges with no game running. `node agent-box/harness.mjs status` prints
which installs resolved.

## T3 project

Register the clone as a project titled **Life Framework**. The saved scripts
are in `agent-box/t3-project.json`: toolchain status, agent-box setup, harness
selftest (on new worktrees), DebugWorld layers, play, validate, and harness
status. New threads should use a worktree so the script pillar can run. The
main checkout stays the world editor.

## Three pillars

| Pillar | What it proves | Command |
| --- | --- | --- |
| Script | Logic behaves across states | `node agent-box/harness.mjs pillar script --run` (worktree only) |
| 3D world | Placement, interaction, and the shot match | `node agent-box/harness.mjs shot world <EntityName>` |
| UI | Menus look right after real navigation | `node agent-box/harness.mjs shot ui <name>` |

Script runs are the existing `[ELTEST]` suite. They refuse the main checkout
because that copy is the world editor. Create a worktree with
`node tools/cli.mjs wt new <feature>` first.

World and UI shots need a rendering play session. A headless dedicated server
has no pixels.

```powershell
node agent-box/harness.mjs play
node agent-box/harness.mjs layers
node agent-box/harness.mjs layer Shop_GeneralStore_Debug
node agent-box/harness.mjs shot ui debug_menu
node agent-box/harness.mjs shot world ATM_Debug
```

`play` stops play mode if it is up, opens `Worlds/DebugWorld/DebugWorld.ent`,
then enters play mode. It passes this machine's toolchain paths so the
Workbench bridge compiles into the open mod. It reads the mode, stops only
when play is already up, opens DebugWorld, reads the mode again, and then
plays, all in one bridge process: a fresh bridge process starts in an unknown
mode and refuses `wb_play` until `wb_state` has run in that same process.

`layers` reads `Worlds/DebugWorld/DebugWorld_Layers` and lists named entities
that carry an `*Action` inline or on their addon prefab (one parent hop).
Vanilla actions such as `SCR_DoorUserAction` stay invisible to that catalog;
`stand` still finds those entities by name. `layer <EntityName>` stands beside
that entity, waits until `look` shows that action (the same list the F key
would offer), performs it, and screenshots the UI. `shot world <EntityName>`
uses the entity coordinates when you omit x y z. `shot ui debug_menu` opens the
debug menu first.

`node agent-box/harness.mjs kit` enters play (or reuses a session that is
already up) and photographs the UI kit sample: the full sheet, a rewritten
balance, a status row with the action hidden, keep and take, and focus on the
field. Nothing in the world opens that screen. The flow is
`agent-box/flows/ui-kit.txt`.

## Named flows

Named flows are the short version of a pillar. Each file in `agent-box/flows/`
is the sequence a player would take (also the `player_flow` MCP tool):

```powershell
node agent-box/harness.mjs flow ui-intake
node agent-box/harness.mjs flow ui-debug-menu
node agent-box/harness.mjs flow ui-debug-menu-nav
node agent-box/harness.mjs flow ui-shop
node agent-box/harness.mjs kit
node agent-box/harness.mjs flow world-atm
node agent-box/harness.mjs flow chain-apple
node agent-box/harness.mjs chains
node agent-box/harness.mjs chains --run
```

Steps: `stand`, `look`, `ui`, `click`, `press F|WheelUp|WheelDown|F10`, `use`,
`input`, `open`, `layer`, `shot ui`, `shot world`, and `wait <ms>`. A shot line
prints `image: <absolute png>` when the capture file is on disk. If the engine
wrote a BMP and the `.png` sibling is missing, the harness converts it.

## Same actions a player gets

The play session polls `$profile:agent-box/command.txt` (Workbench profile and
the game profile). Hooks:

- `press F` calls `SCR_InteractionHandlerComponent.ActionPerform`, the same
  listener the interact key uses. `press WheelUp` and `press WheelDown` call
  `ActionScroll`, which moves the highlighted entry. `press F10` opens the
  debug menu.
- `look` lists `ScriptedUserAction`s within 4m whose `CanBeShownScript` is
  true. That is the list the interact key would offer.
- `ui` lists visible buttons whose names end in `Button`. Faction and spawn
  buttons are included because they carry `SCR_ButtonTextComponent`. Shop and
  ATM buttons are included when a Life Framework menu owns them.
- `click <WidgetName> [index]` fires that button. Text buttons call
  `m_OnClicked`, the invoker the menu inserted. Other buttons call `OnClick`
  on the menu handler, the same call the engine makes for a mouse click.
  `index` picks a later visible widget when several share a name (`BuyButton`).
- `use` finds a `ScriptedUserAction`, checks `CanBeShownScript` and
  `CanBePerformedScript`, then calls `PerformAction`. Named confirm, same
  checks, no dependence on which row is highlighted.
- `stand <EntityName>` asks the server to move the local character beside
  that entity so a 3m action radius can succeed. It does not inject movement
  keys.
- `input` sets one allowlisted menu action (`MenuUp`, `MenuDown`, `MenuLeft`,
  `MenuRight`, `MenuSelect`, `MenuBack`, `EL_OpenDebugMenu`) for about 80ms.
  Movement actions are refused.
- `open DebugMenu` toggles, the same as the debug key. `open DebugMenu show`
  opens it and leaves it open, which is what a UI screenshot uses. `hide` closes
  it only when it is up.

Results come back on `result.txt` as `<id> ok ...` or `<id> error <reason>`.
`error no_session` means play mode is not running, or this build does not
contain `EL_AgentHarness` yet.

MCP tools on the `life-agent-box` server match these commands: `box_status`,
`layer_list`, `layer_play`, `player_press`, `player_look`, `player_ui`,
`player_click`, `player_stand`, `player_use`, `player_input`, `player_open`,
`pillar_shot`, `pillar_script`, `player_flow`.

The two Enfusion servers stay the source for API search, prefabs, and live
Workbench editing (`wb_entity_create`, layers, and the rest). This folder does
not replace them.

## Screenshots

Every shot runs on a **rendering instance** (Workbench play mode or a game
client). `System.MakeScreenshot` captures the screen, so a headless dedicated
server or the `cli test` server cannot produce pixels. Texture checks are
visual only for the same reason.

`EL_ScreenshotCapture`
(`addons/LifeFramework/Scripts/Game/Feature/ScreenshotCapture/EL_ScreenshotCapture.c`)
is the capability. `StartCapture(EL_ScreenshotShot)` runs an async capture:
optional prefab spawn, optional camera aim (reuses the first registered
camera and restores it after), preload, settle, `System.MakeScreenshot`, file
check. Output lands in `$logs:el-captures/<name>_<n>.bmp`, outside the repo
(Workbench: `Documents/My Games/ArmaReforgerWorkbench/logs/el-captures/`), so
captures cannot pollute the addon or the resource database. Every failure
returns a clean `error:<reason>` through `EL_Debug` and never crashes (no
camera on a renderless instance gives `error:no_camera`). The LOGIC test
`screenshot/fail-safe` proves that on the headless server.

The engine writes BMP at native window resolution. The harness and the
`wb_capture` tool convert it to a PNG beside it (pure Node + `zlib`, BI_RGB
24/32 bpp; an exotic variant falls back to the raw BMP path).

Harness shots: UI shots leave the camera where it is. World shots teleport the
character to the camera offset, face the target, and screenshot the player
view, at noon, in Clear weather. The character camera ignores
`CameraManager.SetCamera`, so `EL_ScreenshotCapture.SetCamera` does not move
that view.

### `wb_capture` (Workbench MCP)

`wb_capture` lives on the `enfusion-workbench` MCP server (its
`EMCP_WB_Capture.c` handler uses a start/poll protocol because
`System.MakeScreenshot` writes the file asynchronously). Use that server for
the whole flow so its `wb_launch` deploys the handler:

1. `wb_launch` (gprojPath = the project; a worktree project works if
   `EL_ScreenshotCapture` is compiled in), then `wb_play`.
2. Place the object in edit mode first (`wb_entity_create`) and capture at its
   position, or pass `prefab` + `position` to spawn a fixture at capture time
   (deleted after the shot).
3. Capture, then read the returned PNG:

   ```
   tools\cli call wb_capture '{"position":"128 1 133","cameraOffset":"12 4 12","name":"vehicle_check"}' enfusion-workbench
   ```

   `timeOfDay` (0-24) and `weather` (e.g. `Clear`) pin the environment so
   repeat captures compare. `aimAtTarget: false` captures the current view.

A black or empty image means no renderer was active: check you are in play
mode, not edit.

## Roadmap street visual pass

`node agent-box/harness.mjs report roadmap` is the street visual pass. In a
rendering play session it stands at every `Roadmap_` entity, runs `look`,
takes a world shot, and writes `agent-box/reports/roadmap.html` with each
picture beside that text, plus one wide shot of the row. Names after
`roadmap` limit the walk to that row. A world shot teleports the character to
the camera offset and screenshots the player view.
`EL_ScreenshotCapture.SetCamera` does not move that view. A filtered row's
wide shot stands just north of that row when the street is further north,
because the player camera faces -Z. If a new PNG matches the previous PNG, the
report records `camera_stuck` once and stops.

A row fails when the mesh is white or missing, look offers no interaction on
that site, a menu is blank, or the site is more than 30 m from the rest of the
street. The character's own actions do not count. `Roadmap_Jail_Door` and
`Roadmap_Jail_Cell` pass as painted-light when look returns an action, even if
the center is near-white. A white result on any other site still fails. Menus
are judged from the widget list and `classify` of the `shot ui` picture.

`node agent-box/harness.mjs selftest` marks an empty look and a site 80 m away
as failures. Do not boot the test server for this report. Do not restart
Workbench between sites.

## DebugWorld chains

`node agent-box/harness.mjs chains` checks the probe list against the layer
files, with no game: every required DebugWorld layer has a named stand target
or a recorded gap. `chains --run` is the play-mode e2e: it spawns through
the faction buttons when no character exists, then stands beside each named
probe, reads `look`, and calls `use` on the expected action.
`agent-box/flows/chain-apple.txt` is the apple gather-then-trade sequence on
its own. Probes live in `agent-box/chains.mjs`.

### Probes `chains --run` can reach

| Layer | Entity | Action `look` must show |
| --- | --- | --- |
| ATM | `ATM_Debug` | `EL_ATMAction` |
| Apple | `AppleGatherTree`, `AppleTradeContainer`, `AppleProcessor` | gather, open trader, process |
| Plum | `PlumGatherTree`, `PlumTradeContainer` | gather (prefab only), open trader |
| Tomato | `TomatoGather`, `TomatoTradeContainer` | gather, open trader |
| Farming | `TomatoCrop_Debug` | gather |
| Cement | `CementPlant_Debug`, `CementPile_Debug` | process, gather |
| Gravel | medium and large piles | gather |
| Sand | `SandPile_Debug` | gather |
| Mining | `IronOre_Debug`, `GoldOre_Debug`, `CopperOre_Debug`, `Furnace`, `Pickaxe_Debug` | gather, process, pick up |
| Logging | `LogPile_Debug`, axe, `Sawmill_Debug`, `CuttableTree_1` | gather a log, pick up, process, stand only (melee tree) |
| CrimeChain | `CrimeTrader_Debug`, `CrimeFurnace_Debug` | open trader, process |
| Jobs | `GatherPoint_AppleTree` | gather |
| Shops | general store, black market | `EL_ShopAction` |
| Police | `PoliceStation_Debug` | `EL_DutyAction` |
| Survival | food and water points | gather |
| Houses | `House_Door_Locked` | `SCR_DoorUserAction` |
| Crimes (not `CrimeChain.layer`) | money, weapon, vehicle points | rob actions |
| Trader | `TraderChainCounter` | open trader |

Ore probes stand at the ore meshes (`IronOre_Debug` and friends), not the
empty rock shells 6-8 m away.

### Not an F-key action

- Cuttable pines (`CuttableTree_1` to `_3`) are axe hits through
  `EL_DestructibleResourceComponent`. The probe stands beside the first tree;
  it does not swing.
- `House_Door_Locked` uses vanilla `SCR_DoorUserAction`, which the mod prefab
  does not write, so `layers` omits it; `stand` and `use` address it by name.
- `RobberyPoint_Vehicle` is 7 m from `CrimeScene`; stand on the point, not the
  parent.

### Fixture facts the probes depend on

| Probe | Why it performs |
| --- | --- |
| `TomatoGather`, `TomatoCrop_Debug` | First three crop stage times are 0 and ripe is 30 min, so both are gatherable within a few frames. The crop prefab still inherits `EL_DeleteAction`, which deletes the plant and grants nothing. |
| `SurvivalPoint_Water` | 3.2 m off `SurvivalStation`, inside the look sphere. |
| `CementPlant_Debug` | `Sandbag.et` is a child of the gravel bag with an explicit 1x1 volume slot. The old 2x1 bag was rejected by the inventory, so the plant never saw its third input. |
| `Furnace`, `CrimeFurnace_Debug` | Iron, gold, and copper nuggets use `SLOT_1x1`. `IronOre_Debug` grants 4 nuggets, enough for both furnaces. `use EL_ProcessAction` hits the first action, the iron smelt. |
| `Sawmill_Debug` | `LogPile_Debug` grants `WoodLog_01_Small.et` before any weapon pickup, so the log stays in hand. A later pickup drops it. |
| `PoliceStation_Debug` | The chain opens the debug menu and clicks `FactionPoliceButton` first (sets police and duty); the duty action then toggles. |
| `RobberyPoint_Money` / `_Weapon` / `_Vehicle` | These DebugWorld points set `m_iMinPoliceOnDuty` to 0 (the action classes default to 2 / 1 / 3), so they run while the character is civilian. |

### Launching Workbench for a live run

Launch Workbench from the Arma Reforger install directory with `-gproj`
pointed at the worktree and the default profile. That loads the base-game
addon and the profile EnfusionMCP addon on port 5775. Calling `wb_play` on a
world already open in edit mode can hang the bridge; kill that process and
relaunch. Last full run (2026-10-10): 36 probes, `stand_fail=0`,
`missing_action=0`, `use_fail=0`.
