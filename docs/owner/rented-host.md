# Run the server on a rented game-server host

This is the template for a server rented from a game-server host with a control panel. It covers the
same three things as [install a server from zero](install.md) (the framework, the pack, the first boot)
but for a panel: which files go where, the Workshop IDs to enter, the startup parameters, the log lines
that mean success, and the backups.

**Host: 0grind, with 16 player slots.** Decision 2026-11-15: the flagship alpha runs on a rented 0grind
server (Ryzen 9 7950X3D, 16 GB RAM) with 16 slots, so `game.maxPlayers` is 16. The maintainer's trial (#378)
checks the panel. Every 0grind step below is marked *unverified until #378*: the panel's exact screens and
fields are not checked yet. Once the trial
reports, the marks come off the steps that held and the page is split into one page per host (#379).
Any other host follows the same steps with its own panel names.

Do the install first and read the boot log with the [log lines guide](log-lines.md). This page does not
repeat the install steps or the log lines; it links them.

## What you need from the host

- A dedicated server for Arma Reforger with 16 player slots (the base game's server, Steam app `1874900`).
- The two Workshop items. The core is `4550A2B656E1B569` (Life Framework) and the pack is
  `B1E84D5687E11724` (Life Framework: Everon 1989). The core goes first. The pack depends on it.
- UDP `2001` (game) and UDP `17777` (the A2S query the server browser uses) open to players.
- A way to edit files: the panel's file manager, or FTP or SFTP. *Unverified until #378.*

A `-config` server downloads only Workshop items that are published. Until the first upload lands, the
boot fails on a missing item; see the Problems table in [install a server from zero](install.md).

## 1. Where `server.json` and `config.json` go

| File | What it is | Where it goes on the host |
|---|---|---|
| `server.json` | The base game's server file: name, ports, admin password, `game.mods`, `scenarioId` | The server folder, the one the `-config` argument names |
| `config.json` | This mod's owner config (shop prices, rules, persistence, day cycle) | The engine profile: `<profile>\profile\LifeFramework\config.json` |

The owner config is optional. A server with no `config.json` runs on the shipped defaults and logs the
`no owner config` line. See [where config.json goes](install.md#6-where-configjson-goes).

Keep the two `game.mods` entries and `scenarioId` exactly as the example server sets them. Change the
name, the ports, the maximum players (16 on the 0grind alpha server, and the most the 1989 town is sized for), the admin password and whether the server
is listed. Admin and join passwords stay out of any file you share.

On the maintainer's own machine, `tools\cli host config apply` writes the profile's `config.json` and
`server.json` from `server/flagship` plus the host's `secrets.json`. It refuses while the server runs.
A panel host has no `secrets.json`; enter the passwords in the panel's settings. *Unverified until #378.*

- [ ] `server.json` is in the server folder and names both Workshop items in `game.mods`.
- [ ] `config.json` is in `<profile>\profile\LifeFramework\`, or the file is absent on purpose.
- [ ] `tools\cli config-check <file>` passes on a copy of `config.json` (a checkout is needed).

## 2. Enter the Workshop IDs

On the panel, add the two IDs to the mods list, core first. Some panels take the IDs in a Workshop field
instead, and some read them from `game.mods` in `server.json`. Either way the mods must match the list
above. *Unverified until #378.*

- [ ] The panel shows `4550A2B656E1B569` and then `B1E84D5687E11724`, in that order.
- [ ] The first start downloads both. Players get them on join; they install nothing.

## 3. Startup parameters

The launch line is the same as in [install a server from zero](install.md#4-start-it), with the path of
the panel's own server folder:

```
ArmaReforgerServer -config <server folder>/server.json -profile <server folder>/profile -maxFPS 60
```

- `-config` names `server.json`. Without it the server starts on the base game's defaults.
- `-profile` names the folder the save, logs and config live under.
- `-maxFPS 60` caps the server's frame rate.
- `-loadSessionSave` is added **after the first boot**, once a save exists. Leave it out on the first
  start. Once a save exists and the flag is missing, the server starts the town fresh and logs
  `started a NEW session while N save point(s) exist` ([row 1 of the log table](log-lines.md)).

Some panels run a start script of their own. If yours does, add the four parameters to its launch
line. *Unverified until #378 for 0grind.*

- [ ] The launch line has `-config`, `-profile` and `-maxFPS 60`.
- [ ] After the first boot, `-loadSessionSave` is on the line and stays there.

## 4. Success lines

A good boot shows the lines in [the lines of a good boot](log-lines.md#the-lines-of-a-good-boot), in that
order, in the console log under the profile's `logs\` folder. The panel's console view may show the same
text; the log file is the record. A line with `SCRIPT (E)`, `VME` or `Virtual Machine Exception` is a
failure. [`tools\cli server-doctor`](install.md#9-check-a-log-with-server-doctor) reads a log file and
gives a `PASS` or `FAIL` verdict.

- [ ] `Entered online game state.` appears within about 15 seconds of the engine start.
- [ ] `save loaded: version N` appears on a restart with `-loadSessionSave`.

## 5. Backups

Back up the profile folder. It holds the saves, the roster and the config. On the maintainer's machine
the commands do this:

- `tools\cli host backup` copies the profile into `backups\<time>`.
- `tools\cli host restore <folder>` puts a backup back. It keeps what it replaces under
  `backups\<time>-replaced-by-restore`.
- `tools\cli host prune` keeps the newest 20 backups.
- `tools\cli host drill <backup>` boots a backup on a scratch profile on a pool machine and passes only
  if the save loads with its accounts. Run it on a backup before you rely on it.

On a panel, use the panel's backup or snapshot feature, and copy the profile folder to somewhere else
once a week. *Unverified until #378.*

Stop the server before the copy, so the shutdown save is written. The log shows
`save completed (SHUTDOWN)` line. Stop with the panel's stop button, or with
`tools\cli host stop` on the maintainer's machine.

- [ ] A backup folder exists before the first real session.
- [ ] `tools\cli host drill <backup>` passes on one backup (maintainer's machine only).

## 6. Keep it up

- **Crashes.** `tools\cli host watch` restarts a crashed server, keeps the crash log under
  `backups\<time>-crash`, and stops restarting after three crashes in ten minutes. On a panel, turn on the
  panel's restart-on-crash setting. *Unverified until #378.*
- **Reboots.** `tools\cli host autostart install` registers a Windows scheduled task that runs
  `host watch` at logon. On a panel the host restarts the server; check that the restart keeps
  `-loadSessionSave`. *Unverified until #378.*
- **Ports.** `tools\cli host ports` checks the game and A2S ports are free, or held by the server, and
  prints the router rules to forward.
- **Box check.** `tools\cli host boxcheck` measures a machine you control against the box spec in one
  run. A panel host has no shell, so use `host ports` where you have one and the success lines above.
- **Listing.** `tools\cli host listing <address>:<a2s port>` asks the running server what the server
  browser shows (name, map, maximum players) and compares it with `server/flagship/server.json`;
  `host listing --remote 0grind` does it for the rented flagship. Then join from a machine off your
  network.

## 7. 0grind steps, one line each

| Step | How | Status |
|---|---|---|
| Account and server slot | The 0grind dashboard | Unverified until #378 |
| Install the Arma Reforger Server build | The panel's game installer | Unverified until #378 |
| Upload `server.json` and `config.json` | File manager | Unverified until #378 |
| Enter the two Workshop IDs | Mods field | Unverified until #378 |
| Set the launch parameters | Startup settings | Unverified until #378 |
| Open UDP 2001 and 17777 | Network settings | Unverified until #378 |
| Check the boot log | Console or log file | Unverified until #378 |
| Backup and restore | Panel backup | Unverified until #378 |
| Restart after a crash | Panel restart setting | Unverified until #378 |

When a step is verified on 0grind, the maintainer removes its status note in the same change.
