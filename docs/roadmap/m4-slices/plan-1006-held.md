# Held slices for #1006: each waits on a maintainer answer (docs/roadmap/m4-slice-plan.md, Open questions).
# Create with: toolscli slice new 1006 --plan docs/roadmap/m4-slices/plan-1006-held.md
# Cross-plan dependencies the tool cannot express: S15 needs S4 merged; O17 needs O16 and O9 merged. Create them after those merge.

# Slice O17: Config-only hotfix path in the runbook
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 45 calls
- owns: `docs/ops/hotfix.md`
- depends: none
- acceptance: proof validate (docs links) and the checklist in Acceptance
- risk: none
- release-blocker: no

## Capability
A page for a live config fix: edit `config.json`, `config-check`, backup, warned restart, verify with `server-doctor`; and when a fix needs a script change instead (PR and pack release).

## Acceptance
The page's commands exist; it says where the flagship's `config.json` lives (from the answer to Q4); `validate` passes. Manual (#310): a price fix goes live in one warned restart.

## Notes
Hold until Q4 (where the flagship config lives) is answered.

# Slice S13: An account link code: short, random, bound to one account, expires in ten minutes
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
#805 is optional: hold S13 to S16 until Q5. Core stays neutral (no Discord words).

# Slice S14: /link shows a player their code privately
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

# Slice S15: The server tells the bot a link code was issued
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

# Slice S16: The Discord bot links a member who pastes a valid code
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
Scope card `docs/roadmap/scopes/later/police-missions.md`. Build on `Feature/Quests` tasks and `Feature/Police/LF_Dispatch.c` (read-only). Hold for Q10.

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
