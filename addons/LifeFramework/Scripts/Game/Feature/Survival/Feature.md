# Survival — Design Requirements

> Context file for hunger / thirst / health. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

Survival is **driven by doing**: you eat by consuming food in the world, drink
by drinking, and the body reacts. The readout (HUD) is the sanctioned pure-UI
surface for *state*; the actions are all physical. You do not "open the
survival menu to eat."

## Interaction pattern

Rung 1 for actions (consume the food/drink item in hand), rung 4 for the HUD
readout (accepted — a body-state gauge has no world object).

Today: `EL_CharacterSurvivalComponent` decays stats per frame (hunger
`-dt*0.1`, thirst `-dt*0.15`, health falls when hunger or thirst < 20);
`EL_ConsumableEffectSurvival` restores on item use; `EL_SurvivalHUD` renders;
`EL_SurvivalStats` holds the clamped [0,100] state.

## V1 (shippable)

1. **Health never recovers** — it only ever falls. Add a recovery path
   (rest, food+drink, medical) or the player's only option is slow death.
2. Clamp delta time — a frame hitch drops a big stat chunk in one tick.
3. Zero-health handling — **defined** (verified 2026-08-30): death is a
   **body + respawn** flow, not a soft state. On death the body stays in the
   world with everything it carried, and the player respawns through the death
   screen (see `Character`). Survival's job is to get the body to zero; the
   consequence is the respawn, not a health-regeneration branch.
4. `EL_SurvivalStatsSaveData.Equals` uses exact float `==` (no epsilon).

## Iteration path

- **V2** — visible body feedback (camera/hand effects as hunger falls),
  temperature/weather.
- **V3** — medical system (injuries, treatment), food quality/spoilage.

## Current state

- `EL_SurvivalStats` — clamped state, additive Eat/Drink/Heal, decay driver.
  ⚠️ no recovery, no zero-health handling.
- `EL_CharacterSurvivalComponent` — per-frame decay; ⚠️ no dt clamp.
- `EL_ConsumableEffectSurvival` — restore on use; only values > 0 applied.
- `EL_SurvivalHUD` — readout; `Persistence/` save data.

## Dependencies

- `Character` (the body), `Shop` (buying food), `Gathering` (foraging),
  `Processing` (making food), `Money` (paying), `Quantity` (stacked food).