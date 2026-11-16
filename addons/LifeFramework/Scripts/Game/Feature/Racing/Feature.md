# Racing

## Intent

An evening race around the village: a course is data, a racer must pass its checkpoints in order, and the fastest finish wins the pot.

## Interaction pattern

Nothing in the world yet. This slice is the rules only (#1922); the checkpoint triggers, lobby and sign are later slices of #1011.

## Race course (#1922)

- **Player sees:** nothing yet; a later slice shows the next checkpoint and lap.
- **Owner and side:** `LF_RaceCourse` decides on the server; static and world-free so a licence course can reuse it.
- **Data:** `LF_RaceCoursesConfig` / `LF_RaceCourseRecord` (`LF_RaceConfig.c`): checkpoint ids in order, laps, entry fee, minimum entrants, lobby seconds, payout split. Checkpoints are ids only; the pack places the components and `LF_Places` finds them. Nothing is persisted.
- **Proof:** `racing/course` (LOGIC).

## Race runner (#1923)

- **Player sees:** the start sign's "Enter the race" action and toasts for the fee, the start, each checkpoint, the prize or the refund. The sign prefab is a later slice.
- **Owner and side:** `LF_RaceRunner` on the server, one race per course. `LF_RaceEntryAction` reads the uid from the user entity on the server; the client sends nothing but the action.
- **Money:** fee out with `LF_MoneyUtils.RemoveCash` (`race_fee`, a partial take is handed back), prizes and refunds with `AddCash` (`race_prize`, `race_refund`). A race is detached from the table before any payout, so nothing pays twice. A leaver forfeits (nothing is paid to a leaving body, so the disconnect snapshot carries no duplicate).
- **Data:** nothing persisted. `LF_GameModeRoleplay.OnGameEnd` refunds every racer still in before the engine's shutdown save.
- **Events:** `LF_OnRaceFinished` (`LF_RacingEvents.c`) for the results board.
- **Proof:** `racing/runner` (WORLD).
