# Residents

## Intent

Residents make a small town feel lived in (epic #991). Wave 1 (#2665) is the data and the clock: an owner
defines residents in config and the server can say where each one should be at any game minute, without
anyone walking. Nothing is spawned yet.

## Interaction pattern

None yet: no player-visible object. Wave 2 materialises scheduled residents on a place's anchors (counter,
bench, bus stop) near players.

## Resident cast and timetable (#2665)

- **Player sees:** nothing yet.
- **Owner and side:** server. `LF_ResidentTimetable` builds and answers; `LF_ResidentDef` is the config record.
- **Data:** `Configs/Residents/LF_Residents.conf` (`LF_ResidentsConfig`): per resident `m_sId`, `m_sHomeKind`,
  `m_sWorkKind` (`LF_Places` kinds, never names or coordinates) and `m_aTimetable` lines `m_sTime` ("HH:MM"),
  `m_sPlace` (`home` or `work`), `m_sAnchor`, `m_sActivity` (emote or loiter id, #982). The timetable is per game
  day, no weekdays. A line holds until the next starts; the last holds across midnight. The core ships two neutral
  example residents; the 1989 cast is the pack (#992). Persisted: nothing.
- **Refused at load** (`LF_Debug.Error("Residents", ...)` naming the resident): empty timetable, two lines on one
  minute (overlap), an unreadable time or a place that is neither home nor work. A line whose place kind
  `LF_Places` does not know (no such place placed on the map) is logged and dropped; a day with no line left is
  refused.
- **Proof:** `residents/resident-timetable` (LOGIC).

## Resident anchors (#2857)

- **Player sees:** nothing yet (the marker is invisible to players; wave 2 stands residents on it).
- **Owner and side:** server. `LF_ResidentAnchorComponent` on the marker prefab `Prefabs/Systems/Residents/LF_ResidentAnchor.et`
  (attribute `m_sAnchorId`: `counter`, `bench`, `door`); `LF_ResidentAnchors.Resolve(kind, anchorId, near, spot)` answers.
- **Rules:** the place is the nearest `LF_Places` place of `kind` to `near`. An anchor belongs to it by parent hierarchy, else
  by sitting within 30 m of it and nearer it than any other place of the kind. Empty anchor id: the place origin. Unknown
  anchor id: the place origin, one `LF_Debug.Warn("Residents", ...)` per kind and id pair. No place of the kind: false.
  Entries hold `EntityID`; statics reset through `LF_ResidentAnchors_StaticResetHook`. Persisted: nothing.
- **Proof:** `residents/anchors` (WORLD).
