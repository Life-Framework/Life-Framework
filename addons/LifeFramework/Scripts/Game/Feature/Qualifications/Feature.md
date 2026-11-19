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

Nothing here charges money, times the study or saves a held course. Those belong to the course desk and the
persistence slice that build on this one.

## Tests

`LF_Test_QualificationRules` (`qualifications/rules`, LOGIC): the shipped table, a pass with its unlocks, each
refusal, the rule order, an open course, unlock-list trimming, a missing table.
