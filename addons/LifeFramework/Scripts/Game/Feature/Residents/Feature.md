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

## Resident spawner (#2858)

- **Player sees:** a resident standing on their anchor (the shopkeeper at the counter at 07:30) when they walk within
  150 m, and nobody once they leave (200 m). Nobody walks yet: residents stand on the anchor of their current line.
- **Owner and side:** server. `LF_ResidentSpawner` (started from the game mode like the other sweeps) sweeps every
  `m_iSweepSeconds` (5) through one `CallLater`; `SweepAt(minute, players)` is the same pass with the clock and the
  players handed in, which tests use. The spawned character is an ordinary replicated entity, so a late joiner sees it
  with no extra state and there is no RPC.
- **Rules:** a resident is due where `LF_ResidentTimetable` says at the game minute, on the anchor
  `LF_ResidentAnchors.Resolve` finds nearest the player. Spawn when a player is within `m_iSpawnRadiusM` (150) of
  the anchor, despawn when none is within `m_iDespawnRadiusM` (200) of the character (horizontal metres, so
  a player on a roof counts). A line change to another place or anchor despawns first, then spawns at the new spot only if
  a player is near it. `m_iMaxLive` (24) caps live residents; the rest wait (`LF_ResidentDirector` picks who, see below), and each waiting
  resident is logged once (`LF_Debug.Warn("Residents", ...)`). The prefab is `LF_ResidentDef.m_sPrefab`, default the neutral
  civilian `Npc_Civilian_01`; any AI on it is deactivated. `m_sActivity` is recorded (`ActivityOf`) and logged on
  spawn; no animation or emote plays yet. Live residents are keyed by resident id and hold `EntityID`;
  statics reset through `LF_ResidentSpawner_StaticResetHook`. Persisted: nothing, a restart re-spawns by proximity.
- **Data:** `LF_Residents.conf` root fields `m_iSpawnRadiusM`, `m_iDespawnRadiusM`, `m_iMaxLive`, `m_iSweepSeconds`.
- **Proof:** `residents/spawner` (WORLD).

## Resident talk (#2908)

- **Player sees:** a resident whose dialogue has `greet_<word>` nodes greets them by how they feel about them: warmly for a regular, "I've nothing to say to you." from one who saw a theft (no replies, the talk ends).
- **Owner and side:** server. `LF_Dialogue.Start` calls `LF_ResidentTalk.StartNode`; the word is `LF_ResidentMemory.Tier`. The client is sent node and line ids only.
- **Rules:** node order: `<residentId>_greet_<word>` (pack), `greet_<word>` (core), else the dialogue's `m_sStart`. A character that is not a live resident opens on `m_sStart`. Data: `Configs/Dialogue/LF_Dialogue_Resident.conf` (id `resident` in the dialogue index).
- **Proof:** `residents/resident-talk` (WORLD).

## Resident walk (#2909)

- **Player sees:** a resident whose line moves on (bench to counter) walking to the next anchor at walking pace when they are near, then starting the activity, instead of vanishing and appearing.
- **Owner and side:** server. `LF_ResidentWalk.Move` is called from `LF_ResidentSpawner.Despawn`; `LF_ResidentMover` drives the vanilla AI (a test swaps it).
- **Rules:** see `docs/features/residents.md` "LF_ResidentWalk". Data: `LF_Residents.conf` `m_iMaxWalking` (4). No AIWorld: placed, never walked. Persisted: nothing.
- **Proof:** `residents/walk` (WORLD).

## Resident director (#2910)

- **Player sees:** when more residents are due than the live cap, the ones in the place they stand in (a counter worker
  beside them first) are there, not whoever sorts first by id.
- **Owner and side:** server. `LF_ResidentDirector.Pick(due, players, slots, into)` is pure; `LF_ResidentSpawner.SweepAt`
  builds the due list (`LF_ResidentCandidate`: id, timetable line, anchor spot) and spawns what it returns.
- **Rules:** counter worker (`work` at `counter`) within 15 m of a player first; the rest by horizontal distance to the
  nearest player; ties by id. Persisted: nothing.
- **Proof:** `residents/resident-director` (LOGIC).

## Resident work loop (#4230)

- **Player sees:** a worker with a work activity holds that job's body pose at their spot (#4382). `hammer` bends over and strikes down with both hands (vanilla build-tool clips `p_erc_supplies_demolitionUS_low_*` in `Anims/Workspaces/Work/LF_WorkHammer.asi` on the lockpick's work graph `LF_WorkPick.agr`). `carry` and `sweep` have no vanilla clip that reads as either, so they play the base game's plain idle loiter (`ELoiteringType.LOITERING`), never a hand signal. A placed character does it with `LF_ResidentWorkLoopComponent`; a timetable line that says `carry`, `hammer` or `sweep` reaches it through `LF_ResidentActivity.Apply` (#4366): a new line, a walk (`Release`) or the spawner despawning the resident calls `Stop`. A work line on a static NPC body warns once and stands.
- **Owner and side:** server. `LF_ResidentWorkLoop` (`Begin`, `Check`, `Stop`, `PoseOf`); the pose is a loiter, which the base game sends to every client and to late joiners (the command handler's `RplSave`). A 1 s `CallLater` tick starts the pose again when the loiter ended by itself. Persisted: nothing.
- **Rules:** `PoseOf(activity)` names the job's anim set instance, empty for the idle loiter; a new job pose is one `.asi` on `LF_WorkPick.ast` plus a `PoseOf` branch. Beginning the same job again keeps the pose; a different job swaps it. Not a work name: `LF_Debug.Warn`, no loop. A body with no character controller: warned, stands. Dead or deleted bodies leave the loop. Statics reset through `LF_ResidentWorkLoop_StaticResetHook`.
- **Proof:** `residents/work-loop` (WORLD).
