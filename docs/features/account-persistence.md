# Account & Persistence

### `LF_PlayerAccount` — `Feature/Account/LF_PlayerAccount.c`
- ✅ Persistent per-player state: faction, on-duty flag, wanted level
  (clamped `[0,5]`), character roster. `AddCharacter`/`RemoveCharacter`,
  `GetActiveCharacter`, `SetActiveCharacter`, `IncreaseWantedLevel`.
- ⚠️ `RemoveCharacter` does not repair `m_iActiveCharacterIdx`; a stale index
  is an out-of-bounds read. `SetActiveCharacter` with a non-member sets idx
  to -1 with no guard. Fields are effectively public (workaround for BIS
  ticket T174113).

### `LF_PlayerCharacter` — `Feature/Account/LF_PlayerAccount.c`
- ✅ Immutable record (id, prefab, name, age), `GetFullName`, `Create`
  generates a persistence id.

### `LF_PlayerAccountManager` — `Feature/Account/LF_PlayerAccountManager.c`
- ✅ Async load + cache + `SaveAndReleaseAccount`. Verified 2026-08-30:
  `SaveAndReleaseAccount` now keeps the account resident, so consecutive
  wanted bumps and police-menu reads do not lose the account mid-session
  (the old evict-on-write churn is gone).

### `LF_PlayerAccountRecord` — `Feature/Account/Persistence/` (verified 2026-11-07)
- ✅ Explicit persisted form of an account and its characters (faction,
  duty, wanted, roster, active index). `LF_PlayerAccountManager.ExportAll` /
  `ApplyAll` convert it; `LF_PersistenceComponentSerializer` writes it after
  its `version` field. `account/save-roundtrip` covers the
  export → reset → apply path. EPF is gone; there is no `ApplyTo`.

### `LF_Playtime` — `Feature/Account/LF_Playtime.c` (#692)
- ✅ Playtime per account and per job or service, counted on the server and never
  replicated. Each connected player has a clock; every 15 s, on disconnect and
  before each save the seconds since its last credit go to the account's job now
  (`JobFor`: on-duty `POLICE`/`MEDIC`, else the signed-up delivery job such as
  `PARCEL`, else `CIVILIAN`), read through `LF_PlayerAccount` and
  `LF_CivilianJob.GetSignedUpJob` only. One credit is capped at 300 s (a wall-clock
  jump is not playtime). Keyed by the account uid, so gates read the person across
  characters. Read with `Total(uid)`, `JobSeconds(uid, job)`.
  Persisted as the `playtime` payload (save version 26, `LF_PlaytimeRecord`).
  `account/playtime` and the `e2e/pass*` pair cover it.

### `LF_PersistenceManagerComponent` — `Persistence/` (verified 2026-08-30)
- ✅ Server-side save orchestration on the game-mode entity: autosave timer
  (one AUTO slot per playthrough via `LF_SaveSelection`), manual saves,
  save-cache scan, load/re-apply paths, wipe. Every path passes
  `PassesSaveGates()` (manager present, saving allowed, persistence system
  active) and logs `LF_Debug` under `Persistence`.
