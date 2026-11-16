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

## Packs add and patch events by id (#1952)

- **Player sees:** nothing directly; it decides which events this server can run.
- **Owner and side:** `LF_EventOverlay` (core) loads `Configs/Events/LF_Events.conf`, then each pack overlay in load order, on both sides. Nothing replicates.
- **Data:** a pack ships a `.conf` with root class `LF_EventsConfig` and lists it in the mission header's `m_aLF_ConfigOverlays`. Each `LF_EventDef` has `m_sOp` (the shared override model, `docs/config/override-model.md`):
  - `add`: a new event; an id the core has is refused.
  - `replace`: the event is rebuilt from the pack entry alone, in its place in the list.
  - `patch`: only what the entry sets changes. The cooldown if set. Each listed phase is matched by its NAME: patched field by field when the event has it (cadence, idle timeout, modules; an unset field keeps the core value), appended when it does not. A patch never removes a phase; `replace` does.
  - `remove`: the event is disabled and never starts.
  - no verb: patch an id that exists, add one that does not.
- **Rules:** `replace`, `patch` and `remove` of an unknown id are refused and logged with the pack file and id; the rest of the overlay applies. An event that fails `LF_EventPhaseRules.Validate` after the overlays (a negative time, a duplicate phase name) is logged `[LFDebug:Events]` and left out by `LF_EventOverlay.Collect` and `Find`; the others still load. Two packs changing one id: the later pack wins and a warning names both.
- **Proof:** `events/pack-overlay` (LOGIC) layers a fixture that adds, patches a phase timing, removes and replaces, and refuses a duplicate add and a patch of an unknown id.

## Everon arrival at dawn (#1957)

- **Player sees:** once the town card places the route and a pack patch turns it on: at dawn six people in 1989 travelling clothes, each with a suitcase, walk in from the edge of town to the town hall. The data is in the pack (`LF_Everon1989/Configs/Events/`): the `relocated_arrival` event, its dawn hour condition, its scene, its module refs, three walker prefabs dressed in the three outfit presets. The core stays neutral; it only reads the pack's overlay (#1952).
- **Owner and side:** server; `LF_EventModuleLoader` registers the condition and the scene by id at game start (#2137).
- **Proof:** `everon/arrival-event` (pack leg).

## Modules from config (#2137)

- **Player sees:** what the modules do (a scene of walkers); the loader itself is invisible.
- **Owner and side:** `LF_EventModuleLoader`, server, at game start from `LF_EventShell.StartIfServer`.
- **Data:** `LF_EventsConfig.m_aModules` of `LF_EventModuleRef` (id + `.conf`), core and pack overlays merged by id. The `.conf` root class picks the kind: `LF_EventPrecondition` condition, `LF_EventAbort` abort, `LF_EventScene` scene operation (`LF_EventSceneOperation`).
- **Rules:** a bad ref is logged and skipped, so its phase holds. A scene stages once per phase run and keeps its phase busy until its walkers have left out of sight.
- **Proof:** `events/module-loader` (WORLD), `everon/arrival-event` (pack leg).
