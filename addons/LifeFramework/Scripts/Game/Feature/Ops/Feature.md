# Ops - Design Requirements

Server operations the owner sets and players feel only as a warning: a scheduled restart that
announces itself, then closes through the engine's normal shutdown so the SHUTDOWN save runs.
Everything is server-side, off unless the owner configures a schedule, and talks over the
server bus (`docs/architecture/events.md`). This file describes every planned Ops file so the
slices that add them do not edit it.

## Pieces

- `LF_OpsEvents.c` (contract, #1344): `LF_OnRestartWarning` (`m_iMinutesLeft`,
  `m_iSecondsToRestart`) and `LF_OnRestartDue`. Plain values; published on the server only.
- `LF_RestartWarner` (stub in #1344): `StartIfConfigured()` is called by the game mode beside
  `LF_HostStop.StartIfRequested()`. The ticker (O6T) starts here when a schedule exists and
  publishes the events; a broken schedule logs `LF_Debug.Error` and the server runs unscheduled.
- Schedule (O5): reads the owner's restart times and warning steps from config and answers the
  next restart and the warning steps still ahead.
- Ticker (O6T): a coarse server timer (seconds, not per frame) that publishes one
  `LF_OnRestartWarning` per step and one `LF_OnRestartDue`.
- Toast (O6): subscribes to `LF_OnRestartWarning`; shows each player a restart notice through the
  notifications feature. The text is a localization key, never built in script.
- Siren (O7): subscribes to `LF_OnRestartWarning`; sounds the final-minute warning in the world.
- Close (O8): subscribes to `LF_OnRestartDue`; requests a normal close, like `LF_HostStop`, so the
  engine runs the SHUTDOWN save before the process exits.

## Rules

- Subscribers never wait for the ticker and never call each other.
- Nothing here persists: the schedule is config, the warnings are derived from the clock.
- Proof: `ops/events` (LOGIC, the contract); each later slice adds its own test.
