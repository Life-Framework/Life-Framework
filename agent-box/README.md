# Agent box

Portable setup for a GPU (or any) machine that should build, play, and inspect
Life Framework the way a player sees it. Clone the repo, run one script, and
the Workbench MCP servers plus a small player harness are available.
Player-equivalent actions are `LF_AgentHarness` under
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

Script runs are the existing `[LFTEST]` suite. They refuse the main checkout
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

Session clients once shared one session dir on the machine, so a command from
one worktree reached another worktree's client 1 (a click and a registration)
when a new session started between two runs, and each `up` overwrote the other
checkout's `session.json`, client profiles, and save. Each checkout now has its
own dir (`%LOCALAPPDATA%\LifeFramework\session\<checkout folder>`), `up`
refuses while another checkout's session is live, and the channel is
session-scoped:

- Every `session.mjs up` records a fresh id and the starting checkout in
  `session.json` and launches its clients with `-lfSession <id>`.
- `--client <n>` (and `setBoxes`) addresses each command as
  `<cmd id> s:<session id> <verb> ...`. Before writing, `sendCommand` reads
  `session.json` and refuses with `error session_owned_by_other_checkout
  <root>` when the live session was started from another checkout, or
  `error no_live_session` when none is running.
- A client runs only commands addressed to its own id
  (`LF_AgentCommand.SessionAccepts`); anything else answers
  `error wrong_session` and logs `[LFDebug:AgentHarness] refused command ...`.
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
- `ui` lists the visible widgets `click` accepts: the registration form button
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
  `MenuRight`, `MenuSelect`, `MenuBack`, `LF_OpenDebugMenu`) for about 80ms.
  Movement actions are refused.
