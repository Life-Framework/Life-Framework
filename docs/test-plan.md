# Life Framework — Test Plan & Test Format

> Companion to [features.md](./features.md). Maps every implemented feature to
> the test cases that prove its contract, and defines a test format that
> scales with low LOC. Inspired by Overthrow's autotest suite structure (MIT,
> ArmaOverthrow/Overthrow.Arma4, `Scripts/Game/Tests/`) — see "What we borrow"
> below.
>
> The immediate goal is confidence in the core systems. Several exploration
> findings are **known bugs**; the first tests for those areas are **pin
> tests** — they must go red first, then the fix makes them green
> (el-tdd cadence).

## Format design

### Current framework

`EL_Test` (one class per test) + `EL_TestContext` (True/False/Equal/EqualStr/
Fail) runs in DebugWorld on server start. Registration is automatic: each test
class declares its tier with a `// tier:` comment above it, and
`tools\cli regen-tests` derives `EL_TestRegistrations.generated.c` from the
files (`cli validate` checks it stays in sync). Tiers: `LOGIC` (fast, no
world), `WORLD`, `PERSISTENCE`. CLI gate: `tools\cli test`.

### What it lacks (and what Overthrow showed us)

1. **Multi-tick tests.** `Run(ctx)` is single-shot. EPF async loads
   (`LoadAccountAsync`, survival stats load, spawns) need a "poll until done"
   pattern. Overthrow returns `false` from the main step to run again next
   frame.
2. **No float-epsilon helper.** Every float test hand-rolls
   `Math.AbsFloat(x - y) < 0.001`. One helper kills that boilerplate.
3. **No fixtures.** Every test rebuilds its subjects with hand-set fields.
   Overthrow's `OVT_TEST_LogicFixture` centralizes "hand-built objects start
   zeroed" (the `new` does not apply `[Attribute]` defaults trap) into one
   factory per area.
4. **Low-LOC shape.** Overthrow writes one class per case (high LOC). We
   prefer **one class per subject area, assertions in loops over hand-built
   tables** — fewer classes, fewer `GetName()` overrides, faster to write,
   and the table IS the documentation.

### Proposed extensions (backwards-compatible)

```c
class EL_TestContext   // add to Scripts/Game/Tests/EL_TestContext.c
{
    void EqualFloat(float expected, float actual, float epsilon, string msg)
    void NotNull(Managed obj, string msg)
    void InRange(float value, float lo, float hi, string msg)
    void Pass(string msg)                      // assertion with no failure branch
}
```

Multi-tick polling (`EL_Test.Tick()`) was deferred — nothing in the current
LOGIC-tier suite is async, and the runner loop is single-pass. It stays a
documented option for WORLD/PERSISTENCE tests that wait on EPF async loads.

Overthrow hard lessons we adopt verbatim:

- **`new` applies no `[Attribute]` defaults.** A hand-built subject starts
  zeroed. Set every field the assertion depends on, even when the declared
  default matches. Centralize this in a fixture.
- **Floats compare with epsilon, never `==`.**
- **Cases must be independent and deterministic.** No RNG-gated assertions;
  if a subject has an RNG branch, cover the deterministic branches and name
  the skipped one in a comment.
- **Determinism beats breadth.** Three consecutive runs must be identical.
- **Pin bugs, then fix.** A known bug gets a red test first (per el-tdd), the
  fix follows, the test stays green as the regression gate.

### Tier mapping

| Tier | Runs in | Cost | What belongs |
| --- | --- | --- | --- |
| `LOGIC` | `cli test --tier fast` | seconds | Pure maths, clamping, save-data round trips, whitelist, plate generator, quantity math, level formulas |
| `WORLD` | `cli test` (all) | minutes | Prefab/layout loads, spawns, component wiring, inventory ops, RPC-less server paths |
| `PERSISTENCE` | `cli test` (all) | minutes | Save → reload round trips through public API only |
| Manual | human in DebugWorld | — | Multiplayer/JIP, UI, client-server RPC bridges, restart paths (per AGENTS.md) |

### File layout

- `Scripts/Game/Tests/EL_Test_<Area>.c` — one class per subject area. Registration is
  automatic via `tools\cli regen-tests` (derives the registry from the files).
- `Scripts/Game/Tests/Fixtures/EL_TestFixtures_<Area>.c` — hand-built subjects
  (probes that expose protected state without calling `Replication.BumpMe`).
  The validator scans only top-level `Tests/*.c`, so fixtures need no
  red-proof or registration.
