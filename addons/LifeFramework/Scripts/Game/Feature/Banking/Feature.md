# Banking — Design Requirements

> Context file for the bank branch and account system. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

The bank is a **place**. The player walks into the branch and does their banking
at the counter — transfers, deposits, withdrawals — with a teller behind the
counter. This is period-accurate 1989: to move money between cash and account,
you go to the bank. The only abstract money in the game lives here.

## Interaction pattern (in-world)

Rung 3 target (world-anchored): an NPC teller at a counter opens the transfer
screen; the counter is the gate. A self-service ATM machine (`ATM` feature) is
the rung-2 convenience for cash in/out without a teller.

Today the code is rung 4 (a component with methods and a menu) and — critically —
there are **two unintegrated banking systems**: `EL_ATMManager` (per-player
persistent balance, `Feature/ATM/`) and `EL_BankAccountComponent` (entity bank
with replicated balance, `Feature/Banking/`). Both must collapse into one system
before either becomes the physical branch.

## V1 (shippable)

1. **Unify the two bank systems.** One account model, one persistence path.
   The "second, unintegrated banking system" in
   `EL_BankAccountComponent` is a design debt and a money bug source.
2. Remove the **$20,000 grant to every owner in `OnPostInit`** — that is a
   mint bug, not a feature.
3. Make the money flow honest: Deposit takes cash out of the inventory,
   Withdraw pays it out (the `EL_ATMMenu` money-from-thin-air placeholder must
   die).
4. A bank-branch prefab (building override) with a counter interaction as the
   reference surface.

## Iteration path

- **V2** — NPC teller behind the counter; transfers/withdrawals through the
  teller.
- **V3** — bank security (vault, robberies via `Crime`), per-branch floats,
  interest (already present in `EL_BankAccountComponent`).
- **V4** — cheques/IOUs, joint accounts.

## Current state

- `EL_BankAccountComponent` (`Feature/Banking/`) — entity bank, replicated
  balance, interest, transaction history (capped 100), Deposit/Withdraw/
  Transfer with guards. ❌ $20k grant, persistence race, unthrottled
  `BumpMe`, second unintegrated system.
- `EL_ATMManager` + `EL_BankAccount` (`Feature/ATM/`) — per-player persistent
  balance, deposit/withdraw guards. ⚠️ `SetBalance` unclamped; `CreateAccount`
  silently overwrites.
- `EL_ATMMenu` (`Feature/ATM/`) — ❌ money from thin air (no cash deduction/
  payout), no RPC path, keys not localized.
- Persistence: `Feature/ATM/Persistence/`.

## Dependencies

- `Money` (physical cash is the thing deposited/withdrawn).
- `ATM` (self-service machine at the branch).
- `Crime` / `Police` (bank robberies, if desired).
- `Trader` / `Shop` (store floats, payouts).