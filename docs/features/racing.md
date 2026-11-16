# Racing

### `LF_RaceCourse` — `Feature/Racing/LF_RaceCourse.c` (#1922, 2026-11-16)
- ⚠️ Rules only, no world yet: a course is data (checkpoint ids, laps, fee, minimum entrants, lobby seconds, payout split); checkpoints count only in order, a shortcut is refused and logged, laps count, finish order is by time. Tests: `racing/course` (LOGIC).

### `LF_RaceRunner` — `Feature/Racing/LF_RaceRunner.c` (#1923, 2026-11-16)
- ⚠️ No start sign placed yet: `LF_RaceEntryAction` (sign attributes: courses file and course id) takes the entry fee from carried cash into the pot, in full or not at all; the first entrant opens the lobby, which closes after the course's lobby seconds. Too few entrants refunds everyone. The server checks each racer against the next `LF_RaceCheckpointComponent` (registered in `LF_Places` by id, radius on the component) every 250 ms; when every racer still in has finished (or 15 minutes pass) the pot is paid by the split and the winner also takes the rounding and unfilled places. A disconnect forfeits the fee. The pot is session state: a session end refunds everyone still in before the shutdown save; a crash loses the fees. Publishes `LF_OnRaceFinished`. Tests: `racing/runner` (WORLD).
