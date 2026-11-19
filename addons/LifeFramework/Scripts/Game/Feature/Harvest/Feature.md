# Harvest

## Intent

A player hitches a reaper behind a tractor or a UAZ, drives over a wheat field, and the wheat under the bar is cut
and the grain goes into the hopper. The field is a server-side grid of 3 m cells; only the cut cells are networked.
Design and card list: the harvester spike (epic #3282).

## Interaction pattern

In-world only: the field is the interface and the hopper is a number on the reaper's prompt. Nothing opens a sheet.
The rules (#3285), the field entity (#3287) and the cut replication (#3288) are in; the reaper (#3289) builds on them.

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

## Wheat field entity (#3287)

- **Player sees:** a field of wheat patches in a placed box; cut cells show stubble (the cut itself comes with the reaper, #3289).
- **Owner and side:** `LF_WheatField` (entity, holds the `LF_WheatGrid`) and `LF_WheatFieldView` (the local meshes). Every machine
  but a dedicated server draws its own patches from the bitset; they are local entities, never replicated. `CutCells` and `Regrow`
  are the server calls the replication (#3288) will broadcast.
- **Data:** `Configs/Harvest/LF_WheatFields.conf` (patch, stubble, cell size). The box is `m_fSizeX` x `m_fSizeZ` on the entity.
- **Proof:** `harvest/wheat-field-cut` (WORLD).

## Cut replication (#3288)

- **Player sees:** the same cut strips and stubble as every other player, also when joining late; a dedicated server draws nothing.
- **Owner and side:** a `modded class LF_WheatField` in `LF_WheatFieldReplication.c`. The server batches newly cut cells (one reliable RPC per field per 250 ms, packed bitset words in a string) and sends one regrow RPC; `RplSave/RplLoad` carries the whole bitset for a late joiner. `LF_WheatFieldWire` and `LF_WheatCutBatch` are the pure parts.
- **Proof:** `harvest/wheat-field-snapshot` (LOGIC); the dedicated-server session with a late joiner is in the PR.
