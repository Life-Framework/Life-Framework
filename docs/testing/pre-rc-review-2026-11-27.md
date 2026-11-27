# Pre-RC risk review, 2026-11-27

This review covers every PR merged to `main` from 2026-11-26 18:00 UTC to `921ff3551` (#5115), ahead of the 0.3.0 cut. That is 109 first-parent merges; 69 of them touch `addons/`. A merge gets a row when it touches replication, an RPC, persistence, money, inventory, GUIDs, shop prices or pack data. The review looked only for failures a player would hit: a dedicated server behaving unlike a listen host, a late joiner, a restart load, load order, and a GUID that is duplicated or moved.

The rest are out of scope and get no row:

- Text-only and localization PRs (#5067, #5068, #5070, #5086, #5087, #5088, #5093, #5100, #5102, #5008, #4993 dialogue).
- Night-light layer drafts (#5003 to #5026, #5094, #5095).
- Test-only PRs (#5007, #5037, #5039, #5050, #5073).
- Harness and tooling PRs (#4988, #4989, #5083, #5099, #5101, #5103).

Method: `git diff <merge>^1 <merge> -- addons/` for each listed PR. Configs, prices and GUIDs were checked by hand against `origin/main`. The script diffs had a second, independent read-only defect review (interrogate step 2). Earlier dismissals for #5044, #5058, #4997 and #5059 come from the same day's review notes and were re-read.

| PR | Area | Verdict | Evidence |
|---|---|---|---|
| #5111 crop-rpl | Replication, prefabs | clean | Each of the four new `RplComponent` GUIDs occurs once in `addons/`. The fell action is a local helper with no Rpl, and `HasLocalEffectOnlyScript` makes that explicit. The new plantsync verbs are harness-only. |
| #5110 gun-counter-overlay | Shop prices, catalogue | clean | The gun-store hat price rules were dropped. `LF_ShopPrices_ClothingStore.conf` still prices the same four prefabs, so there is one owner and the hats still sell. The flashlight is priced by GeneralStore and core. Item GUID `{E891CEA8D6C21C81}` moved files and was not duplicated. The net gun-counter offers are unchanged: the overlay used to remove M9 mag and vz58, and now they are simply never offered there. The black market still offers vz58 and its mag. |
| #5098 reland-5096 (after #5096, #5097) | Catalogue, mannequins | clean | The net change removes one offer (`jacket_raincoat_green` from `clothing_rack`). `validate-mannequin-outfits` is green on main. |
| #5085 shop names | Shop stock | clean | Adds `m_sDisplayName` only. No price or prefab changed. |
| #5078 job tables (after #5074, #5077) | Pack jobs, licences | clean | The pack drops the MINER_COPPER and MINER_GOLD gates and unlock rows, because the island has no copper or gold. `LF_ELicenseType` is unchanged, so a saved licence still loads. |
| #5069 copper orders | Pack export orders | clean | Drops the `copper_bars_steamer` template. Export orders are never saved (`LF_ExportOrderBook.c:2`), so no restart can load a missing template. |
| #5058 pay audit | Money | clean | Wholesale fish now pays 11-24 against counter prices of 40-84, and smokehouse sell prices stay under counter prices, so there is no buy-low, sell-high loop. Catalogue overlays merge by id. |
| #5001 fishing cove | Prefab base | clean | `LF_FishingSpot.et` now inherits a bucket instead of a cone. The spot has no actions, so it needs no Rpl. |
| #5022 honey kitchen | Prefab | clean | A mesh swap and an action-context offset. Same processor base. |
| #5014 Levie pub signs | GUID | clean | The new `.conf.meta` GUID `{B6E04A7D91C2358F}` is unique. |
| #4993 text audit | Pack manifest | clean | The `harbourName` default text changed. Nothing matches on the string; only display uses it. |
| #4990 npc voice audit | Dialogue configs | clean | Adds voice rows (event, length, mouth curve). No state, money or replication. |
| #5059 static init | Statics, load order | clean | All 49 lazy getters were scanned. Every direct field read has its getter on the line before. No brace-less body was split, and no receiver/argument order trap remains. |
| #5044 drive harness | RPC | clean | `RpcAsk_LF_DriveRoute` is gated by `LF_DevGate.CheckHarnessRequest`. A locked car is still enforced server-side by `LF_VehicleEntryGuard`. |
| #4997 save budget | Persistence test | clean | Test only, PERF tier. Per-player sections are still measured. |
| #5075 saw output reach | Inventory, processing | clean, one playtest look | The same `m_iOutputAmount` is spawned and counted as before; only the drop point moved. It now swings toward the worker at the offset's length (`LF_ProcessDrop.c:19`). Every shipped offset has height 0, so the spawn height (machine origin) is the same as before. A worker at the saw's short end could get boards dropped near the machine's own collision: check this once in the playtest, but it is not proven. |
| #5109 log hygiene | Persistence JSON, Rpl | clean | `SCR_Json*Context` became `Json*Context`: the old classes are obsolete wrappers over the same calls. The chair `RplComponent` now overrides the vanilla GUID `{5624A88DC2D9928D}` instead of shadowing it. The shop name is resolved in `BuildCatalogItem` on every display path. |
| #5108 baseline shrink | Script | clean | +63 lines, all full-line `// lf-fixed:` comments, 0 deletions. |
| #5052 trader sell rows | Money | clean | Lazy static arrays; every read is guarded or follows creation. The rows are client display only, and the server still sets the payout. |
| #5051 jump pad, signs | Component defaults | clean | `m_bEnabled` became `m_bStartDisabled`, but no `.et`, `.layer` or `.conf` overrides `LF_JumpPadComponent`, so no placed pad loses a setting. |
| #5000, #4995 spawn logic | Money, spawning | clean | The starting bank uses `LF_Utils.MaxInt` (same value, no float rounding). The character spawn goes through `LF_Utils.SpawnEntityPrefab` with the same transform and lift. Nearby queries are synchronous. |
| #4996 sirens | Replication | clean | Only bare `Print` calls became `LF_Debug` calls; the conditions are the same. |
| #4982, #4985 code health | UI, script | clean | Null guards where a missing widget used to crash, and handlers removed on close. Deleted `LF_BaseContainerUtils` had no references. |
| #5104 vehicle prices | Money | clean, a fix | `text = text.Trim()` (`LF_VehiclePrices.c:142`): `Trim()` returns a new string, so the old call never trimmed. |
| #5092 shop list height | UI | clean | List heights and compact rows only. |
| #4990 npc voice audit (script) | Script | clean | All 12 `.c` files gain only `// lf-spoken` comments. |
| #5111 police car pool board | Collision | clean | The removed `RigidBody` sat on a model with no collision geometry; the engine already skipped it and logged a triage line. |

## Outcome

No confirmed player-hitting defect: 0 fixed, 0 issues filed. Nothing here blocks the RC. One playtest look stays open: worker-side board drops at the woodlot saw (#5075), from its short end and its long end.

## Part 2: merges after #5115 (#5116 to #5135)

Scope: every PR merged to `main` after #5115 up to `f181916f8` (#5135) that touches `addons/`. These PRs touched nothing in `addons/`: #5116 (part 1 itself), #5121, #5122, #5126, #5128 (docs images) and #5133. #5120 changes one en_us string only.

Method: as in part 1, `git diff <merge>^1 <merge> -- addons/` for each PR, read by hand against `origin/main`. The prefab diffs were also checked for parents and children: each changed `.et`'s GUID was searched for in `addons/`, to catch a second component of the same class that would shadow the first.

| PR | Area | Verdict | Evidence |
|---|---|---|---|
| #5134 civ plates | Prefab components, Rpl, persistence | clean | Nine civilian cars gain `LF_LicensePlateManagerComponent "{5984C85FA25812F2}"`. Their parents are vanilla prefabs, and none of them has a plate manager. `Vehicle_Base.et` uses the same component GUID, but it is not an ancestor of these cars, so nothing is overridden by mistake. Of the children (the 31 pack colours and liveries, plus `LF_S105_PoliceBase`), none declares its own plate manager, so no car ends up with two. The police UAZ469 inherits vanilla `UAZ469_covered_CIV_blue`, not `EL_UAZ469_covered_CIV_blue`, so it keeps its single manager. Each of the 18 new `LF_LicensePlatePointInfo` GUIDs occurs once. Restart: owned cars already save `record.m_sPlate`, because it is issued at purchase whether or not the prefab has a manager. On restore `SetRegistration` applies the saved plate and frees the plate that spawned with the car (`LF_VehicleOwnership.c:1836`). A late joiner gets the plate through `RplSave`/`RplLoad`. |
| #5135 NearestSettlement | Core, map | clean | Display only, read on the client from local location labels. `Nearest` is unchanged for robbery and bus-stop names. The settlement filter reads the vanilla `MapDescriptorComponent` type, and the PR's island session shows every searched place named after a town on Everon. A world whose labels have no descriptor gets an empty town suffix, not an error. |
| #5132 map icon lookup | UI | clean | Client UI only. The imageset quad names are cached in a `ref map<string, ref array<string>>`. An imageset that lists no names falls back to the old load-and-see path. The harness search now returns the names it found. |
| #5130 courier deposit text | Localization, jobs | clean | The key `LF-Job_RunStarted_Courier` became `...CourierDeposit`, and nothing else in `addons/`, `tools/` or `agent-box/` names the old key. The toast's `%1` is the deposit that `BeginRun` actually took, sent with it as an argument. The other run-started keys ignore the extra argument. |
| #5131 JIP sweep | Harness, police | clean | Adds the read-only `LF_IsWornCuffsShown()` and a `worn=` field to the harness `peers` line. Nothing replicated or saved changed. |
| #5125 F2 crew row, F3 done page | UI, activities | clean, one stale comment | `PayText(0)` now reads "no wage", and only below 0 does it read "pay varies". Every shipped `LF_ActivityDef` sets `m_iPayPerHour`, and only `crew` sets 0. The `PlaytestTown` overlay leaves pay alone, because it merges by `IsVariableSetDirectly`. The attribute help still says "0 for unknown": a pack row that leaves the pay out would now read "no wage". No shipped row does that, so it is not filed. Opening the guide sends one `LF_AskPlayerMenu` request (the existing personal-file request); the answer reprints page 1 only when it changes what page 1 says. |
| #5124 first-hour toasts | Quests, notifications | clean | The new `SLOT_VEHICLE` slot reaches the client: `SendToPlayer` → `LF_DeliverNotification(..., m_sSlot)` (owner RPC). A turn-in card takes `SLOT_STEP` only for an auto turn-in step. `m_sTurnInTitleKey` is a new optional attribute that falls back to the old title. The one pack row that uses it has its key in the pack table, under the unique GUID `{2BF04E64189969DC}`. |
| #5118 drive gate layer | Harness | clean | Read-only probes in `LF_DriveHarness`: raycasts, door lookups and layer names. No RPC, money, save or world write. |
| #5120 playtest text | Localization | clean | One en_us string. |

## Part 2 outcome

No confirmed player-hitting defect: 0 fixed, 0 issues filed. Nothing in #5116 to #5135 blocks the RC.

## Part 3: the overlay folder search cache (#5141)

#5141 made `LF_ConfigOverlay.FolderRows` cache each folder's `ResourceDatabase.SearchResources` result in a static map, on the grounds that the database cannot change while the game runs. This runs on the live server for every config load and every NPC dialogue spawn. The map holds resource names only, never loaded resources: each load still calls `BaseContainerTools.LoadContainer`, so the cache can only go wrong by listing the wrong files.

| Risk | Verdict | Reasoning |
|---|---|---|
| A pack addon mounted after the first search | dismissed | The cache is a static in the core addon's own script module. A server or client mounts every addon before it compiles that module, and a change to the addon set (a client joining a modded server) rebuilds the module, which drops every static. No addon can join a module that already ran this code. |
| The first search runs before all addons are registered | dismissed | The first call comes from a config load (game mode init or a lazy catalogue), which is script, so it runs after the mount. Dedicated server boot order and early init are the same case. Before #5141 the same search ran at the same moment, so whatever the first search saw, the uncached code saw too. |
| World change or mission restart in one process | dismissed for the server, **fixed** for Workbench | On a dedicated server the database is fixed for the process, so a new world sees the same file list. In Workbench the script module survives from one play to the next, while a `.conf` created in the resource browser between two plays joins the database. A cached folder (including one cached as empty) then hid the new overlay until a script reload. Fix: `LF_ConfigOverlay_StaticResetHook` in the same file drops the cache at world end. That costs one search per folder per world (about 19 ms each); the suite runs in one world and keeps its speedup. Test: `LF_Test_ConfigOverlayFolderCache` (RED-OK). |
| A folder with no overlays cached as empty forever | same as above | On the server, an empty result stays correct for the process. In Workbench it now lasts until the world ends. |
| Cache key collision between core and pack folders | dismissed | The key is the addon-relative folder path. `SearchResources` searches the merged file system of every addon and the collector matches by path, so an uncached search for that key returns the same rows whichever parent asked. The core and the pack share one path namespace, so a shared key is the same folder. |
| A caller changes the shared array | dismissed | `LoadFolderOverlays` only iterates the rows. |

Part 3 outcome: no live-server defect. One Workbench-only staleness fixed with a world-end reset.

## Part 4: merges after #5146 (#5144, #5150 to #5165)

Scope: every PR merged to `main` after #5146 that touches `addons/`, plus #5144, which merged just before it: #5144, #5150, #5151, #5156 to #5164. #5165 touches nothing in `addons/`. Method as in parts 1 and 2: `git diff <merge>^1 <merge> -- addons/`, read against `origin/main`. Vanilla behavior was read from the installed game paks (`tools\cli api class`, `game_read`), never from memory.

| PR | Area | Verdict | Evidence |
|---|---|---|---|
| #5162 downed flow: possession manager on the game mode | Game mode components, respawn, GM | clean | `SCR_PossessingManagerComponent` is on vanilla `GameMode_Base.et` (`{56E0B02F4D91C3EF}`). `GameMode_Roleplay.et` has no parent prefab, so the new GUID `{9A5625D5A6562E4B}` shadows nothing. No layer and no child (`GameMode_Roleplay_Debug.et`, the pack's `Gamemode.layer`) declares a second one. What it changes in vanilla: (1) `OnControllableDestroyed` calls `NotifyReadyForSpawn_S` for every dead player, on top of `LF_SpawnLogic.OnPlayerKilled_S`'s own call. That is one more owner RPC, and `LF_OnReadyForRespawn` → `LF_DeathScreen.Open` returns the open screen, so the death screen opens once. (2) `HandlePlayerKilled` returns false only when the dead entity is not the player's main entity, which means a GM possession. Without possession, `GetMainEntity` is the controlled entity, so LF deaths go through as before. No LF code calls `SetPossessedEntity`. (3) `GetPlayerIdFromControlledEntity` (victim ids in `SCR_InstigatorContextData`) falls back to the main-entity loop, which gives the same answer as before. (4) The component's `RplSave`/`RplLoad` adds a possession map to the game mode's JIP stream; server and client load the same prefab. Test `medic/game-mode-possession`. |
| #5162 downed HUD line | Client UI, JIP | clean, one gap | `LF_DownedHud.OnLocal` runs from the downed and stun `RplProp` callbacks on the owning client only, and each tick removes the line when the local character is gone, dead or up. Gap (not a regression; there was no line before #5162): a player who rejoins into their own downed logout body gets no line, because the flag does not change on the new owner, and a listen host's own player gets none (no `onRplName` on authority). Give-up works without the line (`LF_GiveUpController` has its own key and server RPC), so this is not filed. |
| #5162 revive sort priority | Prefab | clean | Two `"Sort Priority"` values on actions of `Character_Roleplay.et`; no GUID change. |
| #5159 medic call for a downed player | Comms, bus | clean | Server only; placed through the existing `LF_ServiceCalls.Place`. Every exit closes the call: a revive (`LF_Revive`, `LF_BedDelivery`), and every player death (`LF_Murder` publishes `LF_OnPlayerKilled` whether or not it is murder). A stun or knockout sets the flag without `LF_Downed.Down`, so it pages nobody. A logout body cleaned up while down leaves its board row, and that row expires (`LF_ServiceBoard.PurgeExpired`). |
| #5160 clinic bed | Spawning, medic | clean | `GetWakeTransform` now starts from the mattress top (`LayPoint`) rather than the bed origin. No `.et` or `.layer` sets `m_vWakeOffset` (the default is `0 0 0`), so no bed is lifted twice. Hospital respawn uses these points only to pick the bed and find the nearest one (XZ), then sits the body with `GetSitTransform`. The new stand search returns false for a boxed-in bed, and the patient then stays on the mattress instead of being put inside the frame. |
| #5158 duty locker cash line | Money (display) | clean | The cash is read from the stored civilian snapshot before `ClockOut` empties it, and the toast is sent only after a clock-out the server accepted. No money moves in this diff. `SuspectNameFor` gives the character name only to an officer the suspect introduced themselves to. |
| #5157 medic playtest text | Medic, police | clean | The give-up toast now names the seconds left (never under 1). The clock-in toast returns before the off-duty line. |
| #5156 police text: `<site>^<town>` places | Places, boards | **fixed** | `DescribePlace` now stores `site^town` for a crime near a sign in a town. Every board, pager, radio and call line reads places through `LF_WantedBoard.ResolvePlace`, except two officer surfaces. The evidence bag's read toast called `WidgetManager.Translate(place)` and the evidence locker's read card printed the stored string, so an officer read "Fuel Station^Saint Philippe" (or the raw keys). Both now go through `ResolvePlace` (`LF_EvidenceBagComponent.ReadLine`, `LF_EvidenceLocker.DescribeAll`). Test `investigation/evidence-place-text`: red on the unfixed code, then green. The locker card had shown raw place keys since before #5156; that is fixed by the same change. `LF_PoliceCallPay` compares two `DescribePlace` results, so the new form does not upset it. The three new crime catalog GUIDs are unique. |
| #5150 crime playtest | Licences, jail, search | clean | `Holds` reads the licence before the ban is applied, so a player who never held it is told they are barred. Bail release skips only the "sentence served" toast. Both search toasts are built on the server, as the officer's toast already was. |
| #5161 round 4 text | Localization | clean | 14 en_us rows: quotes dropped and "alley yard" renamed. Every `%N` is kept except "You bought house %1", which drops a now-unused argument. |
| #5163 money format | Formatter, money text | clean | `LF_Terms.Text` groups a slot only when the translated row writes `{currency:sym}%N`, `{currency:sym} %N` or `$%N`, and only when the argument is plain integer text. All 59 such rows in the core and pack tables were read, and each slot holds money. Counts, minutes and seconds in the same rows (`%2 s`, `%3 min`, `%2 parcel(s)`) sit in other slots and stay as they are. Callers that now pass `FormatMoney(paid)` use rows without a symbol of their own (`LF-ChopFit_Done`, `LF-BuyerSale_Sold`, `LF-CounterOutlet_Sold`), so no row reads "$$". The negative `FormatMoney` cuts the digits from the text, so the lowest int cannot overflow. A locale that writes the symbol after the slot simply gets no grouping. |
| #5151 lot sheet harness verb | Harness, RPC | clean | `RpcAsk_LF_LotSheetHarness` is gated by `LF_DevGate.HarnessBuild` or `CheckPlayerRequest`. `ReadBack` only reads client UI state. |
| #5144 truck license hint | Vehicle lot UI | clean | The client reads each row's class from its offer and the price config that the server's `Buy` reads. Rows are in offer order on both sides; a null offer would misalign them, but `Buy` has always used the same index, so this is not new. |
| #5164 walk and drive harness | Harness | clean | Read-only probes; no RPC, save or world write. |

Found on the way, not a player bug: `--only LF_Test_EvidenceLockerFill` fails "the cool-down is spent" on an unmodified `origin/main` (seen 2026-11-27 on lf-testbox). In the minimal LOGIC world, the convoy refuses the start that `RearmNow` raises and schedules the re-arm again. The test needs either a convoy that cannot start or a WORLD tier. Filed as #5166.

## Part 4 outcome

One confirmed player-facing defect fixed: evidence bag and locker cards showing the raw stored place (#5156). Nothing else in #5144 or #5150 to #5164 blocks the RC. Open for a playtest look: the downed line after a rejoin into a downed logout body (not shown; give-up still works).
