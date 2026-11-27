# Test coverage gaps, 2026-11-27

A scan of what core gameplay logic has no `LF_Test` reference, taken during the 0.3.0 feature freeze (tests only, no
behaviour change). The repo had 1,488 test files at the time and `--tier fast` ran 639 tests with the first nine included.
Round 2 (`ws/test-coverage-1127-r2`) added twelve more, round 3 (`ws/test-coverage-1127-r3`) eleven, round 4 seven and round 5 six; their tables are below the
first one.

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

## Round 2: 12 tests, 0 bugs found

The class-level scan finds only helper classes now, so round 2 scanned by method: every `static` method with a primitive
signature (int, bool, string or float return; no entity, array or map argument) whose name no test file mentions: 535
candidates. Each was read; the money, save, licence, jail and shop-counter ones with a pure rule were tested.

| Test | Feature | What it pins |
|---|---|---|
| `LF_Test_LicenceBitOrder` | License (save, replication) | The mask bit of every licence branch by table position (an old licence moving would change what a late joiner decodes), the 31-bit ceiling, encode and decode round trip in table order, a type with no bit adds nothing, `Has` |
| `LF_Test_LicenceDeskAnswers` | License | Each outcome's answer key, an unknown outcome reads as closed, the shop counter rewords four outcomes and falls back to the desk for the rest, permit wait in hours rounded up, ban in days rounded up, fee text for a cash refusal |
| `LF_Test_MarketRowFine` | Market (save) | A saved price row: version 1 whole cash scales to hundredths, version 2 is read as it is, a damaged value is held to 0..the largest price (no int overflow); `ToFine`, `ToCash` rounding half up, `ClampPrice` |
| `LF_Test_StallRentToken` | Market stall (save) | The rent book behind the restart refund: token is holder, day and unix second, an empty holder or negative fee is refused, the same rent twice is one, a free pitch is held, release of an unknown or empty token is a no-op |
| `LF_Test_MoneyFormatText` | Money | `FormatMoney` grouping, minus outside the symbol, zero, largest int; `FormatDelta` plus, minus and empty for no change |
| `LF_Test_SaveIdsRecords` | Persistence | A renamed job sign-up and turned-in quest rewritten on load, exact id match, null list or row never crashes, a chain of 15 aliases resolves while 16 is refused and the id loads unchanged |
| `LF_Test_SaveIdsPrefabSources` | Persistence | `PrefabSourcesOf` lists the older ids whose chain ENDS at a target, `PrefabLoads`, `NoteMissing` once per GUID, an empty saved id spawns from the first fallback without a missing note |
| `LF_Test_ShopRuleParse` | Shop | `ParseRuleKey`: prefab, find-text and type keys, mode default, an empty id, a non-number, an unknown or third part refused; `RuleKey` forms |
| `LF_Test_ShopLicenceRows` | Shop | `Licence<n>Row` names map back to their index and are never an item line (and the reverse), padded or malformed names are no row |
| `LF_Test_ItemCatalogueNames` | Catalogue, License | `ParseLicence` is exact (a typo, lower case or a number is no licence), the criminal range is the six black-market licences, job and org-right refusal keys |
| `LF_Test_PoliceStaffParse` | Police | `!preject <uid>` shapes, staff roster line (`uid [level] # comment`), `IsValidUid` (characters, 64 limit, no newline or quote that could forge a record line) |
| `LF_Test_JailBreakFuse` | Police (jail) | A charge on the outer door burns the outer fuse, a cell door the cell fuse, anything else the caller's; a cell charge reports the jailbreak offence, the rest the caller's |

All twelve carry a `RedProof()` hook; the batched run failed all twelve (`RED PROOF OK (all 12 listed test(s) fail with the hook on)`).

## Round 3: 11 tests, 0 bugs found

