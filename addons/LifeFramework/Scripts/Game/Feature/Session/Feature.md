# Session

Per-session player facts on the server (M4, parent #1006). `LF_Session.OnPlayerSpawned` is called from
`LF_SessionHooks` (a `modded class LF_GameModeRoleplay` that chains `super.OnPlayerSpawned`), records the
account uid in a set, and publishes `LF_OnPlayerSpawnedEvent` (player id, account uid,
`m_bFirstThisSession`) on `LF_EventBus`. M4 features greet or tell a player something by subscribing, not by
editing the game mode.

- Server only; nothing replicates; the player id never leaves the server.
- The seen-uid set lives for the world: `LF_StaticReset` clears it with the other statics.
- An empty uid (identity not resolved yet) is never recorded and always counts as first; it logs a Warn.
- New session events are appended to `LF_SessionEvents.c`.
