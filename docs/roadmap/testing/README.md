# Testing plan

Every roadmap setup is done only when three gates are green. A scope doc's Proof section is the short list. This file is the case list, the DebugWorld entities, and the shots.

| Gate | Where it runs | Proves |
| --- | --- | --- |
| Headless script | `tools\cli test --tier fast` in a worktree | Rules, refusals, money math, lock state, config guards. No world, no pixels |
| E2E | `tools\cli test` (tier `all`) on DebugWorld | The named entity on the layer loads, the action runs, inventory and lock state match |
| Visual | Workbench play, `agent-box` or `wb_capture` | A mesh is the right object and not the white placeholder. A menu shows the right rows. The headless server has no renderer, so this gate is never part of `cli test` |

`cli test` refuses the main checkout. Open a worktree first.

## File rules

- One test file per setup: `Scripts/Game/Tests/EL_Test_Roadmap_<Setup>.c`. Do not add cases to another setup's file.
- `// tier: LOGIC` or `// tier: WORLD` or `// tier: PERSISTENCE` above each class. `// red-proof:` records one observed failure before the test is trusted.
- `tools\cli regen-tests` after adding the file. `tools\cli validate` checks the registry.
- Logic test name: `roadmap/<setup>`. World test name: `e2e/roadmap-<setup>`.
- Assertions go through public functions. A persistence test mutates, reloads, and reads back. It does not touch save internals.
- A zero fuse, a fixed payout, and a fixed sentence are the test config. Random ranges are not asserted.
- `Run()` is one pass. Time-based behavior (a real fuse, a jail tick) is tested as pure math in LOGIC. The world entity uses a zero duration so the E2E test can see the result in that single pass.

## The layer

Add `Worlds/DebugWorld/DebugWorld_Layers/Roadmap.layer`. Existing layers stay the home of systems already there (apple chain, `Crimes.layer`, `Police.layer`, `HousesKeys.layer`). This layer is the street of release setups that do not have a site yet.

Placement, clear of the current area boxes (the easternmost box is `PRFeatures.layer` near x=182):

- Root entity `RoadmapStreet` at `186 0 120`.
- Sites every 3 m along +X, from the breach door at `186 0 120` to the vehicle lot at `243 0 120`. Ground props at Y=0. `Flags 0x403` on any new traceable mesh.
- Debug terrain is 256 m on X and Z (world 0..256). The row stays on that ground, clear of the police box that ends at x=172. Do not shrink the pitch.
- One `EL_DebugAreaBoundary` (`{AF76315F0A38D270}Prefabs/Debug/EL_DebugAreaBoundary.et`) around the row. `validate-debugworld-layout.ps1` must report no overlap and no raised ground prop.
- The world entity stays in `default.layer`. Do not add `GenericWorldEntity world` here.
- Add `Roadmap.layer` to the required list in `tools/validation/validate-debugworld.ps1`. An empty file fails that check.

Entity names are the contract. Tests find them by name. Shots use the same name. A renamed entity fails the world test and the flow.

Buildings that are larger than a debug pad (the hospital, the fuel station, the villa) are not dropped into this row. The layer gets the till, door, or cabinet. The whole building is a `wb_capture` spawn that is deleted after the shot.

## How a shot is taken

The street pass is one command. Play mode has to be running. It does not boot the test server.

```
node agent-box/harness.mjs report roadmap
```

It walks every `Roadmap_` entity, stands where a player would stand, runs `look`, and takes a world shot. `agent-box/reports/roadmap.html` places each picture beside that text. One wide shot of the row is at the top. A row fails when the mesh is white or missing, look offers no interaction, a menu is blank, or the site is more than 30 m from the rest of the street. `node agent-box/harness.mjs selftest` marks an empty look and a site 80 m away as failures, with no game.

Asset, no menu:

```
node agent-box/harness.mjs shot world <EntityName>
```

Menu, after the interaction:

```
node agent-box/harness.mjs layer <EntityName>
```

That stands at the entity, performs the action, and writes a PNG. Flow files live in `agent-box/flows/roadmap-<setup>.txt` and use the same steps as `flows/world-atm.txt` and `flows/ui-shop.txt`.

