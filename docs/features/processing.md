# Processing

### `LF_RecycleRules` — `Feature/Processing/LF_RecycleRules.c` (#3347, 2026-11-19)
- Recycle recipes as pure logic: a car door gives back 2 of its 4 scrap metal at 50 %, a recipe worth as much as the item is refused at load with the reason, an unknown item gives nothing. No player-facing recycler yet. Tests: `processing/recycle-rules` (LOGIC).

### `LF_RecyclerComponent` — `Feature/Processing/LF_Recycler.c` (#3349, 2026-11-19)
- The recycler bench (`Prefabs/Props/Processing/LF_Recycler.et`): Load takes one item that has a recycle recipe from the worker's inventory on the server, Work is a held action (4 units by default, each worker adds one, so more hands finish sooner), Collect hands over the parts from `LF_RecycleRules.Yield` (overflow drops at the worker's feet). One job at a time per bench. An item with no recipe, a recipe item not carried, a busy bench, a worker out of reach and a collect with nothing ready are each refused in words (`#LF-Recycler_*`). A job in progress is not saved. The core ships no recipe, so the bench does nothing until a pack adds recipes. Tests: `processing/recycler` (WORLD). Grid cell: `look grid:recycler`.

### Scrapyard recipes — `addons/LF_Everon1989/Configs/Processing/` (#3351, 2026-11-19)
- `LF_RecycleConfig` now reads pack overlays (`ApplyLayers`: a row with an item an earlier layer has replaces it, any other is added). The 1989 pack adds the first recipe: car parts break into 3 iron bars (6 at 50 %), worth $150 against the $200 item, and the bar has a market curve (drops per unit, recovers hourly). The new catalogue ids are `car_parts` and `scrap_iron_bar` (`LF_Catalogue_Scrap_Everon1989.conf`). Old radios, tools, copper wire and rubber wait for item prefabs (no loose prefab exists yet). Tests: `everon/scrapyard` (pack leg).
