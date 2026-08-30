# Notifications — Design Requirements

> Context file for player-facing signals. Written against
> `docs/design-philosophy.md`.

## Intent

Notifications are **signals, not transactions** — the sanctioned pure-UI
surface (rung 4) for telling a player "your deposit failed", "wanted level
up", "paycheck received". They never replace a physical action; they announce
the result of one.

## Interaction pattern

Rung 4, accepted. `EL_NotificationManagerComponent` broadcasts server→client
toasts; `SendToPlayer`/`SendToJob`/`SendToAll` + static helpers.

## V1 (shippable)

1. **Fix the RPC targeting** — `RPC_ShowNotification` is `RplRcver.Owner` on
   the game-mode entity; delivery to the intended player is likely wrong.
2. `SendToJob` conflates "job != UNEMPLOYED" with `account.IsOnDuty()` — it
   sends to everyone with any job regardless of duty. Split the semantics.
3. Localize the hard-coded Spanish prefixes, drop the emoji from logs, and
   transmit `m_Color` (it never reaches the client).

## Iteration path

- **V2** — notification tiers (info/warning/crime) with distinct styling and a
  silent "you've been robbed" persistence path.

## Current state

- `EL_NotificationManagerComponent` (`Feature/Notifications/`) — toast
  broadcast. ❌ suspect RPC targeting, conflated SendToJob, Spanish prefixes,
  color never sent.

## Dependencies

- Everything that needs to announce a result: `Crime`, `Police`, `Jobs`,
  `Money`, `Survival`, `Banking`.