A building that is not on the layer:

```
tools\cli call wb_capture '{"position":"400 0 400","cameraOffset":"18 8 18","timeOfDay":12,"weather":"Clear","name":"roadmap-<setup>","prefab":"<prefab>"}' enfusion-workbench
```

`wb_play` first, same MCP session. Read the PNG. White or missing mesh fails the gate. Captures stay in the Workbench logs directory, not in the repo.

## Release setups

### Forced entry

Layer: `Roadmap_Breach_Door` (`EL_Door_Base`, locked), `Roadmap_Breach_Charge` (demo block on the ground beside it).

| Gate | Case |
| --- | --- |
| Headless | Unconfigured target refuses. Open target refuses a second charge. Repair sets the lock. Wanted and the police notify fire once, at arm time, not on offer refresh |
| E2E | `e2e/roadmap-breach` finds `Roadmap_Breach_Door`, runs the zero-fuse config, door `IsLocked()` is false, repair sets it true. Prefab `DemoBlock_M112.et` loads |
| Visual | `flows/roadmap-breach.txt`: `shot world Roadmap_Breach_Door`, then `shot world Roadmap_Breach_Charge`. The charge is the vanilla block, not a white mesh |

Persistence class, same file: save while the door is open, apply the save, `IsLocked()` is still false.

### Bank robbery

Layer: `Roadmap_Bank_Vault` (locked door), `Roadmap_Bank_Bag` (hand-carry container). The municipal-office shell is a capture-only prefab, not placed in the row.

| Gate | Case |
| --- | --- |
| Headless | Payout is cash. `EL_BankAccount` balance stays 0. Bag count and cash per bag match config. A second breach inside the cooldown spawns nothing. Civilian and minimum-police gates match `EL_RobAction` |
| E2E | Zero-fuse vault opens, one bag exists, its MoneyStack quantity is the config value, bank balance unchanged. Same uid discipline as `e2e/narrative-loop` (do not reuse `e2e-narrative-uid`) |
| Visual | `shot world Roadmap_Bank_Bag` shows the crate mesh and, after the action, the stack. `wb_capture` of `EL_Bank.et` (`MunicipalOffice_E_01`) for the shell |

### Jailbreak

Layer: `Roadmap_Jail_Door` (prison-cell door plus `EL_LockComponent`), `Roadmap_Jail_Cell` (cell volume).

| Gate | Case |
| --- | --- |
| Headless | Locked volume returns an escaped position to the cell. Open door does not. Escape does not zero the remaining sentence. Wanted on the breaker increases |
| E2E | Zero-fuse open, confinement predicate is false, repair makes it true. Door prefab loads |
| Visual | `shot world Roadmap_Jail_Door` closed, and a second shot after the test config opens it |

### Hospital robbery

Layer: `Roadmap_Hospital_Till`, `Roadmap_Hospital_Cabinet` (locked container).

| Gate | Case |
| --- | --- |
| Headless | Till pays MoneyStack and does not touch the bank. Cabinet loot is exactly the config list (field dressing, morphine, saline, tourniquet). A missing prefab logs and skips that line. One alert per action |
| E2E | Till once per cooldown. Cabinet yields the four medicine prefabs and they `Resource.Load` |
| Visual | `shot world Roadmap_Hospital_Till` and `shot world Roadmap_Hospital_Cabinet`. `wb_capture` of `HospitalMilitary_E_01` for the building. Each medicine prefab once, so a white placeholder fails |

### Gas station robbery

Layer: `Roadmap_Fuel_Till`.

| Gate | Case |
| --- | --- |
| Headless | Same pins as `crime/rob-reward` and `crime/rob-wanted-clamp`: cash up, bank unchanged, wanted up, cooldown refuses, refusal is logged |
| E2E | `Roadmap_Fuel_Till` pays the fixed test amount once |
| Visual | `shot world Roadmap_Fuel_Till`. `wb_capture` of `FuelStation_E_01` |

### Jailing

