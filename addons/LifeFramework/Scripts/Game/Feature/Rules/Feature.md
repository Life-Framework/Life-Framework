# Rules

## Intent

A new player meets the server's rules as part of arriving, not on a board they can ignore. A short
multiple-choice quiz stands in front of registration, is passed once per account, and explains the
rule behind every wrong answer.

## Interaction pattern

Background plus one sheet (rung 4). There is no world object: the quiz opens by itself on a new
player's screen before the registration form (R-89), the moment the first-join flow
(`LF_CharacterCreationManager.ContinueFlow`) finds the account has not passed. It is a sheet because
reading a rule and picking an answer needs text; it asks nothing a world object could show.

## Quiz (#915)

- **Player sees:** the orientation sheet, one question per page with one button per answer. After the
  last answer a result page: the score, the rule behind each wrong answer and Try again, or Continue
  to registration. Failing the last allowed attempt shows the rules, then removes the player after six
  seconds (a kick, so they can rejoin and start over).
- **Owner and side:** `LF_Quiz` decides on the server (who is asked, scoring, attempt count, the pass).
  `LF_QuizFlow` (`LF_QuizController.c`) moves the player between the sheet and the flow. `LF_QuizSheet`
  is client only. The client is sent question and option text, never the right answer or the rule text,
  and sends back option numbers; the server rescores everything. `LF_CharacterCreationManager` asks
  `LF_Quiz.Required` again on register and on character pick, so a client that skips the sheet is
  sent the questions again.
- **Data:** `LF_QuizConfig` (`Configs/Rules/LF_Quiz.conf`, owner section `rulesQuiz`): `enabled`,
  `repeatOnConnect` (ask on every connect instead of once), `passPercent` (80), `maxAttempts` (3,
  0 = unlimited). `LF_QuizBank` (`Configs/Rules/LF_QuizBank.conf`) holds the questions and ships none:
  the core mechanism is neutral, the flagship's five questions are the pack overlay
  `LF_Everon1989/Configs/Rules/LF_Quiz_Everon.conf` (listed in the mission header), text in the pack's
  string table. A pack overlay that lists questions replaces the list. With no questions, or the
  switch off, nobody is asked. A malformed question is logged and left out.
- **Persisted:** the pass is the entry `flag:rules-quiz` in the account's `progression` record
  (`LF_Progression.NoteFlag`), keyed by account uid, so it is saved with the existing payload and
  needs no new save field or version. `LF_QuestTracker.RestoreTurnedIn` ignores `flag:` entries so a
  character's quest log never shows it. The failed-attempt count is session only.
- **Staff:** `LF_Quiz.Reset(uid)` forgets one account's pass so it retakes the quiz; `LF_Quiz.StatsLine()`
  prints attempts, passes, fails and kicks. Both log through `LF_Debug` (`[LFDebug:Rules]`), as does
  every attempt. They are script entry points: no chat command or sheet yet.
- **Proof:** `rules/quiz` (LOGIC), `persist/quiz-pass` (PERSISTENCE, a save and read round trip), the
  pass is also in `e2e/pass1-seed` and `pass2-verify-load` (a real restart), and
  `node agent-box/session.mjs quiz-proof`. A proof that arrives through the registration form
  (`arrive` in `agent-box/session.mjs`) answers the quiz right first (`passQuiz`).

## Join notice (#1448)

- **Player sees:** one toast, about 12 seconds, at their first spawn of the session (not on a respawn).
- **Owner and side:** `LF_JoinNotice` (`LF_JoinNotice.c`) subscribes to `LF_OnPlayerSpawnedEvent` on the
  server and sends the toast to that player only (`LF_NotificationManagerComponent.NotifyPlayer`).
- **Data:** `LF_JoinNoticeConfig`, owner section `joinNotice`: `enabled` (false in the core) and `key`
  (a localization key, with or without `#`). The pack ships the text: `LF_Everon1989_AlphaNotice`.
  Enabled with an empty key logs one `[LFDebug:JoinNotice]` warning per session and shows nothing.
- **Persisted:** nothing; "once" is per account uid per world session.
- **Proof:** `rules/join-notice` (LOGIC).

## Handbook pages (#1772)

- **Player sees:** nothing yet; this is the data half of the handbook (#642). A later sheet reads the pages.
- **Owner and side:** `LF_Handbook` (`LF_Handbook.c`) loads and orders the pages; `LF_HandbookConfig.c` holds the
  page record, the config root and the list the shared override model (`LF_ConfigOverlay`) merges.
- **Data:** `Configs/Rules/LF_Handbook.conf`: pages with `m_sId`, `m_sTitleKey`, `m_aBodyKeys`, `m_iOrder` (lowest
  first; -1 sorts last in file order). The core ships two neutral pages; a pack overlay adds, replaces,
  patches or removes a page by id. A page with no id, title or body paragraph is logged and left out.
  Not an owner config section yet.
- **Persisted:** nothing.
- **Proof:** `handbook/pages` (LOGIC).

Not done: a world prop at the clerk's desk, the "cannot leave the arrival room" gate beyond no
registration and no spawn, staff chat commands, shuffled options.
