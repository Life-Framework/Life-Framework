# DebugWorld chains and the agent box

`node agent-box/harness.mjs chains` checks the probe list against the layer
files. No game. `node agent-box/harness.mjs chains --run` is the play-mode
e2e: it spawns through the faction buttons when no character exists, then
stands beside each named probe, reads `look` (the same 4m list as the F key),
and calls `use` on the expected action. `agent-box/flows/chain-apple.txt` is
the apple gather-then-trade sequence on its own.

`layers` lists a named entity that has an `*Action` inline, or on its addon
prefab (one parent hop). Vanilla actions such as `SCR_DoorUserAction` stay
invisible to that catalog. `stand` still finds them by name.

## Probes `chains --run` can reach

| Layer | Entity | Action `look` must show |
| --- | --- | --- |
| ATM | `ATM_Debug` | `EL_ATMAction` |
| Apple | `AppleGatherTree`, `AppleTradeContainer`, `AppleProcessor` | gather, open trader, process |
| Plum | `PlumGatherTree`, `PlumTradeContainer` | gather (prefab only), open trader |
| Tomato | `TomatoGather`, `TomatoTradeContainer` | gather, open trader |
| Farming | `TomatoCrop_Debug` | gather |
| Cement | `CementPlant_Debug`, `CementPile_Debug` | process, gather |
| Gravel | medium and large piles | gather |
| Sand | `SandPile_Debug` | gather |
| Mining | `IronOre_Debug`, `GoldOre_Debug`, `CopperOre_Debug`, `Furnace`, `Pickaxe_Debug` | gather, process, pick up |
| Logging | `LogPile_Debug`, axe, `Sawmill_Debug`, `CuttableTree_1` | gather a log, pick up, process, stand only (melee tree) |
| CrimeChain | `CrimeTrader_Debug`, `CrimeFurnace_Debug` | open trader, process |
| Jobs | `GatherPoint_AppleTree` | gather |
| Shops | general store, black market | `EL_ShopAction` |
| Police | `PoliceStation_Debug` | `EL_DutyAction` |
| Survival | food and water points | gather |
| Houses | `House_Door_Locked` | `SCR_DoorUserAction` |
| Crimes (not `CrimeChain.layer`) | money, weapon, vehicle points | rob actions |
| Trader | `TraderChainCounter` | open trader |

## Fixes

Named the action hosts `stand` could not see, and taught `layers` to read
`*Action` classes from the addon prefab (one parent hop). Probes now cover
cement, gravel, sand, the crime trader and furnace, the sawmill, and the ore
meshes themselves (`IronOre_Debug`, `GoldOre_Debug`, `CopperOre_Debug`), so
the 6–8m offset from the empty rock shells is not the stand target.

`TomatoGather` and `TomatoCrop_Debug` set the first three crop stage times to
0 and the ripe stage to 30 minutes, so both are gatherable within a few
frames of play start. The crop prefab still inherits `EL_DeleteAction`, which
deletes the plant and grants nothing. `SurvivalPoint_Water` moved from 4.0m
to 3.2m off `SurvivalStation`, inside the look sphere.
`TraderChainTomatoNpc` moved next to its parent, and `TraderChainCounter` is
a named universal trader.

## Still not an F-key action

Cuttable pines (`CuttableTree_1` through `_3`) are axe hits through
`EL_DestructibleResourceComponent`. The probe stands beside the first tree.
It does not swing. `House_Door_Locked` uses vanilla `SCR_DoorUserAction`,
which is not written in the mod prefab, so `layers` still omits it; `stand`
and `use` address it by name. `RobberyPoint_Vehicle` is 7m from `CrimeScene`;
stand on the point, not the parent.

## Harness bug fixed in this change

Merged `play` called `wb_stop` and `wb_play` in two fresh Workbench bridge
processes. The bridge starts in an unknown mode and refuses `wb_play` until
`wb_state` has run in that same process. `play` now reads mode, stops only
when play is already up, opens DebugWorld, reads mode again, and then enters
play.

## Live run (2026-10-10)

Launch Workbench from the Arma Reforger install directory with
`-gproj` pointed at the worktree and the default profile. That loads the
base-game addon and the profile EnfusionMCP addon on port 5775. A fresh
process can `play` DebugWorld. Calling `wb_play` on a world that is already
open in edit mode hangs the bridge; kill that process and relaunch.

`chains --run` on log `logs_2026-10-10_20-00-15`: 36 probes,
`stand_fail=0`, `missing_action=0`, `use_fail=0`. The log recorded
`Sandbag.et x1`, `Brick.et x10`, two `IronBar.et`, `WoodPlank.et`,
`faction set to POLICE`, and `duty toggled -> off duty`. The dedicated
server fast tier had already passed 54/54 earlier the same day.

The eight probes that refused on the first run, and what let them perform:

| Probe | What changed |
| --- | --- |
| `CementPlant_Debug` | `Sandbag.et` is a child of the gravel bag with an explicit 1x1 volume slot. The old 2x1 bag spawned and the inventory rejected it, so the plant never saw its third input. |
| `Furnace` | Iron, gold, and copper nuggets use a volume slot (`SLOT_1x1`). `IronOre_Debug` grants 4 nuggets, enough for both furnaces. `use EL_ProcessAction` hits the first action, which is the iron smelt. |
| `Sawmill_Debug` | `LogPile_Debug` grants `WoodLog_01_Small.et` before any weapon pickup, so the log stays in hand and the sawmill can count it. A later pickup drops it. |
| `CrimeFurnace_Debug` | Same furnace prefab and the same 4 nuggets, so the second iron smelt still has a stack. |
| `PoliceStation_Debug` | The chain opens the debug menu and clicks `FactionPoliceButton` first. That sets police and duty; the duty action then toggles. |
| `RobberyPoint_Money` / `_Weapon` / `_Vehicle` | Those three DebugWorld points set `m_iMinPoliceOnDuty` to 0. The action classes still default to 2 / 1 / 3. The probes run while the character is still civilian. |
