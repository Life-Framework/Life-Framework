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
  characters. Read with `Total(uid)`, `JobSeconds(uid, job)`, `CollectJobs`.
  Persisted as the `playtime` payload (save version 26, `LF_PlaytimeRecord`).
  `account/playtime` and the `e2e/pass*` pair cover it.

### `LF_PersistenceManagerComponent` — `Persistence/` (verified 2026-08-30)
- ✅ Server-side save orchestration on the game-mode entity: autosave timer
  (one AUTO slot per playthrough via `LF_SaveSelection`), manual saves,
  save-cache scan, load/re-apply paths, wipe. Every path passes
  `PassesSaveGates()` (manager present, saving allowed, persistence system
  active) and logs `LF_Debug` under `Persistence`.
- ✅ A continued save that never reaches the store read (written before the 2026-11-09 EL_ to LF_ rename, pre-release save reset) logs one `LF_Debug.Warn` "save predates the LF_ rename; starting fresh" at `ACTIVE` and runs fresh, no partial load (verified 2026-11-09 with an old save on the test server; docs/persistence.md "Save reset"). Since #980 the line is an `LF_Debug.Error` "continued save never reached the LF store" (a game update that drops saves looks the same) and autosave pauses so the save on disk is kept.
- 🟡 Game updates never cost a save (#980): each save carries a build stamp (`buildstamp` payload, save version 32, `LF_SaveStamp`; the engine's `meta-info.json` `m_sGameVersion` too). A load from another build warns with both builds; a load that loses a payload logs `save load damaged` and pauses autosave. `tools\cli host start` refuses a save from another build until `tools\cli host preflight` passes: back up, boot the save on the installed build, check `save loaded:`, stop, and restore the backup on any failure (`host restore` by hand). Proof: `persist/save-build-stamp`, `tools\cli e2e --old-build`, a live preflight on a faked 1.8.0.13 save. Ops: `docs/ops/game-updates.md`.
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
- ✅ **Playtest host** (`toolscli host start|stop|status|backup`, `tools/host/host.mjs`;
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
  `LF_ConfiscateAction`, `LF_OpenPoliceMenuAction`, `LF_ArmoryDrawAction`,
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
  the body still held. `rejoin/logout-body` (WORLD) covers keep, loot, window, rejoin and death;
  `tools\cli session logout-body-proof` closes a real client and rejoins it.
- ⚠️ The Logged out tag is server-side only (no name tag UI yet), the window is not owner config
  yet, and a downed logout still dies at once under `downedDies`, so the longer window applies only
  with that rule off.
- 🧪 (2026-11-16) **Last position (#1777, first half of #640).** The server keeps where each character last stood: position, heading and the unix time it was taken (`LF_LastPosition`, one `LF_LastPositionRow` per character persistence id). It is taken when a player disconnects, before the game mode's disconnect cleanup, and for every connected player just before each save; a dead body is not taken. Saved as the `lastpos` payload (save version 39). Nothing spawns at it yet; that is the second half of #640. Test `spawn/last-position`.
- 🧪 (2026-11-16) **Rejoin at the saved spot (#1778, second half of #640).** With `m_bRejoinAtLastPosition` on (`LF_Respawn.conf`, off by default: vanilla spawn), a returning character spawns at its `LF_LastPosition` row instead of the spawn point (`LF_RejoinSpot.Resolve`, called from `LF_SpawnLogic.GetCreationPosition`). Order: a jail return (a prisoner is moved into the cell after the spawn, `LF_Jail.OnPlayerSpawned`), the hospital wake (`LF_DisconnectRules.RejoinFor`: jail, hospital or in place), the saved spot, the spawn point. Only the first spawn of a connection is a rejoin; a respawn after a death takes the normal spawn. A spot older than `m_iRejoinMaxAgeHours` (12; 0 = no limit), under the terrain, in water, inside geometry (no standing head room), inside the jail, in a restricted zone (`LF_Zones` rule set that keeps police out) or inside a locked house falls back to the spawn point, with a `[LFDebug:Rejoin]` warning naming the reason. A character with no row (a new arrival) takes the spawn point, so the arrival flow is untouched. The setting is logged once when the config loads (`[LFDebug:Respawn] rejoinAtLastPosition=...`). Test `spawn/rejoin-spot`.
- 🧪 (2026-11-16) **First-spawn bed (#1965, chapter Story: The relocated #1015).** A brand-new character (its `LF_FirstSpawn` mark still pending, #1964) wakes on a bed marked `LF_FirstSpawnBedComponent` (prefab `Prefabs/Medic/LF_FirstSpawnBed.et`, the hospital bed's model, its own list apart from the hospital ward). `LF_SpawnLogic.GetCreationPosition` asks `LF_FirstSpawnBeds.Resolve` after the hospital wake and before the saved spot: the first bed with no living character within a metre (`PickFree`) takes the body, so the second new character lies on the next bed; every bed taken, no bed placed, or any spawn that is not a first spawn (a respawn after a death, a rejoin, an existing save) takes the normal spawn point. The owner plays the hospital get-out-of-bed animation with no bill and steps down beside the bed (`LF_HospitalWake`, `StepOffBed` also knows these beds). First spawn only (#1758). Fixture: two beds in the grid cell `first-night` (`Grid_first_night.layer`); the town building (a hostel, or a homestay with a drumming host) is a later town pass after #1958. Test `spawn/first-spawn-bed`. Logs `[LFDebug:FirstSpawnBed]`. Merged, in-dev.
- 🧪 (2026-11-16) **The flagship rejoins players at the saved spot (#1779, Arrival #1007, refs #641).** `LF_RespawnConfig` reads an owner `respawn` section over `LF_Respawn.conf` (every field: `wakeInHospital`, `billAmount`, `wakeLoiterType`, `wakeRootMotion`, `wakeLieSeconds`, `rejoinAtLastPosition`, `rejoinMaxAgeHours`; a key left out keeps the .conf value, a bad value is refused with the kept value in the log). `server/flagship/config.json` sets `rejoinAtLastPosition` true with a 12 hour stale limit; the core default stays off. The boot line `[LFDebug:Respawn] rejoinAtLastPosition=1 maxAgeHours=12` is what `config-boot-proof` reads (fixture `flagship-arrival`). A new character has no saved row, so the arrival flow is untouched. Test `config/respawn-grace`.
