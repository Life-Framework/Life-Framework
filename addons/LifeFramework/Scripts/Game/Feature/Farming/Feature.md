# Farming — Design Requirements

> Context file for farm plots (#614). Written against `docs/design.md` (in-world first).

## Intent

A player who walks onto the farm expects: get seeds, plant them in the dirt, watch them
grow, harvest. The plot is the interface. There is no menu and no progress bar; the crop
on the bed is the state, and the prompt says it in words.

## Loop (V1, shipped)

1. **Seeds.** `LF_SeedCrate_Cabbage.et` at the farm: `LF_GatherAction` *Take Cabbage seeds*
   gives one `CabbageSeeds.et` packet, four per refill (2 min).
2. **Plant.** An empty `FarmPlot_Cabbage.et` offers *Plant Cabbage seeds*; one packet is used.
3. **Grow.** The server grows the crop through the plot's `LF_CropStage` list (the same stage
   class `LF_BaseCrop` uses): sprouts, half grown, ripe, one minute each in the prefab. The last
   stage holds. The prompt reads *Growing: sprouts* until it is ripe.
4. **Harvest.** *Harvest Cabbage* gives three cabbages (overflow drops at the feet) and the plot
   is empty. Sell at the general store; cabbages also feed the moonshine mash.
5. **Info.** The `farm_plots` chalkboard beside the plots says all of the above.

## Shape

- `LF_FarmPlotComponent`: server state (planted, seconds grown), one replicated stage index,
  local crop copies on every machine but a dedicated server, `ServerPlant` / `ServerHarvest` /
  `ServerAdvance`, `Export` / `Import`.
- `LF_FarmPlots`: live plots by `m_sPlotKey`, the 1 s growth tick, pending rows, `ExportAll` /
  `ApplyAll` (the `farmplots` payload, save version 23).
- `LF_FarmPlotRules`: pure stage math and refusal keys (LOGIC test `farm/plot-rules`).
- `LF_FarmPlotAction`: PLANT and HARVEST modes on the plot.

## Iteration path

- More crops: a plot prefab per crop (crop id, seed, harvest, stages, visual), or a seed that
  names its crop so one plot takes several.
- Seeds bought, not free: a farm stand or general store line.
- Care: watering or weeds that slow growth; spoil if left ripe too long.
- Jobs and levels: farmer pay per harvest through `LF_JobManager` scope rewards.
