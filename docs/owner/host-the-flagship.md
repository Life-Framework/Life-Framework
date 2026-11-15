# Host the flagship server

This page takes the maintainer from "0.1.0 is not on the Workshop yet" to strangers joining
**Life Framework | Everon 1989 (Alpha)** from the in-game Server Browser. Do the steps in order. It runs
the server on your own Windows PC from a worktree of this repo. The flagship itself runs on a rented
0grind server, driven from this PC ([the 0grind section](#0grind-the-rented-flagship)). Any other panel
host is [a different page](rented-host.md), and a server without the repo is [the install guide](install.md).

## Why there are two host modes

`tools\cli host start --flagship` is the local playtest host. It boots the scenario straight from your
checkout with `-noBackend`. A server started that way is **not** in the Server Browser and has **no join
password** (the game checks the password through Bohemia's backend, and `-noBackend` turns the backend
off, #1520). A normal player cannot join it. Use it to test on your LAN.

`tools\cli host start --public` is the real server. It boots from the profile's `server.json`
(`-config <profile>\server.json -profile <profile>`, with no `-noBackend` and no `-addons`). The backend
lists it in the Server Browser and checks the join password. The server downloads the two mods named in
`game.mods` from the Workshop, so this mode only works **after 0.1.0 is published**. `--flagship`
does not switch it on by itself, so a local playtest never goes public by mistake.

Before it launches anything, `host start --public` checks five things and stops if one fails:

- `game.mods` holds two real Workshop IDs (not `0000000000000000` or another placeholder), and they
  are the IDs you confirmed after publishing (step 3).
- `game.password` is set.
- `game.scenarioId` is the flagship `{C638C03B775C31BB}Missions/LF_Everon1989.conf`.
- `game.visible` is not `false`.
- The host's files match `server/flagship/`, so `host drift` is clean.

An empty `game.admins` or `game.passwordAdmin` only prints a warning.

## 1. Publish 0.1.0

Follow [the 0.1.0 upload guide](../release/workshop-upload-0.1.0.md): publish the core first, then the
pack, both **Unlisted** at version `0.1.0`. Run its Workshop boot check
(`tools\cli workshop boot-check 4550A2B656E1B569 B1E84D5687E11724`) and wait for `stage=pass`. A failure
there will also stop the public server.

## 2. Get the two Workshop IDs

The Workshop ID of each item is the address of its page:

| Item | Page | ID |
|---|---|---|
| Life Framework (Core) | `https://reforger.armaplatform.com/workshop/4550A2B656E1B569` | `4550A2B656E1B569` |
| Life Framework: Everon 1989 | `https://reforger.armaplatform.com/workshop/B1E84D5687E11724` | `B1E84D5687E11724` |

The Workshop keeps each project's `.gproj` GUID as its ID, so the table should already be right. Open
both pages after publishing and read the ID from the address bar anyway. If a page shows a different ID,
change `game.mods` in `server/flagship/server.json` (and `server/example/server.json`) in a PR first, core
first.

## 3. Confirm the IDs in config

The IDs are already in `server/flagship/server.json`. Confirm them for this host after publishing.
All commands run from your host worktree (`tools\cli wt new host`, then `cd ..\Life-Framework-ws-host`),
never the main checkout:

```
tools\cli host config apply --mods 4550A2B656E1B569,B1E84D5687E11724 --admins-pending
```

`--mods` refuses IDs that differ from `server/flagship/server.json`. When they match, it records them in
`<host folder>\workshop.json` (the host folder is `%LOCALAPPDATA%\LifeFramework\host`), and
`host start --public` refuses to run without that file. `--admins-pending` is for this first run only;
step 4 explains why.

## 4. Set the password and your admin ID

1. **The join password.** Create `%LOCALAPPDATA%\LifeFramework\host\secrets.json`, outside the repo:

   ```json
   {
     "game": { "password": "<the alpha password>", "passwordAdmin": "<a long admin password>" },
     "staffWebhookUrl": "",
     "announceWebhookUrl": ""
   }
   ```

   Never put a password in `server/flagship/`. The repo is public, and `host config apply` refuses a
   repo file that holds one. [Run a password alpha](password-alpha.md) covers handing the password out.

2. **Your admin ID.** The game gives admin powers to the account IDs in `game.admins`, and
   `host config apply` fills that from the administrators in `server/flagship/staff.txt`. You do not know
   your ID yet. It is the `IdentityId` the server logs when you join, and only a backend-listed server
   logs the real one (a `-noBackend` host logs all zeros). So the first public boot runs with you as
   `#pending` (the `--admins-pending` above), and you are admin from the second boot on. Step 8 covers
   that. Until then, `#login <passwordAdmin>` in chat gives you admin for the session.

3. Apply again whenever you change `secrets.json` or `staff.txt` (stop the host first):

   ```
   tools\cli host config apply --admins-pending
   ```

   It prints what changed, masks every secret, and writes `<host>\profile\server.json` and the owner
   `config.json`.

## 5. Open the ports

```
tools\cli host ports --config %LOCALAPPDATA%\LifeFramework\host\profile\server.json
```

It checks that the ports are free and prints the router rules. On your router, forward to this PC's LAN
address:

| Port | Protocol | What |
|---|---|---|
| 2001 | UDP | the game (`bindPort` and `publicPort`) |
| 17777 | UDP | A2S, which the Server Browser and `host players` query |

Allow `ArmaReforgerServer.exe` through Windows Firewall when it asks on the first start.

## 6. Start it

```
tools\cli host start --public --wait
```

It runs the checks listed at the top. When they pass, it backs the saves up and launches the server. The
first boot downloads both mods into `<host>\workshop`, so it can take several minutes (the wait is 15
minutes; pass `--timeout <s>` for a slower line). It ends with `host: up (pid N)`. If a check fails,
it prints `FAIL ...`, says what to fix, and launches nothing.

Keep it up with one of these:

- `tools\cli host watch --flagship --public` restarts a crash and the scheduled 06:00 and 18:00 closes.
  It ends after `host stop`.
- `tools\cli host autostart install --public` registers a Windows logon task that runs
  `host watch --flagship --public` hidden, so the server comes back after a reboot.
  `host autostart status` shows it.

Without `--public`, `watch` and `autostart` start the local `-noBackend` host. To switch back, run
`host autostart install` without the flag; it replaces the task.

## 7. Check the listing

From a machine outside your network (a phone hotspot is enough), or ask a friend:

```
tools\cli host players <your public IP>:17777
```

It prints the server name, map and player count over A2S. Then open the game and look for the server in
the Server Browser. *(planned, #1461)* `tools\cli host listing` will compare what the browser shows with
`server/flagship/server.json`.

## 8. Your first join, and your admin ID

1. Join as a player (next section) with the password.
2. On the host PC:

   ```
   tools\cli host attendance
   ```

   Every player row ends with `id <uuid>`. Yours is the row with your name.
3. Put it in the roster and open a PR for `server/flagship/staff.txt`:

   ```
   tools\cli staff roster promote Jasper <your uuid>
   ```

4. After it merges: `tools\cli host stop`, `tools\cli host config apply` (no `--admins-pending` now), and
   `tools\cli host start --public --wait`. You are in `game.admins` from now on.

## How a player joins

- **Server Browser:** start Arma Reforger, open **Multiplayer**, search for
  `Life Framework | Everon 1989 (Alpha)`, pick it and enter the password. The game downloads both mods
  the first time.
- **Direct connect:** in the Server Browser, use **Direct Join** with `<your public IP>:2001` and the password.

The password goes out on Discord, not in public posts ([password alpha](password-alpha.md)).

## The first announcement

Draft it, read it, then post it:

```
tools\cli host announce --at 2026-11-20T19:00 --build 0.1.0
tools\cli host announce --at 2026-11-20T19:00 --build 0.1.0 --channel <announcements channel id> --send
```

Only the maintainer runs `--send`. The [session runbook](../ops/session-runbook.md) covers each session
after this one.

## Problems

| You see | Do this |
|---|---|
| `FAIL ... Workshop IDs are not confirmed` | Step 3 |
| `FAIL ... placeholder, not a published Workshop ID` | Fix `game.mods` in `server/flagship/server.json` (PR), then step 3 |
| `FAIL ... game.password is empty` | Step 4.1, then apply |
| `FAIL ... drifted from server/flagship` | `tools\cli host drift` lists the fields; `tools\cli host config apply` |
| `refused: ... administrator has no uid yet` | Add `--admins-pending`, or step 8 |
| The server exits during startup | Read the log `host status` prints. A mod download error means an ID is wrong or an item is Private |
| It runs but nobody finds it | Step 5 ports, step 7 from outside your network |

## 0grind: the rented flagship

The flagship runs on a rented 0grind Arma Reforger server: service `298627`, Linux, Frankfurt, 16 slots,
`144.76.90.77`, game port `2023`/UDP, A2S `3023`, RCON `5023`. There is no shell on the box. The panel
(app.0grind.io) and `tools\cli host remote 0grind <cmd>` drive it from this PC through 0grind's API.
Steps 3 to 6 above are for a server on your own PC; on 0grind, `config push` replaces them.

### One-time API access

The API checks two things: the token, and the IP the call comes from. A wrong one of either returns
`401 Unauthorized`.

1. On app.0grind.io open **Account**, then **API Access** (only the main account sees it). Copy the
   **API Token** into `%USERPROFILE%\.life-framework\0grind-token`, outside the repo. The token on a
   service's **Integrations** page is a different one: it only feeds Server Admin Tools events.
2. In the same section, add this PC's public IPv4 to **Allowed IPv4 Addresses** (at most 10) and save.
   `curl -s https://api.ipify.org` prints the address. A home line can change it; when the commands
   start failing with 401, check it first.
3. `tools\cli host remote 0grind status` should then print the service status.

The tools never print the token or a password, and they keep to 0grind's limit of 1 request per second.

### Panel or tools

| Job | Where |
|---|---|
| Status, start, stop, restart | `host remote 0grind status`, `start --yes`, `stop --yes`, `restart --yes` (or the panel's buttons) |
| Name, password, admins, scenario, mods | `host remote 0grind config push`, never the panel's Config page. The repo is the live config ([hotfix](../ops/hotfix.md)) |
| Ports, bind address, RCON password | The panel. They belong to 0grind's slot, and `config push` keeps them |
| Bringing the server back after the scheduled close | The panel's **Workflows** (below) |
| Restart on Failure, load session save, A2S, startup parameters | The panel's **System** page (`config show` prints them, `status` warns) |
| Logs and saves | `host remote 0grind logs pull`, `saves pull` |
| Panel backups | The panel. They skip world saves, which is why `saves pull` exists |

### The commands

All run from a worktree (`tools\cli wt new host`). Nothing that changes the live server runs without `--yes`.

| Command | Does |
|---|---|
| `host remote 0grind status` | The service status from the API, a warning for each wrong System setting (below), and an A2S query of `144.76.90.77:3023` (name, map, players) |
| `host remote 0grind config show` | The panel's server settings, system settings and `/ServerConfig.json`, every password masked |
| `host remote 0grind config push [--admins-pending]` | Builds `server.json` the way `host config apply` does: `server/flagship/` plus `secrets.json` and the roster's admins. Then it shows the diff against the box's `server_settings`, `/ServerConfig.json`, and the mod's `$profile:LifeFramework/config.json` and `staff.txt`. It writes nothing |
| `... config push --yes` | Writes that diff. It keeps 0grind's ports, bind and public address, A2S block, RCON block and slot count. The change applies on the next restart |
| `host remote 0grind logs pull [--all]` | Copies the newest `/logs/<stamp>/console.log` (with `--all`, every file in that folder) to `<0grind folder>\profile\logs\logs_<stamp>\`. It prints the path for `host triage`, `host summary`, `host attendance` and `server-doctor` |
| `host remote 0grind saves pull` | Copies `/profile/.save` into `<0grind folder>\backups\<stamp>-remote\` and checks each file's size against the listing. This is the offsite copy, so run it after every session |
| `host remote 0grind rcon "#players"` | Runs a read-only RCON command. Any other command (`#kick`, `#restart`, ...) needs `--yes` |

The 0grind folder is `%LOCALAPPDATA%\LifeFramework\host-0grind` (`LF_0GRIND_DIR` overrides it). Put the
box's `secrets.json` there, in the shape step 4 shows. `config push` stages its files in that folder and
refuses a secret it finds in a repo file. With `LF_HOST_DIR` set to that folder, `host triage` and the
other log readers take the pulled log without a path.

### Workflows: the 06:00 and 18:00 restarts

The mod handles the close itself. The owner config's `restarts` warns players 15, 5 and 1 minutes ahead.
At 06:00 and 18:00 server time the mod writes the SHUTDOWN save and closes the server cleanly
(`LF_RestartClose`). On your PC, `host watch` starts it again. 0grind has no watchdog, and a clean exit
is not a failure, so a panel Workflow has to bring the server back:

1. Open the service, then **Workflows**, and create a workflow named `flagship-restarts`.
2. **Workflow Trigger:** **Schedule**. Add two times, **06:02** and **18:02** (a workflow holds up to 5).
   The two minutes give the shutdown save time to finish.
3. **Workflow Steps:** **Start the server**. If the mod's close did not happen, the server is still up and
   the start does nothing.
4. The panel appears to keep schedule times in UTC (its bundle formats them with UTC), and a Linux box's
   clock is usually UTC too. After the first `logs pull`, compare the log's timestamps with the schedule.

On the **System** page:

- **Restart on Failure:** on. A crash comes back without you.
- **Enable Loading Session Save:** on. Without it, every start is a fresh world.
- **PostGame Restart - Clear Saved Sessions** and **Attempt Auto-Recovering from Max Sessions Bug:**
  off. Each one deletes ALL saves.
- **Enable Public A2S Access:** on. The Server Browser and `host players` need it.

`host remote 0grind status` prints a `WARNING` line for each of these that is wrong. Run `saves pull`
after the morning restart, or after any session you care about. The first time, check the pulled log
for `save completed (SHUTDOWN)` before the restart.

### The first push (the lead does it)

After the Workshop items are published (steps 1 and 2):

```
tools\cli host remote 0grind config push --admins-pending
tools\cli host remote 0grind config push --admins-pending --yes
tools\cli host remote 0grind restart --yes
tools\cli host remote 0grind status
```

Read the dry-run diff before you add `--yes`. Then follow step 8 to read your admin id from a pulled log,
using `host attendance <pulled console.log>`.
