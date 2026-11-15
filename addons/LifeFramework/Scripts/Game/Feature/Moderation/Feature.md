# Moderation - Design Requirements

Admins rule from facts, not two players' stories. `LF_IncidentLog` records deaths,
sampled player-on-player damage, and cash moves on the server only, as a subscriber
on `LF_EventBus` (`docs/architecture/events.md`). It is bounded by config, saved in
the `incidents` payload, and never replicated. Query by position and time window.
See `docs/features.md`.

Administrators read incidents in Game Master EDIT mode (`LF_IncidentGm` answers by RPC, admin role checked on the server; `UI/GM/LF_IncidentGmPanel.c`).

Players `/report` their recent incidents; flags (`LF_IncidentFlags`, saved as `incidentflags`) head the Game Master list until an admin types `!handled <number>`.

## Staff relay (#1284)

`LF_StaffRelay` (server only, no state saved) sends `LF_OnAdminAction` and `LF_OnIncidentFlagged` to the staff channel through `LF_StaffWebhook.Send`. Events of one kind collect for 5 s from the first, then one line goes out: the event's own line, or a count when there were more. `LF_IncidentFlags.Report` publishes the flag event after inserting the rows. `Install()` subscribes and starts a 1 s flush tick. A `modded class LF_GameModeRoleplay` in the same file calls it from `OnGameStart` (server only; a client does nothing; calling twice changes nothing), and `LF_StaticReset.ResetAll` calls `Uninstall()` when the world ends.

## Clip stamp (#1173)

- **Player sees:** a small paper tag in the top left (UI kit frame and line, themed): server name, last six characters of their character id, real server date and time in UTC. Off by default; the `LF_ClipStamp` key (F9, gamepad tap RB + View) toggles it. A clip's stamp matches an incident row because both use `System.GetUnixTime` on the server.
- **Owner and side:** the client toggles and draws (`LF_ClipStampHud`); the server answers one RPC per ask (`LF_ClipStampController`: `RpcAsk_LF_ClipStamp` then `RpcDo_LF_ClipStamp`) with its unix time, the server name, the id tail and the owner flag. The client keeps the answer's unix time and tick and counts seconds itself, re-asking every 5 minutes while the stamp is up. No `RplProp`, no `BumpMe`.
- **Data:** `LF_IncidentConfig` (`Configs/Moderation/LF_Incident.conf`): `m_bClipStampAllowed` (owner switch, 1) and `m_sClipStampServerName` ("LF"). Strings: `LF-ClipStamp_Action`, `_Denied`, `_Line`. Nothing persists; the on/off choice lasts for the session.
- **Proof:** `moderation/clip-stamp` (LOGIC: formatter, id tail, clock offset, toggle and owner flag); a `--profile full` picture.
