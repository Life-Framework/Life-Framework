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
- **Data:** `Configs/Events/LF_Events.conf` (`LF_EventsConfig` of `LF_EventDef` of `LF_EventPhase`): per event an id, a cooldown, and phases each with a stable name, a cadence, an idle timeout and a module id list. The phase NAME is the save key, so phases may be added or reordered; never rename a shipped one. State is `LF_EventRunState`, saved by phase name (#1951, below).
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

## Events survive a restart by phase name (#1951)

- **Player sees:** an event that was running at a restart does not replay its scene after the boot; it cools down. A cooldown left at the save still holds after it.
- **Owner and side:** `LF_EventShell.ExportSaved` and `ApplySaved` (`LF_EventShellSave.c`), server only; nothing replicates.
- **Data:** the `worldevents` payload, one `LF_EventShellRow` per event the shell runs: row version first, event id, phase NAME, phase age, cooldown left, seconds since it ended, and the int bag as parallel key and value lists. Times are relative to the save; the shell clock is world seconds since boot.
- **Rules:** idle keeps its cooldown left; a phase the event still has goes to its full cooldown from the load; a phase name it no longer has loads idle with `LF_Debug.Warn`; an id this boot does not run (removed, off, refused) is dropped with `LF_Debug.Warn`. The int bag comes back for every applied row. A load before game start loads the config first and `StartIfServer` keeps those runs. Applying the same rows twice gives the same state, and a re-apply onto a live session leaves a running event alone. An end older than the boot loads as "ended at boot" (the clock restarted), so a gap condition waits longer, never less.
- **Proof:** `events/shell-persist` (WORLD), restart by `wt e2e`.

## Scene markers are checked at game start (#2067)

- **Player sees:** nothing. A pack author sees one `[LFDebug:Events]` warning per broken scene in the server log.
- **Owner and side:** `LF_EventSceneCheck` (core), server only, once per world, three seconds after game start so the placed markers (#1954) have registered. It only reads and logs: the scene stays usable and no event is switched off.
- **Rules:** one line per scene, each fault named once: a gap in the route's orders (waypoints 0, 2, 3 is missing 1; a route starting at 1 is missing 0), waypoints but no `event_target`, no `event_spawn` marker. A marker with no scene id belongs to no scene (the component already logs it). A clean scene logs nothing; the check always ends with one `scene check: N marker(s) read, M scene(s) broken` info line.
- **Proof:** `events/scene-check` (LOGIC, marker lists in script, no world).


## Quest-done condition (#3784)

- **Player sees:** nothing directly; an event that follows a player's errand (a police report, then a raid some minutes later).
- **Owner and side:** server. `LF_QuestTrackerComponent.TurnInQuest` notes the turn-in in `LF_QuestDoneLog` (world seconds); `LF_EventShell.Measure` copies it into `LF_EventContext.m_mQuestDoneAt`. Nothing replicates, nothing is saved.
- **Data:** a condition module `.conf` of root `LF_EventPrecondition`: `m_eKind QUEST_DONE`, `m_sQuestId`, `m_fDelaySeconds` (wait after the turn-in), `m_fWindowSeconds` (how long it then holds; 0 until a restart).
- **Rules:** it holds nothing until the quest was turned in this boot; the latest turn-in counts; an empty quest id holds nothing. A restart forgets the tip-off, so a raid never fires from an earlier session's report.
- **Proof:** `events/quest-done-condition` (LOGIC), `events/quest-done-shell` (WORLD).

## Modules from config (#2137)

- **Player sees:** what the modules do (a scene of walkers); the loader itself is invisible.
- **Owner and side:** `LF_EventModuleLoader`, server, at game start from `LF_EventShell.StartIfServer`.
- **Data:** `LF_EventsConfig.m_aModules` of `LF_EventModuleRef` (id + `.conf`), core and pack overlays merged by id. The `.conf` root class picks the kind: `LF_EventPrecondition` condition, `LF_EventAbort` abort, `LF_EventScene` scene operation (`LF_EventSceneOperation`), `LF_EventDispatchSeize` a dispatch and seize operation (`LF_EventDispatchSeizeOperation`, #3785).
- **Rules:** a bad ref is logged and skipped, so its phase holds. A scene stages once per phase run and keeps its phase busy until its walkers have left out of sight. Only one scene is on stage: a scene whose phase opens while another plays waits (phase held, idle clock restarted each cadence) and goes on the cadence after the other has left (#3780).
- **Proof:** `events/module-loader` (WORLD), `everon/arrival-event` (pack leg).

## Scene from a fixture in a session (#2173)

- **Player sees:** nothing; a harness verb for proofs.
- **Owner and side:** `LF_EventSceneHarness` on the server, asked by the `eventscene` harness verb through the character's dev-gated RPC (like `eventjip`).
- **Data:** a fixture `.conf` of root `LF_EventsConfig`, laid over `LF_EventOverlay.Get()` like a pack overlay; `[hour]` goes to `LF_EventShell.ForceHour`.
- **Proof:** `events/scene-harness` (WORLD): applies a fixture, the event starts at the forced hour, refuses no conf, a missing conf and a wrong root, and `stop` restores the list.

## Event places on the map (#2243)

- **Player sees:** while an event runs, its place is a mark on everyone's map (and an EVENT-style circle where the area is drawn); it leaves when the event ends.
- **Owner and side:** server; `LF_EventMapShare` listens to `LF_OnEventPhase` and puts or removes the `EVERYONE` row `event:<eventId>` in `LF_MapShares`; the row carries the radius and the `EVENT` style, and each client draws the circle in its own `LF_MapAreas` (`LF_MapShareAreas`). Nothing is saved.
- **Data:** the place is the `LF_EventPlace` binding (scene id, radius) resolved to the `event_target` marker through `LF_Places`; label `#LF-EventMap_Place`.
- **Proof:** `events/map-share` (WORLD).

## Memorial rite (#3806)

- **Player sees:** at a memorial the church bell tolls, the nearby radios fall quiet for a while, and each player standing there gets a toast and the speaker's line (a text toast titled with the speaker's name) and a standing award, once per phase run.
- **Owner and side:** `LF_EventMemorialOperation` (server, an operation module; a pack .conf with root class `LF_EventMemorial` registers it through `LF_EventModuleLoader`). The bells are a one-shot RPC on the game mode (`LF_AnnounceMemorialBells`) that each rendering client plays on the nearest vanilla `SCR_BellSoundComponent` (`LF_MemorialBells`); nothing replicates but the existing `LF_AmbientSwitchComponent` of each radio.
- **Data:** the scene id whose `event_target` marker is the memorial, the radius, the toast and line keys, the radio sound slots, ring and silence seconds, the bell strokes and search radius, the award row id. The core names no place, person or sound.
- **Rules:** the first run of a phase run (its start time moved) opens the rite: bells, then the radios of the named slots within the ring go off and come back after the silence (only the ones this switched off). Every run serves the players within the radius not yet served in this phase run. A scene with no placed marker does nothing and logs once per run.
- **Voice (#3827):** a rite may name a dialogue conf and one of its voiced lines (`m_sVoiceDialogue`, `m_sVoiceLine`). On the opening run the server sends one broadcast RPC (`LF_AnnounceMemorialVoice`, in the same run that serves the caption) and each rendering client plays the line's event in its audio project 3D at the marker plus `m_fVoiceHeight` (`LF_MemorialVoice`); the operation is `Busy()` for the line's length so the phase waits for the speech. Once per phase run; a bad dialogue or line logs an error and the caption stands alone.
- **Proof:** `events/memorial` (WORLD); the pack's data is checked in `everon/anniversary-memorial`.
