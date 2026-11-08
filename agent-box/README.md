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

`launch` starts Workbench on this checkout's project with DebugWorld open
(`wb_launch`; a no-op when a Workbench already answers on the bridge port). It
is the one place the harness launches Workbench.

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

Steps: `stand`, `look`, `ui`, `ui text`, `wait-text <text>`, `click`, `type`,
`press F|WheelUp|WheelDown|F10`, `use`, `input`, `open`, `layer`, `shot ui`
(also `ui-shot <name>`), `shot world`, and `wait <ms>`. A shot line
prints `image: <absolute png>` when the capture file is on disk. If the engine
wrote a BMP and the `.png` sibling is missing, the harness converts it.

## Same actions a player gets

### One checkout per command channel

Every Workbench and every game client on the default profile reads the same
`Documents/My Games/.../profile/agent-box/command.txt`, so two worktrees
driving play sessions at once would move each other's character. The first
command from a checkout takes the machine-wide `agent-box-play` lock
(`tmp/wt/locks/` in the main checkout) and hands it to the Workbench or game
process whose command line names that checkout's `addons`. Another
checkout's command then returns `error play_owned_by_other_checkout <root>`.
The lock frees itself when that process exits. A Workbench opened from the
launcher has no project on its command line and runs unclaimed. Clients with
their own `-profile` (the multi-client session) have their own channel.

Session clients share one session dir on the machine
(`%LOCALAPPDATA%\LifeFramework\session`), so a command from one worktree once
reached another worktree's client 1 (a click and a registration) when a new
session started between two runs. The channel is now session-scoped:

- Every `session.mjs up` records a fresh id and the starting checkout in
  `session.json` and launches its clients with `-lfSession <id>`.
- `--client <n>` (and `setBoxes`) addresses each command as
  `<cmd id> s:<session id> <verb> ...`. Before writing, `sendCommand` reads
  `session.json` and refuses with `error session_owned_by_other_checkout
  <root>` when the live session was started from another checkout, or
  `error no_live_session` when none is running.
- A client runs only commands addressed to its own id
  (`EL_AgentCommand.SessionAccepts`); anything else answers
  `error wrong_session` and logs `[ELDebug:AgentHarness] refused command ...`.
  Workbench and default-profile clients have no id and refuse addressed
  commands.
- `session.mjs down` and `restart` refuse a live session another checkout
  started (`--force` overrides). `restart` keeps the id.

LOGIC test `agent-harness/session-scope`; `session.mjs selftest` and
`harness.mjs selftest` cover the address and the refusal.


The play session polls `$profile:agent-box/command.txt` (Workbench profile and
the game profile). Hooks:

- `press F` calls `SCR_InteractionHandlerComponent.ActionPerform`, the same
  listener the interact key uses. `press WheelUp` and `press WheelDown` call
  `ActionScroll`, which moves the highlighted entry. `press F10` opens the
  debug menu.
- `look` lists `ScriptedUserAction`s within 4m whose `CanBeShownScript` is
  true. That is the list the interact key would offer.
- `ui` lists the visible widgets `click` accepts: faction and spawn buttons
  (`SCR_ButtonTextComponent`), debug menu buttons, buttons whose name ends in
  `Button` under a Life Framework handler, and every button of an open UI kit
  sheet whatever its name (they carry `LF_UIButtonRelay`: ATM keys, the price
  list's `Item0Row_Action`, `IdClose`, the sign card's `Close`, the garage's
  `Buy`).
- `ui text` (`harness.mjs ui text`, MCP `player_ui` with `text: true`) reads
  the screen: each clickable with its label, and on a UI kit sheet each field
  with what is typed and each printed line, as `kind:name=text` joined by
  ` | ` (`line:Balance=BALANCE ....... $1,200 | field:RegAge=30 |
  button:DepositButton=DEPOSIT`). The name is the part name the screen gave
  it, the same name `type` and `click` take. `#keys` are translated.