Round 3 (`ws/test-coverage-1127-r3`) scanned every `static` method (any signature without an entity, widget or vector
argument) whose name no test mentions: 1,031 candidates. Most of the big hits (`LF_ChessBoard`, `LF_BanRecord`, `LF_HeistRun`,
`LF_PoliceRanks` settle calls, `LF_VehicleOwnership`) were reached under other names, so each class was checked against the
tests that name it before a test was written. What was left was the save-id rewrites of the record kinds round 2 listed as
open, the poster, board and bus text builders, the job kind lists, the gang flag claim order, the case board wire format and
two small helpers.

| Test | Feature | What it pins |
|---|---|---|
| `LF_Test_SaveIdsPlaceRows` | Persistence | Renamed place ids rewrite on load in hideout, held-ground, house-lock, vehicle (shop) and store stock and unpaid rows; the other fields (lock, owner, stock, price, points) stay as saved; a place alias never renames a crop or job of the same text; null list and row survive |
| `LF_Test_SaveIdsCropJobRows` | Persistence | Renamed crop (farm plot), chain and station place (chain station), job (XP and playtime rows) rewrite on load and keep growth, work, XP and seconds; each kind follows only its own aliases |
| `LF_Test_WantedBoardText` | Police | Five-mark star row clamps below 0 and above 5, `#key` translated and plain text passes, a grid place is built from the grid term, headline names an unidentified suspect, description joins clothing (empty pieces dropped), on foot or driving, and the last-seen place only when known |
| `LF_Test_ExportBoardLine` | Jobs | Export board row: label count, goods, destination (keys translated), open order shows pay and deadline only, taken order shows the known hauler or the word taken |
| `LF_Test_BusTicketCancelName` | Travel | "Cancel trip" action shows the refund and the seconds left; a late bus reads 0, never a minus |
| `LF_Test_KnownNamesSameKey` | Interaction | A learned name survives while the character key matches, drops when another character takes the player id, survives an unknown key, matches exactly |
| `LF_Test_JobKindRules` | Jobs (save) | Delivery jobs are exactly parcel, fuel and taxi (the sign-up list saved and restored), cargo is parcel and fuel only, sign-up and run-start toast keys per job with the courier fallback, the client's Complete-the-run offer |
| `LF_Test_OrgFlagClaimOrder` | Organizations | Flag claim order behind the plain-cloth rule: id order with no claims, unclaimed before claimed, later claim is later, re-picking the own flag keeps the place, a new flag moves to the back, reset forgets |
| `LF_Test_CaseBoardEncode` | Police | Case and card entry shapes, separators and newlines cannot split a field, identified flag, round trip through `Decode`, four cards per case and three cases per board |
| `LF_Test_ReputationWireRange` | Reputation | Wire faction order and slots, the lowest and highest tier (-16, +15) round trip, out-of-range tiers clamp without spilling into the next digit, values below the flag or at twice it are not packed |
| `LF_Test_UtilsInheritChecks` | Core | `IsAnyInherited` and `IsInstanceAnyInherited` (itself, base, empty list, null), `PrefabInherits` empty name or ancestor, the same prefab, a moved prefab (GUID decides), path compare with no GUID |

All eleven carry a `RedProof()` hook; the batched run failed all eleven (`RED PROOF OK (all 11 listed test(s) fail with the hook on)`).

### Looked at and left alone (notes, no refactor in the freeze)

- **The five money action wrappers** (`LF_StallBuyAction`, `LF_StallRentAction`, `LF_LockerRentAction`, `LF_LicenceDeskAction`
  with its driving sibling, `LF_BackRoomSearchAction`) hold no pure rule: each calls a tested server rule and turns the
  answer into a toast. The wording they use is pinned for the licence desk (`LF_Test_LicenceDeskAnswers`). The stall actions'
  `RefusalKey` is a `protected` instance method on a live `LF_StallComponent`: tangled, left. Making it a `static` pure
  function would be trivially safe once the freeze ends, and a test shaped like `LF_Test_LicenceDeskAnswers` would fit.
- **`LF_QuestOwnerStanding`** is three one-line readers (job level of the body, job component, faction key) over a live
  character; the logic is in `LF_QuestStanding` and its callers, which are tested. Nothing pure to pin.
