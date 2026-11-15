# Cross-cutting risk summary (re-audited 2026-08-30; see per-feature rows)

1. **Money — one canonical path, all payouts cash.** The entity bank
   `LF_BankAccountComponent` is **deleted**; `LF_ATMManager`/`LF_BankAccount`
   is the only bank and the ATM is the only cash↔account boundary. Duty pay, benefits,
   trader sells, shop sells, robberies and job rewards all pay `LF_MoneyUtils`
   cash. Shop buy/sell correctness closed (partial refunds), ATM deposit/
   withdraw moves real cash with rollback. Remaining: no shop stock / per-item
   max quantity.
2. **Security — mostly closed.** Fruit-catcher score is server-clamped
   (pin test); `RpcAsk_DebugGrantLicense` removed; quantity RPCs have an
   ownership gate; gather/process actions re-validate server-side and reject
   malformed configurations. Remaining: multiplayer proof for the RPC edges.
3. **Persistence — improved.** Cache-eviction churn fixed (account stays
   resident). The serializer writes a `version` header
   first (4) and `validate-persistence-version.ps1` checks it against the
   payload order. Remaining: jail, breach, and bank-vault state has
   `Export` / `Apply` but is not in the save serializer, so it is lost on
   restart.
4. **Compile risk — resolved.** `LF_EFactionType` is defined in
   `LF_WhitelistManager.c`.
5. **Client/server breaks — mostly closed.** Police arrest/fine bridge and
   notification targeting are fixed; the death screen (client) → respawn RPC
   (server) bridge is new and needs a manual multiplay pass. Remaining:
   hand-carry holster flow is client-only.

