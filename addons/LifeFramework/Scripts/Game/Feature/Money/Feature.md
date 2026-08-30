# Money — Design Requirements

> Context file for the currency layer. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

**Cash is a physical inventory item.** The player carries money, hands it over,
drops it, and gets robbed of it. The only abstract money is the bank balance,
and it lives at the bank. This is the economy backbone every other money path
(`Shop`, `Trader`, `Banking`, `ATM`, `Crime`, `Jobs`) must sit on.

## Interaction pattern (in-world)

Rung 1. Cash is the `MoneyStack` inventory stack. Buying hands cash over;
selling puts cash in; withdrawing pays cash out; robbing takes cash away. There
is no balance-float that a player can reach into from anywhere.

## V1 (shippable)

1. **A single, honest cash API.** Today `EL_MoneyUtils` returns inconsistent
   sentinels (`-1` on some faults, `0` on others) and the doc comments on
   Remove/Take are wrong. Fix the contract, fix the docs.
2. Every money path in the game uses it — no `RemoveAmount`-or-similar
   bypasses. The known exploits (`Shop` partial-funds, `ATM` thin air,
   `Banking` $20k grant) all exist because a path sidestepped or misused this.

## Iteration path

- **V2** — denominations (banknotes of different values), cash visually changes
  with amount.
- **V3** — counterfeit detection, worn notes, cash drops that despawn safely.

## Current state

- `EL_MoneyUtils` (`Feature/Money/`) — `GetCash` (−1 on fault), `AddCash` /
  `RemoveCash` (actual amount, 0 on fault). ⚠️ Inconsistent sentinels, wrong
  doc comments.
- `EL_MoneyFormat` — number formatting (`AbbreviateNumber`).
- `EL_MoneyDeltaTracker` — change tracking.

## Dependencies

- Everything that moves value: `Shop`, `Trader`, `Banking`, `ATM`, `Crime`,
  `Jobs`, `Police` (fines), `License` (fees), `Houses`.
- `Quantity` (stackable cash).