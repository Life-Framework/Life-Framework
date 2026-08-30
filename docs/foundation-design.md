# Foundation Design — Economy, Trade, Processing, Real Estate

> Design for the four foundational Life primitives, mined from the MIT-licensed
> [Overthrow.Arma4](https://github.com/ArmaOverthrow/Overthrow.Arma4) (v1.41)
> reference implementations. Every feature is a small, testable, config-driven
> primitive — the low-LOC philosophy — so the four areas share one stack and one
> set of tests. Contract: `docs/features.md`. Test cases: `docs/test-plan.md`.
>
> **Borrow modes:** **Port** = lift the Overthrow file, re-prefix `OVT_`→`EL_`,
> drop Overthrow-only coupling. **Adapt** = take the architecture, write the
> Life content. **Pattern** = copy the design lesson, write fresh.

## Why this shape is low-LOC

Overthrow's own best code already answers "how do we do a lot with little":
put **pure logic** in `Data/` (world-free, Logic-tier testable), **catalogues**
in `.conf` config classes, one **thin server component** per feature, one
**RPC request component** per player for client→server, and **SaveData**
classes for persistence. The managers that look enormous (`OVT_EconomyManagerComponent.c`
is 2,389 lines) are mostly Overthrow-isms we do not need — town coupling, AI
income, sleep replay, stock replenishment. Stripped to the primitive, each
feature is a few hundred lines plus a config.

Every Overthrow reference below follows this exact split. We copy the split,
not the bulk.

### The primitive stack (shared shape)

| Layer | Where | Responsibility | Test tier |
| --- | --- | --- | --- |
| Pure logic | `Scripts/Game/Data/` | Rules, math, records. No `IEntity`, no replication, no `OVT_Global`. | LOGIC |
| Catalogues | `Scripts/Game/Configuration/` | `.conf`-driven data (`EL_*Config`). Loaded once, owned by a manager. | LOGIC |
| Thin component | `Scripts/Game/Components/` | Holds one piece of per-entity state (stock, store, property tag). Replicates only what clients must see. | WORLD |
| RPC seam | on the player controller | `RpcAsk_*` client→server; **server re-validates everything**, answers via `RpcDo_*` or replicated state. | manual |
| Persistence | `…/Persistence/` | SaveData round-trip through public API only. | LOGIC |

Rules that make this stay small:

1. **One way to count money.** Every mutation funnels through one seam. The
   banking merge is complete: `EL_ATMManager` + `EL_BankAccount` is the sole
   bank, payouts are physical cash, and the ATM owns the cash/account boundary.
2. **The server recomputes price/eligibility through the same pure call the
   client used** (Overthrow's `OVT_ShopSellRules` doctrine — "four callers, one
   rule set"). Client offer and server authority cannot drift.
3. **All-or-nothing mutations.** Funds/stock/inputs are checked *fully* before
   any deduction; a partial failure refunds exactly what was actually removed
   (the `EL_ShopComponent` exploit and `EL_ProcessAction` free-output bug die
   here).
4. **Config, not code.** A server owner retunes the economy in Workbench.

---

## Foundation kit — port first (≈250 LOC)

These are dependency-free and drop in before any feature. Port verbatim,
re-prefixed, minus Overthrow-only fields.

| Primitive | Source (Overthrow) | LOC | Notes |
| --- | --- | --- | --- |
| `EL_MoneyFormat` | `Scripts/Game/Data/OVT_MoneyFormat.c` | 65→40 | Thousands separators, `-$150` sign placement. Hand-rolled because EnforceScript has no locale formatter. |
| `EL_MoneyDeltaTracker` | `Scripts/Game/Data/OVT_MoneyDeltaTracker.c` | 126→90 | HUD "+$500" ticker. Poll-driven, Logic-testable by hand-written sequence. |
| `EL_ShopRules` | `Scripts/Game/Data/OVT_ShopSellRules.c` | 107→80 | `ShopBuysFromPlayers` / `GetSellMultiplier` / `CanSellItem` / `GetBlockReasonKey`. The one rule set behind menu, grey-out and server. |
| `EL_ResourceDefs` + `EL_ResourceAmount` | `OVT_ResourcesConfig.c` + `OVT_ResourceLedger.c` (types) | 53→40 | The resource catalogue (id/title/icon/price) + the "which resource, how many" record. **Entry order is the wire index.** |
| Config load idiom | `OVT_ResourcesConfig.c` (`configRoot`) | — | The `LoadContainer` / `CreateInstanceFromContainer` pattern every catalogue uses. |

Optional, same shape, port when logistics need it: `EL_ResourceLedger`
(`OVT_ResourceLedger.c`, 278→180, drop litres/weight) — the pure holder with
`Add`/`Take`/`Count`/`WouldFit`/`GetLines`, capacity passed in, zero lines
never accumulate. Life's inventory stacks may make this unnecessary for now.

---

## 1. Economy

### Features we want

- **Cash** = `MoneyStack` inventory item (existing `EL_MoneyUtils` — keep).
- **One bank — DONE (verified 2026-08-30)** — persistent balance via
  `EL_ATMManager` + `EL_BankAccount`; `EL_BankAccountComponent` is deleted.
  A second, unintegrated bank is a defect, not a feature.
- **Working ATM RPC path — DONE (verified 2026-08-30)** — Deposit deducts
  *actual* cash removed; Withdraw pays out *actual* cash placed; failures roll
  back; server validates the request.
- **Money formatting** and the HUD delta ticker (foundation kit).
- **One authoritative account seam**: `Deposit(playerId, amt)` and
  `Withdraw(playerId, amt)` are called only by the ATM boundary. Payout callers
  (shop, trader, paycheck, robbery, job reward) use `EL_MoneyUtils` cash and
  never mutate the bank directly. This is the single fix for the money-exploit
  class in `features.md`.

### Primitives

| Piece | Shape | Source (Overthrow) | Borrow | LOC |
| --- | --- | --- | --- | --- |
| `EL_Money` / seam | Pure account record: id, balance, `Deposit`/`Withdraw` invariants (never negative, `<= 0` rejected) | `OVT_PlayerData` money record; `OVT_ShopTransactionComponent.c` result-code discipline | Adapt | ~80 |
| `EL_BankAccount` | Keep existing (`Feature/ATM/EL_BankAccount.c`) | — | — | — |
| `EL_ATMMenu` RPC fix | `RpcAsk_Deposit` / `RpcAsk_Withdraw` on the player controller, server validates cash vs balance | `OVT_ShopTransactionComponent.c` `RpcAsk_*` shape | Pattern | +60 delta |
| `EL_MoneyFormat` / `EL_MoneyDeltaTracker` | Foundation kit | `OVT_MoneyFormat.c`, `OVT_MoneyDeltaTracker.c` | Port | 130 |

**Reference files:** `OVT_MoneyFormat.c`, `OVT_MoneyDeltaTracker.c`,
`Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` (the
`RpcAsk_*` + result-code pattern), `OVT_EconomyManagerComponent.c` (pricing
seams `GetPrice`/`GetBuyPrice`/`GetSellPrice` — pattern only).

---

## 2. Trade (shops + sell)

### Features we want

- **Config-driven price catalogue** — one price per prefab filter
  (`m_sFind`), optional `demand`, `hidden`. Kills the `EL_ShopComponent`
  "no stock, no catalogue, no validation" trio.
- **Finite shop stock** that survives JIP — the shop holds a
  `map<resourceId, amount>`; buy decrements, sell (or restock) increments.
- **Server-authoritative buy** — check full funds **before** `RemoveAmount`;
  refund exactly what was removed; reject a shop that does not sell the item.
  This is the `shop/buy-math` pin test target in `test-plan.md`.
- **Sell path with reasons** — shop-buys rules with a reason key per blocked
  case (`EL_ShopRules`), quantity-aware credit (fixes stack-sells-as-one in
  `EL_TraderManagerComponent`), atomic delete-then-pay.
- **Category/tab UI** eventually reuses the shop model split; not in the
  primitive pass.

### Primitives

| Piece | Shape | Source (Overthrow) | Borrow | LOC |
| --- | --- | --- | --- | --- |
| `EL_PricesConfig` | `configRoot` catalogue: `m_aPrices` (find/cost/demand/hidden) | `Scripts/Game/Configuration/OVT_PricesConfig.c` | Adapt (keep filter+cost+hidden; demand optional) | ~50 |
| `EL_ShopStock` component | `map<int,int>` stock, `AddToInventory`/`TakeFromInventory`/`GetStock`, `RplSave`/`RplLoad` JIP, `StreamInventory` broadcast | `Scripts/Game/Components/Economy/OVT_ShopComponent.c` | **Port** (this is the clean one) | 164 |
| `EL_ShopRules` | Foundation kit: buy-eligible, sell-eligible, reason keys | `Scripts/Game/Data/OVT_ShopSellRules.c` | Port | 80 |
| `EL_ShopTransaction` (server) | `BuyItems`/`SellItems` validated: full-funds check, stock decrement, refund actual, result codes to client | `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` | Pattern (drop town/procurement/AI) | ~150 |
| `EL_TraderManager` fix | Rebuild on `EL_ShopRules` + the transaction seam; atomic delete+pay | existing + pattern | Fix | ~60 delta |

**Reference files:** `OVT_ShopComponent.c` (**portable whole**),
`OVT_ShopSellRules.c`, `OVT_ShopTransactionComponent.c` (pattern),
`OVT_PricesConfig.c`, plus the UI split in `Scripts/Game/Data/OVT_ShopBrowserModel.c`
and `OVT_ShopCategory.c` for the later menu pass.

---

## 3. Processing (gather → craft)

### Features we want

- **Config-driven recipes** — inputs (resource id, qty) → outputs (id, qty).
  Generalizes "apples → apple juice" to anything.
- **Rule-level can-perform gate** — outputs are granted **only if every input
  is fully present**, and consumption uses actual amounts. This kills the
  `EL_ProcessAction` free-output exploit at the pure-logic layer, not in an
  action.
- **Server-side re-validation** — the tool check (`EL_GatherAction`) and the
  input check (`EL_ProcessAction`) run on the **server** in the perform path,
  not just the client's `CanBePerformedScript`. Hostile-RPC protection.
- **Config-driven gather node** — a world-authored component
  (resourceId, units per action, tool, cooldown, max actions) so mappers add
  resource spots without code. Keep the existing `EL_GatherAction`; make it a
  thin server re-validating caller of the node.

### Primitives

| Piece | Shape | Source (Overthrow) | Borrow | LOC |
| --- | --- | --- | --- | --- |
| `EL_RecipeConfig` | `configRoot` catalogue: `m_aRecipes` (inputs[], outputs[], tool?) | `OVT_ResourceRequirements` shape | Pattern | ~40 |
| `EL_RecipeRules` | Pure: `CanPerform(recipe, available)`, `GetShortfall(...)`, `Consume(...)` returns actual consumed, all-or-nothing | `OVT_ResourceRules.c` + `OVT_ResourceRequirements.c` doctrine | Pattern | ~120 |
| `EL_ResourceNode` component | World-authored: resourceId, unitsPerAction, toolPrefab, cooldownSeconds, maxActions, restockSeconds | `Scripts/Game/Components/OVT_ResourceProductionComponent.c` (the authored-site shape) | Adapt | ~90 |
| `EL_ProcessAction` / `EL_GatherAction` fixes | Thin actions calling the rules; server re-validates | existing + pattern | Fix | ~40 delta |

**Reference files:** `OVT_ResourceRequirements.c` (requirement lists, "the
single call behind both the displayed and the consumed figure"),
`OVT_ResourceProductionComponent.c` (authored-site component, **nothing
replicates**, world-query at Init), `OVT_ResourceRules.c`, `OVT_ResourcePack.c`
(compact wire encoding, only if needed).

---

## 4. Real Estate

### Features we want

- **Buy / rent persistent property** — houses, garages, business premises.
  Owner and renter are persistent player ids; state survives restart.
- **Config-driven property catalogue** — which prefab families are ownable
  (resource-name filters), base price, base rent. No town/support coupling for
  now — plain price + optional multiplier.
- **Server-side rent collection** — per-day rent on the economy tick, paid
  from balance or the property reverts (debt → repossess).
- **Home spawn** — `SetHome` on owned property feeds `EL_SpawnLogic`'s spawn
  position selection (Life already spawns account-aware).
- **One request RPC component** on the player controller: buy / sell / rent /
  stop-rent / set-home, every one server-validated.

### Primitives

| Piece | Shape | Source (Overthrow) | Borrow | LOC |
| --- | --- | --- | --- | --- |
| `EL_PropertyConfig` | Per-family: `m_aResourceNameFilters`, `m_BasePrice`, `m_BaseRent`, `m_IsStorage` | `Scripts/Game/Configuration/OVT_RealEstateConfig.c` | **Adapt (near verbatim)** | ~30 |
| `EL_PropertyOwnership` | Pure record: property key, ownerId, rented, price | `OVT_PersistedOwnership` records | Pattern | ~60 |
| `EL_PropertyManager` | Server manager: `GetConfig(entity)` filter-match, `Buy`/`Sell`/`Rent`/`StopRent`, `IsOwner`/`GetOwner`, rent tick, `ApplyPersisted` | `Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c` | Pattern (skip town/starting-home/warehouse bulk) | ~300 |
| `EL_PropertyRequestComponent` | `RpcAsk_BuyBuilding` / `RpcAsk_RentBuilding` / `RpcAsk_SetHome` / … — resolve owning player, server computes price, deducts through the money seam | `Scripts/Game/Components/Controller/OVT_RealEstateRequestComponent.c` | Pattern | ~150 |
| Property SaveData | Round-trip ownership + renter through public API | existing SaveData idiom | — | ~40 |

**Reference files:** `OVT_RealEstateConfig.c` (**portable whole**, 21 LOC),
`OVT_RealEstateRequestComponent.c` (the exact `RpcAsk_*` surface we want),
`OVT_RealEstateManagerComponent.c` (pattern — notably the
`GetConfig(entity)`/`BuildingIsOwnable`/`ScaleRealEstate`/`DemandFactor`
seams and `ApplyPersistedRealEstate`; ignore starting-homes, town demand and
warehouse migration).

---

## Build order

Each step ends verifiable (per `principle-sequence-verifiable-units`):

1. **Foundation kit** — `EL_MoneyFormat`, `EL_MoneyDeltaTracker`, `EL_ShopRules`,
   `EL_ResourceDefs` + config-load idiom. ~250 LOC, pure LOGIC-tier tests.
2. **Money seam + ATM fix** — one authoritative mutation path; `EL_ATMMenu`
   gets working `RpcAsk_*`. Kills money-from-thin-air and the two-bank mess.
   Gate: `bank/*` and `atm/*` tests.
3. **Price catalogue + `EL_ShopStock`** — port `OVT_ShopComponent` shape.
   Gate: `prefabs/load` + a stock round-trip.
4. **Server-authoritative buy + sell** — `EL_ShopTransaction` + trader rebuild
   on `EL_ShopRules`. Gate: `shop/buy-math`, `trader/*`, `shop/quantity-gate`
   pin tests go green.
5. **Processing rules** — `EL_RecipeConfig` + `EL_RecipeRules`, then rewire
   `EL_ProcessAction`/`EL_GatherAction`. Gate: `process/*`, `gather/*` pin
   tests.
6. **Real Estate** — config + ownership + manager + request component + rent
   tick + home spawn. Gate: new `property/*` LOGIC tests + a world
   buy/rent round-trip.

## Test mapping (LOGIC tier where possible)

| Area | New pure tests (from `test-plan.md` format) |
| --- | --- |
| Foundation | `core/money-format` (thousands, negatives), `core/delta-tracker` (seed, accumulate, reset), `shop/rules` (buy/sell eligibility, reason keys) |
| Economy | `bank/account-math` (existing), `atm/rpc-roundtrip` |
| Trade | `shop/buy-math` (existing pin), `shop/stock-decrement`, `trader/sell-reasons` |
| Processing | `process/recipe-gate` (all-or-nothing), `process/partial-inputs` (no free output), `gather/node-cooldown` |
| Real estate | `property/config-match` (filter), `property/buy-rent-invariants`, `property/persist-roundtrip`, `property/rent-collect` |

## Decisions to make (before build)

1. **One bank, which one?** Recommend `EL_ATMManager` as the single bank and
   delete/demote `EL_BankAccountComponent`'s balance — or fold its
   per-entity history in later. Do not keep both.
2. **Cash as `MoneyStack` or int balance?** Overthrow uses int money on the
   player record; Life currently uses a cash item. Keep the cash item (it is
   the robber/lose-on-death surface) but route all math through the seam.
3. **Shop stock replenishment** — port Overthrow's time-based restock later;
   the primitive pass ships with initial stock + buy/sell only.
4. **Real-estate pricing** — plain config price now; add demand/population
   coupling when Life has a population system (that is Overthrow's
   `DemandFactor`, not a Life primitive).

---

## Overthrow reference index

| Area | File | Borrow |
| --- | --- | --- |
| Kit | `Scripts/Game/Data/OVT_MoneyFormat.c` | Port |
| Kit | `Scripts/Game/Data/OVT_MoneyDeltaTracker.c` | Port |
| Kit | `Scripts/Game/Data/OVT_ShopSellRules.c` | Port |
| Kit | `Scripts/Game/Configuration/OVT_ResourcesConfig.c` | Adapt |
| Economy | `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` | Pattern |
| Economy | `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` | Pattern |
| Trade | `Scripts/Game/Components/Economy/OVT_ShopComponent.c` | Port |
| Trade | `Scripts/Game/Configuration/OVT_PricesConfig.c` | Adapt |
| Trade | `Scripts/Game/Data/OVT_ShopBrowserModel.c`, `OVT_ShopCategory.c` | Pattern (UI pass) |
| Processing | `Scripts/Game/Data/OVT_ResourceRequirements.c`, `OVT_ResourceRules.c` | Pattern |
| Processing | `Scripts/Game/Components/OVT_ResourceProductionComponent.c` | Adapt |
| Processing | `Scripts/Game/Data/OVT_ResourcePack.c` | Pattern (if wire encoding needed) |
| Real estate | `Scripts/Game/Configuration/OVT_RealEstateConfig.c` | Port |
| Real estate | `Scripts/Game/Components/Controller/OVT_RealEstateRequestComponent.c` | Pattern |
| Real estate | `Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c` | Pattern |

> Licence: MIT. Keep Overthrow's copyright notice in every adapted file and
> note provenance in the header. See `AGENTS.md` golden rules before writing
> any code.
