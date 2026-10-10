# Tasks

Phase: After release
Extends: `Quests` (`LF_QuestTrackerComponent`)

## Player loop

A workplace or faction office offers a task. The player accepts it in the world, completes the objective, and is paid in cash. The tracker already stores definitions, progress, expiry, and rewards.

## In scope

- Tasks authored as data on the existing tracker.
- One legal example (a delivery that can replace a hard-coded job run) and one police example once [police missions](police-missions.md) need it.
- Rewards pay cash through `EL_MoneyUtils`.

## Out of scope

- A second quest framework.
- A quest hub menu with no location.

## Assets

`LF_QuestTracker`, `LF_QuestGiverComponent`, `LF_QuestAction`, and the DebugWorld `QuestGiver_Start`. Rewards pay through `EL_MoneyUtils`. LNGC has no task board; its dispatch dialog is the police-call case in [police missions](police-missions.md). See [sources](../../sources.md).

## First slice

One board at a workplace, one collect-or-deliver task, cash reward, expiry logged.

## Depends on

- `LF_QuestTrackerComponent`
- `Money`

## Proof

- The existing quest logic and world tests stay green.
- The new task pays once and shows in the quest log.
