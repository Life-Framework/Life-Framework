# Agent harness

Dev-only bridge so an agent on a rendering play session can do what a player
does, then screenshot the result. It is not a gameplay feature and it does not
ship as player content.

The client polls `$profile:agent-box/command.txt`. `press F` calls
`SCR_InteractionHandlerComponent.ActionPerform`. Wheel scroll calls
`ActionScroll`. `click` invokes the same button the mouse would: `m_OnClicked`
for text buttons, `OnClick` for shop and ATM buttons. Named `use` still runs `CanBeShownScript` and
`CanBePerformedScript` before `PerformAction`. Menu keys are an allowlist
(`MenuUp`, `MenuDown`, `MenuLeft`, `MenuRight`, `MenuSelect`, `MenuBack`,
`LF_OpenDebugMenu`). Movement actions are refused.

`stand` is the one exception: the server moves the local character beside a
named DebugWorld entity so a 3m action radius can succeed. It does not inject
WASD.

Screenshots go through `LF_ScreenshotCapture`. UI shots leave the camera
alone. World shots aim at a position.

The Node side, MCP tools, and the three pillars (script, 3D world, UI) live
in `agent-box/`. See `agent-box/README.md`.

On a dedicated server (`agent-box/session.mjs`) the server answers `stand`,
`use`, and `state` over an owner RPC, so a client's harness gets the result or
the refusal (`dev_gate_closed`). `state` reads the account, money, job, and
organization on the server and adds the client's last toast.

`peers` is a client-local read of every streamed-in player body (`downed=`, vanilla `life=`), the
view a late joiner has of a downed player (`session.mjs downed-proof`, M2 #255).

`ui text` reads an open UI kit sheet the way a player reads it: each button
with its label, each field with what is typed, each printed line, named by the
part the screen gave it. `click` takes any kit button (`LF_UIButtonRelay`).
`stand` turns the character to face the entity.

The playtest mission closes the gate. A session started with
`-scrDefine LF_HARNESS` (`session.mjs up --mission playtest`) opens it for the
harness RPCs only (`LF_DevGate.CheckHarnessRequest`); the define changes the
script checksum, so only clients with the same define can join that server.