- ✅ A continued save that never reaches the store read (written before the 2026-11-09 EL_ to LF_ rename, pre-release save reset) logs one `LF_Debug.Warn` "save predates the LF_ rename; starting fresh" at `ACTIVE` and runs fresh, no partial load (verified 2026-11-09 with an old save on the test server; docs/persistence.md "Save reset"). Since #980 the line is an `LF_Debug.Error` "continued save never reached the LF store" (a game update that drops saves looks the same) and autosave pauses so the save on disk is kept.
- 🟡 Game updates never cost a save (#980): each save carries a build stamp (`buildstamp` payload, save version 32, `LF_SaveStamp`; the engine's `meta-info.json` `m_sGameVersion` too). A load from another build warns with both builds; a load that loses a payload logs `save load damaged`, stops at that payload (the later ones keep their live state, the read is positional) and refuses every save: `SaveGame()`, `AutoSave()` and, through `SetSavingAllowed(false)` after the boot load, an in-session re-apply or at game end, the admin and shutdown saves (#2489). `tools\cli host start` refuses a save from another build until `tools\cli host preflight` passes: back up, boot the save on the installed build, check `save loaded:`, stop, and restore the backup on any failure (`host restore` by hand). Proof: `persist/save-build-stamp`, `tools\cli e2e --old-build`, a live preflight on a faked 1.8.0.13 save. Ops: `docs/ops/game-updates.md`.
- 🟡 Wipe step (#1274): `tools\cli host wipe --confirm <host dir name>` refuses without the typed confirm and while the host runs; otherwise it writes a `wipe`-tagged backup, moves the save root aside (nothing deleted), leaves an empty save root, logs a row to `backups/wipes.log` and prints the `host restore` line. A wipe is announced 24 h ahead in the alpha (#1413): it also refuses unless `website/content/sessions.json` holds a `next` wipe session whose posting time (`announced` as an ISO date-time; `announcedAt`, `noticeAt`, `postedAt` are read first) is at least 24 h old, and prints the time the wipe becomes allowed. Proof: `tools/validation/host-wipe-selftest.mjs`.
- 🟡 Port check (#1460): `tools\cli host ports [--config server.json] [--external host:a2sport]` reads the game, A2S and RCON (when set) ports from `server.json` (default `server/flagship/server.json`, else `server/example/`), asks `netstat` and `tasklist` who holds each UDP port and gives it `free`, `held-by-server` or `held-by-other`; exit 0 only when all are free (before a start) or all held by the server (after). It prints the router forward rules a PC host needs (game public to bind port, A2S; never RCON) and `--external` probes A2S from outside with a 4 s bound. Proof: `tools/validation/host-ports-selftest.mjs`.
- 🟡 Renamed content still loads (#965): saves name content by stable id (a prefab by its GUID, a job, quest, chain or crop by its config id, a place by its key), and a pack ships an alias table (`LF_SaveAliasesConfig`, listed in the mission header's config overlays; the core file is empty). Every store read loads old ids through it and logs each alias used once. A prefab that no longer loads is logged once and held in the stored inventory, inside its container, through every save, and comes back once the content or its alias returns. Save format unchanged (version 32). Proof: `persist/save-aliases`, `persist/held-missing-content`, `tools\cli wt e2e <slug> --content-ids` (three boots: rename, hold, restore). Audit of what each payload stores: `docs/persistence.md` "Stable ids".
- ✅ Shutdown save is engine-owned: the engine requests the SHUTDOWN save point
  on graceful exit; the manager observes the result via `OnAfterSave` and logs
  an `LF_Debug.Error` if a session ends with no shutdown save. The old manual
  `RequestSavePoint(SHUTDOWN)` from `OnGameEnd` always lost the race against
  the persistence teardown on the Workbench (fixed 2026-08-30). The engine
  writes it only under a mission header; a bare `DebugWorld.ent` boot writes
  none. `wt e2e` pass 1 ends through the host stop file on
  `EL_DebugTest.conf`, asserts the SHUTDOWN save landed before the statics
  reset, and pass 2 loads it (test `e2e/shutdown-save-loaded`, #830).
- ✅ Persistence system resolution is stable across reloads: the systems config
  is now `Configs/Systems/LifeFrameworkSystems.conf` (unique GUID, referenced
  explicitly from both missions), replacing the GUID-colliding
  `ChimeraSystemsConfig.conf` override. Resolution retries at game start and
  logs an `LF_Debug.Error` when the entry is truly missing.
- ✅ **Continuing a saved world needs `-loadSessionSave`** (verified 2026-11-07 on
  the dedicated test server: fresh, `-loadSessionSave`, and kept-saves-without-flag
  boots each logged their line): the engine loads the latest save point at launch only with
  that flag (vanilla behavior). Script does not auto-load: `LoadLatestSave()` is a
  game-state transition that boots the world twice and drops connected players.
  At game start on a dedicated server that was not launched from a save, the
  manager scans the mission's saves and logs `LF_Debug.Error` ("started a NEW
  session while N save point(s) exist") when any exist; with none it logs a
  fresh-session info line. See `docs/persistence.md`.
- ✅ **Playtest host** (`tools\cli host start|stop|status|backup`, `tools/host/host.mjs`;
  verified 2026-11-08 on the dedicated server): `start` runs `LF_PlaytestTown.conf`
  from a profile outside every worktree (`%LOCALAPPDATA%LifeFrameworkhost`,
  `LF_HOST_DIR`), passes `-loadSessionSave` when a save exists, and backs the saves
  up first. `stop` writes `$profile:lf-host-stop`; `LF_HostStop` closes the game
  normally so the engine writes the SHUTDOWN save, then the CLI backs the saves up.
  The server owns the machine-wide `server-2001` lock while it runs. The
  no-shutdown-save error now fires at teardown, not on the SHUTDOWN state change
  that comes before the save. Selftest: `tools/validation/host-selftest.mjs`.
  `host players [address:a2sport]` (#1297, merged, in-dev) sends an A2S_INFO query
  (challenge step included, 3 s timeout, default `127.0.0.1:17777`) and prints the
  server name, map and players/max. Operator tool for the session runbook and the
  opening gate's player target; nothing publishes it. `tools/host/a2s.mjs`; selftest
  `tools/validation/a2s-selftest.mjs`.
  `host listing [address:a2sport] [--version V] [--map M] [--open]` (#1461, in-dev) asks the
  running server what the browser shows over A2S_INFO and compares name, max players and
  the password flag with `server/flagship/server.json`. It fails when the name is not exactly
  `Life Framework | Everon 1989 (Alpha)` or the password flag is off (`--open` waives the
  password after the alpha); `--version` and `--map` add those checks, since server.json holds
  neither. `tools/host/listing.mjs`; selftest `tools/validation/host-listing-selftest.mjs`.
  `host verify [--newest N]` (#1454, merged, in-dev) opens each backup and checks it is not
  empty, holds a save point a restore can load (`meta-info.json` plus save data), and has a
  readable build stamp (`m_sGameVersion`). A good backup gets a sha256 recorded beside it
  (`backups/<name>.sha256.json`, never inside the folder); a later run flags one whose
  checksum changed. One line per backup (`ok`, `empty`, `nosave`, `nostamp`, `changed`,
  `error`), exit 1 on any bad one. Read only; `host drill` is the full boot test.
  `tools/host/verify.mjs`; selftest `tools/validation/host-verify-selftest.mjs`.
  `host applications [--all]` (#1458, in-dev) reads the host profile's
  `LifeFramework/police_applications.txt` and prints the open police applications
  oldest first (name, age, availability, why), then the officers on trial with the
  hours left; `--all` adds rejected, expired and trial-free approved rows. Rows with
  six or seven fields (older files) read; a malformed row is skipped with a warning
  naming its line. Read only: approving stays in the game. `tools/host/applications.mjs`;
  selftest `tools/validation/host-applications-selftest.mjs`.
  `host attendance [<console.log>] [--json]` (#1459, merged, in-dev) reads the
  engine's `Creating player` and `Player disconnected` lines from the newest host log
  (or a path) and prints the unique players (one `IdentityId`, else the name, so a
  rejoin under a new name is the same player), the peak concurrent count and the first
  time it was reached, and the minutes each player stayed. `--json` is the document
  `host history` (#1409) and `host gate` (#1410) read. It handles a leave glued
  mid-line onto another line, a missing leave, and midnight. `tools/host/attendance.mjs`;
  selftest `tools/validation/host-attendance-selftest.mjs`.
  `host bans [--player <uid>] [--open] [--file <path>]` (#1609, merged, in-dev) reads
  the host's `$profile:LifeFramework/ban_record.txt` (the `V1|ACTION|targetUid|unix|staffUid|duration|incidentIds|reason`
  rows of the ban record, #1285) and prints one line per ban and unban with time, reason,
  staff uid and incident ids; a ban a later unban of the same uid lifted shows as `closed`
  with who closed it, and `--open` keeps only bans no later unban closed, so an appeal is
  answered from this list. `--player` keeps one uid. A malformed or unknown-version row is
  reported with its line number and skipped. Read only. `tools/host/bans.mjs`; selftest
  `tools/validation/host-bans-selftest.mjs`.
  `host summary [<console.log>] [--perf <csv>] [--triage <file>] [--out <dir>]
  [--target 16] [--json]` (#1501, merged, in-dev) is the runbook's "after" step: it writes
  `<host>/sessions/<stamp>.md` with the session length, unique players and peak against
  the player target (the evidence for "sessions at the player target"), scheduled
  restarts, host stops and crash backups inside the session, the triage groups known and
  new, and the perf verdict of the newest `<host>/perf` file. It calls the attendance,
  perf and log-triage functions, not their CLI text. `tools/host/summary.mjs`; selftest
  `tools/validation/host-summary-selftest.mjs`. The page opens with a "Server doctor"
  section (#1605, merged, in-dev): server-doctor's verdict on the same log (booted, or the
  stage it stopped at with the doctor's fix line), the mods loaded, the server name from
  `server.json`, and the known triage lines it marked with their advice. A log with no
  engine header gets no such section. Selftest `tools/validation/host-summary-doctor-selftest.mjs`.
  Joining: the host runs `-noBackend`, so every player has the NULL platform
  identity. Until 2026-11-08 that made `GetPlayerUID` empty and the faction pick
  failed with `#EL-CC_Error_NoAccount` (nobody could join).
  `LF_Utils.ChooseIdentityFallback` now keys a player by Steam name on a
  `-noBackend` dedicated server: stable across restarts, one character per name
  (`spawn/identity-fallback`; join, restart, rejoin seen live).
  Listen host with the online service (launcher "Play on my own", hosting from the
  game menu, a friend joining it): the platform identity arrives about a second after
  the join. Playtest build 4 opened the account under a name stand-in first, so the
  signed form R-89 found no account ("Your account is still loading") and nobody
  spawned. `LF_PlayerIdentity` now waits for the real identity (the splash stays up;
  a toast after 3 s), keys by name only after 30 s with no answer, and moves a
  stand-in account onto the real key when the identity turns up late (records keep
  their keys; an empty stand-in beside a returning account is dropped, two accounts
  with characters are both kept). A signed form with no account opens one, and with
  no identity it comes back with the reason (`persist/late-identity`,
  `node agent-box/listen-host.mjs`).
- ✅ **Flagship in the scenario list, playable as a local game** (#374, merged, in-dev): the pack's
  `LF_Everon1989.conf` is the one listed Everon scenario ("Life Framework: Everon 1989", 16 players,
  one preview picture); the older `LF_PlaytestTown.conf` stays bootable by id for existing servers and
  saves but is hidden from the list (`m_bShowInScenarioMenu 0`), so the flagship is not offered twice.
  Its details text (`LF-Mission_Everon1989_Details`, pack table) is the first-run guide and says a
  local game is a preview. `validate-pack-missions` fails two listed headers on one world and a
  `m_sDetails` key that is no string-table row. `listen-host.mjs` boots the flagship as a listen host,
  passes the rules quiz and signs the form; proven on lf-cloud 2026-11-14 (identity arrived 3.6 s late,
  character spawned).
- ✅ **The flagship header runs real-server values** (refs #374, #1339, merged, in-dev): the flagship was a copy of
  the playtest header, and its world inherits DebugWorld's game mode prefab, which switches on the playtest
  helpers (test cars, bank and cash top-up, the "Send me to jail" board, the spawn hub globe, test dispatch calls,
  the near convoy drop). `LF_MissionHeader.m_bLF_PlaytestHelpersOff` (new, default 0) closes them all through
  `LF_MissionHeader.PlaytestHelper`, which `LF_SpawnLogic` and `LF_GameModeRoleplay.IsConvoyNearDrop` read;
  `LF_Everon1989.conf` sets it and no longer sets `m_bLF_FirstPlayerPolice` or `m_bLF_InstantPoliceApproval`, and its
  details text no longer promises top-up, test cars or auto-police. `LF_PlaytestTown.conf` stays hidden
  (`m_bShowInScenarioMenu 0`, which the Scenarios list honours) with the helpers on: `session up --mission playtest`,
  `host` and the e2e `--mission playtest` boot it. `validate-pack-missions` fails a listed header with the helpers
  on or a playtest-only flag set, and fails a hidden flagship. Test `host/playtest-helpers-off`. Not in the header
  (the server config): the 2-hour day and the join password (M4 decisions).
- ✅ **One-command playtest** (`tools\cli playtest [stop|status|shortcut]`,
  `Play Life Framework.cmd`, `tools/host/playtest.mjs`): fast-forwards the detached
  checkout `..\Life-Framework-playtest` to `origin/main` (only while no host runs),
  runs its `host start --wait`, then opens `ArmaReforgerSteam.exe` visibly with the
  persistent profile `%LOCALAPPDATA%\LifeFramework\player` (`LF_PLAYER_DIR`), waits
  for the host's `join allowed` line, and prints friend join lines. A running host
  is reused and joined with its own addon. `stop` closes the game, then `host stop`.
- ✅ **Playtest download** (2026-11-10, `tools/release/`, `.github/workflows/playtest-release.yml`):
  `LifeFramework-Playtest.zip` on the latest GitHub release holds the addon (tracked files only, plus a
  fresh `resourceDatabase.rdb`: `tools\cli playtest release` refreshes it and the workflow packs) and
  `Play Life Framework.cmd`, a PowerShell launcher that finds Arma Reforger through the Steam registry and
  `libraryfolders.vdf`, then offers *Play on my own* (`-server LF_PlaytestTown.conf -addr 127.0.0.1:2302`)
  or *Join the community server* (`-client` the address in `website/public/playtest-server.json`, set with
  `tools\cli playtest server <address>|off`). The homepage and /playtest link the stable
  `releases/latest/download/` URL. Selftest `tools/validation/release-selftest.mjs`. ⚠️ Windows only; the
  zip is not signed, so SmartScreen may warn on first run.

### `LF_AccountReplication` — `Feature/Account/LF_AccountReplication.c`
- ✅ **Client copy of the account** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): `LF_PlayerAccountManager` is
  server-only, so client-side offers read a replicated copy. Faction, duty,
  wanted, and police rank pack into one int (`LF_AccountStateBits`) carried by
  an `[RplProp]` on the player's `SCR_ChimeraCharacter`; the on-duty police
  count is an `[RplProp]` on `LF_GameModeRoleplay`. The server writes both
  from the account setters (`SetFaction`/`SetOnDuty`/`SetWantedLevel`/
  `SetPoliceRank`), on `OnPlayerSpawned`, and on a 10 s self-healing pass;
  `BumpMe` only on change. Late joiners get the values with the snapshot.
  `GetState(entity)` answers from the account on the server and from the
  replica on a client. Consumers: `LF_RobAction`, `LF_DutyAction`,
  `LF_ConfiscateAction`, `LF_ArmoryDrawAction`,
  `LF_RankPromoteAction`, `LF_BreachRepairAction`, and (since 2026-11-08) the
  jail actions: arrest, escort, escort-arrest, put in and take out of a
  vehicle. Before that their offers read the server-only account, so on a
  dedicated client an on-duty officer was never offered an arrest. Every
  server path still re-checks the real account; the jail cell and escort holds
  are checked on the server only. `tools/validation/validate-offer-checks.mjs`
  fails a `CanBeShownScript` / `CanBePerformedScript` that reads
  `LF_PoliceUtils.GetAccountForEntity` or `LF_PlayerAccountManager`.
- ✅ **Armory actions hidden from non-police** (2026-11-08, #157): Draw and
  Promote at `Roadmap_Police_Armory` are shown only to on-duty police
  (`LF_ArmoryDrawAction.ShownTo`, replicated state); a civilian no longer sees
  them greyed. The server still refuses in `PerformAction`. Test:
  `playtest/offers`.
- ⚠️ `LF_RobWeaponAction`, `LF_RobVehicleAction`, and `LF_Bank` still count
  police and read accounts directly in their offers; move them to
  `LF_AccountReplication` next.


- 🧪 (2026-11-16) **First spawn (#1964, chapter Story: The relocated #1015).** The character-creation flow marks a brand-new character (`LF_FirstSpawn.Mark`); its first spawn through `LF_SpawnLogic.CreateCharacter` reads and clears the mark (`LF_FirstSpawn.Consume`) and runs `OnFirstSpawn`, so the first-night rule (#1758) applies on first spawn only. A later spawn (after a death, a rejoin or a restart) and a character from an old save are never first. The mark is saved per character persistence id as the `firstspawns` payload (save version 37), so a character created and not yet spawned is still new after a restart. Tests `character/first-spawn`, `character/first-spawn-persist`.

### `LF_LogoutBody` — `Feature/Character/LF_LogoutBody.c` (#1973, in-dev)
- ✅ A logged-out character stays in the world as a body for 5 minutes (10 while downed and not
  dying): the server takes it from the leaving player just before the base game would delete it,
  knocks it out (`LF_Knockout`, so the vanilla inventory lets anyone loot it) and tags it Logged out
  (`IsLoggedOut`). Killing it is an ordinary death. It despawns when the window closes or the player
  rejoins; it is never saved, so a restart ends it. Where the rejoin lands stays
  `LF_DisconnectRules.RejoinFor` (#1972).
- ✅ No dupes: while the body exists, every item taken from or put on it re-captures the account's
  carried snapshot, and its death drops the snapshot, so the rejoin and a restart restore only what
  the body still held. Each body has its own inventory listener, so an item moved re-captures that
  body only, not every kept body (#2404, `rejoin/logout-body-dirty`, `perf/life-128`: 1 snapshot per
  item instead of one per kept body). `rejoin/logout-body` (WORLD) covers keep, loot, window, rejoin and death;
  `tools\cli session logout-body-proof` closes a real client and rejoins it.
- 🧪 (2026-11-16) **Rejoin takes the body back (#1975).** A player who rejoins while their logout body still stands gets that body back as it is: where it lies, cuffed, downed or robbed, with only what is left on it (`LF_LogoutBodyRejoin.TakeBack`, asked first by `LF_SpawnLogic.CreateCharacter`; the logout knockout ends, a real downed state stays). Nothing is restored or re-issued on it, so there is never a second copy. Jail and hospital (`LF_DisconnectRules.RejoinFor`) come first: the body ends and the normal spawn restores the snapshot, which already holds what the body carried. After the window there is no body and the rejoin rule (#1778) applies. A body killed while its player was away takes the ordinary death path (`LF_SpawnLogic.OnAccountDied_S`: snapshot dropped, next spawn wakes in hospital). Test `rejoin/logout-body-takeback`; `tools\cli session logout-body-proof` closes a real client and rejoins it into the same body.
- 🧪 (2026-11-17) **The taken-back body stands up (#2156).** The leaving client held the logout knockout ragdoll, so the server had no hold to release and the rejoiner lay prone and unconscious. `LF_LogoutBodyRejoin.EndLogoutKnockout` takes the hold over on the server so the knockout end releases the ragdoll and settles; the new owner, 2.5 s after it takes control (`SCR_CharacterControllerComponent.OnControlledByPlayer`), wakes a body that lies INCAPACITATED or ragdolled with nothing keeping it down (`WakeOnControl`: vanilla `RefreshRagdoll(0)` and `UpdateConsciousness`) and then stands it from prone (`StandUp`, `SetStanceChange`). A body downed for real, knocked out or held unconscious by vanilla stays down. Test `rejoin/logout-body-wake`; two-client proof: `logout-body-proof`, then the rejoiner reads `life=ALIVE stance=STAND`.
- ⚠️ The Logged out tag is server-side only (no name tag UI yet), and a downed logout still dies at once under `downedDies`, so the longer window applies only
  with that rule off.
- 🧪 (2026-11-17) **The window is in the owner config schema (#2158, from #2070).** `LF_LogoutBodyConfig` was never marked `configRoot`, so the schema generator skipped it. `logoutBody.windowSeconds` and `logoutBody.downedWindowSeconds` are now in `docs/config/config.schema.json` and the example (regenerated with `node tools/config/schema.mjs --write`), so the owner form offers them. A knockout of a logged-out body logs `<uid> (logged out)` (`LF_Knockout.Uid` through `LF_LogoutBody.UidOfBody`) instead of "npc". Test `rejoin/logout-body`. Merged, in-dev.
- 🧪 (2026-11-16) **Last position (#1777, first half of #640).** The server keeps where each character last stood: position, heading and the unix time it was taken (`LF_LastPosition`, one `LF_LastPositionRow` per character persistence id). It is taken when a player disconnects, before the game mode's disconnect cleanup, and for every connected player just before each save; a dead body is not taken. Saved as the `lastpos` payload (save version 39). Nothing spawns at it yet; that is the second half of #640. Test `spawn/last-position`.
- 🧪 (2026-11-17) **A downed logout's hospital rejoin survives a restart (#2101, Arrival #1007, from #2063).** A player who logs out downed dies at the logout and wakes at the hospital with the bill; that pending wake used to be session state, so a restart before the rejoin put them at the plaza with no bill (an escape hatch, maintainer decision 2026-11-15). `LF_DisconnectRules.Apply` now records the logout (`LF_HospitalRejoin.Record`, uid and logout time) and the `hospitalrejoin` payload (save version 40) carries it while the wake is pending (a logout whose kill did not happen is not saved); a load marks the wake again (`LF_HospitalRespawn.MarkDied`), so the rejoin wakes at a bed and writes the bill once. A row is dropped by the next save once the wake is taken. Test `persist/hospital-rejoin`; `wt e2e` for the real restart. Logs `[LFDebug:HospitalRejoin]`. Merged, in-dev.
- 🧪 (2026-11-16) **Rejoin at the saved spot (#1778, second half of #640).** With `m_bRejoinAtLastPosition` on (`LF_Respawn.conf`, off by default: vanilla spawn), a returning character spawns at its `LF_LastPosition` row instead of the spawn point (`LF_RejoinSpot.Resolve`, called from `LF_SpawnLogic.GetCreationPosition`). Order: a jail return (a prisoner is moved into the cell after the spawn, `LF_Jail.OnPlayerSpawned`), the hospital wake (`LF_DisconnectRules.RejoinFor`: jail, hospital or in place), the saved spot, the spawn point. Only the first spawn of a connection is a rejoin; a respawn after a death takes the normal spawn. A spot older than `m_iRejoinMaxAgeHours` (12; 0 = no limit), under the terrain, in water, inside geometry (no standing head room), inside the jail, in a restricted zone (`LF_Zones` rule set that keeps police out) or inside a locked house falls back to the spawn point, with a `[LFDebug:Rejoin]` warning naming the reason. A character with no row (a new arrival) takes the spawn point, so the arrival flow is untouched. The setting is logged once when the config loads (`[LFDebug:Respawn] rejoinAtLastPosition=...`). Test `spawn/rejoin-spot`.
- 🧪 (2026-11-16) **First-spawn bed (#1965, chapter Story: The relocated #1015).** A brand-new character (its `LF_FirstSpawn` mark still pending, #1964) wakes on a bed marked `LF_FirstSpawnBedComponent` (prefab `Prefabs/Medic/LF_FirstSpawnBed.et`, the hospital bed's model, its own list apart from the hospital ward). `LF_SpawnLogic.GetCreationPosition` asks `LF_FirstSpawnBeds.Resolve` after the hospital wake and before the saved spot: the first bed with no living character within a metre (`PickFree`) takes the body, so the second new character lies on the next bed; every bed taken, no bed placed, or any spawn that is not a first spawn (a respawn after a death, a rejoin, an existing save) takes the normal spawn point. The owner plays the hospital get-out-of-bed animation with no bill and steps down beside the bed (`LF_HospitalWake`, `StepOffBed` also knows these beds). First spawn only (#1758). Fixture: two beds in the grid cell `first-night` (`Grid_first_night.layer`); the town building (a hostel, or a homestay with a drumming host) is a later town pass after #1958. Test `spawn/first-spawn-bed`. Logs `[LFDebug:FirstSpawnBed]`. Merged, in-dev.
- 🧪 (2026-11-16) **The flagship rejoins players at the saved spot (#1779, Arrival #1007, refs #641).** `LF_RespawnConfig` reads an owner `respawn` section over `LF_Respawn.conf` (every field: `wakeInHospital`, `billAmount`, `wakeLoiterType`, `wakeRootMotion`, `wakeLieSeconds`, `rejoinAtLastPosition`, `rejoinMaxAgeHours`; a key left out keeps the .conf value, a bad value is refused with the kept value in the log). `server/flagship/config.json` sets `rejoinAtLastPosition` true with a 12 hour stale limit; the core default stays off. The boot line `[LFDebug:Respawn] rejoinAtLastPosition=1 maxAgeHours=12` is what `config-boot-proof` reads (fixture `flagship-arrival`). A new character has no saved row, so the arrival flow is untouched. Test `config/respawn-grace`.
- 🧪 (2026-11-16) **The first-night room (#1966, chapter Story: The relocated #1015).** The hostel of #1958 (`MountainHotel_01_green`) stands dressed in the grid block `first-night-room` (3 x 2 cells at c7r5, `Grid_first_night_room.layer`): three `LF_FirstSpawnBed` beds in the upstairs dormitory (two between the bunk rows, one in the west room; each has its own `m_vStandOffset` so the woken body steps down clear of the bunks), two `Nightstand_01` night tables, a `CabinetMetal_01_grey_V1` locker at the end of the east alcove and a `Suitcase_01` beside the west bed. The hotel's own furniture set brings the bunks, tables and wardrobes. Base-game props only, nothing 1989 would not have had in a guesthouse. Placed with `grid place` and `place ground --y-hint 0.5` (the upstairs floor is 3.4 m above the hotel pivot, the hint keeps the probe under the ceiling). A fixture for the town card #2125, which copies the entries by their offsets from `FirstNightRoom_Hotel` (pivot 127.33, 0, 100.739; the beds, props and suitcase are the entity coordinates minus that pivot). Check: `tools\cli look grid:first-night-room` (the interior needs a camera inside; the PR pictures came from one), `audit placement --grid`. A first spawn on these beds is the manual check of #1965. Merged, in-dev.
- 🧪 (2026-11-17) **The hostel, pass 1 BUILD (#2290, epic #2201).** The pack hostel (`Prefabs/Buildings/Hostel/Hostel.et`, the town house `House_Town_E_2I02`, pack grid claim `hostel` at c14r0) gets its systems: two `LF_FirstSpawnBed` first-night beds on the first floor (one in each dormitory, each with its own `m_vStandOffset` clear of the bunks and the sofa), two `BunkBed_02` in the west dormitory, a reception counter (`Desk_01_long`, the guest register, a phone) facing the front door, four `CabinetMetal_01_grey_V1` lockers and cases in the luggage hall, and one shadowless spot per room (6 lights, 0 shadow casters, MEDIUM budget: 149 of 300 entities). The washrooms and kitchen come with the shell; the attic stays shut. Existing systems only, no new components. Rows are in building space, written by `node tools/buildings/hostel/fitout.mjs` (`--check` gives the same bytes, `--report` the counts) as the `HostelKit_*` block of `Grid_hostel.layer`, turned through the building's yaw read from `Grid_hostel_Main`. Check: `node tools/buildings/hostel/looks.mjs --set rooms --run`, `tools\cli audit placement --grid --strict`. Merged, in-dev.
- 🧪 (2026-11-18) **The first night on the island: Saint-Philippe hostel slot (#2321, #1758).** The island mapping puts the hostel in town house b14 (`House_Town_E_2I02`). Kit `tools/town/kits/hostel.json` (extracted from the grid fit-out `Grid_hostel.layer`, `building.mode: override` on `Hostel.et`) carries the two `LF_FirstSpawnBed` beds (role `interactable`, each with its own `m_vStandOffset`), two bunks, the reception desk with register and phone, three suitcases (no locker yet); `town fit saint-philippe hostel` writes `Place_saint-philippe_hostel.layer` (place kind `HostelBed`, claim `saint-philippe-hostel.json`, destruction hooked). The four lockers of the grid fit clipped the island host wall or stood outside it, and were left out, and the six room lights are not in the kit yet. No logic change: a brand-new character still wakes on the first free bed through `LF_FirstSpawnBeds.Resolve` (tests `spawn/first-spawn`, `spawn/first-spawn-bed`, `spawn/first-spawn-persist`), every later spawn takes the normal point. A DRAFT town-draft: nothing merges before the maintainer approves the pictures. Merged after review, in-dev.
- 🧪 (2026-11-19) **Waking in a bed no longer floats above it (arrival beds, hospital beds).** The bed wake loiter (type 7) is the base game's sit-on-the-bed-and-stand-up animation, made for a character whose feet are on the floor beside the bed, facing it. The body used to spawn on the bed origin; physics pushed it onto the mattress, so the whole pose played half a metre too high (sitting, then standing in the air above the bed), and on a crowded bed the get-up carried it 5.7 m off and the 1.5 m step-off search gave up, leaving it 0.86 m above the floor. Now `LF_BedWakePose` gives the wake transform: feet on the floor probed from 0.3 m above the bed origin (desk and sill tops beside the bed are not floor), 0.4 m out from the mattress side the stand offset names, facing the bed, at the first spot along that side (then the other side) with room for a standing body (`BodyFits`, players' bodies seen through). The stand point is floor-probed too. The owner remembers its bed's stand spot at the wake start (`LF_HospitalWake.RememberBed`) and moves the body only when the get-up ends off the floor or more than 2.5 m away (`NeedsStepDown`). The patient laid in a hospital bed by a medic still uses the mattress (`GetWakeTransform`); the respawn uses `GetSitTransform`. Harness verb `bedcheck` measures every first-spawn and hospital bed against the floor under it. Test `spawn/bed-wake-pose`. Merged, in-dev.
- 🧪 (2026-11-19) **The Saint-Philippe hostel has a sign.** The first building a newcomer sees had a bare front: only the butcher's plate on the next house read as lettering. The `hostel` kit (`tools/town/kits/hostel.json`) gains a row `hostel-fascia`, an `LF_Sign_Fascia` seated flush on the front wall over the arched door (centred on it, 4.2 m wide, clear of the string course and the windows), painted bottle green with cream capitals like the bakery and butcher plates. Its sign def `hostel` is its own pack file `Configs/Signs/LF_Signs_Everon1989/LF_Signs_Hostel.conf` with the title `Hostel` (`LF-Sign_Hostel_Title`, `_What`, `_How` in the pack table). `town fit saint-philippe hostel --refit` wrote the row into `Place_saint-philippe_hostel.layer`.
- 🧪 (2026-11-21) **Every spawn and teleport point is measured (#3912).** `LF_SpawnPointAudit` (Feature/Character/Spawning) lists every point a script puts a player body at: first-spawn bed wake and stand points, hospital bed wake and stand points, every authored house spawn point and the outside point, the jail cell and the release point, and each role spawn point. Each is measured the way the arrival bed was: the gap to the floor under it (all layers, characters and trees seen through; over 5 cm fails), whether the point stands on a prop mesh (furniture-sized, 0.15 to 1.5 m tall), and whether a standing body (0.6 m wide, 1.55 m tall above 0.15 m) fits. Harness verb `spawncheck` prints one `[LFDebug:SpawnPoint]` line per point and answers `ok points=<n> worst=<m>` or names the first three failures; test `spawn/point-floor` sweeps whatever world is loaded and fails a new floating or crowded point. Fixed at the source: a bed's stand point (`StandOnFloor`) overlapped the bed by 3 cm and a nightstand beside it, so it now falls back to the wake point the sit search already picked with room; the third dormitory bed of the grid first-night room, boxed in by the hotel's interior, is gone from the fixture. On the Saint-Philippe island 7 of 9 points pass; the two clinic bed points fail for want of room and stay with #3894. Quests and events place no player: the only other teleports are the debug grid teleport (a grid cell by design) and NPC spots (residents are floor-snapped, #3885 and #3912). Merged, in-dev.
- 🧪 (2026-11-21) **The clinic's two respawn beds have standing room (#3894).** The Saint-Philippe clinic was rebuilt through `town author` / `town stamp`: the two `LF_HospitalBed` rows moved into the north-west ward (the old spots straddled an inner wall and one stood against the other), 1 m of aisle between them, and the pack furniture's infusion stand that stood in that aisle is hidden by the kit's clear list. `spawncheck` on the island now answers `ok points=9`. Merged, in-dev.
- 🧪 (2026-11-22) **The hostel is a stamped kit (#3888).** The Saint-Philippe hostel was rebuilt through `town author` / `town stamp` around the island building's own prefab: the reception desk (it overlapped the host's bookshelf and wardrobe) and its notebook, phone table and ashtray are gone, the second upstairs bunk that stood in a door swing is gone, and the HOSTEL fascia hangs over the door where the audit can reach it. The two first-spawn beds did not move: `spawncheck` on the island answers `ok points=9` and `bedcheck` `ok beds=4`. Merged, in-dev.
- 🧪 (2026-11-27) **A patient put in a clinic bed lies on the mattress, not under it (#5153).** "Put patient in bed" laid the patient at the bed origin (`GetWakeTransform` = origin + a zero offset), which is the floor under the frame: on the Saint-Philippe clinic bed (two beds side by side, a wall near) the patient woke prone under the frame and the step-down found no room. `LF_HospitalBedComponent.GetWakeTransform` now gives `LF_BedWakePose.LayPoint`: the middle of the bed's real bounds, the height from a probe down onto the bed itself (bounds top when the probe misses). The sit/step-down search (`SitCandidates`) tries both long sides (the stand offset's first), then the foot and head ends, then the same three wider rings (0.35 m each), never a point whose body overlaps the bed (`InsideBed`). The medic's step-down (`LF_HospitalBedComponent.FindStandSpot`) leaves the patient on the mattress, with a `[LFDebug:Respawn]` warning that names the bed's prefab, when no spot has room, instead of moving them under or into the frame. Town layers are unchanged. Tests `spawn/bed-spot-search` (pure geometry), `spawn/bed-wake-pose`; proof `session.mjs clinic-bed-proof` (`up --mission island --clients 2 --profile full`): on the Saint-Philippe clinic bed the patient lay 0.40 m over the floor, then stood on the floor beside the bed. Merged, in-dev.
