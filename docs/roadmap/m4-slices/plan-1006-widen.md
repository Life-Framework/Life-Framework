# Slices for #1006 from the widening re-cut of 2026-11-14 (docs/roadmap/m4-slice-plan.md, "Widening the graph").
# Contract-first slices land an interface, a data shape or a stub so their dependents stop waiting for a finished
# feature; split slices take the test, doc or data part off the critical path. The revised cards of the older issues are
# patched in place in plan-1006.md and plan-1006-decisions.md; this file holds only the new cards.
# Dependencies on keys outside this file (O5, S1, P1, G1 ...) are issue numbers once created; the issue bodies carry them.
# Created 2026-11-14: CF #1343, OC #1344, OS #1345, SW #1346, SJ #1347, O6T #1348, O13W #1349, O16R #1350, G3C #1351, G4E #1352.

# Slice CF: The M4 config sections exist as shapes: restarts, staff webhook and day cycle
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Ops/LF_RestartConfig.c`, `addons/LifeFramework/Scripts/Game/Feature/Moderation/LF_StaffWebhookConfig.c`, `addons/LifeFramework/Scripts/Game/Feature/Atmosphere/LF_DayCycleConfig.c`, `addons/LifeFramework/Configs/Ops/**`, `docs/config/config.schema.json`, `docs/config/config.example.json`, `tools/config/schema-overrides.json`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_M4ConfigSections.c`
- depends: none
- acceptance: test LF_Test_M4ConfigSections
- risk: none
- release-blocker: no

## Capability
The three config sections M4 adds are declared once, with no behaviour behind them: `restarts` (daily times HH:MM, warning leads in minutes, default 15, 5, 1; empty list means no scheduled restarts), `staff` (`webhookUrl`, empty by default) and `dayCycle` (mode `vanilla`, `accelerated` or `realtime`, day length in real minutes, night speed, start hour, clock offset; default `vanilla`). The schema and example are regenerated in this one slice, so O5, S4 and X1 read the classes and never touch the generated files.

## Acceptance
LF_Test_M4ConfigSections (`config/m4-sections`, LOGIC): a fixture config with all three sections reads back every value through `LF_ConfigService`; a config without them gives no restarts, an empty webhook URL and `vanilla`. `node tools/config/schema.mjs --check` is clean after the regen. Red first.

## Notes
Contract slice: K1 (#1258) no longer gates O5, S4 and X1, because they no longer add config fields. Section classes follow `Core/Config/Feature.md` (`[BaseContainerProps(configRoot: true)]`, `static const string SECTION`). Field names are the contract the dependents code against: write them in each class's comment exactly. The flagship value (a 120-minute accelerated day) goes in `server/flagship/config.json` (C1), never as a core default. The `staff.webhookUrl` value is host-only (C2).

# Slice OC: Restart warnings are bus events, and the game mode starts the M4 tickers
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Ops/LF_OpsEvents.c`, `addons/LifeFramework/Scripts/Game/Feature/Ops/LF_RestartWarner.c`, `addons/LifeFramework/Scripts/Game/Feature/Ops/Feature.md`, `addons/LifeFramework/Scripts/Game/Feature/Atmosphere/LF_DayCycleApply.c`, `addons/LifeFramework/Scripts/Game/Core/LF_GameModeRoleplay.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_OpsEventsContract.c`
- depends: none
- acceptance: test LF_Test_OpsEventsContract
- risk: none
- release-blocker: no

## Capability
`LF_OpsEvents.c` declares `LF_OnRestartWarning` (minutes left, seconds to the restart) and `LF_OnRestartDue` on the server bus; `LF_RestartWarner.StartIfConfigured()` and `LF_DayCycleApply.ApplyFromConfig()` exist as stubs that log one `[LFDebug:Ops]` / `[LFDebug:Atmosphere]` Info line and return; the game mode calls both beside `LF_HostStop.StartIfRequested()`. After this slice no M4 slice edits `Core/LF_GameModeRoleplay.c`.

## Acceptance
LF_Test_OpsEventsContract (`ops/events`, LOGIC): a subscriber receives one `LF_OnRestartWarning` with the published minutes and one `LF_OnRestartDue`; both stubs run without a VME and log their line. Red first.

## Notes
Contract slice: the toast (O6), the siren (O7) and the close (O8) subscribe to these events instead of waiting for the ticker, and X2 fills `LF_DayCycleApply.c` without touching the game mode. Event shape: `Feature/Moderation/LF_ModerationEvents.c` (plain values only, `docs/architecture/events.md`); `validate-registries` checks the file. `Feature/Ops/Feature.md` describes every planned Ops file (schedule O5, ticker O6T, toast O6, siren O7, close O8) so the later slices do not edit it.

# Slice OS: The restart siren exists as a prefab the placer can use
## Card
- type: visual
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Ops/LF_RestartSirenComponent.c`, `addons/LifeFramework/Prefabs/Props/Ops/**`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_RestartSirenPrefab.c`
- depends: none
- acceptance: test LF_Test_RestartSirenPrefab
- risk: none
- release-blocker: no

## Capability
A siren prop prefab (a vanilla pole-mounted loudspeaker or siren mesh) carries `LF_RestartSirenComponent`, which registers with `LF_Places` under the kind `restart-siren` and does nothing else yet; O7 gives it a voice. It is a core option for other servers: the flagship pack places none (decision 2026-11-15), so P1 does not use it.

## Acceptance
LF_Test_RestartSirenPrefab (`ops/siren-prefab`, WORLD): spawns the prefab, finds it through `LF_Places` by kind, and its mesh resolves (not an empty `MeshObject`). Red first. One `look` picture of the prop in the PR.

## Notes
Contract slice: it takes the prefab off O7, so the siren logic stays separate from the prefab. Vanilla assets first (`asset_search`, `game_duplicate`); a new prefab's `.meta` GUID is generated, never copied. Map independence: no coordinates. O7 fills the component's behaviour and owns the file after this slice.

# Slice SW: The staff webhook has its interface: one Send call with a capture seam
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 30 min / 45 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Moderation/LF_StaffWebhook.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_StaffWebhookSeam.c`
- depends: none
- acceptance: test LF_Test_StaffWebhookSeam
- risk: none
- release-blocker: no

## Capability
`LF_StaffWebhook.Send(string text)` exists on the server. A test seam (`SetCaptureForTest`) records sent lines; without a capture this stub logs one `[LFDebug:Moderation]` Info line and sends nothing. S4 later puts the real POST behind the same call.

## Acceptance
LF_Test_StaffWebhookSeam (`moderation/webhook-seam`, LOGIC): with a capture installed two `Send` calls record two lines in order; with none, `Send` logs and returns. Red first.

## Notes
Contract slice: the relays (S5, S10) code against `Send` and the seam and no longer wait for S4's HTTP work. Keep the class server-only; clients never call it.

# Slice SJ: sessions.json has a schema the announce command and the servers page share
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 45 calls
- owns: `website/content/sessions.schema.json`, `tools/validation/sessions-json-selftest.mjs`, `tools/validation/fixtures/sessions/**`
- depends: none
- acceptance: fixture tools/validation/sessions-json-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`website/content/sessions.schema.json` fixes the shape `host announce` writes and the servers page reads: `{ "next": null | { "at": ISO 8601 with offset, "build": string, "wipe": bool, "announced": ISO 8601 } }`, plus two fixtures (a scheduled session, none scheduled).

## Acceptance
The selftest validates both fixtures, rejects a `next` without `at` and a `wipe` that is not a boolean, and, when `website/content/sessions.json` exists, validates it too. Red first.

## Notes
Contract slice: G7 builds against the fixtures instead of waiting for O14. Decision 2026-11-14: next session time only; no player count and no up/down field, so the schema has none.

# Slice O6T: The restart ticker publishes the warnings from the schedule
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Ops/LF_RestartWarner.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_RestartWarnerFires.c`
- depends: O5, OC
- acceptance: test LF_Test_RestartWarnerFires
- risk: none
- release-blocker: no

## Capability
On the server a ticker (once a second, not per frame) reads the schedule and publishes `LF_OnRestartWarning` at each lead time and `LF_OnRestartDue` at the restart time, each once, and logs `[LFDebug:Ops]`.

## Acceptance
LF_Test_RestartWarnerFires drives the warner with a fake clock across 17:44 to 17:46 (restart 18:00, leads 15, 5, 1) and expects exactly one warning event for the 15-minute lead; across 17:59:58 to 18:00:02 one due event. Red first.

## Notes
Split from O6 (#1265): O6 is now the toast, which only subscribes. Fills the stub OC landed in `LF_RestartWarner.c`; the game mode already calls `StartIfConfigured()`.

# Slice O13W: server-doctor shows the effective config and marks known log lines
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 45 calls
- owns: `tools/lib/server-doctor.mjs`, `tools/validation/server-doctor-wire-selftest.mjs`
- depends: O10, O12, O13
- acceptance: fixture tools/validation/server-doctor-wire-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`server-doctor` on a profile folder adds the effective-config section from `tools/lib/server-doctor-config.mjs` and marks each error line known or new through `tools/lib/log-triage.mjs`; a bad config field turns the verdict to FAIL.

## Acceptance
The selftest runs the O12 good-boot fixture with the O13 bad-config fixture and expects both sections and FAIL; with the good config, PASS and the known/new marks. Red first.

## Notes
Split from O12 and O13, which now build their parts side by side and leave the wiring here.

# Slice O16R: The ops pages match the real host commands
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 50 calls
- owns: `docs/ops/session-runbook.md`, `docs/ops/hotfix.md`, `docs/ops/rented-box.md`, `tools/validation/ops-doc-commands-selftest.mjs`
- depends: O2, O3, O8, O10, O14, O15, C2, O16, O17, O18
- acceptance: fixture tools/validation/ops-doc-commands-selftest.mjs
- risk: none
- release-blocker: no

## Capability
The runbook, the hotfix page and the rented-box checklist were written from the plan; this slice turns every planned command they name into the real one (flags as `tools\cli host help` prints them) and links the pages that now exist.

## Acceptance
The selftest finds every `tools\cli host <sub>` and `tools\cli <cmd>` in the three pages and fails on any that `host help` or `tools\cli help` does not list; no "planned" marker is left. `validate` passes. Red first: run it before the edits and see the planned names fail.

## Notes
Split from O16 (#1275), O17 (#1324) and O18 (#1276) so those three docs start now. Manual (maintainer, #305): a second person runs a session from the runbook.

# Slice G3C: Film trailer shots seven and eight: the recruit panel and the rules board
## Card
- type: visual
- tier: T1
- model: sonnet
- machines: full-profile
- budget: 45 min / 60 calls
- owns: `website/public/media/m4-trailer/c/**`
- depends: G1, P1, S9
- acceptance: proof film shots m4-trailer 7-8
- risk: none
- release-blocker: no

## Capability
The two shots that need M4 work: an administrator approving a police recruit in the Game Master panel, and the rules board at the plaza (no siren: the flagship places none), filmed on the full profile.

## Acceptance
Two clips exist and pass the film tool's checks; one still each in the PR.

## Notes
Split from G3 (#1294), which keeps shots five and six (the quiz and a moderator at the incident panel, both built before M4) and no longer waits for P1 and S9. Use the shot numbers G1's list gives these two subjects.

# Slice G4E: The trailer edit: storyboard, captions, music bed and the run-your-own end card
## Card
- type: visual
- tier: T1
- model: sonnet
- machines: none
- budget: 40 min / 60 calls
- owns: `video/m4-trailer.json`, `video/cues/m4-trailer/**`
- depends: G1
- acceptance: fixture video/m4-trailer.json against video/storyboard.schema.json
- risk: none
- release-blocker: no

## Capability
The post-production storyboard for the 60 to 90 second cut: clip order and lengths from G1's shot list pointing at the `a/`, `b/` and `c/` clip folders, caption cues, the music bed and the title and run-your-own end cards (framework, install guide and generator links, as on the join page).

## Acceptance
`video/m4-trailer.json` validates against `video/storyboard.schema.json`; `tools\cli film-make video/m4-trailer.json --no-vo` with placeholder clips renders the title and end cards. Red first: the file is missing.

## Notes
Split from G4 (#1295), which then only renders the cut from real clips. Format: `video/README.md`, `tools/film/post.mjs`, the earlier `video/trailer-571.json`. Commit the edit with the job (the film scripts rule).
