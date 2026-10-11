# DebugWorld playtest

Play the release on your own computer. You need Arma Reforger and Arma Reforger Tools. You do not need the dedicated server. Install and open the project with the [setup guide](setup.md), then come back here.

This session is Workbench play of DebugWorld. The sites sit on one street. Walk up to each one and try it. Send back what you did, what you expected, and what you want built next. That note becomes the next agent prompt.

## Get the release branch

```sh
git clone https://github.com/Life-Framework/Life-Framework.git
cd Life-Framework
git checkout release/roadmap
```

If you already have the clone:

```sh
git fetch origin
git checkout release/roadmap
git pull
```

`main` does not have this street yet. Stay on `release/roadmap` until that branch is merged.

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

Arrest a wanted player into `Roadmap_Jail_Cell`. Repair an open breach. There is no vehicle escort in this build.

### Organization, hideout, convoy

At `Roadmap_Org_Desk`, the screen should show an empty organization, a name field, create, deposit, withdraw, and buy processing. If the screen is blank, that is the bug to report.

Create an organization, deposit cash, and buy the processing upgrade. The fund is not your personal bank balance.

`Roadmap_Hideout_Door` opens for a member and refuses everyone else. `Roadmap_Hideout_Machine` works only after the processing upgrade. Leave something in `Roadmap_Hideout_Stash`.

Run the contraband convoy from `Roadmap_Convoy_Start` to `Roadmap_Convoy_Drop`. Delivery pays cash once. Leaving the vehicle or dying should fail the run. An on-duty officer can confiscate the flagged goods.

### Restart

Stop play and start the scenario again.

Still there: organization members, the fund, the processing upgrade, hideout contents, and the vehicle lock.

Expected to reset: police rank goes back to patrol.

Write down anything that did not match that.

## Not in this build

Do not file these as broken. File them under "build next" if you want them.

- Buying a house
- A general crafting bench
- Revive
- A police mission board
- World events
- New guns, cars, or aircraft
- Driving a suspect to jail

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
