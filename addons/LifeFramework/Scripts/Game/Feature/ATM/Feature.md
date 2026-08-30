# ATM — Design Requirements

> Context file for the self-service cash machine. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

The ATM is a **physical machine at the bank** (or at a corner, like 1989) that
converts between physical cash and the bank balance. It is the self-service
convenience layer on top of `Banking`; the branch teller is the full-service
one.

## Interaction pattern (in-world)

Rung 2 target: the player walks to the ATM machine, interacts with it, and does
a cash-in / cash-out transaction. The machine is the gate — there is no
"withdraw from anywhere" path.

Today the code is rung 4 (a menu you can open) and it is a **money exploit**:
Deposit never deducts cash, Withdraw never pays out — money from thin air.

## V1 (shippable)

1. **Make the money flow honest** — Deposit takes real cash out of the
   player's inventory, Withdraw pays real cash in. This is the #1 blocker.
2. Server-guarded transaction path (RPC ask → server validates balance and
   cash → result). The current menu has no RPC path at all.
3. A physical ATM prefab at the bank branch as the interaction point.

## Iteration path

- **V2** — transaction history at the machine, receipts.
- **V3** — ATM robberies / tampering (police response), daily withdrawal limits.

## Current state

- `EL_ATMManager` + `EL_BankAccount` (`Feature/ATM/`) — session map registry +
  async loader; persistent per-player balance with deposit/withdraw guards.
  ⚠️ `SetBalance` unclamped; `CreateAccount` silently overwrites an existing
  account; the manager inherits `ScriptedUserAction` (misfit).
- `EL_ATMMenu` — ❌ **money from thin air**, no RPC path, buttons server-guarded
  so a real client does nothing, no insufficient-funds feedback, keys not
  localized.
- `EL_ATMAction` / `EL_CharacterATMComponent` — interaction and per-character
  wiring.
- Persistence: `Feature/ATM/Persistence/`.

## Dependencies

- `Banking` (the account system — must be unified with it).
- `Money` (physical cash).
- `Quantity` (stack-aware cash handling via `MoneyStack`).