- `wait-text <text> [--timeout ms]` polls `ui text` until a label, field, or
  line contains the text (case-insensitive) or a widget has that name. Use it
  after `use` opens a screen and after a button the server answers
  (`wait-text DEPOSITED`). Times out with `text_not_found <text>`.
- `ui-shot <name>` is `shot ui <name>`: the client's screen with the UI on it.
  Only a game client captures the UI; Workbench play writes the 3D view only.
- `click <WidgetName> [index]` fires that button. Text buttons call
  `m_OnClicked`, the invoker the menu inserted. Other buttons call `OnClick`
  on the menu handler, the same call the engine makes for a mouse click.
  `index` picks a later visible widget when several share a name (`BuyButton`).
- `type <FieldName> <Text>` writes one plain token (letters, digits, `_`) into
  the edit box of a visible field, such as the registration form's
  `RegFirstName`, `RegLastName`, `RegAge`. `click SignButton` then signs it.
- `use` finds a `ScriptedUserAction`, checks `CanBeShownScript` and
  `CanBePerformedScript`, then calls `PerformAction`. Named confirm, same
  checks, no dependence on which row is highlighted. An action with
  `HasLocalEffectOnlyScript` (ATM terminal, shop price list, sign Read) runs
  on the client, the way the F key runs it; other actions run on the server.
- `click` also takes a UI kit list row's `<row>_Action` button (the price
  list's `Item0Row_Action`).
- `stand <EntityName>` asks the server to move the local character beside
  that entity so a 3m action radius can succeed, 1.2 m along +X and turned to
  face it, so `press F` aims at the entity. It does not inject movement keys.
- `input` sets one allowlisted menu action (`MenuUp`, `MenuDown`, `MenuLeft`,
  `MenuRight`, `MenuSelect`, `MenuBack`, `EL_OpenDebugMenu`) for about 80ms.
  Movement actions are refused.
- `die` asks the server to kill the local character (dev gate), so a session
  can prove the death screen and the hospital respawn. `state` also reports
  `pos=x,y,z` and `bills=` (hospital bills carried).
- `open DebugMenu` toggles, the same as the debug key. `open DebugMenu show`
  opens it and leaves it open, which is what a UI screenshot uses. `hide` closes
  it only when it is up.

Results come back on `result.txt` as `<id> ok ...` or `<id> error <reason>`.
`error no_session` means play mode is not running, or this build does not
contain `EL_AgentHarness` yet.

MCP tools on the `life-agent-box` server match these commands: `box_status`,
`layer_list`, `layer_play`, `player_press`, `player_look`, `player_ui`,
`player_wait_text`, `player_click`, `player_stand`, `player_use`, `player_input`, `player_open`,
`pillar_shot`, `pillar_script`, `player_flow`.

The two Enfusion servers stay the source for API search, prefabs, and live
Workbench editing (`wb_entity_create`, layers, and the rest). This folder does
not replace them.

## Filming

`tools/film/film.mjs` records the game window while a flow plays. It does not
talk to the engine; the harness drives the player and ffmpeg records the
screen (desktop duplication `ddagrab`, NVENC when the GPU has it). Screen
capture records whatever is on top, but film never takes focus: on a second
monitor it lifts the game window without activating it, on the primary monitor
it records the rectangle as it is. Screen capture cannot see the hidden
`LF-Agents` desktop, so film a client you started on a visible monitor.
Screenshots (below) work on `LF-Agents`.
If `ddagrab` delivers no frame within 5 s (seen on a box with a virtual
display adapter), it falls back to `gdigrab` of the same rectangle.

Film a flow in a game client playing DebugWorld (Workbench play works too;
pass its play window title):

