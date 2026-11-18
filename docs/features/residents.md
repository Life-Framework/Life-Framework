# Residents

### `LF_ResidentTimetable` — `Feature/Residents/LF_ResidentTimetable.c` (#2665, 2026-11-17)
- A resident's day is data and the server can say where they should be at any game minute (home or work, the anchor there, the activity), with nobody walking. Tests: `residents/resident-timetable` (LOGIC).

### `LF_ResidentDef` — `Feature/Residents/LF_ResidentDef.c`, `Configs/Residents/LF_Residents.conf` (#2665, 2026-11-17)
- A resident is config: id, home and work place kinds (found through `LF_Places`), and a per-game-day timetable of `HH:MM` lines. The core ships two neutral example residents; a pack adds, replaces or removes residents by id through an overlay (`replace` swaps the whole record; `patch` of a timetable is not supported). Tests: `residents/resident-timetable` (LOGIC).

### `LF_ResidentAnchors` — `Feature/Residents/LF_ResidentAnchors.c`, `LF_ResidentAnchorComponent.c`, `Prefabs/Systems/Residents/LF_ResidentAnchor.et` (#2857, 2026-11-18)
- A map maker drops a resident anchor (id `counter`, `bench`, `door`) inside a place; `LF_ResidentAnchors.Resolve(kind, anchorId, near, spot)` turns a timetable line into a world position and facing. The place is the nearest `LF_Places` place of the kind to `near`; an anchor belongs to it by parent hierarchy, else loose within 30 m. An empty anchor id is the place origin; an unknown id falls back to the origin and logs `LF_Debug.Warn("Residents", ...)` once per kind and id pair. Server side, nothing persisted. Tests: `residents/anchors` (WORLD).
