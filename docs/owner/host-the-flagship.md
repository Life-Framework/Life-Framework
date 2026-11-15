# Host the flagship server

This page takes the maintainer from "0.1.0 is not on the Workshop yet" to strangers joining
**Life Framework | Everon 1989 (Alpha)** from the in-game Server Browser. Do the steps in order. It runs
the server on your own Windows PC from a worktree of this repo. A rented panel host is
[a different page](rented-host.md), and a server without the repo is [the install guide](install.md).

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
(`tools\cli workshop boot-check 79636E668EA37AC9 B1E84D5687E11724`) and wait for `stage=pass`. A failure
there will also stop the public server.

## 2. Get the two Workshop IDs

The Workshop ID of each item is the address of its page:

| Item | Page | ID |
|---|---|---|
| Life Framework (Core) | `https://reforger.armaplatform.com/workshop/79636E668EA37AC9` | `79636E668EA37AC9` |
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
tools\cli host config apply --mods 79636E668EA37AC9,B1E84D5687E11724 --admins-pending
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
