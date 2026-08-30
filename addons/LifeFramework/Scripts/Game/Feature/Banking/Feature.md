# Banking — Design Requirements

> Context file for the bank branch and account system. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

The bank is a **place**. The player walks into the branch and does their banking
at the counter — transfers, deposits, withdrawals — with a teller behind the
counter. This is period-accurate 1989: to move money between cash and account,
you go to the bank. The only abstract money in the game lives here.

**Money boundary (enforced, verified 2026-08-30):** every payout in the game —
paychecks, trader/shop sells, robbery hauls, job rewards — pays **physical
cash**. The bank balance moves **only** through the ATM (`Feature/ATM`). There
is exactly one bank: `EL_ATMManager` + `EL_BankAccount` (per-player, keyed by
UID, persisted). The old entity bank `EL_BankAccountComponent` was deleted.

## Interaction pattern (in-world)

Rung 3 target (world-anchored): an NPC teller at a counter opens the transfer
screen; the counter is the gate. A self-service ATM machine (`ATM` feature) is
the rung-2 convenience for cash in/out without a teller.

## V1 (shippable)

1. **Unify the two bank systems — DONE (verified 2026-08-30).** One account
   model (`EL_ATMManager`/`EL_BankAccount`), one persistence path; the entity
   bank `EL_BankAccountComponent` is deleted.
2. Remove the **$20,000 grant** — DONE: `m_iStartingBalance` defaults to 0 and
   persistence restore is authoritative.
3. Make the money flow honest — DONE: `EL_CharacterATMComponent.RpcAsk_Deposit`
   removes cash (partial removals rolled back), `RpcAsk_Withdraw` pays cash
   (all-or-nothing with balance restore).
4. A bank-branch prefab (building override) with a counter interaction as the
   reference surface — pending.

## Iteration path

- **V2** — NPC teller behind the counter; transfers/withdrawals through the
  teller.
- **V3** — bank security (vault, robberies via `Crime`), per-branch floats,
  interest.
- **V4** — cheques/IOUs, joint accounts.

## Current state

- `EL_ATMManager` + `EL_BankAccount` (`Feature/ATM/`) — the canonical bank:
  per-player persistent balance, deposit/withdraw guards. ⚠️ `SetBalance`
  unclamped; `CreateAccount` silently overwrites.
- `EL_CharacterATMComponent` (`Feature/ATM/`) — cash-moving RPC bridge; the
  only cash↔account boundary.
- `EL_ATMMenu` (`Feature/ATM/`) — deposit/withdraw UI on the bridge.
- Persistence: `Feature/ATM/Persistence/`.

## Dependencies

- `Money` (physical cash is the thing deposited/withdrawn).
- `ATM` (self-service machine at the branch).
- `Crime` / `Police` (bank robberies, if desired).
- `Trader` / `Shop` / `Jobs` (payouts — all cash now).