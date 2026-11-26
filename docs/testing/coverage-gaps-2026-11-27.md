# Test coverage gaps, 2026-11-27

A scan of what core gameplay logic has no `LF_Test` reference, taken during the 0.3.0 feature freeze (tests only, no
behaviour change). The repo had 1,488 test files at the time and `--tier fast` ran 639 tests with these nine included.

## How the scan worked

For every non-test `.c` file under `addons/LifeFramework/Scripts/` and `addons/LF_Everon1989/Scripts/`:

1. take each class name and count whole-word references in every test file;
2. take each `static` method name and check whether any test file mentions it.

A class with no reference is a certain gap. A method with no mention is only a hint: a test often reaches it through a
public caller (`LF_DutyPay.CarrySeconds`, `LF_JailBail.Pay` -> `LF_Jail.EndOnBail`). Each candidate was read against the
tests that name its class before a test was written.

Result: 1,256 source files, 163 with a class no test names. Most of those are world actions
(`*Action.c`), UI sheets and views, Workbench plugins, harness verbs and `*_StaticResetHook` classes: they are driven
through the engine, not through script, and are covered by `look`, `agent-box` proofs and session play instead.
The rest were pure logic, and those are what this note ranks.

## Now covered (9 tests, 0 bugs found)

| Test | Feature | What it pins |
|---|---|---|
| `LF_Test_MoneyDeltaTracker` | Money | HUD "+$500" ticker: first reading seeds, changes accumulate and restart the countdown, quiet clears, net zero hides, bad timeouts ignored, Reset re-seeds |
| `LF_Test_ShopSellRules` | Shop | Which shops buy from players, gun-dealer multiplier, stack pays unit price times quantity, `CanSellItem` and the block-reason order |
| `LF_Test_LicenceFeeWallet` | License, ATM | Licence fee comes from the bank first then cash, refund goes cash first, overdraw takes only what exists, no body is bank-only, no account is cash-only (WORLD) |
| `LF_Test_LicenceWhereNames` | License | Which licences a counter sells, fee 0 for the rest, resolve a licence by enum name or desk key, unknown names fall back |
| `LF_Test_ProgressionLicenceClear` | Account | `ClearFlag` leaves other flags and turn-ins, `ClearAllLicences` (licences-persistence switch) empties lists, keeps records, counts honestly |
| `LF_Test_ShiftBoardThousands` | Level | XP column digit grouping, zero, boundaries, negatives |
| `LF_Test_PerkSyncTrades` | Level | Which jobs a level rise re-syncs the perk counter for |
| `LF_Test_JailDoorTargets` | Police | Jailbreak breach target ids: empty refused, exact match only, cell and outer door separate, empty outer clears |
| `LF_Test_BuyerDemandList` | Market | Buyer-demand row ids (buyer plus good GUID), insert past the end, replace inside it |

Every one carries a `RedProof()` hook; the batched run failed all nine (`RED PROOF OK (all 9 listed test(s) fail with the hook on)`).

## Checked and already covered (no test added)

`LF_DutyPay` (rank-change carry), `LF_FineTicket` (deadlines, late amount, desk payment), `LF_TreatmentBills` (issue, debt,
sweep, persist), `LF_JobLevels` (legacy career curve both ways), `LF_StallRules` / `LF_StallRentBook`, `LF_MarketBook`,
`LF_BuyerDemandBook`, `LF_LostAndFound`, `LF_JailBail`, `LF_JailBreak` layers and `LF_ChopFitRules`. The "untested method"
hints for these were all reached through their callers.

## Still open, ranked by risk to money, progress and saves

1. **`LF_ATMLive`** (397 lines, ATM/): the live-terminal glue has no class reference. Its rules sit in `LF_ATMQuick` and
   `LF_ATMManager`, which are tested; the glue is not. Needs a WORLD test with a character and a terminal.
2. **`LF_StallBuyAction`, `LF_StallRentAction`, `LF_LockerRentAction`, `LF_LicenceDeskAction`, `LF_BackRoomSearchAction`**:
   the money-moving action wrappers. The servers' rules they call (`LF_StallRules`, `LF_LockerRules`, `LF_LicenceDesk`) are
   tested; the wrapper's wiring (who pays, what is refused before the rule runs) is not. A user-action test with a
   fixture character would cover all five.
3. **`LF_CharacterShopComponent`** (315 lines): the shop component's buy and sell serve path. `LF_ShopRules` is now covered; the
   component that applies it is not.
4. **`LF_SaveIds`**: 25 of 35 static methods are not named by a test (`PrefabSourcesOf`, `PrefabLoads`, `NoteMissing`,
   `ResolveAccounts`). The alias resolve path is exercised end to end by the e2e save tests but the rarely taken
   branches (an alias chain, a missing prefab, a held inventory) have no focused test.
5. **`LF_QuestOwnerStanding`**: the live-character reader behind quest accept (level, job, faction). Needs a character plus
   a job config; the base class is covered through `LF_TestQuestStanding`.
6. **`LF_Jail` placement and release**: `GroundedReleasePoint` (trace-based) and `SetReleaseOffset` are only touched through
   the full arrest tests.
7. Quest UI (`LF_QuestMenu`, `LF_QuestMenuRowComponent`) and `SCR_InventoryMenuUI` patches: UI, covered by sessions.

## How to add one

`tools\cli feature new` is for features; for a gap test copy the nearest `LOGIC` test (`LF_Test_ATMQuick.c` is a good
shape), put the act inside `if (!RedProof())`, give every observed value a wrong default, and run
`tools\cli test --no-build --only <class>` then one batched `--red`.