Layer: `Roadmap_Jail_Cell` (shared with jailbreak), `Roadmap_Police_Desk`.

| Gate | Case |
| --- | --- |
| Headless | Sentence length comes from wanted level and clamps to the config max. Arrest of a clean player refuses. Expiry reduces wanted. `GetJailPosition()` is not the literal `"127 1 137"` once the cell exists; the test reads the placed cell origin |
| E2E | Arrest of a wanted test account moves the stand-in to `Roadmap_Jail_Cell`. Zero-duration config releases and reduces wanted |
| Visual | `shot world Roadmap_Jail_Cell`. If the sentence UI is ported (`LNGC_JailDurationDialog`), `layer Roadmap_Police_Desk` and `shot ui roadmap-jail-duration` shows the duration field and the confirm button |

Persistence: remaining seconds survive apply-save.

Vehicle escort is a later slice of this setup. Headless covers nearest free non-driver seat and "not the escorting officer." Two clients in a vehicle are a manual note in the test file, not a `cli test` case. Visual for that slice is `shot world` of the police M151 with a door open, plus the put-in action offered in `look`.

### Police ranks

Layer: `Roadmap_Police_Armory`.

| Gate | Case |
| --- | --- |
| Headless | Kit list is the rank's prefabs only. Self-promotion refuses. A lower rank's arrest refuses and is logged. A missing prefab drops that line and leaves the rank usable |
| E2E | On-duty lower rank receives the M9 prefab from the armory action. Higher rank receives `Rifle_M16A2.et`. The other does not |
| Visual | `shot world Roadmap_Police_Armory`. `wb_capture` of the LNGC police cap, turtleneck, and trousers, and of `S105_police.et`. `layer Roadmap_Police_Armory` if the armory opens a menu; the rows must be the two ranks, not a blank list |

### Contraband

Layer: `Roadmap_Hideout_Machine`, `Roadmap_Hideout_Stash`, `Roadmap_Convoy_Start`, `Roadmap_Convoy_Drop`.

| Gate | Case |
| --- | --- |
| Headless | Non-member process refuses. Delivery pays cash once. A second delivery of the same run pays nothing. Confiscation removes only the flagged prefab |
| E2E | Member processes one stack at `Roadmap_Hideout_Machine`. A spawned job vehicle from `Roadmap_Convoy_Start` to `Roadmap_Convoy_Drop` pays the fixed cash. Black-market trader still rejects a non-civilian |
| Visual | `shot world Roadmap_Hideout_Machine` and `shot world Roadmap_Hideout_Stash` (ShineStash mesh, not white) |

Persistence: a stack left in the stash is still there after apply-save.

### Resources

No new row. The chains already have layers: `AppleChain`, `PlumChain`, `TomatoChain`, `LoggingChain`, `MiningChain`, `CementChain`, `GravelChain`, `SandChain`.

| Gate | Case |
| --- | --- |
| Headless | Existing `EL_Test_ActionGuards` stays green. A new case: gather reward and process reward stay 0 unless a job scope says otherwise |
| E2E | Existing `e2e/narrative-loop` (apple, trader, shop, ATM). Add `e2e/roadmap-resources` for one furnace line: two iron nuggets become one iron bar, inventory conserved |
| Visual | `shot world` of the named juicer, furnace, and sawmill already on those layers. One new flow `roadmap-resources.txt` shoots furnace and sawmill so the release gate has a current PNG |

### Civilian jobs

Layer: `Roadmap_Job_Depot` (post counter), `Roadmap_Job_Drop`, `Roadmap_Job_TaxiPad`, `Roadmap_Job_FuelCan`.

| Gate | Case |
| --- | --- |
| Headless | A completed run pays once and adds job XP (`AddExperience` is a no-op today; this test is written red first). Under-level signup refuses. Abandoned run pays nothing |
| E2E | Parcel from `Roadmap_Job_Depot` to `Roadmap_Job_Drop` on the hand-carry prefab. Fuel can prefab loads. Taxi fare pays the fixed amount without a second client |
| Visual | `shot world Roadmap_Job_Depot`, `shot world Roadmap_Job_FuelCan`, `shot world Roadmap_Job_TaxiPad` with the S105. `layer Roadmap_Job_Depot` for the signup list: parcel, fuel, taxi |