- **`LF_PoliceRanks.PromoterRankNeeded` and `PromoterOutranksNewRank`** look untested by name, but `LF_Test_PolicePromoteRung`
  runs them on one-, two- and five-rung ladders through `Promote` and `CanPromote`.
- **`LF_DutyBonus.MedicCap`** is `LF_MedicDutyBonus.Cap`, which `LF_Test_DutyBonusCap` pins.
- **`LF_PriceCurve.ClampPrice` with a floor above the ceiling:** its comment says the floor wins, but `LF_Utils.ClampInt`
  applies the floor first and the ceiling second, so the ceiling wins. It is not reachable (`LF_MarketGoodDef` refuses a floor
  above its ceiling at load), so no test pins either answer and no issue was filed; fix the comment after the freeze.
- **`LF_SaveIds.PrefabSourcesOf`** reads as if it lists every id on a chain; it lists only the ids whose chain ENDS at the
  target (an id one hop from the middle of a chain is not a source of the middle). Callers count a kind of item by its
  final id, so that is right, and the test pins it.

## Round 4: 7 tests, 1 bug found

Round 4 (`ws/coverage-round-4`) went back to the class-level scan, this time counting a class as covered only when its name
appears in a test as a whole word, and ranked what was left by risk: money, shops, jobs, jail, vehicles, saves. Most of the
130 untested files are harness verbs, world actions, UI sheets and Workbench plugins. The pure rules left were these.

| Test | Feature | What it pins |
|---|---|---|
| `LF_Test_MiningForgeHeat` | Mining | Forge heat: 1 nugget for 1 bar after 5 s, a held nugget starts it, a second start is refused, the nugget stays until the delay is up, a negative step never rewinds, a zero delay finishes inside Start, a heat that lost its nugget makes no bar |
| `LF_Test_PhotoIdCardConfigClamp` | Identity (money) | Replacement card fee: the config value, 0 is free (not the default), a negative or huge fee is held to the schema, show time held to 2..60 s, no config means the default fee and time |
| `LF_Test_ATMLiveFigures` | ATM (money) | The cash machine screen: figures unknown until the server answers, an answer (done or refused) carries the new balance and cash, a balance line, zero, negative or over-limit amount is never sent and never leaves the screen "working", the world-end reset forgets it all |
| `LF_Test_DutySecondsToNextRank` | Police, Medic (jobs) | `SecondsToNextRank`: rung total minus the clock, the second rung counts the total served, an already-due rung, the top rung and a civilian read `int.MAX`, medics use the medic clock and ignore police time |
| `LF_Test_VehiclePriceGuidKey` | Vehicles (money) | `ParseGuidKey`: 16 hex digits with or without braces, any case, blanks ignored, upper-cased; short, long, non-hex, empty, braces-only, a path and an unclosed brace refused. **Found a bug** (below) |
| `LF_Test_VehicleOwnsAny` | Vehicles (saves) | The starter-car gate: an owner with any record owns a car, an empty owner or another uid (also one that merely starts with it) does not, scrapping the last car makes the owner car-less again |
| `LF_Test_GarageMaxBuyable` | Vehicles (money) | Slots for sale are max minus base and never negative; a base above the max leaves the cap at the max and no next price |

All seven carry a `RedProof()` hook; the batched run failed all seven (`RED PROOF OK (all 7 listed test(s) fail with the hook on)`).

**Bug found and fixed:** `LF_VehiclePriceModel.ParseGuidKey` called `text.Trim();` and threw the result away (`Trim` returns the
trimmed string), so an owner's vehicle price key with a blank around it (`" {54C3CC22DEBD57BE} "`) was read as "not a GUID"
and the price was silently not applied. Now `text = text.Trim();`.