- Add the tier comment (`// tier: LOGIC|WORLD|PERSISTENCE`) above each test class.

---

## Test cases by feature

> Status (2026-08-29): the LOGIC-tier cases below marked ✅/🎯/🔒 **are
> written and registered** (27 classes total, `cli validate` green). The
> license-plate range pin was **re-scoped to a format test**: `RandomInt`
> output makes "Z/9999 unreachable" non-deterministic to observe, so it is a
> code-review finding in features.md, not an assertion. Quantity clamp/combine
> landed as WORLD-tier (`EL_Test_QuantityStack`), because `AddQuantity` routes
> through `SetQuantity`, which requires an owner with a master `RplComponent`.
> Remaining ⏳ rows are not yet written.

Legend: 🎯 = pin test for a known bug (red first), ✅ = contract test (should
pass once framework works), 🔒 = persistence round trip, 👤 = manual only,
⏳ = not yet written.

### Core & Utilities

| Test | Tier | Asserts |
| --- | --- | --- |
| `core/format-abbreviate` | LOGIC ✅ | `AbbreviateNumber`: 999→raw, 0, negatives, K/M/B suffixes |
| `core/max-min-int` | LOGIC ✅ | `EL_Utils.MaxInt/MinInt` edge pairs |
| `core/component-data-suffix` | WORLD ⏳ | 🎯 `EL_ComponentData.Get` name-strip convention (`XxxClass`→`Xxx`); breaks for a non-`Class` name |

### Character Creation & Survival

| Test | Tier | Asserts |
| --- | --- | --- |
| `survival/stats-fresh` | LOGIC ✅ | fresh `EL_SurvivalStats` = 100/100/100 |
| `survival/stats-clamp` | LOGIC ✅ | Eat/Drink/Heal clamp to [0,100] both directions; `Eat(-50)`→50; `SetHealth(-10)`→0 |
| `survival/stats-decay` | LOGIC ✅ | `UpdateStats(dt)`: hunger −0.1·dt, thirst −0.15·dt; health −0.05·dt when hunger<20 or thirst<20; health unchanged above 20 |
| `survival/stats-decay-zero` | LOGIC ✅ | `UpdateStats(0)` no-op; negative dt keeps stats in range |
| `survival/save-roundtrip` | LOGIC ✅ | 🔒 `EL_SurvivalStatsSaveData` ReadFrom→ApplyTo preserves values and re-clamps corrupt data (hunger=999→100) |
| `survival/save-equals` | LOGIC ✅ | `Equals` true for identical stats |
| `account/create` | LOGIC ✅ | `EL_PlayerAccount.Create(uid)` sets persistent id, default faction CIVILIAN, empty characters |
| `account/character-crud` | LOGIC ✅ | AddCharacter sets active when flagged; GetActiveCharacter returns it; HasCharacters; RemoveCharacter keeps a different active char |
| `account/active-index` | LOGIC ✅ | 🎯 RemoveCharacter of the active char leaves a stale index (returns a different char); roster/shift pin |
| `account/wanted-clamp` | LOGIC ✅ | SetWantedLevel clamps [0,5]; IncreaseWantedLevel(±) clamps |
| `account/save-roundtrip` | LOGIC ✅ | 🔒 `EL_PlayerAccountSaveData` ReadFrom→ApplyTo round-trips faction/duty/wanted/characters; `Equals` order-insensitive |
| `account/manager-cache` | LOGIC ✅ | `EL_PlayerAccountManager` AddToCache/GetFromCache/Reset; unknown uid returns null |
| `spawn/getcreationprefab` | WORLD ⏳ | `EL_SpawnLogic` picks active character's prefab; spawns with default loadout items into matching purpose storages |
| `spawn/nop-spawnpoint` | WORLD ⏳ | 🎯 `GetCreationPosition` with no spawn point does not leave uninitialized output garbage |
| `creation/age-validation` | LOGIC ⏳ | Age [18,80] accepted, outside rejected; empty name rejected |
| `creation/temp-character` | WORLD ⏳ | 🎯 `CreateTemporaryCharacter` uses placeholder `{YourTempCharacterPrefab}` — the flow cannot complete |

### Money, ATM & Banking

