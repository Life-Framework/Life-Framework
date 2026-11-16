# Events

### `LF_EventDef`, `LF_EventPhase`, `LF_EventPhaseRules` — `Feature/Events/` (#1948, 2026-11-16)
- ✅ An event is config: an id, ordered named phases (cadence, idle timeout, module ids) and a cooldown; pure rules advance a phase when its conditions hold, end the event on an abort, an idle timeout or the last phase, and a cooldown blocks a restart until it runs out. Not yet ticked by a shell, not persisted. Tests: `events/phase-rules` (LOGIC).

### `LF_EventPrecondition`, `LF_EventAbort`, `LF_EventContext` — `Feature/Events/` (#1949, 2026-11-16)
- ✅ Condition modules (AND-ed) hold a phase until the in-game hour is in range (may wrap midnight), enough players are online or nearby, and a cooldown since the event last ended is over; abort modules (OR-ed) end the event on an hour window, too few players online or nearby, or a phase that lasts too long. Pure over a measured `LF_EventContext`; the shell (#757) fills it and feeds `LF_EventPhaseRules.Decide`. Tests: `events/preconditions` (LOGIC).
