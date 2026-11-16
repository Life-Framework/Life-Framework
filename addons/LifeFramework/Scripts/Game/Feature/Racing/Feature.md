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

## Flagship dirt loop (#1927)

- **Data:** `addons/LF_Everon1989/Configs/Racing/LF_RaceCourses_Everon1989.conf` holds course `dirt_loop`: checkpoints `dirt_cp_1` to `dirt_cp_5`, 3 laps, fee 50, 2 entrants minimum, 60 s lobby, payout 60/30/10. The town card places the checkpoints under those ids.
- **Proof:** `tools/validation/pack-race-course-selftest.mjs` (the core test cannot name a pack file); the flagship mission header lists the file so `LF_RaceCourses` loads it.
