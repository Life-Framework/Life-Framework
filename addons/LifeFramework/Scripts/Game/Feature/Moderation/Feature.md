# Moderation - Design Requirements

Admins rule from facts, not two players' stories. `LF_IncidentLog` records deaths,
sampled player-on-player damage, and cash moves on the server only, as a subscriber
on `LF_EventBus` (`docs/architecture/events.md`). It is bounded by config, saved in
the `incidents` payload, and never replicated. Query by position and time window.
See `docs/features.md`.

Administrators read incidents in Game Master EDIT mode (`LF_IncidentGm` answers by RPC, admin role checked on the server; `UI/GM/LF_IncidentGmPanel.c`).

Players `/report` their recent incidents; flags (`LF_IncidentFlags`, saved as `incidentflags`) head the Game Master list until an admin types `!handled <number>`.
