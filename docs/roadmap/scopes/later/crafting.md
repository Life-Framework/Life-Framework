# Crafting

Phase: After release
Extends: `Processing`

## Player loop

A player stands at a bench and turns carried materials into a product, using a recipe list. Fixed world machines (apple press, ore processing) stay as they are.

## In scope

- A bench prefab with a config recipe list: inputs in inventory, outputs in inventory.
- The same all-or-nothing checks the process action already uses.
- One example recipe made of items the resource chains already produce.

## Out of scope

- A new recipe for contraband. Hideout processing stays on [contraband](../crime/contraband.md).
- Custom crafted meshes. Outputs are existing prefabs.

## Assets

Bench prop: LNGC `Prefabs/Compositions/Misc/SubCompositions/CraftingBench.et`, or `ItemProcessor_Base` if that composition pulls extra UI. Recipe fields to copy from `LNGC_CraftingRecipeDef`: input ids, counts, seconds, license, hidden, bulk. Store them as prefab resources the way `EL_ProcessAction` does on `Furnace.et` (iron nugget → iron bar is the example). Do not store LNGC virtual item ids. See [sources](../../sources.md).

## First slice

One bench, one recipe, refused when inputs are missing, no partial consume.

## Depends on

- [Resources](../work/resources.md)
- `EL_ProcessAction` rules

## Proof

- Logic: malformed recipe and missing inputs refuse before inventory changes.
- World: one successful craft on DebugWorld.
