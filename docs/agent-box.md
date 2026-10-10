# Agent box

The GPU-box harness lives in [`agent-box/README.md`](../agent-box/README.md).

It is the setup script, the Cursor MCP config, and the three test pillars
(script suite, world screenshot, UI screenshot). Player-equivalent actions are
`EL_AgentHarness` under `Scripts/Game/Feature/AgentHarness/`.

`press F` calls the interaction handler's `ActionPerform` listener. `layers`
scans `Worlds/DebugWorld/DebugWorld_Layers` for entities that own an action.
`layer <EntityName>` stands beside that entity, performs the action, and
screenshots the UI. Named sequences live in `agent-box/flows/` and run with
`node agent-box/harness.mjs flow <name>` (or the `player_flow` MCP tool).

On a T3 server, register the clone as a project titled **Life Framework** and
use the scripts in `agent-box/t3-project.json`. This GPU box already has that
project, bound to `C:\src\Life-Framework`, with worktree threads as the default.
