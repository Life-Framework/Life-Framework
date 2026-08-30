# Police — Design Requirements

> Context file for law enforcement. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

Policing is **physical**: you go on duty at the station, you arrest at the
scene, you confiscate on the suspect. The management surface (wanted list,
fines, records) is a station terminal — a world-anchored rung 3 — not a menu
that works from anywhere.

## Interaction pattern

Rung 1 for duty/arrest/confiscate; rung 3 target for the station terminal.

Today: `EL_DutyAction` toggles the on-duty flag; `EL_PoliceMenu` shows the
wanted list with server-guarded Arrest/Fine; `EL_ConfiscateAction` deletes
weapons from the nearest wanted player within 5 m; `EL_OpenPoliceMenuAction`
is the entry point.

## V1 (shippable)

1. **Arrest/Fine are unreachable from a real client**: the menu runs on the
   client but both methods early-return unless `Replication.IsServer()`, with
   no `RpcAsk_*` bridge. Add the client→server ask → server-validate → do path
   (and validate the target on the server, never trust the client's pick).
2. Guard `account.GetActiveCharacter()` (null deref) and replace the hard-coded
   jail position `"0 0 0"`.
3. `EL_ConfiscateAction` deletes items while iterating a live `FindItems`
   array (mutation-during-iteration risk) and gives no compensation/wanted
   reduction.
4. Duty toggle evicts the account (cache churn) — fix via `Account`.
5. `#EL-Fined %1!` builds a non-key — localize properly.

## Iteration path

- **V2** — handcuffs / tasers, physical transport to jail (Phase 3 roadmap).
- **V3** — search & frisk, station terminal (rung 3), records.

## Current state

- `EL_PoliceMenu` — wanted list; ❌ unreachable Arrest/Fine, unguarded deref,
  hard-coded jail.
- `EL_DutyAction` — duty toggle; ⚠️ cache churn.
- `EL_ConfiscateAction` — weapon confiscation; ⚠️ mutation-during-iteration.

## Dependencies

- `Crime` (the wanted state it enforces), `Account` (duty, wanted), `Character`
  (inventory), `Houses` (raiding later), `License` (revocation).