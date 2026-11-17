# Notifications

### `LF_NotificationManagerComponent` — `Feature/Notifications/`
- ✅ Server→client notification broadcast with toast renderer; `SendToPlayer`/
  `SendToJob`/`SendToAll` + static `NotifyPlayer`/`NotifyJob`/`NotifyAll`.
  Before 2026-11-07 the toast layout
  `{4C38FD68D45325D2}UI/Layouts/Notifications/EL_NotificationToast.layout` did not
  exist, so every toast (fines, arrests, rank denials, lot, lock, jail) failed silently. It
  now exists: a paper slip in the UI kit palette with a stamp-ink type strip (`Background`,
  `ColorStrip`, `Icon`, `Title`, `Message`, `LF_NotificationToastUI` on the root).
  Slips that overlap in time stack downwards. Timing reads the real clock
  (`System.GetTickCount`), not an assumed 16 ms frame. `notifications/toast-layout` (WORLD)
  instantiates the layout on the dedicated server and checks every named widget;
  `notifications/toast-timing` (LOGIC) checks show/fade/remove. On-screen look is
  visual-only: manual check pending (verified 2026-11-07)
- ✅ **Toasts are visible on a game client** (2026-11-08, #157): until now no toast
  was ever drawn. The slip's frame slot had a height of -48 (`OffsetBottom -92` under
  `OffsetTop 140`), the fills were `PanelWidget`s (near-transparent) nested inside
  an `ImageWidget` (which draws no children), and the text column's negative
  stretch offsets ran the message past the slip. Now: 420x120 slip, sibling image
  fills, wrapped message, fade driven from `Tick` (`LF_NotificationToastUI.OpacityAt`).
  Seen at 1280x720 on a dedicated client (contraband refusal). New
  `tools/validation/validate-layout-slots.mjs` fails a point-anchored slot whose
  `Offset*` and `Size*` disagree (it also fixed `EL_SurvivalHUD.layout`).
- ✅ **Targeted delivery fixed** (verified 2026-08-30): `RpcDo_ShowNotification`
  is `RplRcver.Broadcast` with a client-side `playerId` filter (`-1` = all),
  plus a direct call for the listen-server host so the authority sees its own
  toast. `SendToJob` duty semantics now read the account's `IsOnDuty()`, not a
  `job != UNEMPLOYED` tautology.
- ✅ **Private targeted delivery** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): `SendToPlayer` sends an Owner RPC on
  the target's `SCR_PlayerController` (`LF_DeliverNotification`), so only that
  client receives it; the listen host's own controller calls the handler
  directly. Broadcast is left for `SendToAll` only. `SendToPolice` /
  `NotifyPolice` reach connected on-duty police by account faction.
  `LF_Utils.NotifyEntity` (shop, robbery, duty,
  confiscate toasts) route through it; `LF_Utils.Notify` still shows a hint on
  the calling machine only. Since 2026-11-08 the courier job (sign-up, paid,
  abandoned), gather/process rewards, weapon and vehicle robbery, and the
  contraband payout toast through `NotifyEntity`; on a dedicated server they
  had reached nobody. enforce-lint L8 fails an `LF_Utils.Notify` after an
  `if (!Replication.IsServer()) return;` guard.
- ⚠️ Hard-coded Spanish prefixes, emoji in logs, `m_Color` never transmitted.