```powershell
node tools/film/film.mjs fetch          # once: portable ffmpeg into tools/film/.bin (git-ignored)
node tools/film/film.mjs probe          # which encoders actually open on this box
# Game client on DebugWorld (a separate process; no dedicated server needed):
#   ArmaReforgerSteam.exe -world Worlds/DebugWorld/DebugWorld.ent
#     -worldSystemsConfig {2104C177A245B6D1}Configs/Systems/LifeFrameworkSystems.conf
#     -addonsDir <worktree>\addons -addons LifeFramework -window -noBackend
node agent-box/harness.mjs flow ui-intake                     # spawn the character
node tools/film/film.mjs flow film-debugworld --window "Arma Reforger"
node tools/film/film.mjs publish agent-box/reports/film/<clip>.mp4 --pr <N>   # dry run; --yes uploads
```

Use the release `ArmaReforgerSteam.exe`; the Diag build stops on a modal
dialog for a vanilla HUD VME. The exact title `Arma Reforger` wins over
`Arma Reforger Workbench`. The first in-game clip held the machine-wide
`server-2001` lock (`tools/wt.mjs` `acquireLock`) while the client ran, as a
precaution; whether a `-world` client listens on 2001 is not verified.

A clip lands in `agent-box/reports/film/` with a `.preview.gif` and a `.json`
of the flow steps. Check a few frames before publishing
(`ffmpeg -ss 8 -i <clip>.mp4 -frames:v 1 -update 1 f.png`). `publish` uploads both to the rolling `film-evidence`
prerelease and comments on the PR with the GIF inline. `flow film-smoke` is a
capture-only smoke flow that needs no game. Showcase camera passes use
`LF_FilmCameraRunner` (`Scripts/Game/Feature/Film/`), not yet wired to a verb.

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

**Workbench play captures no UI.** `System.MakeScreenshot` in a Workbench
play session writes the 3D view only: a `shot ui` with the debug menu or the
sign card open shows the world behind it (2026-11-08). Judge a Workbench menu
from `ui` (the widget list) and the `[ELDebug:UIKit]` lines; photograph menus
in a game client.

Harness shots: UI shots leave the camera where it is. World shots teleport the
character to the camera offset, face the target, and screenshot the player
view, at noon, in Clear weather. `shot world <name> x y z ox oy oz` sets the
offset; `0 0 2` reads printed text on a prop whose face points +Z. The posed
character takes no damage for 8 s, so the drop from a high offset does not
kill it. The character camera ignores
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

## Town manifest

`agent-box/town.mjs` (`TOWN`) is the one list of M1 sites: spawn, post office,
traders, ATM, shops, vehicle lot, the robbery and breach sites, police
station, jail, org registry, hideout, garage, mining, and farm. Each site names
an entity (`entity`; the spawn is the named town spawn `Spawn_Town`) or, for
unnamed placements like the clerk, a prefab (`prefab`, every placement counts
and the site sits at their centre).
It also lists the actions `look` must offer there, the `LF_Signs.conf` id of
its sign (or `null`), and its journey step. It holds no positions. Sites and
signs resolve against the layer scan (`loadWorld` in `layers.mjs`), so moving
a site to another layer or street needs no manifest edit. A sign is any
placed entity whose `LF_SignComponent.m_sSignId` (inline or from its prefab)
matches.

```powershell
node agent-box/harness.mjs town                 # offline: resolve every site against the layers
node agent-box/harness.mjs report town          # play mode: writes agent-box/reports/town.html
node agent-box/harness.mjs report town atm bank # only those site ids
```

Offline (`town`, `tools\cli validate` via `validate-town-manifest.mjs`), a
site fails when its entity is missing or placed more than once, the layer (or
its addon prefab) lacks an expected action, or it is more than 30 m from
its nearest neighbour. In play mode, `report town` stands at each named site,
waits for the expected actions in `look`, takes a world shot, and opens a menu
action the way `report roadmap` does. It then stands at the sign, checks that
`look` offers `LF_SignReadAction` there, opens Read, and judges the card from
the widget list and a `shot ui`. A row fails when the site is white or
missing, `look` lacks an expected action, or a menu is blank.

