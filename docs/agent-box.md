# Agent box

The GPU-box harness lives in [`agent-box/README.md`](../agent-box/README.md).

It is the setup script, the Cursor MCP config, and the three test pillars
(script suite, world screenshot, UI screenshot). Player-equivalent actions are
`EL_AgentHarness` under `Scripts/Game/Feature/AgentHarness/`.

`press F` calls the interaction handler's `ActionPerform` listener. `click`
fires the same button invoker or `OnClick` a mouse click does. `layers`
scans `Worlds/DebugWorld/DebugWorld_Layers` for entities that own an action.
`layer <EntityName>` stands beside that entity, performs the action, and
screenshots the UI. Named sequences live in `agent-box/flows/` and run with
`node agent-box/harness.mjs flow <name>` (or the `player_flow` MCP tool).
Chain probes and the gaps they cannot reach are in
[`agent-box-chains.md`](agent-box-chains.md). `node agent-box/harness.mjs chains`
checks them without booting the game. `chains --run` stands, looks, and uses
each named probe in a play session.

## Fresh machine

```powershell
powershell -ExecutionPolicy Bypass -File agent-box\setup.ps1
node agent-box/harness.mjs selftest
```

Open `addons/LifeFramework/LifeFramework.gproj`, build, and play DebugWorld once
so the harness script is in the running session. `node agent-box/harness.mjs status`
prints which installs resolved.

`.cursor/mcp.json` is machine-local. `.cursor/.gitignore` keeps it out of git.

## T3 project

Register the clone as a project titled **Life Framework**. The saved scripts
are in `agent-box/t3-project.json`: toolchain status, agent-box setup, harness
selftest (on new worktrees), DebugWorld layers, play, validate, and harness
status. New threads should use a worktree so the script pillar can run. The
main checkout stays the world editor.

This server already has that project, with worktree threads as the default.
