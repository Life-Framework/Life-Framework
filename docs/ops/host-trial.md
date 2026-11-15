# 0grind trial worksheet (#378)

The maintainer's worksheet for the seven-day 0grind trial. The trial answers one question: can 0grind run the
flagship 24/7 with the real core and pack, or is another host needed. Each row below names a requirement, how to
check it in the trial, the log line or file that proves it, and a pass/fail box. Fill the boxes and the
evidence column as you go; the filled sheet goes into #378 and the decision goes into `docs/decisions.md`.

The move itself is in [rented-box](rented-box.md). Use this sheet to decide whether that move can use 0grind.
The public site does not promise anything about 0grind; a row here is a question, not a claim.

## Before you start

- [ ] The two Workshop items are published: core `79636E668EA37AC9` and pack `B1E84D5687E11724`. A missing item
      fails the first boot with `Addon <id> - Addon was not found on workshop`; that is a row 1 failure, not a
      host failure.
- [ ] You have the trial's control panel and its file manager, or SFTP, open in one window.
- [ ] A console log copy is kept on your own machine for each day of the trial. Copy it before the trial ends.

## How to read the boxes

- **Pass**: the check's proof appears exactly as written, the first time or after the step named.
- **Fail**: the proof does not appear, or a `SCRIPT (E)`, `VME:` or `Virtual Machine Exception` line appears
  next to it. Write the panel screen or the log line in the evidence column.
- **Gap**: the host cannot do the step at all. A gap is a result, not a failure of this sheet; record it and open
  an issue.

## 1. Workshop mods

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| Install both items by ID | Enter `79636E668EA37AC9` then `B1E84D5687E11724` in the panel's mods field (or `game.mods` in `server.json`), core first | First boot downloads both; no `was not found on workshop` line | [ ] Pass [ ] Fail [ ] Gap |
| Load order is the core first | Read the mod list the panel shows, or the boot log's addon list | Core `79636E668EA37AC9` listed before the pack | [ ] Pass [ ] Fail [ ] Gap |
| Pin a version | Look for a version field or a "lock" option per item; note what it offers | The panel's own screen, saved as evidence | [ ] Pass [ ] Fail [ ] Gap |

## 2. File access

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| Write the owner config | Put `config.json` at `<profile>/profile/LifeFramework/config.json` by the file manager, SFTP or API the host offers | `tools\cli config-check <file>` passes on the copy you uploaded | [ ] Pass [ ] Fail [ ] Gap |
| Write the whitelist | Upload `whitelist.txt` to the same folder | The file is present after a restart | [ ] Pass [ ] Fail [ ] Gap |
| Read `console.log` | Open the log from the panel, or fetch it over SFTP | The file's `save loaded:` or `Entered online game state.` line is readable | [ ] Pass [ ] Fail [ ] Gap |
| Edit `server.json` | Change the server name and `game.maxPlayers`, then restart | The name shows in the browser listing after the restart | [ ] Pass [ ] Fail [ ] Gap |

Record which access you used: RDP, SSH, SFTP, a web file manager or an API. The answer decides how the host
page is written, so write it down even when it passes.

## 3. Startup parameters

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| Set `-config`, `-profile`, `-maxFPS 60` | Find the startup settings in the panel and enter the launch line from [install](../owner/install.md) | The panel's saved line is a screen capture | [ ] Pass [ ] Fail [ ] Gap |
| Add `-loadSessionSave` after the first boot | Add it after one save exists, then restart | The restart's log has `save loaded: version N, game build X, running Y` | [ ] Pass [ ] Fail [ ] Gap |
| Leave it out on purpose | Only for a first boot with no save | The log does not start the town fresh on a restart; see the row 1 table in [log lines](../owner/log-lines.md) | [ ] Pass [ ] Fail [ ] Gap |

## 4. Persistence across restart and update

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| Profile survives a restart | Join, make a visible change (buy a shop item, own a house), stop with the panel's stop button, start | The change is still there after the restart | [ ] Pass [ ] Fail [ ] Gap |
| Stop writes the shutdown save | Read the log after the stop | `save completed (SHUTDOWN)` appears before the process exits | [ ] Pass [ ] Fail [ ] Gap |
| Profile survives a game update | Let the panel update the server build once, then start | The log's `save loaded` line names the new build and the save loads | [ ] Pass [ ] Fail [ ] Gap |
| Save is not damaged | Read the log after the restart | No `save load damaged` and no `continued save never reached the LF store` | [ ] Pass [ ] Fail [ ] Gap |

