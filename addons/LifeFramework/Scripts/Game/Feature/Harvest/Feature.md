# Harvest

## Intent

A player hitches a reaper behind a tractor or a UAZ, drives over a wheat field, and the wheat under the bar is cut
and the grain goes into the hopper. The field is a server-side grid of 3 m cells; only the cut cells are networked.
Design and card list: the harvester spike (epic #3282).

## Interaction pattern

In-world only: the field is the interface and the hopper is a number on the reaper's prompt. Nothing opens a sheet.
The rules (#3285), the field entity (#3287), its cut replication (#3288) and the hitch (#3284) are in; the cut tick (#3289) is in.

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

## Implement hitch (#3284)

- **Player sees:** a reaper behind a UAZ469 or a Z7211 once something hitches it (the implement rack, H8). Other players
  and late joiners see it follow the vehicle.
- **Owner and side:** `LF_ImplementHitch` (server statics: `Hitch`, `Unhitch`, `FindHitched`, `RowFor`) and
  `LF_ImplementComponent` on the reaper (width, replicated hopper kg, a log line per machine as the vehicle moves).
- **Data:** `Configs/Harvest/LF_Hitches.conf`: the reaper prefab and one row per towing vehicle (prefab or ancestor,
  local offset and angles). The reaper is a rigid child with no physics body; detaching deletes it.
- **Proof:** `harvest/implement-hitch` (WORLD); `session hitch-sync` (`up --clients 1 --profile full`, it joins client 2 late): harness verb `hitch` (`LF_ImplementHitchHarness.c`) spawns a UAZ469 on the grass south of the debug grid, hitches, pushes it 50 m east and deletes it the garage way; both clients must see the reaper behind it, then gone.

## Cut replication (#3288)

- **Player sees:** the same cut strips and stubble as every other player, also when joining late; a dedicated server draws nothing.
- **Owner and side:** a `modded class LF_WheatField` in `LF_WheatFieldReplication.c`. The server batches newly cut cells (one reliable RPC per field per 250 ms, packed bitset words in a string) and sends one regrow RPC; `RplSave/RplLoad` carries the whole bitset for a late joiner. `LF_WheatFieldWire` and `LF_WheatCutBatch` are the pure parts.
- **Proof:** `harvest/wheat-field-snapshot` (LOGIC); the dedicated-server session with a late joiner is in the PR.

## Owned implements and the rack (#3295)

- **Player sees:** buy a reaper at the farm supply counter ($1,200, once); at the implement rack, *Take out reaper* hitches it to a UAZ469 or Z7211 within 8 m, *Put back reaper* unhitches it and dumps the hopper with a warning toast.
- **Owner and side:** `LF_OwnedImplements` (server: owned list per character record key, what is out, `Purchase`, `TakeOut`, `PutBack`), `LF_ImplementRackAction` (the two prompts), `LF_HarvestSavePayload` (save section `harvest`). Only the owned list is saved; what is out is session state, so a reaper that is out at a restart is back on the list, not in the world.
- **Proof:** `harvest/implement-rack` (WORLD), `harvest/persist-owned-implements` (PERSISTENCE), `everon/farm-supply` (pack leg).

## Reaper cut tick (#3289)

- **Player sees:** the wheat under the bar of a hitched reaper turns to stubble at 0.5 to 6 m/s, the hopper climbs 10 kg a cell, and a hint says *Too fast to cut, slow down* or *Hopper full, unload at the granary* once each time it starts.
- **Owner and side:** `LF_ReaperCutTick` (server only): a registry of reapers (filled by `LF_ImplementComponent`) and a 200 ms loop. Speed is the towing vehicle's physics velocity; the bar sits at the implement's hitch offset in the vehicle's frame; the field is the `LF_WheatField` whose box holds it.
- **Data:** the hopper is `LF_ImplementComponent` `m_iHopperKg` (`RplProp`), capped at `LF_WheatFieldRules.HOPPER_CAP_KG`. Session-only: the reaper is not saved, so a restart or an unhitch loses the grain (H7 and H8 own persistence or payout).
- **Proof:** `harvest/reaper-cut` (WORLD); session `hitch fieldspawn|drive|state` on grid cell slice-3287.

## Harvest contract (#3297)

- **Player sees:** at the farm board, one contract per ripe field; take it with an own reaper or a $300-deposit rental kit (Z7211 with the DLC, else UAZ469); at 95 % cut a $40 bonus and FARMER XP; returning the kit gives the deposit back, keeping it past 30 minutes forfeits it. The board prompts are `LF_HarvestBoard` (#3656).
- **Owner and side:** `LF_HarvestContract` (server statics: `Post`, `Take`, `Settle`, `ReturnKit`, a 5 s sweep). Contracts are session state; open deposits are the save section `harvest_deposits`, refunded to the bank once on the next boot. Money moves state-first so a repeat pays nothing; a kit is never deleted with someone seated.
- **Proof:** `harvest/harvest-contract` (WORLD).

## Farm contract board (#3656)

- **Player sees:** a cork notice board (poster style) at the wheat field gate with four prompts: *Post contract*, *Take with own reaper*, *Take with rental kit*, *Return kit*. The server answers each with a toast. A rental kit stands five metres from the board on the side away from the field. A `harvest` chalkboard (the deposit and the bonus) stands on the other side of the gate.
- **Owner and side:** `LF_HarvestBoard` (server) finds the board's field (nearest `LF_WheatField` within 90 m), checks the user stands within 6 m and calls `LF_HarvestContract`. `LF_HarvestBoardAction` is the prompt; the rental prompt runs on the acting client only (`HasLocalEffectOnlyScript`), asks `LF_VehicleDlcGate.LocalOwns` for the Z7211 and sends it with one RPC (`SCR_PlayerController.LF_AskHarvestRental`); the other three run on the server. Prefab `Prefabs/Harvest/LF_HarvestBoard.et` inherits `LF_Sign_Board`.
- **Proof:** `harvest/harvest-board` (WORLD); a rental on a host with no Rally content refunds the deposit (`KIT_FAILED`).


- **Player sees:** at the farm board, one contract per ripe field; take it with an own reaper or a $300-deposit rental kit (Z7211 with the DLC, else UAZ469); at 95 % cut a $40 bonus and FARMER XP; returning the kit gives the deposit back, keeping it past 30 minutes forfeits it. The board prompts come with the placement card.
