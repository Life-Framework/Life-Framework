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

The ATM is the **only** cash↔account boundary (verified 2026-08-30): every
payout in the game pays cash, and this machine is where cash becomes a balance
or a balance becomes cash.

## V1 (shippable)

1. **Money flow is honest — DONE (verified 2026-08-30).** Deposit takes real
   cash out of the player's inventory (partial removals rolled back), Withdraw
   pays real cash in (all-or-nothing with balance restore). Server-guarded via
   `EL_CharacterATMComponent.RpcAsk_Deposit` / `RpcAsk_Withdraw`.
2. Server-guarded transaction path (RPC ask → server validates balance and
   cash → result) — DONE.
3. A physical ATM prefab at the bank branch as the interaction point — pending.

## Iteration path

- **V2** — transaction history at the machine, receipts.
- **V3** — ATM robberies / tampering (police response), daily withdrawal limits.

## Current state

- `EL_ATMManager` + `EL_BankAccount` (`Feature/ATM/`) — the canonical bank:
  session map registry + async loader; persistent per-player balance with
  deposit/withdraw guards. ✅ `SetBalance` clamps corrupt values;
  `CreateAccount` rejects empty IDs and preserves existing accounts on
  duplicate creation (verified 2026-08-30).
- `EL_ATMMenu` — deposit/withdraw UI on the cash-moving RPC bridge; amount
  guards via `EL_ATMManager.IsValidAmount`.
- `EL_ATMAction` / `EL_CharacterATMComponent` — interaction and per-character
  wiring.
- Persistence: `Feature/ATM/Persistence/`.

## Dependencies

- `Banking` (the account system — unified; this is it).
- `Money` (physical cash).
- `Quantity` (stack-aware cash handling via `MoneyStack`).
