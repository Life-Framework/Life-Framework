# AssetAnim

## Intent

An asset that is in use moves: a drill's chuck spins, sparks fly from the bit, the motor is heard.
Everyone nearby sees and hears the same, including a player who joins while it runs.

## Interaction pattern

Nothing to press. The feature that uses the asset switches it on and off: the vault robbery does
for the drill (`Feature/Crime/LF_DrillScene.c`). No sheet, no action of its own.

## Asset animation (#340)

- **Player sees:** the parts of an asset turning or swinging, a particle effect at a point on it,
  a looped sound from it, for as long as it is in use. A tool can hang in the air where a
  character's hands are while the character's own animation plays.
- **Owner and side:** `LF_AssetAnimComponent` on the asset's prefab. The server calls
  `SetActive`, `Pose` and `Release`; they are replicated properties (`m_bActive`, `m_bPosed`,
  `m_vPosePosition`, `m_vPoseAngles`), so late joiners get them with the entity. Every machine
  that renders turns the bones each frame (`Animation.SetBone`; the frame event is on only while
  something moves or the asset is held), spawns the effects on the sockets and starts the sound
  loops; a machine that could not start a loop (out of earshot, just joined) retries every 2 s.
  `LF_AssetAnim` holds the motion rules (spin angle, hinge and slide progress, ease).
- **Data:** the component block in the prefab (`m_aBones`, `m_aEffects`, `m_aSounds`) is generated
  from the asset's spec (`tools/assets/<slug>/spec.json`, `anim`) by `tools\cli asset anim <slug>
  --fix` and checked on every `validate` (`asset-anim-selftest`). Do not edit it by hand. Nothing is
  persisted: an asset comes back from a restart switched off and unheld.
- **Held pose:** a posed asset has its physics asleep and is put back at its pose every frame it
  strays (the engine drops and wakes an item that has just left an inventory). In an inventory the
  storage owns its place and the pose is not applied. `Release` sets it down at a given spot on
  every machine.
- **Fail-safe:** a missing bone is skipped; a missing socket or particle logs `LF_Debug.Error` and
  the effect plays at the origin or not at all; no sound manager logs a warning and stays silent.
  Only the server may switch or pose; a client call is refused and logged.
- **Harness:** `animhold`, `animoff` (server: a running drill held in front of the caller, for a
  late-join proof), `animseen` (client: what this machine shows of the nearest animated asset).
- **Proof:** `assetanim/asset-anim` (WORLD); `session.mjs drill-scene-proof [--audio]` (late join,
  two clients, filmed). Log `[LFDebug:AssetAnim]`.
- **Playbook:** `docs/asset-pipeline/animation.md`.