| Test | Tier | Asserts |
| --- | --- | --- |
| `bank/account-math` | LOGIC ✅ | `EL_BankAccount`: Deposit(>0) raises balance; Withdraw(>balance)=false and balance unchanged; balance never < 0 via Deposit/Withdraw; Withdraw(<=0)=false |
| `bank/save-roundtrip` | LOGIC ✅ | 🔒 `EL_BankAccountSaveData` round-trip preserves balance + id; `Equals` ignores id |
| `atm/manager-registry` | LOGIC ✅ | `EL_ATMManager` AddAccount/CreateAccount/GetAccount; Deposit/Withdraw on known/unknown accounts |
| ~~`bank/component-math`~~ | — | `EL_BankAccountComponent` **deleted 2026-08-30** — no second bank, no entity-bank tests |
| ~~`bank/component-initial`~~ | — | $20k `OnPostInit` grant **gone** with the deleted entity bank |
| `shop/buy-math` | WORLD ⏳ | 🎯 **Funds exploit**: buyer with $50 buying a $100 item must FAIL — code fixed (refund + fail) and the money primitive is pinned by `EL_Test_MoneyCash`, but no direct `BuyItem` test yet |
| `shop/quantity-gate` | LOGIC ✅ | `IsQuantityAllowed` boundaries; `ComputeTotalPrice` (see `EL_Test_Shop`) |
| `trader/sell-value` | LOGIC ⏳ | `GetItemValue` scans configured list, 0 for unlisted |
| `trader/sell-pays-cash` | WORLD ⏳ | Selling deletes item and pays `AddCash` (verified in code 2026-08-30); unlisted item rejected with the correct return code |
| `trader/blackmarket-faction` | WORLD ⏳ | 🎯 Black-market rejects police account (currently fails closed but cache-keyed) |
| `trader/quantity-stacks` | WORLD ⏳ | 🎯 Quantity stack (e.g. 100 rounds) sells for value × stack (code now multiplies by quantity — needs a test) |

### Gathering, Processing & Resources

| Test | Tier | Asserts |
| --- | --- | --- |
| `gather/tool-requirement` | WORLD | ✅ Gather requires the configured tool (right hand / left gadget / inventory) else cannot be performed |
| `gather/counter-restock` | WORLD | 🎯 Remaining gathers decrements to 0, restocks after timeout; boundary `>` semantics on restock deadline (off-by-one) |
| `gather/perform-revalidates` | WORLD | 🎯 `PerformAction` must re-check the tool on the server (currently only client-evaluated) — hostile RPC can gather without it |
| `process/recipe-check` | WORLD | ✅ `CanBePerformedScript` true only when every input amount present |
| `process/consume-produce` | WORLD | ✅ PerformAction consumes inputs, produces outputs, drops at offset when forced |
| `process/missing-inputs` | WORLD | 🎯 **Free-output exploit**: inputs absent → outputs must NOT be granted (currently granted); empty input list must be rejected |
| `resource/tool-damage` | LOGIC | ✅ tool→damage table lookup; unmapped source deals 0 |
| `resource/non-tool-damage` | LOGIC | 🎯 unmapped tool deals 0 silently — decide whether that is a bug (no feedback) or intended; pin whichever is wrong |

### Jobs, Level, Licenses, Whitelist