Sign checks: no sign with that id within 8 m, a card with no title or no
section text (keys resolved through the string table), Read not offered, or a
card blank in play. `TOWN.requireSigns` is `true` since M1-09 placed the
signs (`Signs.layer`), so these fail the row, offline and in play. Set to
`false`, they print as `sign: pending (...)` warnings instead. `selftest` covers a missing sign, a blank card, a sign 9 m away, an 80 m
outlier, an empty look, an action on another entity, and the flag.

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

## Multi-client session (dedicated server)

One dedicated server and up to three game clients run side by side on one PC
(verified 2026-11-08 on an RTX 3080, 16 GB, Windows 11). Steam starts a second
`ArmaReforgerSteam.exe`; each client gets its own `-profile`, a 640x360 window
along the top of the screen, and joins `127.0.0.1:2001` with `-client`.

```powershell
node agent-box/session.mjs up --clients 2     # server, then clients; waits for both faction menus
node agent-box/session.mjs up --clients 2 --mission playtest   # LF_PlaytestTown, harness build
node agent-box/session.mjs restart            # playtest: shutdown save, load, same clients rejoin
node agent-box/harness.mjs --client 1 ui      # any harness verb, aimed at one client
node agent-box/session.mjs state 2            # faction, duty, wanted, rank, cash, bank, job, org, last toast
node agent-box/session.mjs proof              # client 1 robs Roadmap_Fuel_Till, client 2 (police) gets the alert
node agent-box/session.mjs down               # closes every process the session started
```

- `up` runs from a worktree and holds the machine-wide `server-2001` lock while
  the server lives (`--wait` queues behind a test or host run). `down` frees it.
- The server runs the bare DebugWorld, not `LF_PlaytestTown.conf`, so the world's
  game mode keeps dev tools open and the harness passes `EL_DevGate`. On the
  playtest mission a non-admin's `stand` and `use` come back as
  `error dev_gate_closed` (LOGIC test `harness/dev-gate-dedicated`).
- `up --mission playtest` runs `LF_PlaytestTown` the way `tools\cli host start`
  does (dev gate closed, debug menu closed, host stop file) and adds
  `-scrDefine LF_HARNESS` on the server and every client. In that build the
  harness RPCs (`stand`, pose, `use`) pass their own gate
  (`EL_DevGate.HarnessBuild`, `[ELDebug:AgentHarness] stand allowed: LF_HARNESS
  build`); the debug menu stays refused. The define changes the script
  checksum, so a client without it (a real player) is refused at the door, and
  `host start` never passes it (host selftest; the suite pins
  `HarnessBuild()` false in `harness/dev-gate-dedicated`). Police and cash
  fixtures from the debug menu do not exist there.
- `restart` (playtest mission only) writes the host stop file, waits for the
  server to close after its `SHUTDOWN` save, closes the clients, boots the
  server with `-loadSessionSave`, and relaunches the same client profiles in
  the same order, 15 s apart, so player ids and the `LF_SESSION_IDENTITY`
  accounts match. It waits for `flow start for player <n>` per client.
- Server and clients pass `-noBackend` and `-scrDefine LF_SESSION_IDENTITY`.
  Without the backend every client has the NULL identity and the same Steam
  name, so the define gives each player id its own identity. The define must be
  on both sides; a mismatch is refused with `remote script source code checksum
  does not match`.
- The session dir is `%LOCALAPPDATA%\LifeFramework\session` (`LF_SESSION_DIR`),
  outside every worktree, with no junctions. Client logs are written when the
  client exits; read them after `down`.
- Memory: each client takes about 2.1 GB private, the server about 0.8 GB. With
  Workbench also open the server logged `Out of memory. Free system ~300 MB`
  during client load and kept running.
- Police comes from the debug menu (`FactionPoliceButton`): the session has no
  whitelist file, so the faction menu refuses police.
- `shot world` and `stand` move the body on the owning client (the server
  pose alone is overwritten by a remote client), so `--client <n> shot world`
  photographs from the hidden `LF-Agents` desktop. M1-25 took its village
  evidence this way (2026-11-08).
