# Quests

## Intent

A new player is told what to do first and led through the first errands (the arrival chain), then
offered more errands by quest givers around town. The full contract is `docs/features/quests.md`.

## Interaction pattern

In-world first: quest givers are NPCs at their posts (the registry clerk, the harbour master), talked
to with F; steps put a waypoint on the screen and a mark on the map (`LF_ArrivalGuide`). The welcome
guide is the one sheet that is not tied to an object: it opens by itself after a new character's
first wake-up and from a key, because it explains the controls and places before the player knows
where any object is.

## Welcome guide

- **Player sees:** a five-page paper sheet a few seconds after a new character gets out of the arrival
  bed: first steps (the clerk, with *Set waypoint to the clerk*), money, the map, going on duty, more
  help (the manual link and the reopen key). Previous and Next turn pages; *Don't show again* stops it
  opening by itself; Close puts it away until the next join. F3 (gamepad: right bumper and Y) opens it
  at any time.
- **Owner and side:** `LF_WelcomeGuide` holds the pending set on the server (`LF_SpawnLogic.OnFirstSpawn`
  marks it, `Dismiss` clears it); `LF_WelcomeGuideController` (modded `SCR_PlayerController`) carries
  the asks and the owner-only "show" answer; `LF_WelcomeGuideSheet` is client only.
- **Data:** string keys `LF-WelcomeGuide_*` (core, neutral; the Everon pack overrides four paragraphs
  with Saint-Philippe places); input action `LF_WelcomeGuide`. Persisted: the `welcomeguide` save
  section (character ids whose guide still opens by itself).
- **Proof:** `quests/welcome-guide` (LOGIC), `quests/welcome-guide-world` (WORLD),
  `quests/welcome-guide-persist` (PERSISTENCE), `node agent-box/session.mjs welcome-guide-proof`.
