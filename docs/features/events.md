# Events

### `LF_EventDef`, `LF_EventPhase`, `LF_EventPhaseRules` — `Feature/Events/` (#1948, 2026-11-16)
- ✅ An event is config: an id, ordered named phases (cadence, idle timeout, module ids) and a cooldown; pure rules advance a phase when its conditions hold, end the event on an abort, an idle timeout or the last phase, and a cooldown blocks a restart until it runs out. Not yet ticked by a shell, not persisted. Tests: `events/phase-rules` (LOGIC).

### `LF_EventScene`, `LF_EventScenePlan`, `LF_EventScenePlanner` — `Feature/Events/` (#1953, 2026-11-16)
- ✅ A scene is data: placed waypoint ids in walking order, an NPC count, a role or outfit, spawn stagger, walk timeout, despawn delay, an optional dialogue id per NPC and an optional parked-vehicle slot. The pure planner keeps the configured waypoint order, trims the count to a hard cap (12), skips the scene with a logged reason when the free AI budget cannot hold every walker, and refuses a second scene while one runs (a world reset drops the running scene; a restart does not resume it). The shell (#757) will read the free AI budget and call it; nothing spawns yet and no phase carries a scene yet. Tests: `events/scene-plan` (LOGIC).
