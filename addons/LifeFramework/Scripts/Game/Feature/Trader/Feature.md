# Trader — Design Requirements

> Context file for the sell-side of the economy. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

The player sells goods **to a merchant who exists in the world** — the
black-market dealer in a back room, the universal trader at the market stall.
The player hands the goods over and is paid. Same physical-store philosophy as
`Shop`: the merchant is a place, not a menu.

## Interaction pattern (in-world)

Today: a trader-owned inventory storage — dropping an item into the storage
deletes it and credits the seller's ATM account (`EL_TraderManagerComponent` +
`EL_InventoryStorageManagerComponent`). That is a physical-ish rung 1 action
(the storage is a real object) but the payout path is broken (see current
state).

Target: a **trader NPC / merchant station** the player walks to and interacts
with. Offer the goods, see the quote, confirm — the transaction happens at the
counter. Payment is physical cash or a bank deposit the teller/merchant makes
on the spot, never minted.

- Universal trader — legal goods, market price.
- Black market — restricted goods (weapons, contraband), civilians rejected,
  higher risk/reward.

## V1 (shippable)

1. Fix the sell path's correctness: the null `GetOwner()` deref, the null
   `m_aTradableItems` foreach crash, delete-then-pay not atomic, quantity stacks
   selling as one item, and the payout going to an ATM bank balance keyed
   differently than the caller. **The sell path must credit the seller
   exactly the quoted value and only after the goods are actually gone.**
2. A merchant station (sign + storage + payout) as the reference sell point.

## Iteration path

- **V2** — merchant NPC with a quote interaction (offer → confirm → paid).
- **V3** — per-item quoted prices that respond to stock/supply; black-market
  risk (police patrols near the dealer).
- **V4** — bartering, haggling, bulk discounts.

## Current state

- `EL_TraderManagerComponent` + `EL_InventoryStorageManagerComponent`
  (`Components/InventorySystem/`) — sell-into-storage pays the ATM account.
- ❌ Broken: null `GetOwner()` crash, null `m_aTradableItems` foreach crash,
  delete-then-pay not atomic, stacks sell as one item, wrong "not tradable"
  return code, black-market check likely fails open, payout goes to bank not
  cash, `Print("testtesttest")` debug line in
  `EL_RestrictedInventoryStorageComponent`.
- `EL_OpenTraderAction` — the interaction entry point.
- Prefabs: `Prefabs/Trader/UniversalTrader.et`, `Prefabs/Trader/BlackMarketTrader.et`.

## Dependencies

- `Money` (cash payout), `Banking`/`ATM` (banked payout).
- `Shop` (same physical-commerce philosophy; vehicle *purchases* are Shop's).
- `Quantity` (stack-aware sell amounts).
- `Crime` / `Police` (black-market risk and the wanted consequences).