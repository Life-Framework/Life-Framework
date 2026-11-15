# Rented box checklist

The move from the maintainer's PC to a rented machine that runs the flagship 24/7 (#311, epic #230). Do it
once the trigger in step 1 is met, not before. It is host-agnostic: everything here holds for any Windows or
Linux box. The part that depends on the host is left for the host #378 picks (the last section).

How to read it: each step names a command or a file. A command that has not merged yet is in code font as its
card names it and marked *(planned)*; when it merges, drop the mark. Run the steps in order and tick the
box when the step's check passes. Sessions on the live server follow [game-updates](game-updates.md) for
builds and the session runbook (O16, #1275) for a normal session.

## 1. The trigger

Rent a box only when both hold (decision 2026-11-08, [m4-outline](../roadmap/m4-outline.md)):

- [ ] Several sessions in a row reached the player target (about 40). Count them from the Discord announcements
      and the session notes; there is no automatic count.
- [ ] Donations cover three months of the box. Read the cost from `website/content/costs.json`: the
      `Game server` row's `amount` (USD) and `period`. Three months is `amount` times 3 for a `month` period.
      While `amount` is `null` the maintainer has not priced a box, so the trigger is not met. The file holds
      costs only; the donations total comes from the maintainer's donation account, by hand.

When both are true, fill the `Game server` row's `amount`, set its `status` to `paying`, and set `updated`, in
the same PR as the move. Never type an estimate as a real amount (the file's own rule).

## 2. Box spec for 40 players

A working spec, not a measurement. The 128-player budget in [performance](../architecture/performance.md) says
the LF script cost is small and linear, so the engine and the network set the size, not our scripts. Confirm it
at the dry run (step 9) and write the real figures here.

| Part | Ask for | Why |
|---|---|---|
| CPU | 4 or more cores, fast single-thread (the game loop is one hot thread) | Server FPS is the limit; core count past 4 helps little |
| RAM | 16 GB | The server, the OS, and headroom for the save at its cap (about 6 MB, performance.md) and the backups' disk cache |
| Disk | 60 GB SSD | The server install, two Workshop mods, 20 rolling backups (`host prune` keeps 20), logs |
| Network | 100 Mbit up, unmetered or 2 TB a month or more | 40 players of replication; check the host's cap before paying |
| OS | Windows Server 2022 or later, or Ubuntu 22.04 or later | The `tools\cli host` commands are Windows; Linux runs `server/example/start.sh` (step 6) |
| Address | A fixed public IPv4 | Players and DNS point at it |
| Access | Remote Desktop or ssh with a key | Nothing in the checklist needs the host's web panel except what step 10 records |

- [ ] The host's plan lists these, or better, and the price is the figure that went into `costs.json`.

## 3. Ports

The game listens on UDP 2001 and the A2S query on UDP 17777 (`server/example/server.json`: `bindPort`,
`a2s.port`). Both are UDP.

- [ ] Open UDP 2001 and UDP 17777 in the host's firewall panel and in the OS firewall.
- [ ] Nothing else is open to the world. Remote Desktop (TCP 3389) or ssh (TCP 22) is limited to the
      maintainer's address, or to Tailscale as in [worktrees](../worktrees.md) "A rented box".
- [ ] If either port is taken, change `bindPort`/`publicPort` and `a2s.port` in `server.json` together, forward
      the new numbers, and write them in the DNS note of step 8.
- [ ] Check from outside the box (a phone off Wi-Fi, or another machine): the server appears in the in-game
      browser, or the join address connects. A browser that does not list it means UDP 2001 or 17777 is shut
      ([install](../owner/install.md) troubleshooting table).

## 4. Install the server and mods

- [ ] Install **Arma Reforger Server** (Steam app 1874900) with SteamCMD or Steam, on the current build.
      `tools\cli host status` prints the installed build.
- [ ] The `tools\cli host` commands in this page need a clone of the repo and Node 22 on the box: an anonymous
      HTTPS clone at `stable`, never a token or password on a rented machine ([worktrees](../worktrees.md) "A
      rented box"). A box with only the start script (below) boots the server but cannot run `host backup`,
      `host restore` or `host prune`; copy the profile by hand there.
- [ ] Copy the flagship folder: `server/flagship/` from the repo at `stable` (the live branch,
      [release-process](../release-process.md)). It holds `server.json` and `config.json` once C1 (#1321)
      merges; until then start from `server/example/`.
- [ ] In `server.json` set `game.maxPlayers` to 40 and the server name; the join password and admin password
      go in the host-only `secrets.json`, never in the repo (`host config apply`, below).
- [ ] `tools\cli server-template check` passes on the folder: the core mod first, then the pack, the scenario
      a header inside a listed addon.
- [ ] `tools\cli config-check server/flagship/config.json` passes.
- [ ] First start downloads both mods from the Workshop. It must not stop with `Addon <id> - Addon was not found
      on workshop`; if it does, the Workshop item is not published or the id is wrong.

## 5. Profile and save copy, with a drill

The live town is the save under the host profile (`<host dir>\profile`; the backups sit beside it in
`<host dir>\backups`). Moving it is a copy of two folders, then a proof that it loads. Do the copy with both
servers stopped.

On the old host (the maintainer's PC):

1. [ ] Announce the downtime on Discord, with the time (`tools\cli host announce --at <time>`; see the
       [session runbook](session-runbook.md) for `--build` and the `sessions.json` edit).
2. [ ] `tools\cli host stop --wait` so the engine writes the shutdown save.
3. [ ] `tools\cli host backup` and note the folder name it prints. This is the copy that travels.
4. [ ] Copy the whole `backups/<that folder>` to the new box (scp, or an archive). Leave the old profile
       untouched until step 9 passes: it is the rollback.

On the new box:

5. [ ] Put the backup under the new host dir's `backups/`, and run `tools\cli host restore <folder name>` to
       lay it into the profile. `restore` keeps what it replaces under `backups/<time>-replaced-by-restore`.
6. [ ] **Drill before opening:** `tools\cli host drill <folder name>` *(planned)* boots a throwaway copy with
       `-loadSessionSave` and passes only on a `save loaded:` line and at least one applied account record. Without
       `host drill`, start the box once without announcing it and read the console log
       (`tools\cli logs "save loaded:"`): it must show `[LFDebug:Persistence] save loaded: version N, game build X,
       running Y`, with none of `save load damaged`, `continued save never reached the LF store`, `VME:` or
       `SCRIPT (E)`. Stop it with `host stop` so the shutdown save is written.
7. [ ] After a different game build than the save's, run `tools\cli host preflight` first
       ([game-updates](game-updates.md)); `host start` refuses a save from another build until one passes.
8. [ ] Check the numbers the town carries across: an account balance, a house owner, and a vehicle lock the
       maintainer remembers from the old host.

A drill that fails means do not open the box. Fix the cause, or keep playing on the old host.

## 6. Start, watch and restart

- [ ] Windows: `tools\cli host start --flagship`, or the start script in the flagship folder. Linux:
      `LF_REFORGER_SERVER=~/reforger-server ./start.sh -loadSessionSave`. Both pass `-maxFPS 60` and keep the
      owner `config.json` in the profile.
- [ ] **Watchdog.** `tools\cli host watch` *(planned)* supervises the process: an exit without the stop file
      is a crash, so it keeps the crash log under `backups/<time>-crash/`, backs the saves up, and restarts with
      `-loadSessionSave` within a minute. Until it merges, or on Linux, use the OS: a Windows scheduled task
      (trigger "at startup", restart on failure every minute) or a systemd unit with `Restart=on-failure`. Both
      must pass `-loadSessionSave`, and a stop through `host stop` must not trigger a restart.
- [ ] Kill the server process by hand once. It is back, with the crash backup, inside a minute, and the log
      shows `save loaded:`.
- [ ] **Restart schedule.** The flagship restarts at 06:00 and 18:00 server time, with in-game warnings at 15, 5
      and 1 minute ([m4-slice-plan](../roadmap/m4-slice-plan.md), "Decisions"). The times are set in
      `server/flagship/config.json` (`restarts.times`), and the in-world warning counts down to them.
- [ ] A scheduled restart is also a backup point: `host stop` backs up, `host start` backs up. Rotate with
      `tools\cli host prune --keep 20` (dry-run it with `--dry-run`); tagged backups (preflight, wipe, crash,
      replaced-by-restore) are never deleted.

## 7. Secrets and config

The live `config.json` is public in this repo, so no password or webhook goes in it.

- [ ] Write `secrets.json` in the host dir on the box only: the join password, the admin password and the staff
      webhook URL.
- [ ] `tools\cli host config apply` *(planned)* writes the profile's `config.json` and `server.json` from the
      repo files plus `secrets.json`. Until it merges, copy `config.json` to
      `profile/profile/LifeFramework/config.json` and fill the passwords in `server.json` by hand on the box.
- [ ] `secrets.json` is in no backup that leaves the box, and in no commit.

## 8. DNS and the address

- [ ] Decide the name players use (for example `play.life-framework.org`) and add an `A` record to the box's
      IPv4 in Cloudflare DNS, **DNS only** (grey cloud): the proxy carries web traffic, not game UDP.
- [ ] TTL 300 seconds during the move, so a rollback to the old address is quick; raise it afterwards.
- [ ] Join by `<name>:2001` from outside the network. The browser listing uses the IP and A2S port, so it needs
      step 3 too.
- [ ] Update every place that shows the address: the Discord announcement template, the website `sessions.json`
      *(planned, O14)*, and `docs/playtest.md`, which today says the server is one PC.

## 9. Dry run, then open

The dry run is the manual check of #311, on `lf-testbox` (the LAN laptop) or on the box before the town moves.

- [ ] Run steps 4 to 6 on `lf-testbox` or an empty profile on the new box, with `maxPlayers` at 40.
- [ ] Join with two clients, one after the other, save, `host stop`, `host start -loadSessionSave`, and see the
      same four load lines [dedicated-session](../dedicated-session.md) lists.
- [ ] Note server FPS and RAM at the player count reached, and replace the working spec in step 2 with them.
- [ ] Move the real save (step 5), pass its drill, open the ports, and announce.
- [ ] Keep the old host's profile for a week. Delete it only when the new box has run through a restart and a
      backup rotation.

## 10. After the move

- [ ] Run `tools\cli host triage` *(planned)* on the first full session's console log; file what is new.
- [ ] Add the box's one-off facts (below) and the monthly cost to `website/content/costs.json`.
- [ ] Open a follow-up issue for anything this checklist got wrong, and fix this page the same day.

## The host (left for #378)

Filled in by the maintainer after the 0grind trial (#378) or whichever host is chosen, and then split into the
per-host page of #379.

| Question | Answer |
|---|---|
| Host and plan | |
| Price per month, and where it was written in `costs.json` | |
| Does it install mods by Workshop id, or by a file upload? | |
| File access (RDP, ssh, SFTP, web panel) | |
| Start parameters it lets us set (`-config`, `-profile`, `-loadSessionSave`, `-maxFPS`) | |
| Does it keep `profile/` across a restart, and can we copy it out? | |
| Console log location, and the log lines that mean it is up | |
| Restart and crash behaviour: its own, or `host watch` | |
| Port forwarding and firewall panel | |
| Backups the host keeps itself, and how long | |
| Support route and its response time | |
