# Qualifications

> A character takes a course to earn a qualification that other features check. Issue #3424 (slice Q1 of #1035).

## Model

A **course** is a config row (`Configs/Qualifications/LF_Qualifications.conf`, `LF_QualificationConfig`): an id, the
faction it is open to (empty is open to all), the minimum rank, the fee in carried cash, the study time in in-game
minutes, and the **unlock ids** it grants. Unlocks are strings other features check (`detective`, `traffic`), never
a number. A pack adds, replaces, patches or removes courses by id through an overlay (`LF_ConfigOverlay`, #962).

`LF_QualificationRules.Check(config, courseId, faction, rank, held, cash)` is a pure function over plain values. It
returns an `LF_QualificationVerdict`: a pass carries the unlock ids, the fee to charge and the study time to start;
a refusal carries a localization key and, where the sentence has a number, that number (`%1`). The first failing rule
wins, in this order: unknown course, already held, wrong faction, rank too low, short purse.

Nothing in the rules charges money or times the study; the course desk does.

## Held courses (#3425)

`LF_Qualifications` keeps each character's held course ids under the character's record key. `Grant(character,
courseId)` adds a course the table lists; `Has(character, unlock)` reads the held courses' unlock ids from the table each
time, so a pack that changes a course's unlocks changes them for every holder. The `qualifications` save section
(`LF_QualificationsSavePayload`, one row per held course) replaces the whole set on load, drops and logs a course the
table no longer lists, and a boot whose save predates the section (`OnAbsent()`) starts with nobody holding a course; a re-apply to a live session keeps the live set.

## Tests

`LF_Test_QualificationRules` (`qualifications/rules`, LOGIC): the shipped table, a pass with its unlocks, each
refusal, the rule order, an open course, unlock-list trimming, a missing table.

`LF_Test_QualificationsRestart` (`qualifications/restart`, PERSISTENCE): two grants survive a save and load, a
second load changes nothing, an unknown saved course is dropped, `OnAbsent()` gives none; the `wt e2e` reload boot
finds the e2e character's two courses.

## Training desk (#3426)

`LF_TrainingDesk.Take(user, courseId)` is the server step the desk's held action (`LF_TrainingDeskAction`, one per course on `Prefabs/Props/Police/LF_TrainingDesk.et`) runs when the hold completes: on duty, `LF_QualificationRules.Check`, `LF_MoneyUtils.RemoveCash` for the fee, `LF_Qualifications.Grant`, a toast. All or nothing: a failed grant refunds the fee. The prefab's `Duration` per action is a quarter second per in-game study minute. Leaving mid-study never reaches the server, so nothing is charged. Test: `LF_Test_TrainingDesk` (`qualifications/training-desk`, WORLD).