| Test | Tier | Asserts |
| --- | --- | --- |
| `level/threshold-math` | LOGIC ✅ | `GetExperienceForNextLevel` = level×100 |
| `level/bonus-formulas` | LOGIC ✅ | bonus formulas (1+(level−1)·0.1 / ·0.05) |
| `level/levelup-cascade` | LOGIC ✅ | CheckLevelUp +250 from level 1 → level 3, XP drained, +2 SP, +2 total SP; `0 ≤ xp < next` invariant |
| `level/spend-skillpoints` | LOGIC ⏳ | SpendSkillPoints rejects when SP < amount; deducts exactly; total ≥ spent invariant |
| `level/setters-unguarded` | LOGIC ⏳ | 🎯 `SetLevel(50)` with 0 XP / `SetSkillPoints > total` — inconsistent state currently possible (decide: guard or accept) |
| `job/setjob-gating` | WORLD ⏳ | SetJob POLICE requires POLICE_ACCESS license; MEDIC requires MEDIC_ACCESS; same-job no-op |
| `job/paycheck-math` | LOGIC ⏳ | salary × (1 + (level−1)·0.05), `Math.Round`; interval resets |
| `job/paycheck-cash` | WORLD ⏳ | Paycheck pays `GiveCash` (verified in code 2026-08-30) — no bank branch remains; short payout logged |
| `job/per-job-progress` | LOGIC ⏳ | GetAllJobLevels/SetAllJobLevels round-trip; invariant `m_mJobLevels[GetJob()] == GetJobLevel()` |
| `job/fruitcatcher-cap` | LOGIC ✅ | `EL_GetFruitCatcherRewardCount` clamps to `EL_FRUIT_CATCHER_MAX_SCORE` (see `EL_Test_Security`) |
| `job/debug-license` | ✅ (code) | `RpcAsk_DebugGrantLicense` **removed** 2026-08-30 — grep-verified absent from `Scripts/Game` |
| `license/catalog` | LOGIC ⏳ | Every `EL_ELicenseType` has a config; UNEMPLOYED + FARMER_TOMATO granted initially |
| `license/purchase-math` | LOGIC ⏳ | `CanAffordLicense` SP ≥ cost; `CanUnlockLicense` gates on owned/config/whitelist/level |
| `license/purchase-spend` | LOGIC ⏳ | 🎯 `PurchaseLicense` must not spend SP when unlock later fails (currently double-charge risk) |
| `license/whitelist-job` | LOGIC ⏳ | 🎯 Whitelisted license gates must check the RIGHT job (currently hard-coded POLICE for every whitelisted license incl. MEDIC) |
| `whitelist/basic` | LOGIC ⏳ | IsFactionRestricted (POLICE/MILITARY/MAFIA), IsJobRestricted (POLICE), add/remove/get idempotent. **Blocked: `EL_EFactionType` unresolved — the file cannot compile yet** |
| `whitelist/persistence` | LOGIC ⏳ | (documented) resets every start — decision to persist is a TODO, not a bug; assert the current in-memory contract only |

### Crime & Police

| Test | Tier | Asserts |
| --- | --- | --- |
| `crime/cooldown` | WORLD | ✅ rob actions blocked during cooldown; allowed after |
| `crime/civilian-only` | WORLD | ✅ non-civilian cannot rob (null account currently passes — decide intent) |
| `crime/min-police` | WORLD | ✅ rob requires ≥ configured on-duty police |
| `crime/rob-reward` | LOGIC ✅ | rob pays cash, never the bank; wanted +1 (see `EL_Test_RobReward` — rewritten 2026-08-30) |
| `crime/rob-wanted-clamp` | LOGIC ✅ | wanted clamps at 5 across repeated robberies (account cache churn fixed) |
| `crime/police-count` | LOGIC | ✅ on-duty police count logic (extract to pure helper if needed) |
| `police/duty-toggle` | WORLD | ✅ duty flag toggles; police menu requires on-duty police |
| `police/arrest` | WORLD | 🎯 arrest teleports to jail, sets wanted 0, saves (server-side logic) |
| `police/fine-math` | LOGIC | ✅ fine reduces wanted by amount/1000; insufficient cash → failure |
| `police/menu-rpc` | WORLD | ✅ **bridge exists** (verified 2026-08-30): `EL_AskPoliceArrest/Fine` → `RpcAsk_EL_PoliceArrest/Fine` — needs a manual client pass |

### Notifications

| Test | Tier | Asserts |
| --- | --- | --- |
| `notify/config-defaults` | LOGIC | ✅ `EL_NotificationConfig` sets title/message/duration/type; color derived from type |
| `notify/sendtojob` | WORLD | ✅ `SendToJob` targets job == type and checks account `IsOnDuty()` (fixed in code; needs client display proof) |
| `notify/rpc-targeting` | WORLD | ✅ broadcast delivery filters by target player ID, with a listen-server host direct-call (fixed in code; needs client display proof) |

### Quantity & Inventory

| Test | Tier | Asserts |
| --- | --- | --- |
| `quantity/stack-clamp` | WORLD ✅ | `EL_Test_QuantityStack`: AddQuantity/RemoveQuantity clamp to [0,max], partial vs whole, out `change`, `SetQuantity(0)` deletes the entity |
| `quantity/combine` | WORLD ⏳ | `CanCombine` (not self, same prefab, capacity); `Combine` conserves: source decrease == dest increase == transferred; amount −1 = all |
| `quantity/split` | WORLD ⏳ | 🎯 Split of a quantity-1 stack must not spawn-and-delete via `SetQuantity(0)` (currently 0-split bug) |
| `quantity/sort` | LOGIC ⏳ | `SortByQuantity` descending non-increasing; `SortByQuantity(..., false)` ascending |
| `quantity/amount-accounting` | WORLD ⏳ | `EL_InventoryUtils.GetAmount/AddAmount/RemoveAmount` return actual amounts; Add fills stacks; Remove drains smallest-first; shortage returns 0 |
| `quantity/rpc-ownership` | WORLD ⏳ | ✅ quantity split/transfer RPCs validate range plus ownership; another player's carried items are rejected (fixed in code; needs multiplayer proof) |
| `quantity/persist` | LOGIC ⏳ | 🔒 `EL_QuantityComponentSaveData` ReadFrom/ApplyTo; quantity 1 skipped (DEFAULT) |

