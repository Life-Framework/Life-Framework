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

## Flagship dirt loop (#1927)

- **Data:** `addons/LF_Everon1989/Configs/Racing/LF_RaceCourses_Everon1989.conf` holds course `dirt_loop`: checkpoints `dirt_cp_1` to `dirt_cp_5`, 3 laps, fee 50, 2 entrants minimum, 60 s lobby, payout 60/30/10. The town card places the checkpoints under those ids.
- **Proof:** `tools/validation/pack-race-course-selftest.mjs` (the core test cannot name a pack file); the flagship mission header lists the file so `LF_RaceCourses` loads it.

## Race panel (#1924)

- **Player sees:** during a race a small panel on the left: lap (2 / 3), next checkpoint (3 / 5), place (1 / 4) and the race clock; "Finished" at the line, "Race over" when the race settles, then it goes after six seconds.
- **Owner and side:** `LF_RaceHud.Sync` on the server, run after every `LF_RaceClock` tick, lobby close and time limit (modded `LF_RaceClock`). It sends `RpcDo_LF_RaceHudState` (eight ints, `SCR_PlayerController`, owner only) when a racer's lap, checkpoint, place or field size changed, never per frame, and `RpcDo_LF_RaceHudEnd` once the race is over for that racer. The client runs the clock from the elapsed time in the last update. The client sends nothing.
- **Place:** finishers first by finish time, then most checkpoints passed, then who reached that count first (the server notes the race time of each racer's last new checkpoint), then uid.
- **UI:** `UI/Layouts/Racing/LF_RaceHud.layout` is a theme-metal plate and a column (a HUD strip exception in `ui-kit-baseline.json`); the heading and four rows are the kit's `LF_UIHeading` and `LF_UIStatusRow` parts in theme colours. Strings `LF-RaceHud_*`.
- **Data:** nothing persisted.
- **Proof:** `racing/race-hud` (LOGIC).
