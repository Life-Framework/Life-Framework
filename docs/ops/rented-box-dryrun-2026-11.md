# Rented-box dry run, November 2026 (#1607)

The record of walking [rented-box](rented-box.md) (#311, epic #230) on a stand-in for the rented machine. Run
on 2026-11-15 against `main` at 75fdc38. The checklist says `lf-testbox`; the maintainer pointed this run at
`lf-cloud` (Windows NT 10.0.26100, ssh over Tailscale, Node 22, git, a server install at
`C:\LF\ArmaReforgerServer`, build 1.9.0.37) because it has no player on it. Everything ran in the scratch folder
`C:\LF\dryrun-1607`, which was removed with `tools/safe-rm.mjs` at the end; nothing else on the box was installed
or changed (no firewall rule, no scheduled task, the other server process there was left alone).

The old host was a scratch host folder on the maintainer's PC (`LF_HOST_DIR`) holding a copy of the newest real
backup, `2026-11-10_21-39-00-stop`, so the live town was never touched.

PASS means the step's command ran and its check held. GAP names what failed and the issue that tracks it. NOT RUN
says why the step was out of reach. BLOCKED means the command is not on `main`.

## Result by step

| Step | Result | What was run, and what came back |
|---|---|---|
| 1 Trigger | NOT RUN | Needs session counts and the donations total; the maintainer's, by hand. `costs.json` `Game server` amount was not read. |
| 2 Box spec | BLOCKED, then PASS (read once) | `host boxcheck` (#1496, PR #1543) is not on `main`, so it is BLOCKED here. Run once read-only from its branch, `host boxcheck --ssh lf-cloud` printed: cores PASS (8, want 4), RAM PASS (31.6 GB, want 16), install PASS (1.9.0.37), disk FAIL (39.6 GB free, want 60), verdict FAIL 1 of 4. The disk miss is this stand-in's, not the checklist's. |
| 3 Ports | PARTLY | `host ports` on the box: every port free, ready to start, forward rules printed (UDP 2001 and 17777). Opening the firewall and the check from outside were NOT RUN (no changes to the box's firewall allowed). |
| 4 Install server and mods | GAP | Clone at `stable` has no `server/flagship` or `server/example` and an old `tools/host` (#1641). A clone of `main` needs `agent-box/` and `addons/` (1.1 GB) beside `tools/`, and `ENFUSION_SERVER_PATH` set (#1642). `server-template check`, `config-check` and the Workshop download were NOT RUN (that is `workshop boot-check`). |
| 5.1 Announce | PARTLY | `host announce --at 2026-11-30T18:00:00Z` drafted the post; it then failed writing `website/content/sessions.json` because the scratch clone had no `website/` (a missing folder in the dry-run clone, noted in #1642). `--send` NOT RUN. |
| 5.2 Stop old host | NOT RUN | No server was running on the scratch host; `host stop` was not pointed at the PC, which hosts other work. |
| 5.3 Backup | PASS | `host backup` wrote `backups/<time>-manual` on the scratch host. |
| 5.4 Copy to the box | PASS | `scp -r` of the backup folder to `C:\LF\dryrun-1607\host\backups\` (193 KB). |
| 5.5 Restore | PASS | `host restore <folder>` on the box laid it under `profile/profile/.save`; `host status` then showed 10 save points and the save and game build both 1.9.0.37. |
| 5.6 Drill / boot the copy | PASS with a GAP | `host drill` was NOT RUN (it boots on a pool machine, not this box). The hand route: `host start --force --wait` loaded the copy: `save loaded: version 22, game build unknown, running 1.9.0.37`, `applied 1 account record(s)`, `applied 1 bank account record(s)`, `session continued from a save point (-loadSessionSave)`. No `save load damaged`. But the log holds one `SCRIPT (E): Virtual Machine Exception` at the first frame: `Ammo_Taser.et` not in `AmmoList.conf` (#1643), which the step's "none of `VME:`" rule counts as a failure. `host start` itself refused without `--force` (#1642). |
| 5.7 Other-build preflight | NOT RUN | Save and game were the same build (`host status`). |
| 5.8 Numbers carried across | NOT RUN | Needs the maintainer's memory of a balance, house owner and lock. The log shows 1 account and 1 bank record applied. |
| 6 Start, watch, restart | PARTLY | `host watch --force` supervised the host. A hand kill of the server (06:25:47 box time) wrote `backups/<time>-crash` (console log kept), restarted within 4 s, and the new log said `save loaded: version 34, game build 1.9.0.37, running 1.9.0.37`. `host stop --wait` ended it with a shutdown backup. GAP: `host autostart install --dry-run` refuses in a plain clone and has no override (#1642). The 06:00 and 18:00 restart schedule and the `host prune` rotation were NOT RUN (`host prune` and the schedule need a long run). |
| 7 Secrets and config | GAP | `host config apply --dry-run` stopped at `the administrator has no uid yet, so game.admins cannot be written` (#1644). `host drift` reported both host files missing and `host ready` said NO-GO on drill and config, which is correct for an unconfigured box. `secrets.json` was NOT RUN (no secret may be put on the stand-in). |
| 8 DNS and address | NOT RUN | Cloudflare and the public address are the maintainer's; nothing here for a stand-in. |
| 9 Dry run, then open | PARTLY | The boot-from-the-copied-save part of steps 4 to 6 ran (above). Two clients, the second-boot check, server FPS and RAM at 40 players, opening ports and the real move were NOT RUN: no game client was available on the box. The working spec in step 2 therefore still has no measured figures. |
| 10 After the move | NOT RUN | After the real move. |

## Gaps filed, all under #230

- #1641 `stable` is behind `main`; step 4's clone has no `server/flagship` and old `host` code.
- #1642 `host start` and `host autostart` refuse in a plain clone; the box clone needs `agent-box/`, `addons/`,
  `website/` and `ENFUSION_SERVER_PATH`.
- #1643 the copied save boots with a VME for the missing taser ammo entry.
- #1644 `host config apply` refuses until the staff roster admin has a uid.
- #1636 (already filed) `host verify` says `nosave` on backups that hold a save point: it said so on every real
  backup here, including the copy that then booted with its accounts, so it is a false negative.

Stale text in [rented-box](rented-box.md) itself (owned by #1557, so only listed here): `host announce`,
`host drill`, `host watch` and `host config apply` are marked *(planned)* but are on `main`; `host autostart`,
`host ready`, `host verify` and `host disk` exist and the page does not name them.

## What the dry run says about the move

The core path works on a plain Windows box: restore a copied backup, boot it with `-loadSessionSave`, get the
accounts back, and have the watchdog recover a kill with a crash backup and a clean reload. The move is not ready
for the maintainer to run from the page as written: fix the `stable` lag and the clone and `--force` notes
(#1641, #1642), add the admin uid (#1644), and settle the boot VME (#1643). The 40-player figures (step 9) still
need two real clients on the box.
