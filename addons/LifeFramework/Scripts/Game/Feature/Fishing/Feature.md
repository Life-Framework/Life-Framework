# Fishing - Design Requirements

> Context file for the fishing rules (#2927, chapter M6 "Gone fishing"). Full design: `docs/architecture/fishing.md`.

## Intent

A calm job: cast from a quay or shore, wait for a bite, strike inside a short window, land a fish. Every step that
pays is decided on the server. This card is the pure rules and the data; nothing is wired to the world yet.

## Interaction pattern

Rung 4 background: pure rules and a config, no screen, no world, no replication in this card.

## Current state

- `LF_FishingConfig` (`Configs/Fishing/LF_Fishing.conf`): rods, baits, species, tables and globals; `Sanitize` removes a
  bad row alone with `LF_Debug.Error("Fishing", ...)`. Water kinds are the strings OCEAN and POND (the cast maps the
  engine's `EWaterSurfaceType` to them in a later card, so the rules hold no engine enum).
- `LF_FishingRules`: static and pure; rolls in 0..1 come from the caller.
- Cast, bite and strike (#2930): `LF_FishingSession` (server state, `CastAt`, `Bite`, `Strike`; the PlayerController bridge `LF_AskFishingCast`, `LF_AskFishingStrike`, `LF_FishingEvent` is a modded `SCR_PlayerController` in the same file) and `LF_FishingRodComponent` (the rod hand tool plus the owner fire input).
- Spot stock reuses `LF_GatherStock` (Feature/Gathering): thins per catch, regrows on a timer, never saved.
- `LF_FishingSpotComponent` (`Prefabs/Systems/Fishing/LF_FishingSpot.et`, #2929) registers the marker in `LF_Places` as
  kind `FishingSpot`; `FindSpotFor(target)` is the cast's lookup (radius cover, nearest marker wins, null is open water).
- The fishing rows of `Configs/Catalogue/LF_ItemCatalogue.conf` put the trout on the `fish_counter` outlet and the rod and bait tin on
  the `chandler`; a pack overlay changes the prices by item id.

## Decided

- Size is a prefab (small, medium, large), not a weight float. Open water and an emptied spot give small fish only.
- The core ships one neutral species set with empty fish prefabs and placeholder rod and bait prefabs; the pack
  overlays the 1989 species, tables and prices (#800).

## Not built yet

The float prop and its sound, the `LF_OnFishCaught` event, the licence gate on the fish counter, the FISHER job track, the pack's species and prices.

## Dependencies

`Core` (`LF_Debug`, `LF_Utils`, `LF_StaticResetHook`), `Core/Config` (`LF_ConfigRecord`), `Feature/Gathering` (`LF_GatherStock`).
