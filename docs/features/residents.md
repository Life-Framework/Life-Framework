# Residents

### `LF_ResidentTimetable` — `Feature/Residents/LF_ResidentTimetable.c` (#2665, 2026-11-17)
- A resident's day is data and the server can say where they should be at any game minute (home or work, the anchor there, the activity), with nobody walking. Tests: `residents/resident-timetable` (LOGIC).

### `LF_ResidentDef` — `Feature/Residents/LF_ResidentDef.c`, `Configs/Residents/LF_Residents.conf` (#2665, 2026-11-17)
- A resident is config: id, home and work place kinds (found through `LF_Places`), and a per-game-day timetable of `HH:MM` lines. The core ships two neutral example residents; a pack adds, replaces or removes residents by id through an overlay (`replace` swaps the whole record; `patch` of a timetable is not supported). Tests: `residents/resident-timetable` (LOGIC).

### `LF_ResidentMemory` - `Feature/Residents/LF_ResidentMemory.c`, `LF_ResidentMemoryConfig.c`, `Configs/Residents/LF_ResidentMemory.conf` (#2859, 2026-11-18)
- How each resident feels about each character, as pure server rules keyed `residentId|uid`: `helped` raises, `crime_seen` lowers, clamped -100..100, no decay. Callers read a word (`refuses`, `wary`, `neutral`, `friendly`, `regular`) from config thresholds, never the number. A per-pair cooldown stops the same reason twice; a crime seen is gossiped one hop to the residents the witness knows (`m_aKnows`) at the config share, rounded toward zero. An unknown reason is refused with a `Residents` warning. Not saved yet (#2861). Tests: `residents/resident-memory` (LOGIC).
