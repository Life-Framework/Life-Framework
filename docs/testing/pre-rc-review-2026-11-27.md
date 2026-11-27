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
