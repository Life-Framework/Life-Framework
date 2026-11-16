# Events

### `LF_EventWaypointComponent` — `Feature/Events/LF_EventWaypointComponent.c` (#1954, 2026-11-16)
- ⚠️ Markers only, no event runs on them yet: a placed scene marker with a scene id, a role (waypoint, spawn, target) and an order. It registers in `LF_Places` (`event_waypoint` and `event_spawn` as `<scene>#<order>`, `event_target` as `<scene>`), so an event names a scene by id and core holds no coordinates. `FindWaypoints` and `FindSpawns` return the scene's markers lowest order first (a gap keeps the order); `FindTarget` returns the one target. A marker with no scene id, or a negative order, is logged (`[LFDebug:Events]`) and not registered; a second marker with a taken order is refused by `LF_Places`. Prefabs `Prefabs/Systems/Events/LF_EventMarker_{Waypoint,Spawn,Target}.et`; test route `grid_event_scene` (four waypoints, two spawns, a target) in grid cell `event-scene`. Tests: `events/waypoints` (WORLD).
# Events

### `LF_EventDef`, `LF_EventPhase`, `LF_EventPhaseRules` — `Feature/Events/` (#1948, 2026-11-16)
- ✅ An event is config: an id, ordered named phases (cadence, idle timeout, module ids) and a cooldown; pure rules advance a phase when its conditions hold, end the event on an abort, an idle timeout or the last phase, and a cooldown blocks a restart until it runs out. Not yet ticked by a shell, not persisted. Tests: `events/phase-rules` (LOGIC).

### `LF_EventPrecondition`, `LF_EventAbort`, `LF_EventContext` — `Feature/Events/` (#1949, 2026-11-16)
- ✅ Condition modules (AND-ed) hold a phase until the in-game hour is in range (may wrap midnight), enough players are online or nearby, and a cooldown since the event last ended is over; abort modules (OR-ed) end the event on an hour window, too few players online or nearby, or a phase that lasts too long. Pure over a measured `LF_EventContext`; the shell (#757) fills it and feeds `LF_EventPhaseRules.Decide`. Tests: `events/preconditions` (LOGIC).