## 5. Restart, update and crash controls

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| Manual restart | Use the panel's restart button | The server is back and `Entered online game state.` appears within about 15 seconds of start | [ ] Pass [ ] Fail [ ] Gap |
| Crash restart | Kill the server process once, from the panel if it offers a kill, or by hand over the host's shell | It is back within a minute, with `save loaded:` in the log | [ ] Pass [ ] Fail [ ] Gap |
| Stop does not restart | Use the panel's stop | The panel stays stopped | [ ] Pass [ ] Fail [ ] Gap |
| Scheduled restart | Set one restart time in the panel, if it has the feature | The restart happens at the time and the log shows the save before it | [ ] Pass [ ] Fail [ ] Gap |
| Panel does what `host watch` does | `host watch` runs only on a box you control. Record what the panel's own restart-on-crash keeps: the crash log, a backup before the restart, and `-loadSessionSave` | Panel setting screen | [ ] Pass [ ] Fail [ ] Gap |

The flagship's restart times are an open question (see [rented-box](rented-box.md) section 6). Do not set a
schedule in the trial until the maintainer has answered it.

## 6. RCON and the HTTP API

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| RCON is reachable | Ask the panel whether it exposes RCON, and on which port | The port number the panel gives, and whether it is UDP or TCP | [ ] Pass [ ] Fail [ ] Gap |
| In-world warnings can be sent | Send one warning through RCON or the HTTP API, if the host offers one | The warning shows in game | [ ] Pass [ ] Fail [ ] Gap |

Record this even when it is a gap: in-world warnings before a restart need one of the two.

## 7. Backups

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| Panel backup exists | Make one from the panel's backup or snapshot feature | The backup is listed with a time | [ ] Pass [ ] Fail [ ] Gap |
| Copy the profile out | Copy the profile folder to your own machine by the file manager, SFTP or the host's download | The copy is on your machine; `tools\cli host drill <backup>` passes on it (maintainer machine only) | [ ] Pass [ ] Fail [ ] Gap |
| Copy before each session | Repeat the copy on two days | Two dated copies on your machine | [ ] Pass [ ] Fail [ ] Gap |
| How long the host keeps backups | Read the host's policy page | The number of days or copies, in writing | [ ] Pass [ ] Fail [ ] Gap |

## 8. Ports and reachability

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| Ports are open | `tools\cli host ports --external <address>:17777` (it prints the forward rules a PC host needs) | Exit 0 and `held-by-server` after the start; the game is UDP 2001, A2S is UDP 17777 | [ ] Pass [ ] Fail [ ] Gap |
| Open the ports in the host's panel | Network or firewall settings | The panel shows UDP 2001 and 17777 open | [ ] Pass [ ] Fail [ ] Gap |
| Server appears in the browser | `tools\cli host listing` *(planned, #1461)*; until it merges, join from a machine off your network and look in the in-game browser | The server name in the browser and a join | [ ] Pass [ ] Fail [ ] Gap |
| Players can query A2S | `tools\cli host players <address>:17777` | Server name, map and the player count | [ ] Pass [ ] Fail [ ] Gap |

## 9. 40 players: CPU, memory and upload

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| Measure CPU, memory and upload | `tools\cli host perf --every 30 --for 120` on a Windows box you control, with the server as the only busy network user. On a Linux panel, use its CPU and network graphs for the same window and write the figures down | `host perf` prints one `[LFDebug:HostPerf] verdict` line; the CSV is in `<host>/perf/` | [ ] Pass [ ] Fail [ ] Gap |
| Mean CPU under 80% of one core | Read the verdict's CPU figure | Verdict names mean CPU | [ ] Pass [ ] Fail [ ] Gap |
| Mean upload under the budget | Read the verdict's upload figure against `--upload-mbit` (default 20) | Verdict names mean upload | [ ] Pass [ ] Fail [ ] Gap |
| Forty players, the real count | Join with the players you have; note the count when `host players` reads it | `host players` lists the count | [ ] Pass [ ] Fail [ ] Gap |

`host perf` is Windows only. A panel that runs Linux gives you the figures from its graphs, not from the script,
so write which source you used. The 40-player budget is the one in [rented-box](rented-box.md) section 2.

## 10. Panel access and cost

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| Second admin account | Add a second login for a second person | The second login signs in and sees the server | [ ] Pass [ ] Fail [ ] Gap |
| Price for 40 player slots | Read the plan that covers 40 slots | The monthly price as shown on the host's own page | [ ] Pass [ ] Fail [ ] Gap |
| Price written down | Put the figure in `website/content/costs.json` only if the host's page shows it | The `costs.json` row with its date | [ ] Pass [ ] Fail [ ] Gap |

## 11. Box check

| Check | How to check in the trial | Proof | Result |
|---|---|---|---|
| One-run box check | `tools\cli host boxcheck` *(planned)*; until it merges, run `host ports` and the success lines in section 5 | One summary line per check | [ ] Pass [ ] Fail [ ] Gap |

## The decision

| Field | Answer |
|---|---|
| Host chosen, or the fallback | |
| Rows that failed or were a gap | |
| Issues opened for each gap | |
| Date of the decision | |

Write the decision in `docs/decisions.md` and link the filled sheet in #378. The sheet itself is the evidence:
screen captures and log lines, not a summary.
