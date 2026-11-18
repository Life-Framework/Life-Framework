# Harvest

## Intent

A player hitches a reaper behind a tractor or a UAZ, drives over a wheat field, and the wheat under the bar is cut
and the grain goes into the hopper. The field is a server-side grid of 3 m cells; only the cut cells are networked.
Design and card list: the harvester spike (epic #3282).

## Interaction pattern

In-world only: the field is the interface and the hopper is a number on the reaper's prompt. Nothing opens a sheet.
This slice is the pure rules; the entity (#3287), the replication (#3288) and the reaper (#3289) build on it.

## Wheat field rules (#3285)

- **Player sees:** nothing yet. Later cards turn these rules into cut wheat, a hopper counter and two hints:
  *Too fast to cut, slow down* and *Hopper full, unload at the granary*.
- **Owner and side:** `LF_WheatGrid` (the cell grid, the cut bitset and the bar footprint) and `LF_WheatFieldRules`
  (the speed gate, the yield, the hopper) are pure logic with no entity, world or clock; the server calls them.
- **Data:** a cell is `row * columns + column` (columns along world X from the box's low corner, rows along Z); a
  60 x 40 m box is 20 x 13 = 260 cells. The cut state is one bit a cell in `array<int>` words, 32 to a word, so 260
  cells are 9 ints (`ExportWords`/`ImportWords` are the late-join snapshot). The bar is `width` across the heading and
  `depth` along it (heading is a yaw, 0 facing +Z); a cell counts when the bar overlaps it by more than 1 cm.
  Speed cuts from 0.5 to 6 m/s (`STOPPED` below, `TOO_FAST` above), a cell yields 10 kg, the hopper holds 600 kg
  (`HOPPER_FULL`). Strings: `LF-Harvest_TooFast`, `LF-Harvest_HopperFull`. Nothing is persisted: the cut state is
  session-only and a restart regrows the field.
- **Proof:** `harvest/wheat-field-rules` (LOGIC).
