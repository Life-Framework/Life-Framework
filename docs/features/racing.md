# Racing

### `LF_RaceCourse` — `Feature/Racing/LF_RaceCourse.c` (#1922, 2026-11-16)
- ⚠️ Rules only, no world yet: a course is data (checkpoint ids, laps, fee, minimum entrants, lobby seconds, payout split); checkpoints count only in order, a shortcut is refused and logged, laps count, finish order is by time. Tests: `racing/course` (LOGIC).

### `LF_RaceCourses` — `Feature/Racing/LF_RaceCourses.c` (#2006, 2026-11-16)
- ⚠️ Loader only, no world yet: the core ships no course. `LF_RaceCourses.Get()` layers each pack overlay of `LF_RaceCoursesConfig` in load order, then the owner's `racing.courses` section, through the shared override model (add, replace, patch, remove by course id); `Find(id)` returns the record. Every course goes through `LF_RaceCourse.IsValid`; a bad row is logged (`[LFDebug:Racing]`) and dropped, never kept. The flagship course is pack data (#1927). Tests: `racing/courses` (LOGIC).
