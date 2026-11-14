# Moderation - Design Requirements

Admins rule from facts, not two players' stories. `LF_IncidentLog` records deaths,
sampled player-on-player damage, and cash moves on the server only, as a subscriber
on `LF_EventBus` (`docs/architecture/events.md`). It is bounded by config, saved in
the `incidents` payload, and never replicated. Query by position and time window.
See `docs/features.md`.

Administrators read incidents in Game Master EDIT mode (`LF_IncidentGm` answers by RPC, admin role checked on the server; `UI/GM/LF_IncidentGmPanel.c`).

Players `/report` their recent incidents; flags (`LF_IncidentFlags`, saved as `incidentflags`) head the Game Master list until an admin types `!handled <number>`.

## Clip stamp (#1173)

- **Player sees:** a small paper tag in the top left (UI kit frame and line, themed): server name, last six characters of their character id, real server date and time in UTC. Off by default; the `LF_ClipStamp` key (F9, gamepad right stick) toggles it. A clip's stamp matches an incident row because both use `System.GetUnixTime` on the server.
- **Owner and side:** the client toggles and draws (`LF_ClipStampHud`); the server answers one RPC per ask (`LF_ClipStampController`: `RpcAsk_LF_ClipStamp` then `RpcDo_LF_ClipStamp`) with its unix time, the server name, the id tail and the owner flag. The client keeps the answer's unix time and tick and counts seconds itself, re-asking every 5 minutes while the stamp is up. No `RplProp`, no `BumpMe`.
- **Data:** `LF_IncidentConfig` (`Configs/Moderation/LF_Incident.conf`): `m_bClipStampAllowed` (owner switch, 1) and `m_sClipStampServerName` ("LF"). Strings: `LF-ClipStamp_Action`, `_Denied`, `_Line`. Nothing persists; the on/off choice lasts for the session.
- **Proof:** `moderation/clip-stamp` (LOGIC: formatter, id tail, clock offset, toggle and owner flag); a `--profile full` picture.