- `shoot <playerId>` (dev gate) deals one lethal KINETIC hit from the local character to that
  player through `HandleDamage`, the path a bullet takes, so the safe zone and the murder hook
  see it. Replies `shot player=N refused=R alive=A downed=D` (a lethal hit downs a player, #255).
- `peers` (read-only, runs on the client) lists every player body this client has streamed in as
  `player=N downed=D life=ALIVE|INCAPACITATED|DEAD`: the replicated downed flag and the vanilla
  life state as THIS client sees them, so a late joiner's view of a downed player is readable.
  `state` also reports the own `downed=` and `life=`.
- `session.mjs bed-proof` (2 clients): the medic loads a downed client into the hospital ambulance, the bleed-out stays paused, the medic puts them in `Hospital_Bed_2`, the patient wakes with the reduced bill.
- `siren <EntityName> <mode>` (dev gate) sets a placed vehicle's siren mode on the server through the driver key's rule (`LF_SirenManagerComponent.ApplyRequest`); replies `siren changed=C on=O broadcasts=N`. `session.mjs siren-proof` runs it on the ambulance and the police car and checks client 2.
- `giveup` presses the give-up key for the local player (#256); the server refuses it before `m_fGiveUpDelaySeconds`. `state` reports `bleed=` (seconds left as this client reads them, -1 when not downed).
- `die` asks the server to kill the local character for real, downed or not (dev gate; `LF_Downed.Finish`), so a session
  can prove the death screen and the hospital respawn. `state` also reports
  `pos=x,y,z`, `bills=` (hospital bills carried), and `tickets=` / `owed=`
  (unpaid police tickets carried, the amount on the oldest).
  `session.mjs ticket-proof` drives M1-19 on a 2-client session (roster player 2,
  short ticket deadlines in `LF_Police.conf` for the run).
- `open DebugMenu` toggles, the same as the debug key. `open DebugMenu show`
  opens it and leaves it open, which is what a UI screenshot uses. `hide` closes
  it only when it is up.

Results come back on `result.txt` as `<id> ok ...` or `<id> error <reason>`.
`error no_session` means play mode is not running, or this build does not
contain `LF_AgentHarness` yet.

MCP tools on the `life-agent-box` server match these commands: `box_status`,
`layer_list`, `layer_play`, `player_press`, `player_look`, `player_ui`,
`player_wait_text`, `player_click`, `player_stand`, `player_use`, `player_input`, `player_open`,
`pillar_shot`, `pillar_script`, `player_flow`.

The two Enfusion servers stay the source for API search, prefabs, and live
Workbench editing (`wb_entity_create`, layers, and the rest). This folder does
not replace them.

## Filming

### In-engine (use this)

The game client films itself. `LF_FilmRecorder`
(`Scripts/Game/Feature/Film/LF_FilmRecorder.c`) calls `System.MakeScreenshot`
from a per-frame callqueue tick at a fixed rate into `$logs:el-film/<name>/`
(the window's backbuffer: world, HUD, menus, toasts), and on stop writes
`index.txt` with every shot's real time and the frame-time stats.
`tools/film/engine.mjs` encodes the BMPs into an mp4 at true speed (concat
demuxer with per-frame durations, NVENC, `libx264` fallback), makes the GIF
preview, and deletes the BMPs. No window, focus, or visible desktop is
involved, so it works on the hidden `LF-Agents` desktop. A game client only:
Workbench play writes the 3D view without UI (Screenshots, below), and a
dedicated server refuses with `error no_renderer`.

```powershell
node agent-box/session.mjs up --clients 2
node agent-box/journey.mjs job earn fund atm shop --film      # one mp4 per step + the joined run, linked from journey.html
node tools/film/film.mjs flow world-atm --engine --client 1   # an agent-box flow, filmed in-engine
node agent-box/session.mjs film 20 --client 2                 # 20 s of what client 2 sees
node agent-box/session.mjs proof --film                       # a session proof, filmed (client 1; --film-client n)
node tools/film/film.mjs publish agent-box/reports/film/<clip>.mp4 --pr <N> --yes
node agent-box/session.mjs down
```

The harness verb is `film start <name> <fps> [max seconds]` / `film stop` /
`film status`; `stop` answers `ok $logs:el-film/<name> shots=… seconds=…
fps=… gameFps=… frameMs=… shotFrameMs=… maxGapMs=… stalls=… failed=…`.
`shotFrameMs` is the mean time of the frame right after a shot, `frameMs` of
every other frame, so their difference is what a shot costs. Default 15 fps
(`--film-fps`, `--fps`), at most 30, at most 300 s per recording.

Measured 2026-11-08, RTX 3080, two 1280x720 clients capped at 30 fps
(`-maxFPS 30`) on the hidden desktop: capture 14.7-15.1 fps, game 29.5-30
fps, `shotFrameMs` 33.3-33.5 vs `frameMs` 33.2-34.5, one 243 ms stall in a
64 s journey (the shop menu opening, not a shot frame). At the 30 fps cap a
shot costs nothing visible; uncapped it would show up as `shotFrameMs` above
`frameMs`. A 1280x720 BMP is 2.76 MB, so 15 fps writes about 41 MB/s until the
step is encoded (about 20 ms per frame with NVENC); a journey step's frames are
deleted as soon as its clip is written. A 64 s, six-step journey clip was
7.8 MB.

### Screen capture

`tools/film/film.mjs` records the game window while a flow plays. It does not
talk to the engine; the harness drives the player and ffmpeg records the
screen (desktop duplication `ddagrab`, NVENC when the GPU has it). Screen
capture records whatever is on top, but film never takes focus: on a second
monitor it lifts the game window without activating it, on the primary monitor
it records the rectangle as it is. Screen capture cannot see the hidden
`LF-Agents` desktop: `ddagrab` (DXGI desktop duplication) and `gdigrab` (GDI
BitBlt of the screen) both read only the input desktop the user sees, and a
window on another desktop is never composited into it.
`record --window "Arma Reforger" --pid <n>` picks one client when several
share the title.

An unfocused game window also stops presenting to the desktop: a screen-capture
journey clip on a `--visible` 1280x720 session (2026-11-08) held one frame for
68 s while the harness screenshots showed every screen. That is why the journey
and session films are in-engine (above). Screen capture is still the tool for a
focused window, such as Workbench play on the user's own screen.
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

`LF_ScreenshotCapture`
(`addons/LifeFramework/Scripts/Game/Feature/ScreenshotCapture/LF_ScreenshotCapture.c`)
is the capability. `StartCapture(LF_ScreenshotShot)` runs an async capture:
optional prefab spawn, optional camera aim (reuses the first registered
camera and restores it after), preload, settle, `System.MakeScreenshot`, file
check. Output lands in `$logs:el-captures/<name>_<n>.bmp`, outside the repo
(Workbench: `Documents/My Games/ArmaReforgerWorkbench/logs/el-captures/`), so
captures cannot pollute the addon or the resource database. Every failure
returns a clean `error:<reason>` through `LF_Debug` and never crashes (no
camera on a renderless instance gives `error:no_camera`). The LOGIC test
`screenshot/fail-safe` proves that on the headless server.

The engine writes BMP at native window resolution. The harness and the
`wb_capture` tool convert it to a PNG beside it (pure Node + `zlib`, BI_RGB
24/32 bpp; an exotic variant falls back to the raw BMP path).

**Workbench play captures no UI.** `System.MakeScreenshot` in a Workbench
play session writes the 3D view only: a `shot ui` with the debug menu or the
sign card open shows the world behind it (2026-11-08). Judge a Workbench menu
from `ui` (the widget list) and the `[LFDebug:UIKit]` lines; photograph menus
in a game client.

Harness shots: UI shots leave the camera where it is. World shots teleport the
character to the camera offset, face the target, and screenshot the player
view, at noon, in Clear weather. `shot world <name> x y z ox oy oz` sets the
offset; `0 0 2` reads printed text on a prop whose face points +Z. The posed
character takes no damage for 8 s, so the drop from a high offset does not
kill it. The character camera ignores
`CameraManager.SetCamera`, so `LF_ScreenshotCapture.SetCamera` does not move
that view.

### `wb_capture` (Workbench MCP)

`wb_capture` lives on the `enfusion-workbench` MCP server (its
`EMCP_WB_Capture.c` handler uses a start/poll protocol because
`System.MakeScreenshot` writes the file asynchronously). Use that server for
the whole flow so its `wb_launch` deploys the handler:

1. `wb_launch` (gprojPath = the project; a worktree project works if
   `LF_ScreenshotCapture` is compiled in), then `wb_play`.
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
`LF_ScreenshotCapture.SetCamera` does not move that view. A filtered row's
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

A site may name `when`, the player state its actions need (the armory needs
an on-duty officer, the escort car a restrained suspect). The report walks as
a civilian, so a missing action there is a warning (`needs <when>`), not a
failure. An action on an unnamed prefab child (the duty desk's punch clock)
counts for its site; one named after another entity does not. Before opening a
menu the report stands at the site again, because the world shot moved the
character to the camera, and it waits up to 4.5 s for a server-filled sheet.

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
the registration form when no character exists, then stands beside each named
probe, reads `look`, and calls `use` on the expected action.
`agent-box/flows/chain-apple.txt` is the apple gather-then-trade sequence on
its own. Probes live in `agent-box/chains.mjs`.

### Probes `chains --run` can reach

| Layer | Entity | Action `look` must show |
| --- | --- | --- |
| ATM | `ATM_Debug` | `LF_ATMAction` |
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
| Shops | general store, black market | `LF_ShopAction` |
| Police | `Roadmap_Police_Desk` | `LF_DutyAction` (Clock in / Clock out) |
| Police | `Roadmap_Police_WantedBoard` | `LF_WantedBoardReadAction` |
| Survival | food and water points | gather |
| Houses | `House_Door_Locked` | `SCR_DoorUserAction` |
| Crimes (not `CrimeChain.layer`) | money, weapon, vehicle points | rob actions |
| Trader | `TraderChainCounter` | open trader |

Ore probes stand at the ore meshes (`IronOre_Debug` and friends), not the
empty rock shells 6-8 m away.

### Not an F-key action

- Cuttable pines (`CuttableTree_1` to `_3`) are axe hits through
  `LF_DestructibleResourceComponent`. The probe stands beside the first tree;
  it does not swing.
- `House_Door_Locked` uses vanilla `SCR_DoorUserAction`, which the mod prefab
  does not write, so `layers` omits it; `stand` and `use` address it by name.
- `RobberyPoint_Vehicle` is 7 m from `CrimeScene`; stand on the point, not the
  parent.

### Fixture facts the probes depend on

| Probe | Why it performs |
| --- | --- |
| `TomatoGather`, `TomatoCrop_Debug` | First three crop stage times are 0 and ripe is 30 min, so both are gatherable within a few frames. The crop prefab still inherits `LF_DeleteAction`, which deletes the plant and grants nothing. |
| `SurvivalPoint_Water` | 3.2 m off `SurvivalStation`, inside the look sphere. |
| `CementPlant_Debug` | `Sandbag.et` is a child of the gravel bag with an explicit 1x1 volume slot. The old 2x1 bag was rejected by the inventory, so the plant never saw its third input. |
| `Furnace`, `CrimeFurnace_Debug` | Iron, gold, and copper nuggets use `SLOT_1x1`. `IronOre_Debug` grants 4 nuggets, enough for both furnaces. `use LF_ProcessAction` hits the first action, the iron smelt. |
| `Sawmill_Debug` | `LogPile_Debug` grants `WoodLog_01_Small.et` before any weapon pickup, so the log stays in hand. A later pickup drops it. |
| `Roadmap_Police_Desk` | The chain opens the debug menu and clicks `FactionPoliceButton` first (sets police and duty without the inventory swap); the desk then offers Clock out. A real officer must be on the police roster (whitelist file) to clock in. |
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
`ArmaReforgerSteam.exe`; each client gets its own `-profile`, a window in a row
along the top, and joins `127.0.0.1:2001` with `-client`.

Client size is `--res WxH`, 1280x720 by default; use `--res 1920x1080` for
final evidence. `System.MakeScreenshot` writes the window's backbuffer, so a
`shot ui` is the size of the client and includes every menu, sheet, and toast.
Clients run on the hidden `LF-Agents` desktop: a 1920x1080 window never covers
the user's screen or takes focus, and screenshots still work there.
`--visible` starts the clients on the user's desktop instead (with `-noFocus`),
for screen capture; in-engine filming (Filming) needs neither.

```powershell
node agent-box/session.mjs roster 2             # put player id 2 on the police roster (before up)
node agent-box/session.mjs up --clients 2     # server, then clients; waits for both registration forms
node agent-box/session.mjs up --clients 2 --mission playtest   # LF_PlaytestTown, harness build
node agent-box/session.mjs up --clients 2 --res 1920x1080      # evidence size, hidden desktop
node agent-box/session.mjs up --clients 2 --visible            # on the user's desktop, for screen capture
node agent-box/session.mjs restart            # playtest: shutdown save, load, same clients rejoin
node agent-box/harness.mjs --client 1 ui      # any harness verb, aimed at one client
node agent-box/session.mjs state 2            # faction, duty, wanted, rank, cash, bank, job, org, last toast
node agent-box/session.mjs proof              # client 1 robs Roadmap_Fuel_Till, client 2 (police) gets the alert
node agent-box/session.mjs police-proof       # client 2 clocks in at the desk, client 1 robs the till, both read the wanted board
node agent-box/session.mjs murder-proof       # client 1 kills client 2 at the fuel station (wanted 3), then shoots on the square (refused); needs m_bDownedEnabled 0 in LF_Medic.conf for the run
node agent-box/session.mjs downed-proof       # M2 #255: client 1 downs client 2 (alive, INCAPACITATED, no murder); client 1 and a late-joining client 3 (`session.mjs join 3`) read it downed; the downed player's death is the murder, then the hospital respawn
node agent-box/session.mjs down               # closes every process the session started
```

- `flow arrival-chain` drives the M1-18 arrival chain on a fresh client, from form R-89 to
  the clerk's welcome.
- `up` runs from a worktree and holds the machine-wide `server-2001` lock while
  the server lives (`--wait` queues behind a test or host run). `down` frees it.
- The server runs the bare DebugWorld, not `LF_PlaytestTown.conf`, so the world's
  game mode keeps dev tools open and the harness passes `LF_DevGate`. On the
  playtest mission a non-admin's `stand` and `use` come back as
  `error dev_gate_closed` (LOGIC test `harness/dev-gate-dedicated`).
- `up --mission playtest` runs `LF_PlaytestTown` the way `tools\cli host start`
  does (dev gate closed, debug menu closed, host stop file) and adds
  `-scrDefine LF_HARNESS` on the server and every client. In that build the
  harness RPCs (`stand`, pose, `use`) pass their own gate
  (`LF_DevGate.HarnessBuild`, `[LFDebug:AgentHarness] stand allowed: LF_HARNESS
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
- The session dir is `%LOCALAPPDATA%\LifeFramework\session\<checkout folder>`
  (`LF_SESSION_DIR`), one per checkout, outside every worktree, with no junctions. Client logs are written when the
  client exits; read them after `down`.
- Memory: each client takes about 2.1 GB private, the server about 0.8 GB. With
  Workbench also open the server logged `Out of memory. Free system ~300 MB`
  during client load and kept running.
- Police: there is no faction screen. `roster <id>` writes the server profile's
  `LifeFramework/whitelist.txt` with that player id's session identity before
  `up`; that client can then Clock in at `Roadmap_Police_Desk`. `proof` still
  uses the debug menu (`FactionPoliceButton`), which sets the role without the
  inventory swap. The wanted card's posters are logged on the reading client
  (`[LFDebug:WantedBoard] poster N: ...`); read the client log after `down`.
- `shot world` and `stand` move the body on the owning client (the server
  pose alone is overwritten by a remote client), so `--client <n> shot world`
  photographs from the hidden `LF-Agents` desktop. M1-25 took its village
  evidence this way (2026-11-08).

## Journey (multi-client)

`node agent-box/journey.mjs` (or `harness.mjs journey`, `harness.mjs report
journey`) plays the M1 journey on a running `session.mjs up --clients 2` and
writes `agent-box/reports/journey.html`. Each step is driven from a game client
the way a player does it: `stand` (facing the site), `use`, then the screen's
own buttons and fields (`click`, `type`, `wait-text`). It reads `state` on
the clients it touches before and after, judges the change, and takes
`ui-shot`s: the screens a step opens (form, ATM terminal, price list, cash
sheet, ledger) and one picture from each client it touched at the end of every
step, refusals and toasts included. The report puts the pictures beside the verdict, the state table
(changed cells marked), and every command with its result.

```powershell
node agent-box/session.mjs up --clients 2          # bare DebugWorld
node agent-box/journey.mjs                         # every step that fits the session
node agent-box/journey.mjs atm shop                # only those (arrive runs first when needed)
node agent-box/journey.mjs --list
node agent-box/session.mjs down

node agent-box/session.mjs up --clients 2
node agent-box/journey.mjs job earn atm shop --film   # in-engine: an mp4 per step + the joined run in reports/film/, linked from the report
node agent-box/session.mjs down

node agent-box/session.mjs up --clients 2 --mission playtest
node agent-box/journey.mjs job earn atm restart    # restart: save, stop, load, rejoin, compare
node agent-box/session.mjs down
```

| Step | Client | Done by | Passes when |
| --- | --- | --- | --- |
| `arrive` | 1, 2 | faction Civilian, Town, R-89 form typed and signed | both are CIVILIAN |
| `job` | 1 | `LF_JobSignUpAction` at `Roadmap_Job_Depot` | `job` is not UNEMPLOYED |
| `earn` | 1 | start run, stand at `Roadmap_Job_Drop`, drop off | cash rises |
| `fund` | 1 | debug menu `GiveCashButton` (fixture; bare DebugWorld only) | cash +1000 |
| `atm` | 1 | ATM terminal: key the amount, `DepositButton`, `wait-text DEPOSITED` | bank +amount, cash -amount (1200, or 100 when less is carried) |
| `shop` | 1 | price list `Item0Row_Action`, `PayButton` | cash falls |
| `vehicle` | 1 | `LF_VehicleBuyAction` at `Roadmap_Vehicle_Lot` (S105, 1000 from the bank) | bank -1000; "blocked" when the bank holds less |
| `give` | 1, 2 | stand at `Player<id>`, `LF_GiveCashAction`, type 50, hand over | -50 / +50 |
| `police` | 2 | debug menu `FactionPoliceButton` (fixture; bare DebugWorld only) | POLICE on duty |
| `rob` | 1, 2 | `LF_RobAction` at `Roadmap_Fuel_Till` | wanted and cash rise; an on-duty officer's last toast is the robbery alert |
| `org` | 1 | org desk: type the name in the ledger's field, Sign | `org` is the new name |
| `garage` | 1 | stand at `Roadmap_Garage`, `look` | store or retrieve offered |
| `restart` | 1, 2 | `session.mjs restart` (playtest mission only) | cash, bank, org, job unchanged |

This is the harness M1-23 (#108) proves the journey with, not that proof:
police clock-in at the desk, dispatch, tickets, jail, hideout, stash, and
contraband are M1-23's to add as their scopes land. Debug-menu fixtures make
the bare DebugWorld run reach the car in one pass; the playtest run has no
fixtures, so a step that needs more money than a courier run pays reports
"blocked". First runs (2026-11-08): bare DebugWorld 10 of 12 (org: the desk was
still the old menu, #168), playtest restart kept cash, bank, and org but lost
the signed-up job; the `jobsignups` payload (save version 11) keeps it since.
