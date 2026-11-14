# The nine slices held on a maintainer answer (docs/roadmap/m4-slice-plan.md, "Decisions (2026-11-14)").
# The answers are in. Each held slice is now one of: created, dropped, or deferred past the alpha opening.
# Nothing in this file is created by `slice new`: the dropped cards sit before the first `# Slice` heading, which the plan parser skips,
# and the deferred cards below are created later, under the chapter that follows the opening, with
#   tools\cli slice new <chapter> --plan docs/roadmap/m4-slices/plan-1006-held.md

## Disposition

| Slice | Was held on | Decision (2026-11-14) | Now |
|---|---|---|---|
| O17 Config-only hotfix page | Q4: where the live `config.json` lives | In this repo under `server/`, so changes go through PRs | **Created**: card O17 in [`plan-1006-decisions.md`](plan-1006-decisions.md), with the slices the answer needs (C1, C2) |
| S13 Account link code | Q5: Discord account linking (#805) in M4? | Not in M4 | **Dropped** |
| S14 `/link` command | Q5 | Not in M4 | **Dropped** |
| S15 Notify the bot | Q5 | Not in M4 | **Dropped** |
| S16 Bot links the member | Q5 | Not in M4 | **Dropped** |
| M1 Police calls board | Q10: police missions (#234) before or after opening | After the alpha opens | **Deferred**: not an opening-day slice; created for the chapter after opening |
| M2 Fine call, paid once | Q10 | After the alpha opens | **Deferred** |
| M3 Rank filter | Q10 | After the alpha opens | **Deferred** |
| M4 Station board placement | Q10 | After the alpha opens | **Deferred** |

## Dropped: #805 Discord account linking (S13 to S16)

Reason: the maintainer decided Discord account linking is not in M4 (2026-11-14). #805 stays open in its own milestone and
the four cards stay here as the starting point if it comes back. Nothing in the created cards depends on them. Dropping them also
removes the only slice that would have posted a second webhook (`linkWebhookUrl`). If #805 returns, S15 needs S4 (#1283) merged first.

### S13: An account link code: short, random, bound to one account, expires in ten minutes
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Account/LF_AccountLinkCode.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_AccountLinkCode.c`
- depends: none
- acceptance: test LF_Test_AccountLinkCode
- risk: none
- release-blocker: no

## Capability
The server issues a six-character code for an account uid, keeps only its hash with an expiry, and verifies a code once; a second issue replaces the first.

## Acceptance
LF_Test_AccountLinkCode: issue, verify ok once, verify again fails, expired code fails (fake clock). Red first.

## Notes
Dropped: #805 is not in M4 (decision 2026-11-14). Core stays neutral (no Discord words).

### S14: /link shows a player their code privately
## Card
- type: capability
- tier: T2
- model: sonnet
- machines: session
- budget: 30 min / 45 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Account/LF_AccountLinkCommand.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_AccountLinkCommand.c`
- depends: S13
- acceptance: test LF_Test_AccountLinkCommand
- risk: none
- release-blocker: no

## Capability
A player types `/link` and gets the code in a private toast, rate-limited.

## Acceptance
LF_Test_AccountLinkCommand runs the command for a player and expects one issued code and a rate-limit refusal on the second call within a minute. Red first. The test file `Tests/LF_Test_AccountLinkCommand.c` is owned too.

### S15: The server tells the bot a link code was issued
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 30 min / 45 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Account/LF_AccountLinkNotify.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_AccountLinkNotify.c`
- depends: S13
- acceptance: test LF_Test_AccountLinkNotify
- risk: none
- release-blocker: no

## Capability
On issue, the server posts `{uid, codeHash, expires}` to a separate `linkWebhookUrl` (or the staff one) so the bot can match a pasted code.

## Acceptance
LF_Test_AccountLinkNotify captures the payload and expects the hash, never the plain code. Red first.

### S16: The Discord bot links a member who pastes a valid code
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 60 calls
- owns: `tools/discord-mcp/link.mjs`, `tools/discord-mcp/link.test.mjs`
- depends: S15
- acceptance: fixture tools/discord-mcp/link.test.mjs
- risk: none
- release-blocker: no

## Capability
The bot keeps the posted hashes, and when a member pastes a code that hashes to a live entry it stores `discordId <-> accountUid` in its own store (never the game save) and replies.

## Acceptance
The test feeds a notify payload and a paste and expects one stored link; an expired or wrong code links nothing. Red first.

## Deferred: police missions (#234), M1 to M4

Reason: the maintainer decided police missions come after the alpha opens (2026-11-14). The calls board needs a live roster of
officers, and the opening is about sessions, restarts and staff, not new gameplay. The cards are unchanged below and are created under the chapter
after the opening. No opening-day card depends on them.

# Slice M1: On-duty police see a calls board; off-duty use is refused and logged
## Card
- type: capability
- tier: T2
- model: sonnet
- machines: test
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Police/LF_PoliceCallBoard*.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_PoliceCallBoardDuty.c`
- depends: none
- acceptance: test LF_Test_PoliceCallBoardDuty
- risk: none
- release-blocker: no

## Capability
A calls board component lists open police calls to on-duty officers; an off-duty player's use is refused with a toast and `[LFDebug:Police]`.

## Acceptance
LF_Test_PoliceCallBoardDuty: off-duty refused and logged, on-duty sees the list. Red first.

## Notes
Scope card `docs/roadmap/scopes/later/police-missions.md`. Build on `Feature/Quests` tasks and `Feature/Police/LF_Dispatch.c` (read-only). Deferred: after the alpha opens (decision 2026-11-14). The calls board needs a live roster of officers.


# Slice M2: One call: fine a wanted player at the marker; completion pays the officer once
## Card
- type: capability
- tier: T2
- model: opus
- machines: test
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Police/LF_PoliceMissionFine.c`, `addons/LifeFramework/Configs/Police/LF_PoliceMissions.conf*`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_PoliceMissionFinePaysOnce.c`
- depends: M1
- acceptance: test LF_Test_PoliceMissionFinePaysOnce
- risk: money
- release-blocker: no

## Capability
The first call: go to the marker, fine an eligible wanted player, and the officer is paid in cash once.

## Acceptance
LF_Test_PoliceMissionFinePaysOnce completes the call twice and expects one payment. Red first.

## Notes
Fines stay on `LF_PoliceUtils` and MoneyStack (scope card). Independent reviewer before merge.


# Slice M3: Higher ranks see calls lower ranks do not
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 30 min / 45 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Police/LF_PoliceMissionRank.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_PoliceMissionRank.c`
- depends: M2
- acceptance: test LF_Test_PoliceMissionRank
- risk: none
- release-blocker: no

## Capability
Each call names a minimum rank from the rank config; the board filters by the officer's rank.

## Acceptance
LF_Test_PoliceMissionRank: a cadet does not see a sergeant call, a sergeant sees both. Red first.


# Slice M4: Place the calls board in the flagship station
## Card
- type: visual
- tier: T1
- model: sonnet
- machines: full-profile
- budget: 35 min / 50 calls
- owns: `addons/LF_Everon1989/Prefabs/Props/Police/LF_PoliceCallBoard*`, `addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers/Village_Dressing.layer`
- depends: M1
- acceptance: proof look station (before and after) and audit placement --strict
- risk: none
- release-blocker: no

## Capability
A board prop with the calls component hangs in the station, placed with `tools\cli place`.

## Acceptance
`audit placement --strict` passes; before and after pictures in the PR.