### License Plates & Vehicles

| Test | Tier | Asserts |
| --- | --- | --- |
| `licenseplate/format` | LOGIC ✅ | `GenerateLicensePlate()` matches the `AA BB NNN(N)` shape (2 uppercase, space, 2 uppercase, space, 3-4 digits) |
| `licenseplate/range` | LOGIC (review) | 🎯 digits in [100, 9999] and letters A–Z inclusive — currently `Z` and `9999` unreachable (off-by-one). **Not a deterministic assertion** — RNG output makes "unreachable" unobservable; kept as a code-review finding in features.md |
| `licenseplate/uniqueness` | LOGIC ⏳ | 🎯 issued plates are tracked (currently no registry — decide: uniqueness required?) |

### Data / Resources (World smoke)

| Test | Tier | Asserts |
| --- | --- | --- |
| `prefabs/load` | WORLD | ✅ every feature prefab loads and spawns (extend existing `EL_Test_Prefabs`) |
| `layouts/load` | WORLD | ✅ every menu layout + localization loads (extend existing `EL_Test_Data`) |
| `money-stack-prefab` | WORLD | ✅ (exists) MoneyStack prefab valid |

---

## Manual-only surface (needs a client or a human)

These are provable only by a human in DebugWorld or a multiplayer session per
AGENTS.md:

- Every `RpcAsk_*` client→server path end to end (job set, fruit catcher,
  quantity split, ATM deposit/withdraw, death-screen respawn).
- UI rendering (survival HUD, ATM menu, shop menu, police menu, death screen,
  hand-carry slot, notification toasts).
- Multiplayer / JIP state delivery.
- True restart persistence (save → restart → load), not in-session round trips.
- Hand-carry holster/weapon transition on a real client.
- **Death flow:** die with a full inventory → the body stays with all items →
  the death screen appears → Respawn re-creates the account character with the
  faction loadout → the old body remains lootable. Verify no instant auto-
  respawn and no corpse deletion.
- Performance: paycheck saves on every XP gain under load.

---

## What we borrow from Overthrow (MIT)

| Overthrow artifact | What we use | Where |
| --- | --- | --- |
| Tiered suites (Logic world-free via `GetWorldFile() → ResourceName.Empty`) | Our LOGIC tier / `--tier fast` | already have |
| `OVT_TEST_LogicFixture` factories + `FloatEquals` epsilon | `Fixtures/EL_TestFixtures_*.c` + `ctx.EqualFloat` | new |
| Multi-tick `bool` main step | `EL_Test.Tick()` polling | new |
| "Hand-built objects start zeroed" rule | Fixture factory discipline | new |
| Pin-bugs-then-fix / always-failing MetaSuite | Pin tests per known bug | new |
| `[BaseContainerProps()]` mandatory on suite class | n/a (we register in `CollectTests`) | — |
| Engine-native `SCR_AutotestSuiteBase` (`-autotest`) | **Not adopted** — it runs in the *game client* (focus-stealing window). Our headless dedicated-server runner is better for long autonomous agent runs. | — |

## Getting started (green-light checklist)

1. Land the dependency surgery + a green `cli test` baseline first. **In
   progress** (the server cannot boot until the EPF dependency chain is
   removed).
2. Framework extensions: `ctx.EqualFloat/NotNull/InRange/Pass` — **done**.
   Multi-tick `Tick()` deferred (no async LOGIC cases yet).
3. `Fixtures/` folder with one fixture file per area — **done**
   (`EL_TestFixtures_Level.c`); probes expose protected state without
   `Replication.BumpMe`.
4. LOGIC-tier pin + contract tests — **done** (survival, account, bank, level,
   format, licenseplate), all registered, `cli validate` green (compile
   check included). See status markers in the tables above.
5. Prove each can go red (perturb, run, observe, revert) — **pending the
   build baseline**. Every file carries a `// red-proof:` comment describing
   the perturbation; the actual red run happens on the first working
   `cli test`.
6. World-tier tests land once the build+server baseline is green (money,
   quantity, shop/trader exploits, police RPC, crime, spawn, notifications).
