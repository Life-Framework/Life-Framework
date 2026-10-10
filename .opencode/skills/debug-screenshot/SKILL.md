---
name: debug-screenshot
description: "Use when asked for a screenshot, a look at an area, or to log into the DebugWorld and go somewhere. Drives the Workbench play viewport with windows-mcp."
---

# DebugWorld player screenshot

Prefer the agent box for anything a player does inside the game. It calls the
same interaction and menu code the keys call, and it returns a file path:

```
node agent-box/harness.mjs flow ui-debug-menu
node agent-box/harness.mjs flow ui-shop
node agent-box/harness.mjs press F
node agent-box/harness.mjs shot ui <name>
node agent-box/harness.mjs shot world <EntityName>
```

`press F` is `SCR_InteractionHandlerComponent.ActionPerform`. `input MenuDown`
and `input MenuSelect` are the menu keys. See `agent-box/README.md`.

`wb_capture` aims a free camera and does not log a player in. Use it for the
3D pillar when you do not need the player pawn. A desktop shot of Workbench,
including the faction and spawn buttons, is still a `windows-mcp` `Screenshot`.

This fits in under a minute only when Workbench is already in play mode on
`Worlds/DebugWorld`, windowed, on the 1920x1080 layout below. Do not cold-launch
Workbench inside that budget.

## Server

User MCP `windows-mcp` (`C:\Users\user\.local\bin\windows-mcp.exe`, Python 3.13).
Tools: `Screenshot`, `Click`, `Shortcut`, `Wait`, `Move`, `Type`, `DisplayInventory`.
A new session loads it from `~/.cursor/mcp.json`. Do not reinstall it.

`Screenshot` returns a half-scale image plus `Screenshot Coordinate Scale`
(2.0 at this setting). Multiply every image pixel by that scale before `Click`.
`Screenshot` is the capture to use. `Snapshot` walks UI Automation and is not
enabled. The window list from this server comes back empty on this machine, so
do not depend on `App` to focus Workbench. Focus by clicking the game view.

## Clicks

The intake menus are cursor UI inside the game view, not at desktop (200, 200).
On the current Workbench arrangement the two buttons are stable:

| Button | Screen `loc` |
| --- | --- |
| First (Civilian, then Town Square or Police station) | `[654, 326]` |
| Second (Police, then Mining Camp or Outpost) | `[654, 376]` |

If the panel is not where the last shot showed it, read the button center off
the new image and scale it. Do not reuse these numbers on another resolution.

The first click on an unfocused viewport only focuses Workbench and highlights
the button. If the menu did not change, click the same point again.

## Sequence

1. `Screenshot`.
2. Faction menu: click Civilian unless police was asked, then click again if
   the menu stayed up. Spawn menu: Town Square, Mining Camp, station, or
   outpost. Default area is Town Square, which is the first button, same point
   as Civilian.
3. `Wait` 4 seconds. `Screenshot`. That image is the answer.
4. Already in the world (no menu): do not click. There is no teleport. F10
   opens the debug menu (cash, faction, job). It does not move the player.
   Say which spot they are already standing in.

Do not send WASD or mouse-look. This server does not drive gameplay, and
exclusive fullscreen captures black.
