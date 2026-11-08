# DebugWorld playtest

Play the release on your own computer. You need Arma Reforger and Arma Reforger Tools. You do not need the dedicated server. Install and open the project with the [setup guide](setup.md), then come back here.

This session is Workbench play of DebugWorld. The sites sit on one street. Walk up to each one and try it. Send back what you did, what you expected, and what you want built next. That note becomes the next agent prompt.

## Get the release branch

```sh
git clone https://github.com/Life-Framework/Life-Framework.git
cd Life-Framework
git checkout release/next
```

If you already have the clone:

```sh
git fetch origin
git checkout release/next
git pull
```

`main` has the first street but not the latest sites (vehicle escort, among others). Stay on `release/next` until that branch is merged.

## Start DebugWorld

1. Open `addons/LifeFramework/LifeFramework.gproj` in Workbench.
2. Build for PC.
3. Play the scenario **Life Framework Test Suite** (`Missions/EL_DebugTest.conf`). That loads DebugWorld. It does not run the automated test suite unless you launch that suite yourself. Do not run `tools\cli test`.
4. Pick Civilian or Police and finish character creation.
5. Find the release street. The sites are named `Roadmap_` and sit together. Older debug props (crops, the mine, the shop, the ATM) are elsewhere in the same world.

Walk up to an object. The interact key offers the same actions you see when you look at it. If looking at a site offers nothing, write that down.

Stop play mode and start the same scenario again when a step says restart.

## What to try

Do these as separate passes if one character cannot be both civilian and police. Name the `Roadmap_` entity in your note.

### Arrive

Create a civilian, then later a police officer. Say whether the faction screen and the name screen actually appeared.

### Civilian work

At `Roadmap_Job_Depot`, take the parcel job. Carry it to `Roadmap_Job_Drop`. You should be paid cash once. Abandoning the job should pay nothing.

Level 1 is the parcel. Level 2 opens fuel delivery (`Roadmap_Job_FuelCan`). Level 3 opens the taxi (`Roadmap_Job_TaxiPad`). If a job you have not earned is offered, or an earned job is missing, say so.

Crops, logs, and ore are the older chains already in DebugWorld (apples, plums, tomatoes, logging, mining, cement, gravel, sand). Gather one and sell it if you can find the trader.

### A vehicle

At `Roadmap_Vehicle_Lot`, buy a vehicle. Lock `Roadmap_Vehicle_S105` and unlock it with the key. The wrong key should fail.

### One robbery

A till pays cash onto you. Your bank balance stays put, and you become wanted. Try `Roadmap_Fuel_Till` or `Roadmap_Hospital_Till`. A second try during the cooldown should be refused.

A locked target is a breach. Plant the charge at `Roadmap_Breach_Door`, `Roadmap_Bank_Vault`, `Roadmap_Hospital_Cabinet`, or `Roadmap_Jail_Door`, then wait. The bank pays with the bag at `Roadmap_Bank_Bag`, not with a bank deposit. The cabinet should give medicine. The hideout door is not a breach.

### Police

Use a police character. At `Roadmap_Police_Desk`, go on duty. At `Roadmap_Police_Armory`, take the kit.

A new officer is patrol: a pistol, fines up to 1000, and no arrest. Senior is a rifle, fines up to 5000, and can arrest. An hour on duty promotes. Promoting yourself should be refused.

Arrest a wanted player into `Roadmap_Jail_Cell`. Repair an open breach. At `Roadmap_Escort_Car`, an on-duty officer can put a wanted player on foot into a passenger seat; the sentence starts when they reach the cell.

### Organization, hideout, convoy

At `Roadmap_Org_Desk`, the screen should show an empty organization, a name field, create, deposit, withdraw, and buy processing. If the screen is blank, that is the bug to report.

Create an organization, deposit cash, and buy the processing upgrade. The fund is not your personal bank balance.

`Roadmap_Hideout_Door` opens for a member and refuses everyone else. `Roadmap_Hideout_Machine` works only after the processing upgrade. `Roadmap_Hideout_Stash` is a crate mesh with no storage yet, so it cannot hold items.

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
2. Start the game's diag executable from the game folder with the addon and your address:

   ```
   ArmaReforgerSteamDiag.exe -addonsDir "<their clone>\addons" -addons LifeFramework -client <your address>
   ```

   `-client` connects on port 2001. On the same network use the LAN address `host start` printed. Over the internet use your public IP.

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
