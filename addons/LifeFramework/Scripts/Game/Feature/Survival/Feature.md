# Survival — Design Requirements

> Context file for hunger / thirst / health. Written against
> `docs/design.md` (in-world first).

## Intent

Survival is **driven by doing**: you eat by consuming food in the world, drink
by drinking, and the body reacts. The readout (HUD) is the sanctioned pure-UI
surface for *state*; the actions are all physical. You do not "open the
survival menu to eat."

## Interaction pattern

Rung 1 for actions (consume the food/drink item in hand), rung 4 for the HUD
readout (accepted — a body-state gauge has no world object).

Today (2026-11-08, M1-28, decision "Survival needs are light"): the server
ticks `LF_SurvivalStats` once a second with rates from `LF_SurvivalConfig`
(`Configs/Survival/LF_Survival.conf`). Needs drain slowly (a meal lasts a
session). Low needs hold the character's real health (damage manager) at 75% of
max through `LF_SurvivalHealth` and make exertion drain stamina
1.5x (`LF_SurvivalStaminaPenalty`); they never kill. Met needs regenerate
health by giving back what needs took. `LF_ConsumableEffectSurvival` restores on item use; `LF_BetaHud` shows
the replicated gauges.

## V1 (shippable)

1. Health recovery: done. Met needs give back what they took; vanilla healing is untouched.
2. Delta clamp: done. One tick applies at most `m_fMaxTickDeltaSeconds`.
3. Zero health: survival never takes health below the cap, so it never kills.
   Death from other causes is the body-and-respawn flow (see `Character`).
4. `LF_SurvivalStatsSaveData.Equals` uses exact float `==` (no epsilon).

## Iteration path

- **V2**: a hunger sound cue (stomach growl) and eat/drink sounds tied to the
  consumable, temperature/weather.
- **V3**: medical system (injuries, treatment), food quality/spoilage.

## Current state

- `LF_SurvivalHealth`: the real-health cap and give-back (server).
- `LF_SurvivalStats`: clamped hunger/thirst, additive Eat/Drink, config-driven
  `UpdateStats(dt, config)`, `AreNeedsLow`, `GetStaminaDrainMultiplier`.
- `LF_SurvivalConfig`: every rate, the cap, the stamina multiplier.
- `LF_CharacterSurvivalComponent`: server tick, dt clamp, owner-only
  replicated hunger/thirst/low flag with throttled `BumpMe`.
- `LF_SurvivalStaminaPenalty`: modded `SCR_CharacterStaminaComponent`.
- `LF_ConsumableEffectSurvival`: restore on use; only values > 0 applied.
- `LF_BetaHud`: the hunger/thirst readout. The old `EL_SurvivalHUD` menu preset was never
  opened and is removed; its layout stays only as an `LF_Test_Data` load fixture.
- `Persistence/` save data.

## Dependencies

- `Character` (the body), `Shop` (buying food), `Gathering` (foraging),
  `Processing` (making food), `Money` (paying), `Quantity` (stacked food).