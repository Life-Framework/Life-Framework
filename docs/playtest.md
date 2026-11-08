# DebugWorld playtest

There are two ways to play. **Host** runs the village on this PC as a dedicated server, the way friends will play it: start with [Play tonight](#play-tonight). **Workbench play** runs it inside Workbench for one person: see [Start DebugWorld](#start-debugworld). Both use `main`; the old `release/next` branch is merged and gone.

Walk up to each site and try it. Send back what you did, what you expected, and what you want built next. That note becomes the next agent prompt. The last QA pass is [playtest/qa-2026-11-08.md](playtest/qa-2026-11-08.md).

## Play tonight

You need Arma Reforger, the Arma Reforger Server (Steam app 1874900), Node, and a clone set up with the [setup guide](setup.md).

1. Host, from a worktree (the main checkout refuses):

   ```sh
   git -C <your clone> pull
   tools\cli wt new playtest          # once; afterwards: cd ..\Life-Framework-ws-playtest && git merge origin/main
   cd ..\Life-Framework-ws-playtest
   tools\cli host start               # add --wait if a test run holds port 2001
   ```

   Wait for `host: up`, then `session continued from a save point` or `fresh session`, then the LAN addresses.

2. Join on this PC. Start the release game executable (not the Diag one: a vanilla HUD script error opens a modal dialog there) from the game folder:

   ```
   cd "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger"
   ArmaReforgerSteam.exe -client 127.0.0.1 -addonsDir "<your worktree>\addons" -addons LifeFramework
   ```

   A friend uses the same line with their own clone at the same commit and your address ([How a friend joins](#how-a-friend-joins)).

3. Pick **Civilian**, then **Town Square**, then fill in form R-89 (given name, surname, age 18 to 80) and sign it. You wake on the village square with 500 cash and an ID card.

4. When you are done: `tools\cli host stop` (never close the server window; that skips the save). Next time `tools\cli host start` continues where you stopped.

Your account is keyed by your Steam name on this server (it runs without the Bohemia backend). Two players with the same Steam name would share one character, so give friends distinct names. A name change starts a new character.

### What to try tonight

In this order, it is the M1 loop:

1. Parcel job at the post office (`Roadmap_Job_Depot`), deliver at `Roadmap_Job_Drop`. Expect "Run paid: $40".
2. Deposit cash at the ATM, then buy the S105 at the car dealer (`Roadmap_Vehicle_Lot`). Cars are paid from the bank.
3. A second player clocks in as police at the station (police needs the whitelist or an admin; see [Police](#police)), the first robs `Roadmap_Fuel_Till`. The officer gets an alert toast; the robber is wanted.
4. The officer arrests the robber and escorts them to the cell. A new officer can do both from the first minute on duty.
5. Found an organization at `Roadmap_Org_Desk`, buy processing, open the hideout.
6. `host stop`, `host start`, rejoin: cash, bank, car, organization, and hideout are still there.

### Known rough edges (2026-11-08)

Do not spend the evening on these; they are filed.

- Everyone, players and NPCs, renders in underwear on a game client (#150).
- The registration form, ATM, and org screens are dark and hard to read; the form's labels read "Editbox" (#110, M1-13 is rebuilding these screens).
- The parcel and bank bags are invisible in your hands (#155).
- Fuel delivery opens after five parcel runs and the taxi after fifteen runs in all (#156). The game never shows a level; the drop-off tells you when new work opens.
- The police duty desk and wanted board are not on the street yet (M1-11); duty is the panel in front of the station.
- A contraband run without an organization is refused with no message (#157).
- Toasts are the only alert: no alarm at the site, no dispatch board (M1-20).

## Start DebugWorld

1. Open `addons/LifeFramework/LifeFramework.gproj` in Workbench.
2. Build for PC.
3. Play the scenario **Life Framework Test Suite** (`Missions/EL_DebugTest.conf`). That loads DebugWorld. It does not run the automated test suite unless you launch that suite yourself. Do not run `tools\cli test`.
4. Pick Civilian or Police and finish character creation.
5. You start on the village square at the west end of the main street. Walk east: post office, bank and ATM, general store and market, car dealer and garage, police station, hospital and fuel station, org registry with the hideout down the alley to its east. The farm is north of the square, the quarry and builders' yard south of the street, the forest at the east end, and the contraband barn up the dirt track past the village. The map is in `docs/roadmap/m1-25-village.md`. The fenced yard to the south-west is the test annex; nothing there is part of the playtest.

Walk up to an object. The interact key offers the same actions you see when you look at it. If looking at a site offers nothing, write that down.

Stop play mode and start the same scenario again when a step says restart.

## What to try

Do these as separate passes if one character cannot be both civilian and police. Name the `Roadmap_` entity in your note.

### Arrive

Create a civilian, then later a police officer. Say whether the faction screen and the name screen actually appeared.

### Civilian work

At `Roadmap_Job_Depot`, take the parcel job. Carry it to `Roadmap_Job_Drop`. You should be paid cash once. Abandoning the job should pay nothing.

Level 1 is the parcel. Level 2 opens fuel delivery (`Roadmap_Job_FuelCan`). Level 3 opens the taxi (`Roadmap_Job_TaxiPad`). If a job you have not earned is offered, or an earned job is missing, say so.

Crops, logs, and ore are the older chains (apples, plums, tomatoes on the farm; logging at the forest edge; mining, cement, gravel, sand south of the street). Gather one and sell it at the market stalls next to the general store.

### A vehicle

At `Roadmap_Vehicle_Lot`, buy a vehicle. Lock `Roadmap_Vehicle_S105` and unlock it with the key. The wrong key should fail.

### One robbery

A till pays cash onto you. Your bank balance stays put, and you become wanted. Try `Roadmap_Fuel_Till` or `Roadmap_Hospital_Till`. A second try during the cooldown should be refused.

A locked target is a breach. Plant the charge at `Roadmap_Breach_Door`, `Roadmap_Bank_Vault`, `Roadmap_Hospital_Cabinet`, or `Roadmap_Jail_Door`, then wait. The bank pays with the bag at `Roadmap_Bank_Bag`, not with a bank deposit. The cabinet should give medicine. The hideout door is not a breach.

### Police

Use a police character. On the hosted server the faction screen refuses Police until the player is on the whitelist, and there is no admin login in this mode, so write the file by hand:

1. Have the player join once. The host log (`tools\cli host status` prints its path) shows `[ELDebug:Whitelist] join allowed: uuid=<their id>`.
2. `tools\cli host stop`, then create `%LOCALAPPDATA%\LifeFramework\host\profile\profile\LifeFramework\whitelist.txt` with five lines: `1`, `1`, the uuid, `POLICE`, `0` (version, one faction entry, who, which faction, no job entries).
3. `tools\cli host start`. A player who has not made a character yet picks Police on the faction screen. A character that already exists keeps its faction (M1-11 replaces the faction screen with clocking in at the desk).

Go on duty at the panel in front of the police station (`PoliceStation_Debug`, south side of the street): `Roadmap_Police_Desk` beside it has no actions yet (M1-11 moves duty there). At `Roadmap_Police_Armory`, take the kit.

A new officer is patrol: a pistol and fines up to 1000. Senior is a rifle and fines up to 5000. Both can arrest and escort. An hour on duty promotes. Promoting yourself should be refused.

Arrest a wanted player into `Roadmap_Jail_Cell`. Repair an open breach. At `Roadmap_Escort_Car`, an on-duty officer can put a wanted player on foot into a passenger seat; the sentence starts when they reach the cell.

### Organization, hideout, convoy

At `Roadmap_Org_Desk`, the screen should show an empty organization, a name field, create, deposit, withdraw, and buy processing. If the screen is blank, that is the bug to report.

Create an organization, deposit cash, and buy the processing upgrade. The fund is not your personal bank balance.

`Roadmap_Hideout_Door` opens for a member and refuses everyone else. `Roadmap_Hideout_Machine` works only after the processing upgrade. `Roadmap_Hideout_Stash` is a real container: members open it and store items, everyone else is refused with a sound and a toast, and its contents survive a restart.

Run the contraband convoy from `Roadmap_Convoy_Start` to `Roadmap_Convoy_Drop`. Delivery pays cash once. Leaving the vehicle or dying should fail the run. An on-duty officer can confiscate the flagged goods.

### Restart

Stop play and start the scenario again.

Still there: organization members, the fund, the processing upgrade, hideout ownership and its lock, and the vehicle lock.

Expected to reset: police rank goes back to patrol. A jail sentence, an open breach, and the bank bags on the ground are not saved yet and are lost.

Write down anything that did not match that.

## Not in this build

Do not file these as broken. File them under "build next" if you want them.

- Buying a house
- A general crafting bench
- Revive
- A police mission board
- World events
- New guns, cars, or aircraft

## Host a playtest on a dedicated server

This runs the town on your PC as a dedicated server so friends can join and keep what they earn across restarts. You need the Arma Reforger Server (Steam app 1874900) and Node. It uses the mission **Life Framework Playtest Town** (`Missions/LF_PlaytestTown.conf`): DebugWorld with the debug menu and harness tools closed to everyone except server admins, and no test suite.

Host from a worktree. `host start` refuses in the main checkout, because that copy belongs to the world editor:

```sh
tools\cli wt new playtest          # once
cd ..\Life-Framework-ws-playtest
tools\cli host start
```

`host start`:

- copies any existing saves to `backups\<time>-start`, then boots with `-loadSessionSave` so the latest save continues. With no save it starts fresh.
- waits for the world and prints the session line: `session continued from a save point (-loadSessionSave)` or `fresh session: no save point exists for this mission`.
- prints the join addresses and returns. The server keeps running in the background.

Other commands:

| Command | Does |
|---|---|
| `tools\cli host status` | Running or stopped, the newest save point, the newest backup, the log path, join addresses |
| `tools\cli host stop` | Asks the server to close normally, waits for `save completed (SHUTDOWN)`, then copies the saves to `backups\<time>-stop` |
| `tools\cli host backup` | Copies the saves now |

Always stop with `host stop`. Closing the window or killing the process skips the shutdown save, so the restart loses everything since the last autosave (every 10 minutes).

The host profile, saves, logs, and backups live in `%LOCALAPPDATA%\LifeFramework\host` (set `LF_HOST_DIR` to move it). Nothing there is inside a worktree, so pruning a worktree or running tests never touches the saves. Restore a backup by stopping the host and copying the backup folder back over `profile\profile\.save`.

Only one server can run on this PC at a time. Every server listens on UDP port 2001. If a test run in another worktree holds the port, `host start` says so; `host start --wait` queues behind it. While the host runs, `tools\cli test` in any worktree waits for it.

### How a friend joins

The mod is not on the Workshop yet, so your friend needs the same addon you host:

1. Clone the repository and check out the same commit you host (`git rev-parse HEAD` in your worktree).
2. Start the release game executable from the game folder with the addon and your address. Not `ArmaReforgerSteamDiag.exe`: it stops on a modal dialog for a vanilla HUD script error.

   ```
   ArmaReforgerSteam.exe -client <your address> -addonsDir "<their clone>\addons" -addons LifeFramework
   ```

   `-client` connects on port 2001. On the same network use the LAN address `host start` printed. Over the internet use your public IP.

3. Each player is keyed by their Steam name (the server runs with `-noBackend`, so there is no platform identity). Distinct names keep distinct characters across restarts.

### Firewall and port forwarding

Allow `ArmaReforgerServer.exe` through Windows Firewall when Windows asks (private and public networks for internet play). For friends outside your network, forward UDP 2001 on your router to this PC's LAN address. Port 2001 is fixed in this mode: the server ignores `-bindPort` when it runs a mission directly, and the JSON server config (`-config`) cannot be combined with a local addon.

## Send feedback

Open a GitHub issue with the **Playtest** form: https://github.com/Life-Framework/Life-Framework/issues/new/choose

If that form is not listed yet, open **Feature request** or **Bug report** and paste the block below. One issue per problem or idea. A screenshot of the site, the menu, or a white mesh belongs on the issue.

```markdown
### Title

### Where
DebugWorld entity name, and Civilian or Police.

### Did
1.

### Expected

### Happened
Blank menu, missing action, white mesh, wrong payout, or a site you could not reach.

### Build next
The change an agent should make. Name the entity. Leave this empty if you only want the problem recorded.
```

A useful "build next" line is specific: "At Roadmap_Org_Desk the menu never appears. Open it as a visible screen with the organization name, fund, and the processing purchase." A vague line ("add more crime") is still welcome, and a second sentence that names a place makes it a prompt.
