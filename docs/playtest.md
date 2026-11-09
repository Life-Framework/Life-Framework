# M1 playtest: Life Framework

The M1 playtest is a small 1989 Everon town, published on the Arma Reforger
Workshop and hosted from the maintainer's PC. Play it, then tell us what broke.
Feedback goes through GitHub issues.

## Subscribe and join

1. In Arma Reforger, open the **Workshop**, search for **Life Framework**, and
   subscribe. Wait for the download to finish.
2. Open the server browser and join **Life Framework Playtest Town**. The game
   enables the mod for you. If the server is not listed, it is not running;
   session times are announced on [Discord](https://discord.gg/MjMexJteqz).
3. On your first join, fill in registration form R-89 (name and age). Everyone
   starts as a civilian; there is no faction screen.
4. Your character is keyed to your Steam name. Use the same name and you keep
   your cash, bank, job, cars, and organization across sessions.

The server is one PC, not a 24/7 host. It may be offline or full.

## What to try

Signs and prompts in the world lead the way. In this order, it is the M1 journey:

1. **Arrival.** Fill in form R-89, then follow the clerk on the square: your ID
   card, the town board, and a parcel run ("Run paid: $40").
2. **Job.** Sign up at the post office counter and deliver the parcel. Five
   parcel runs open fuel delivery, fifteen open the taxi.
3. **Bank.** You start with $1,000 in the bank. Deposit and withdraw at the ATM.
4. **Shop.** The general store sells food, drink, and a dressing from a price
   sheet.
5. **Vehicle.** Read the price sheet at the car lot and buy a car. Your character
   also starts with an S105 in the garage and a UAZ at the impound lot (playtest
   only): read the garage ledger, retrieve one, and collect the other at the
   impound desk for the fee.
6. **Police.** On a fresh save the first player to join is on the police roster.
   Clock in at the station desk; the kit includes the key to the station car. A
   second player robs the fuel till: the officer gets the radio dispatch, reads
   the wanted board, arrests the robber, and the robber goes to the cell. Clock
   out at the same desk to swap back.
7. **Organization and hideout.** Found an organization at the registry desk,
   deposit cash, and buy the processing upgrade. Take the alley yard lease, open
   the hideout door, and store something in the stash. Run contraband from the
   barn to the pub.
8. **Restart.** Leave and rejoin: your cash, bank, job, organization, and cars
   should be back.

### Known rough edges

- Esc does not close the wanted card or the organization ledger right after
  founding; use the button or walk away (#199).
- If an arrest leaves you standing where you were instead of in the cell, say so.
- Some NPCs may appear half-dressed.
- Sign titles can be hard to read from the road.

## Not in this build

Do not report these as broken. File them under "build next" if you want them.

- Buying a house
- A general crafting bench
- Revive
- A police mission board
- World events
- New guns, cars, or aircraft

## Report a bug

Open a GitHub issue with the **Playtest** form:
https://github.com/Life-Framework/Life-Framework/issues/new/choose

One issue per problem or idea. A screenshot of the site, the menu, or a white
mesh belongs on the issue. If the Playtest form is not listed, open **Bug
report** and paste this block:

```markdown
### Title

### Where
The site or entity name, and whether you were civilian or police.

### Did
1.

### Expected

### Happened
Blank menu, missing action, white mesh, wrong payout, or a site you could not reach.

### Build next
The change you want. Name the site. Leave this empty if you only want the problem recorded.
```

A useful "build next" line is specific: "At the organization desk the menu never
appears. Open it as a visible screen with the organization name, fund, and the
processing purchase." A vague line ("add more crime") is still welcome; a second
sentence that names a place makes it a prompt.

## For developers

- **Workbench play:** open `addons/LifeFramework/LifeFramework.gproj`, build for
  PC, and play **Life Framework Test Suite** (`Missions/EL_DebugTest.conf`). That
  loads DebugWorld. It does not run the automated suite unless you launch that
  suite yourself.
- **Local one-command playtest:** `tools\cli playtest` in a clone, then
  `tools\cli playtest stop`. It runs the town as a dedicated server and joins it.
- **Host a server yourself:** `server/README.md`, and `docs/setup.md` for the
  toolchain. The publish checklist is
  [playtest/workshop-publish.md](playtest/workshop-publish.md).
- **QA reports:** [playtest/rc-2026-11-08.md](playtest/rc-2026-11-08.md) and
  [playtest/qa-2026-11-08.md](playtest/qa-2026-11-08.md).
