# Agent harness

Dev-only bridge so an agent on a rendering play session can do what a player
does, then screenshot the result. It is not a gameplay feature and it does not
ship as player content.

The client polls `$profile:agent-box/command.txt`. `press F` calls
`SCR_InteractionHandlerComponent.ActionPerform`. Wheel scroll calls
`ActionScroll`. Named `use` still runs `CanBeShownScript` and
`CanBePerformedScript` before `PerformAction`. Menu keys are an allowlist
(`MenuUp`, `MenuDown`, `MenuLeft`, `MenuRight`, `MenuSelect`, `MenuBack`,
`EL_OpenDebugMenu`). Movement actions are refused.

`stand` is the one exception: the server moves the local character beside a
named DebugWorld entity so a 3m action radius can succeed. It does not inject
WASD.

Screenshots go through `EL_ScreenshotCapture`. UI shots leave the camera
alone. World shots aim at a position.

The Node side, MCP tools, and the three pillars (script, 3D world, UI) live
in `agent-box/`. See `agent-box/README.md`.
