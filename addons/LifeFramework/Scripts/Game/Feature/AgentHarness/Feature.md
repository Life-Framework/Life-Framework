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
the refusal (`dev_gate_closed`). In a build compiled with `-scrDefine LF_HARNESS` (session builds only) the
stand-ins `drill`, `keycard`, `charge`, `shelf`, `crate`, `keys`, `scanner`, `shoot`, `shootnpc`, `siren` and `die`
pass the harness gate like `stand` and `use`, so a playtest-mission session can film the heists (#571); any
other build still needs an administrator or dev tools. `sound [SOUND_EVENT]` (`LF_AgentHarness_Sound.c`) runs on the
client and needs no gate: it reports which sound events the weapon in hand knows, plays the named one there
(`DIRECT_<event>` plays it through `AudioSystem` instead of the weapon's sound component), and reports the stun buzz. `state` reads the account, money, job, and
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

`LF_HarnessStaff.c` is the staff side of the police application for the proofs (#1450): the `staffapprove <uid>`
verb (server, through `LF_AskHarnessScanner`) calls `LF_PoliceApplicationStaff.ApproveByUid` with the sender's account and
`AdminChatChannel.IsAdminPlayer`, so the staff check still decides. `up --admin-client N` seeds `harness-admins.txt` and a harness
build gives that client `SESSION_ADMINISTRATOR` (`GrantAdminIfListed`, on connect and on spawn; the engine refuses a script grant of
`ADMINISTRATOR`), which `IsAdminPlayer` counts in a harness build only; `--no-instant-approval` and
`--skip-playtime-gate` seed `harness-flags.txt`, which the application form reads through `InstantApproval` and
`ApplyMinPlaytimeHours`. Every entry point is a no-op unless `LF_DevGate.HarnessBuild()`.

`LF_HarnessChat.c`: harness `chat [channel] <text>` sends one chat line on this client through `SCR_ChatComponent.SendMessage`, so the server's channel handler (`AdminChatChannel` for `!handled`, `!wadd`, `!preject`) sees a real sender and re-checks its role.