Looked at and left alone: `LF_QuizController` (`LF_QuizFlow` talks to live controllers; the pure part is `LF_Quiz`, tested),
`LF_LicenceGrant.ServerRun` (needs a connected player; its parse is `ParseLicence`, tested in round 2),
`LF_DisconnectRules.RejoinReason` (a log sentence), `LF_ReputationOverlay` (a merge over pack files, covered by the pack
overlay tests), `LF_VehicleHandover` and `LF_PlacementAim` (world and camera).

## Round 5: 6 tests, 0 bugs found

Round 5 (`ws/coverage-round-5`) took the "left alone" list from round 4 at its word: the rules behind the world actions are
tested, the actions' own wiring is not. The scan counted every `*Action.c` whose class name no test file names (50 of them);
the ones with a money, inventory or state effect and a prefab that spawns in DebugWorld were chosen. Each test finds the
action on the spawned prop the way the engine holds it (`LF_TestActionFinder`, a new helper over the prefab's
`BaseActionsManagerComponent`), so `Init` has run against the real owner, then calls `PerformAction(prop, character)` on a
fixture character, the server path a player's use takes. Nothing names a town coordinate or distance; reach cases move the
fixture a few metres from a prop spawned beside it.

| Test | Action | What it pins |
|---|---|---|
| `LF_Test_FuelBuyAction` | `LF_FuelBuyAction` (fuel, money) | No car at the pump charges nothing and takes no litre; with a car 30 cash buys 15 litres at 2.00, the tank, station stock and purse move together; a broke buyer and a perform with no buyer move nothing; with fuel stock off the prompt hides and a perform sells nothing |
| `LF_Test_LootSearchAction` | `LF_LootSearchAction` (loot, inventory) | A forged perform with no searcher fills nothing; the first search fills the crate, a second while stocked adds nothing, an emptied crate inside its cooldown stays empty; the prompt needs a searcher |
| `LF_Test_LockerRentAction` | `LF_LockerRentAction` (lockers, money) | Out of reach of every bank takes nothing; short cash takes nothing; with the price the rent leaves the purse once and the locker is recorded under the character's own key; a second perform and a perform with no character move nothing |
| `LF_Test_StallRentAction` | `LF_StallRentAction` (market, money) | After closing and one short of the fee rent nothing; with the fee the stall goes to the performer's key and the fee leaves the purse; a second renter keeps their cash and the holder cannot rent a second stall the same day; no character moves nothing |
| `LF_Test_StallBuyAction` | `LF_StallBuyAction` (market, money, inventory) | A stall with nothing listed sells nothing; a buyer one short and the holder buying their own listing move nothing; a paying buyer gets one item for the cheapest price, the seller is paid the price less the cut, the dearer listing stays; after closing nothing is sold; the prompt shows only on a rented stall |
| `LF_Test_LostAndFoundAction` | `LF_LostAndFoundReclaimAction` (lockers, money, inventory) | A stranger with nothing waiting does not see the prompt, is refused and loses no cash; the owner one short of the fee takes nothing and the things keep waiting; with the fee the owner pays once and gets the item back; a second perform takes no second fee |

All six carry a `RedProof()` hook; the batched run failed all six (`RED PROOF OK (all 6 listed test(s) fail with the hook on)`).
The first green run caught one test mistake, not a game bug: the pump finds a car with a sphere query, and a car moved with
`SetOrigin` in the same frame is not found, so `LF_Test_FuelBuyAction` spawns the car at the pump after its no-car step.

Looked at and left alone (round 5):

- **`LF_GarageBuySlotAction`**: its server rules (`LF_GarageSlotStore.Buy`) are covered by `LF_Test_GarageBuySlot`, but the
  action needs an entity with `LF_VehicleGarageComponent`, which no prefab carries (the garage shell gets it from a layer
  override in `Place_saint-philippe_carlot.layer`). A test would have to add the component at runtime. The action's own gates
  (desk reach, rate limit, alive) stay uncovered.
- **`LF_BackRoomSearchAction`**: needs a shop with a back room and an on-duty officer; `LF_Test_BackRoomRules` pins the raid
  rule. No spawnable prefab carries both.
