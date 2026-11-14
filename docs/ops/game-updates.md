# Game updates and saves

A Steam update to Arma Reforger can change how the engine reads a save. 1.7.0.41 changed
the persistence UUID layout and every server lost its saves. A live server must not lose
characters, money and houses that way. This page is the owner's routine (#980).

## The rule

**After a game update, run `tools\cli host preflight` before opening the server.**

`tools\cli host start` refuses a save written by another game build until a preflight
passes (`--force` overrides it). `tools\cli host status` shows both builds.

## What stamps a save

- The engine writes the build into every save point's `meta-info.json` (`m_sGameVersion`,
  such as `1.9.0.37`). The tools read it before a boot.
- The mod writes its own stamp, the `buildstamp` payload (save version 32,
  `LF_SaveStamp`): the game build (`GetGame().GetBuildVersion()`) and our save version.
  The load logs `[LFDebug:Persistence] save loaded: version N, game build X, running Y`. A
  save from another build also logs a warning that names both builds.
- The installed build is the file version of `ArmaReforgerServer.exe`, which is the same
  string.

## What the preflight does

1. Backs up the save folder to `backups/<time>-preflight-<old>-to-<new>`.
2. Boots the host on the installed build with `-loadSessionSave`.
3. Waits for the `save loaded:` line, then stops the server so the engine writes the
   SHUTDOWN save.
4. Fails if the log has no `save loaded:` line, a `save load damaged` or `continued save
   never reached the LF store` error, a script error or VME, or no SHUTDOWN save.
5. **On a failure it restores the backup.** The saves the failed boot left are moved to
   `backups/<time>-replaced-by-restore`; nothing is deleted. Do not open the server.

`tools\cli host restore [<backup folder name>]` puts the newest (or a named) backup back
by hand, with the same keep-what-it-replaces rule.

## When a load goes wrong in a running server

- A payload that fails to read logs a warning and leaves that feature's live state alone.
  If any payload failed, the load ends with `LF_Debug.Error` `save load damaged: N
  payload(s) failed to read ...` and **autosave pauses**, so the next autosave does not
  overwrite the AUTO save point with what is left.
- A continued save that never reached the LF store (the engine dropped it, or it predates
  the LF_ rename) logs `continued save never reached the LF store`. The world runs fresh.
- When either happens in the boot load, the server also logs `saving disabled for this
  session` and calls `SaveGameManager.SetSavingAllowed(false)`: no autosave, no manual save,
  and **no engine shutdown save**, so the save on disk stays the newest save point. Proven
  on 2026-11-14 with a save whose WorldState blob was moved away: two save points before
  the boot and the stop, two after.
- In both cases: stop the server, `tools\cli host restore`, and find the cause before
  opening again.

## Diagnosing a bad load

Add `-persistenceDiag logs` (engine option, 1.7 and later) to the server launch to log the
engine's persistence work, then grep the console log for `Persistence` and
`[LFDebug:Persistence]`. `tools\cli host status` names the newest log.

## For developers

- `tools\cli e2e --old-build` (or `wt e2e <slug> --old-build` with the worktree's CLI)
  writes the save boot's stamp as game build 1.8.0.13, rewrites `meta-info.json` to
  match, has the build gate back the save up to `server/profile/test-backups/`, and the
  reload boot on the real build must load the whole save.
- The day a game update lands, the test pool runs `wt e2e` on the new build before
  anything is merged.
- Read the changelog the day it ships and add a row below for anything touching
  persistence, inventory, vehicles or replication.

## Changelog notes

| Build | What touches us |
|---|---|
| 1.6.0.76 | More than 999 save points could crash the server (fixed). |
| 1.6.0.119 | Loading save data with deleted prefabs crashed (fixed). |
| 1.7.0.41 | Persistence UUID generation changed its layout; old save data is ignored. Every server lost its saves. Adds `-persistenceDiag logs`. |
| 1.8.0.13 | Persistence script rules were not invoked before this build (fixed); earlier builds skipped them silently. |
| 1.9.0.37 | Installed on 2026-11-14; the current baseline. |

Source: Arma Reforger changelogs 1.4.0.53 (Aug 2025) to 1.9.0.37 (Nov 2026), read
2026-11-14 (#980).