### Organizations

Layer: `Roadmap_Org_Desk`.

| Gate | Case |
| --- | --- |
| Headless | Withdraw without rank refuses. Upgrade price is all-or-nothing. Personal ATM balance does not change when the fund pays. One membership per character |
| E2E | Create, add a member, deposit a fixed cash amount, buy the processing-slot flag |
| Visual | `layer Roadmap_Org_Desk`. The menu lists name, fund balance, and the upgrade. Empty state before create is a second shot |

Persistence: members, fund, and the flag survive apply-save.

### Hideouts

Layer: `Roadmap_Hideout_Door` plus the machine and stash from contraband.

| Gate | Case |
| --- | --- |
| Headless | Non-member unlock refuses. Machine without the processing-slot upgrade refuses and logs |
| E2E | Member unlocks `Roadmap_Hideout_Door`. Non-member does not |
| Visual | `shot world Roadmap_Hideout_Door`. `wb_capture` of `Garage_E_01` for the shell |

Persistence: organization id on the lock survives apply-save.

### Vehicles

Layer: `Roadmap_Vehicle_S105`, `Roadmap_Vehicle_Lot`.

| Gate | Case |
| --- | --- |
| Headless | Plate generator covers the full alphabet and the top number. A second issue does not reuse a live plate. Purchase debits the configured price or refuses |
| E2E | Buy spawns the S105 prefab, owner matches, wrong key does not unlock, matching key does. Extends the existing `EL_Test_VehicleLock` fixture rather than a second lock implementation |
| Visual | `shot world Roadmap_Vehicle_S105` (body, not white). `wb_capture` of `UAZ469_covered_CIV_blue.et`, `S1203_transport_beige.et`, and `M923A1_transport.et`. `layer Roadmap_Vehicle_Lot` for the catalog rows |

Persistence: owner id and key id match after apply-save. Garage store/retrieve, once ported, is a second persistence class: store, apply-save, retrieve, same prefab and lock flag.

## After release

Same three gates. Add the entities to `Roadmap.layer` when the setup starts, inside the same boundary, further along +X at the 3 m pitch. Extend the boundary with the row. Stop before the terrain edge at 256. Do not open a second layer.

| Setup | Layer entities | Headless | E2E | Visual |
| --- | --- | --- | --- | --- |
| Private housing | `Roadmap_House_Door` | Non-owner unlock refuses. Price is all-or-nothing | Buy, lock, store one item | `shot world Roadmap_House_Door`. Office menu via `layer` |
| Crafting | `Roadmap_Craft_Bench` | Missing inputs and a bad recipe refuse before inventory changes. Output count matches | One iron-nugget recipe on the bench | `shot world Roadmap_Craft_Bench`. Menu rows via `layer` |
| Tasks | `Roadmap_Task_Board` | Reward pays cash once. Expired task pays nothing | Accept, deliver, quest log shows complete | `layer Roadmap_Task_Board` and `shot ui roadmap-quest-log` |
| Revive | `Roadmap_Medic_Pad` | No item refuses. Timer expiry calls the death path once | Zero-timer downed state reaches `EL_DeathScreen` | `shot ui roadmap-death-screen`. Downed character at the pad |
| Police missions | `Roadmap_Police_Board` | Off-duty accept refuses. Completion pays once | Fine at the marker completes the call | `layer Roadmap_Police_Board` shows the call row |
| Events | `Roadmap_Event_Marker` | A second event does not start. End despawns the prop | Marker entity exists only while the test event runs | `shot world Roadmap_Event_Marker` during the event, and a shot after end with the prop gone |

## Done means

1. `tools\cli validate` includes `Roadmap.layer` and the new test registration.
2. `tools\cli test --tier fast` is green for every new LOGIC class.
3. `tools\cli test` is green for the new WORLD and PERSISTENCE classes.
4. Each visual row above has a PNG read by a person or an agent, and a white or missing mesh was fixed in that same change.
