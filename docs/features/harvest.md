# Harvest

### `LF_WheatGrid` and `LF_WheatFieldRules` — `Feature/Harvest/LF_WheatFieldRules.c` (#3285, 2026-11-18)
- ⚠️ Wheat field rules only, no player-visible effect yet: a field is a 3 m cell grid (60 x 40 m is 260 cells) with a cut bitset, a reaper bar footprint, a 0.5 to 6 m/s speed gate, 10 kg a cell and a 600 kg hopper. Cut wheat, the hopper and the reaper follow in #3287, #3288 and #3289. Tests: `harvest/wheat-field-rules` (LOGIC).