- **`LF_LockerOpenAction`**: the open path ends in a client RPC to the player's controller, which a fixture body does not
  have; `LF_Test_LockerRoundtrip` covers `OpenAt` and the close.
- **`LF_StashContainerAction`**: `Take` is covered by `LF_Test_StashContainerRefill`; the only prefab with the component is
  the pack's container, so a test would be a pack test for a one-line wrapper.
- **`LF_VehicleLockAction`, `LF_HouseSaleBuyAction`, `LF_LiveryShopAction`** and the jail escort actions: the rule classes
  are named by many tests (`LF_Test_VehicleLock`, `LF_Test_HouseSaleBuy`, `LF_Test_LiveryShop`, `LF_Test_Jail*`).
- **UI sheets and harness verbs** stay with `look` and the `agent-box` proofs (rule 11).

## Checked and already covered (no test added)

`LF_DutyPay` (rank-change carry), `LF_FineTicket` (deadlines, late amount, desk payment), `LF_TreatmentBills` (issue, debt,
sweep, persist), `LF_JobLevels` (legacy career curve both ways), `LF_StallRules` / `LF_StallRentBook`, `LF_MarketBook`,
`LF_BuyerDemandBook`, `LF_LostAndFound`, `LF_JailBail`, `LF_JailBreak` layers and `LF_ChopFitRules`. The "untested method"
hints for these were all reached through their callers.

## Still open, ranked by risk to money, progress and saves

1. **`LF_ATMLive`** (397 lines, ATM/): the live-terminal glue has no class reference. Its rules sit in `LF_ATMQuick` and
   `LF_ATMManager`, which are tested; the glue is not. Needs a WORLD test with a character and a terminal.
2. **`LF_LicenceDeskAction`, `LF_BackRoomSearchAction`, `LF_GarageBuySlotAction`**: the money-moving action wrappers left after
   round 5, which covered `LF_StallBuyAction`, `LF_StallRentAction`, `LF_LockerRentAction`, `LF_LostAndFoundReclaimAction`,
   `LF_FuelBuyAction` and `LF_LootSearchAction` with `PerformAction` on a fixture character (`LF_TestActionFinder`). The licence desk
   needs a desk prop and a spawned clerk; the other two need a prop no prefab provides (see "Round 5").
3. **`LF_CharacterShopComponent`** (315 lines): the shop component's buy and sell serve path. `LF_ShopRules` is now covered; the
   component that applies it is not.
4. **`LF_SaveIds`** (mostly done in rounds 2 and 3): `PrefabSourcesOf`, `PrefabLoads`, `NoteMissing`, `ResolveSignUps`,
   `ResolveProgression`, the chain limit and the place, crop, chain and job rewrites of hideout, held-ground, house, vehicle
   (shop), store, farm plot, chain station, job XP and playtime rows now have tests. Still open: the rewrites that go through
   `ResolvePrefab` (the prefab fields of vehicle, store and snapshot rows; they need a prefab that loads) and `ResolveAccounts`,
   `ResolveSnapshots`, `ResolveItemState`, `ResolveItemSerials`, `ResolveItemKeys`, `ResolveItemStolen`, `ResolveFenceSerials`,
   `ResolveEvidenceLocker` and `NoteHeld`. They are covered end to end by `LF_Test_E2E_ContentIds`.
5. **`LF_QuestOwnerStanding`**: three one-line live-character readers; nothing pure to test (round 2 read it).
6. **`LF_Jail` placement and release**: `GroundedReleasePoint` (trace-based) and `SetReleaseOffset` are only touched through
   the full arrest tests.
7. Quest UI (`LF_QuestMenu`, `LF_QuestMenuRowComponent`) and `SCR_InventoryMenuUI` patches: UI, covered by sessions.

## How to add one

`tools\cli feature new` is for features; for a gap test copy the nearest `LOGIC` test (`LF_Test_ATMQuick.c` is a good
shape), put the act inside `if (!RedProof())`, give every observed value a wrong default, and run
`tools\cli test --no-build --only <class>` then one batched `--red`.
