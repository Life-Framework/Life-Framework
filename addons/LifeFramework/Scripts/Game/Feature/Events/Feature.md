# Events

## Intent

A world event (a convoy, a drop, the relocated families arriving at dawn) is data: an id, an
ordered list of named phases and a cooldown. The rules that move it along are pure, so the whole
cycle is tested without a world. This slice is the data model and the rules only; the shell that
ticks events, the modules, the zone, persistence and the announcements follow (#757, #1015).

## Interaction pattern

None yet. An event is a place the player walks to, announced by a siren, a radio line or a board
entry (docs/decisions.md "In-world first"); no event menu. The shell slices add the world side.

## Event phase rules (#1948)

- **Player sees:** nothing directly; this is the data and the rules every event runs on.
- **Owner and side:** `LF_EventPhaseRules` decides on the server, in server seconds.
- **Data:** `Configs/Events/LF_Events.conf` (`LF_EventsConfig` of `LF_EventDef` of `LF_EventPhase`): per event an id, a cooldown, and phases each with a stable name, a cadence, an idle timeout and a module id list. The phase NAME is the save key, so phases may be added or reordered; never rename a shipped one. State is `LF_EventRunState` (not persisted yet).
- **Rules:** conditions are AND-ed to advance, aborts OR-ed and end the event, an idle timeout ends it, finishing the last phase ends it, and each end starts the cooldown; a cooldown blocks `Start` until it runs out. A bad definition (no id, no phases, duplicate or empty phase name, negative time) is refused and logged `[LFDebug:Events]`.
- **Proof:** `events/phase-rules` (LOGIC).
