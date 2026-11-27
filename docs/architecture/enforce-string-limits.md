# Enforce string limits

Where an Enforce string is cut, measured, and every production site that can pass the limit. PR #2520
found it by accident: a save rewritten with `Substring` and `+` was 9388 chars and came back 8204, so
`LoadFromString` failed (`expected 50, got 0`) with no error from the engine. Pinned by
`LF_Test_LongStringLimits` (LOGIC, `tools\cli test --no-build --only LF_Test_LongStringLimits`).

Measured 2026-11-17, Arma Reforger 26266485, sizes 7000 to 140000 chars (slice #2626, parent #2612).

## The limit table

The cap is 8191 chars: one 8192-byte buffer less the terminator. Only two operations cut.

| Operation | Result for a 12 KB / 140 KB input | Cut? |
|---|---|---|
| `a + b`, `a = a + b` | whole (`b + b` of 140000 is 280000) | no |
| `s += x` | whole | no |
| `s.Substring(start, len)` | **at most 8191 chars**, silently; a `len` past 8191 is clamped | **yes** |
| `s.Substring(start, len)` with `start + len` past the end of `s` | VM exception ("Wrong parameter value"); it ends the whole test run | crash |
| `string.Format("%1/%2", long, x)` | **8191 chars** | **yes** |
| `string.Format("%1/%2", x, long)` | **8190 chars** (cut one earlier) | **yes** |
| `s.Replace(a, b)` (in place) | whole, grows past 8191 | no |
| `Split`, `IndexOf`, `Contains`, `Get(i)`, `ToLower` | whole; `Split` rows sum to the input less the separators | no |
| `JsonSaveContext.SaveToString()` | whole (140008 for a 140000 value) | no |
| `JsonLoadContext.LoadFromString` and `ReadValue` | whole | no |
| `FileHandle.Write` then `ReadLine` of one line | whole (140000) | no |
| An RPC string param, either direction | whole at every size tried, 1 KB to 256 KB (`rpc-string-proof`, below) | no |

So a long string is safe to build, hold, hash, split, serialise and write. It is cut only when it goes
THROUGH `Substring` (a long result) or `string.Format` (a long argument). Joining pieces of under 8 KB
with `+` is safe; this is the workaround `LF_Test_SaveNewerVersion.WriteWithVersion` uses (2 KB pieces
to a file).

A cut gives no error and no log line: the string is shorter and valid-looking, and the failure shows up
later (a parse failure, a missing tail). `docs/debug-log-triage.md` has the symptom row.

## Production sites that can pass 8191

Grep of `addons/**/Scripts` (non-test) for `.Substring(` (74 files, 183 hits) and `string.Format(`,
then read where the input can grow. Worst case is at 128 players and the shipped content unless noted.
**Verdict**: `FIX` a real splice, `RPC` a single string sent over the wire (measured whole to 256 KB
by #2674, so `OK` now; sized below), `OK` bounded.

| # | Site | What it builds | Worst case | Verdict |
|---|---|---|---|---|
| 1 | `Core/Config/LF_ConfigMigration.c:88,92,115` `StampVersion`, called from the migration loop (line 166) | Splices `schemaVersion` into the config JSON text with `Substring(open+1, len-open-1)` (the tail) and `Substring(j, len-j)` | The owner's `config.json`: the shipped shops section is 3.8 KB, the stream's own warning threshold is 32 KB. **Any config text past 8191 chars loses its tail at the first migration** | **FIX**: the one real splice, the same shape as #2520. Join pieces of under 8 KB with `+`, or stamp through a JSON context |
| 2 | `Core/Config/LF_ConfigStream.c` payload; `LF_GameModeRoleplay.c:176` `RpcDo_LF_ConfigStream(.., string payload)` | One JSON string of every config section, broadcast | 3.8 KB shipped, 32 KB warn; grows with the owner's `priceRules`, catalogue and terms sections | **OK** (#2674): 3.8 KB is 1.5% of the 256 KB measured whole; the hash check still guards a cut |
| 3 | `Feature/Quests/LF_QuestLog.c` `LF_QuestLogFormat.Encode` (built with `+=`, safe) to `LF_QuestTrackerComponent.c:984` `RpcDo_Log(string serialized)` | `id/status/index:value,...;` per quest | About 35 to 60 B per quest in the log, one owner RPC, so not multiplied by 128. 8191 is reached at about 140 to 230 quests in the log | **OK** (#2674): 12 quests ship (core plus pack), about 0.3 KB |
| 4 | `Feature/Organizations/LF_OrganizationPlayer.c:103` `MemberLines` to `LF_OrganizationCharacter.c:323` `RpcDo_LF_OrganizationLedger(.., string members, ..)` | `name/rank;` for every member, built with `+=` (safe) | About 45 B per member: 128 members in one organisation is about 5.8 KB, long names and ranks reach 8 KB. No org member cap found in the code | **OK** (#2674): names are capped at 32 + 32 (`LF_RegistrationRules.NAME_MAX_LENGTH`) and ranks are config keys, so at most about 100 B a member; 256 KB is about 2600 members |
| 5 | `Feature/Rules/LF_Quiz.c:153` `BuildPayload` to `LF_QuizController.c:63` `RpcDo_LF_ShowQuiz(string payload, ..)`; `RpcAsk_LF_QuizAnswers(string answers)` | The whole question bank, one row per question with its options | About 200 B a question (question 80, four options 30 each): 8191 at about 40 questions | **OK** (#2674): the pack bank has 5 questions sent as localization keys, about 0.6 KB; answers going up are a few bytes |
| 6 | `Feature/CharacterCreation/LF_CharacterCreationController.c:55` `RpcDo_LF_ShowCharacterSelect(string names, ..)` | One account's character names in one string | A few names | OK |
| 7 | `Feature/Map/LF_MapMarksSync.c:330,389`, `LF_OrgMapMarks.c:391-395`, `LF_MapShares.c:317`, `LF_MapMarks.c:84` | `array<string> texts` of marks; each text cut with `Substring(0, TEXT_CAP 32 or TEXT_MAX 64)` | 64 B a text, so the `Substring` is safe; the packet is marks x 64 B across 128 players | OK for the string cut; the array RPC size is a packet question |
| 8 | `Feature/Terms/LF_TermBook.c:337,341,366,367,435,440` | `result += text.Substring(i, length - i)`: the tail of a localised template after the last placeholder | A template text, hundreds of chars | OK until one template passes 8191 chars |
| 9 | `Feature/Moderation/LF_StaffWebhook.c:161` `CutUtf8` | `text.Substring(0, end)`, `end` at most `MAX_TEXT` 1900 | 1900 | OK |
| 10 | `Police/LF_PoliceApplicationGm.c:263` (160), `Ops/LF_HostSay.c:86` (200), `Racing/LF_RaceRecords.c:144` (48), `Police/LF_PoliceApplicationStaff.c:44-60` (one file line) | Substring with a fixed cap, or of one line | Under 300 | OK |
| 11 | `Core/LF_Utils.c:426-431,688,728`, `LF_ContentReload.c:35-37`, `LF_SaveIds.c:195`, `Shop/*`, `Comms/*`, `Rules/*Sheet*`, `Chess/*`, `Medic/LF_TreatmentBills.c`, `Police/LF_Wanted*`, `Packs/*` | Substring of a name, key, id, plate, date stamp, row name or path | A key or id, under 200 | OK |
| 12 | `Feature/AgentHarness/*` (about 35 `Substring` calls); `RpcDo_LF_HarnessResult(.., string detail)` | Harness command parsing and result text | Dev tooling, not a player path | OK |
| 13 | `string.Format(` with a payload argument | Every hit read passes a count, an id or a name and builds a log line (`LF_Debug`); none builds a saved or sent payload | Log lines only | OK |

Not found in production: a `Substring` splice of a save store (the persistence path writes through
`JsonSaveContext`, which keeps a long string whole), a ledger built with `string.Format`, or a board
answer built through `Substring`. `LF_Test_SaveNewerVersion` held the only splice of a save and is fixed.
This is a read of the grep hits, not a proof: an input of 8 KB at a site marked OK was not run.

## Follow-ups to card

1. **Fix** `LF_ConfigMigration.StampVersion` (site 1), the only production splice that cuts. A red test
   that stamps a 12 KB config and checks the tail survives.
2. ~~Measure the RPC string limit~~: done in #2674, whole to 256 KB both ways (section below). Not
   added to `LF_Test_LongStringLimits`: that test is LOGIC tier and an RPC needs a server and a client.
3. ~~Count the shipped quests, quiz questions and org members~~: done in #2674; all far under (section
   below), so nothing is paged.
4. **Guard**: a `validate` check for a `.Substring(` whose length is a `Length()` expression of an
   unbounded string in `Core/` and `Persistence/`, or a rule in AGENTS.md.

## RPC string limit, measured (#2674)

Measured 2026-11-17, Arma Reforger 26266485, a dedicated server and one client on the same PC
(`session up --clients 1`, lowspec), reliable channel. `agent-box/proofs/rpc-string-proof.mjs` drives the
harness verb `rpcstring <chars> down|up` (`Feature/AgentHarness/LF_RpcStringProbe.c`, dev gate): `down` is
one `RplRcver.Owner` RPC from the server, `up` one `RplRcver.Server` RPC from the client. Each side compares
the length and `string.Hash()` with what the sender put in the call.

| Chars | Server to client (owner RPC) | Client to server (server RPC) |
|---|---|---|
| 1024 | whole | whole |
| 4096 | whole | whole |
| 8191 | whole | whole |
| 8192 | whole | whole |
| 8193 | whole | whole |
| 16384 | whole | whole |
| 32768 | whole | whole |
| 65536 | whole | whole |
| 131072 | whole | whole |
| 262144 | whole | whole |

So an RPC string param is not cut at 8191, or at 32 KB, or at 256 KB (the probe's own cap). No log line
on either side mentioned the size. The 8191 cut belongs to `Substring` and `string.Format` only.
Not measured: a link with real latency and loss (the engine splits a reliable message itself, but a
256 KB message to 128 clients is 32 MB of upload at once, which is a bandwidth question, not a cut), and
anything past 256 KB.

**The four single-string RPCs at their real worst case** (sites 2 to 5 above):

| RPC | Worst case shipped | Grows with | Room under 256 KB |
|---|---|---|---|
| `RpcDo_LF_ConfigStream` | 3.8 KB | the owner's `config.json` | 67x |
| `RpcDo_Log` (quest log) | about 0.3 KB, 12 quests | quests added to the pack | about 9000 quests |
| `RpcDo_LF_OrganizationLedger` members | 128 members about 13 KB at the longest names | members (no cap) | about 2600 members |
| `RpcDo_LF_ShowQuiz` | about 0.6 KB, 5 questions | questions in the bank | about 2000 questions |

None can reach the measured size, so no fix slice is filed. `LF_ConfigStream.WARN_BYTES` (32 KB) is a log
threshold below what is now proven and stays as it is.

Rerun: `tools\cli session up --clients 1`, then `node agent-box/session.mjs rpc-string-proof`
(`--sizes 1024,65536` for a subset), then `session down`.
