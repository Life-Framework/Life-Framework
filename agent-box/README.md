# Agent box

Portable setup for a GPU (or any) machine that should build, play, and inspect
Life Framework the way a player sees it. Clone the repo, run one script, and
the Workbench MCP servers plus a small player harness are available.

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

`layers` reads `Worlds/DebugWorld/DebugWorld_Layers`. `layer <EntityName>` stands beside that entity, waits until `look` shows that action (the same list the F key would offer), performs it, and screenshots the UI. `shot world <EntityName>` uses the entity coordinates when you omit x y z. `shot ui debug_menu` opens the debug menu first.

`play` stops play mode if it is up, opens `Worlds/DebugWorld/DebugWorld.ent`, then enters play mode. It passes this machine's toolchain paths so the Workbench bridge compiles into the open mod.

Named flows are the short version of a pillar. Each file in `agent-box/flows/`
is the sequence a player would take:

```powershell
node agent-box/harness.mjs flow ui-intake
node agent-box/harness.mjs flow ui-debug-menu
node agent-box/harness.mjs flow ui-debug-menu-nav
node agent-box/harness.mjs flow ui-shop
node agent-box/harness.mjs flow world-atm
node agent-box/harness.mjs flow chain-apple
node agent-box/harness.mjs chains
node agent-box/harness.mjs chains --run
```

`chains` checks that every required DebugWorld layer has a named stand target
or a recorded gap. `chains --run` is the play-mode e2e: stand, look, and use.
See `docs/agent-box-chains.md`.

Steps: `stand`, `look`, `ui`, `click`, `press F|WheelUp|WheelDown|F10`, `use`, `input`,
`open`, `layer`, `shot ui`, `shot world`, and `wait <ms>`. A shot line prints
`image: <absolute png>` when the capture file is on disk. If the engine wrote
a BMP and the `.png` sibling is missing, the harness converts it.

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

Screenshots go through `EL_ScreenshotCapture` (`$logs:el-captures/`). UI shots
leave the camera where it is. World shots aim at the position, at noon, in
Clear weather.

MCP tools on the `life-agent-box` server match these commands: `box_status`,
`layer_list`, `layer_play`, `player_press`, `player_look`, `player_ui`,
`player_click`, `player_stand`, `player_use`, `player_input`, `player_open`,
`pillar_shot`, `pillar_script`, `player_flow`.

The two Enfusion servers stay the source for API search, prefabs, and live
Workbench editing (`wb_entity_create`, layers, and the rest). This folder does
not replace them.

## Fresh machine

1. Install Git, Node 18+, and the Reforger game, tools, and dedicated server.
2. Clone and `git config core.hooksPath .githooks`.
3. Run `agent-box\setup.ps1`.
4. Open `addons/LifeFramework/LifeFramework.gproj` and play DebugWorld once so
   the harness script is compiled in.

`node agent-box/harness.mjs selftest` checks the command formatter with no
game running. `node agent-box/harness.mjs status` prints which installs resolved.
