# Everon pack expectations

Each `*.json` here states what the flagship pack must sell or field once the
core files and the pack overlays are merged. `node tools/validation/validate-everon-config.mjs`
loads the core catalogue and police files, applies the overlays the mission header
(`addons/LF_Everon1989/Missions/LF_Everon1989.conf`, `m_aLF_ConfigOverlays`) names in
list order, then checks every fixture. A card that fills an overlay adds a fixture
for what it put there; a fixture the overlays cannot satisfy fails the check.

Merge rules mirror the game: records are keyed by item id, `outlet/item` or rank id;
a field a later layer does not write keeps the lower layer's value; `m_sOp "remove"`
or `m_bRemoved 1` takes the record out.

`red/` is not read by the normal run. `--fixtures tools/validation/fixtures/everon/red`
runs it and must exit 1 (`missing-offer.json` expects an offer no overlay has).

## Kind `outlets`

```json
{
  "kind": "outlets",
  "outlet": "gun_counter",
  "offers": [
    { "item": "pistol_pm", "licence": "FIREARM_PERMIT" },
    { "item": "rifle_m21", "present": true, "minRank": 0, "price": 1500 },
    { "item": "nightstick", "present": false }
  ]
}
```

- `outlet`: the outlet id a shop counter names (`m_sOutlet`).
- `offers[].item`: the catalogue item id.
- `offers[].present`: default `true`; `false` means the outlet must not offer it.
- Gates, checked only when written: `licence`, `job`, `minRank`, `orgRight`
  (empty and 0 are the "no gate" defaults).
- `price`: the merged catalogue price of the item.

## Kind `ranks`

```json
{
  "kind": "ranks",
  "count": 3,
  "ranks": [
    { "index": 0, "id": "patrol", "fineCap": 1000, "dutySecondsForNext": 3600 },
    { "id": "senior", "titleKey": "EL-Rank_Senior", "payPerSecond": -1, "kit": ["Rifle_M16A2.et"] }
  ]
}
```

- `count` is the exact size of the police ladder; `minCount` a lower bound.
- A rank is picked by `index` (its place in the ladder) or by `id`.
- Fields, checked only when written: `id`, `titleKey`, `fineCap`, `payPerSecond`,
  `dutySecondsForNext`; `kit` lists substrings that must appear in the rank's kit prefabs.
