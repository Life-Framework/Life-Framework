# Playtest: Life Framework

The playtest is a small 1989 Everon town, published on the Arma Reforger
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
   The duty kit includes a radio tuned to the police frequency (41000 kHz; the
   medic kit is on 42000). Two officers hear each other on it; a civilian
   does not.
7. **Organization and hideout.** Found an organization at the registry desk,
   deposit cash, and buy the processing upgrade. Take the alley yard lease, open
   the hideout door, and store something in the stash. Run contraband from the
   barn to the pub.
8. **Restart.** Leave and rejoin: your cash, bank, job, organization, and cars
   should be back.

### M2 additions

The M2 systems run in the same playtest build. The money ledger and incident
log that back them stay on the server and invisible.

- **Payphone.** The booth by the square takes one coin from the cash you carry.
  Pick *Report a crime*, *Report an emergency*, or *Report someone suspicious*.
  An on-duty officer hears it on the radio and sees the line on the wanted
  board. One player can call once a minute. With nobody on duty nothing
  connects and no coin is taken.
- **Downed, not dead.** A lethal hit knocks a player unconscious instead of
  killing them. You lie on the ground, cannot move or act, take no further
  damage, and others can search you. A downed player bleeds out after three
  minutes unless someone uses a bandage, tourniquet, or saline bag on them, and
  each one adds time (morphine adds none). Hold X (pad Y) after 30 seconds to
  give up, which is death. Disconnecting while downed is death too. NPCs still
  die as before. There is no revive yet.
- **The fence.** A dealer in the net-fenced test annex sells crime tools only to
  proven criminals: a drill after 3 completed crimes, a breaching charge after
  5, a keycard after 8. Rob a till, steal a vehicle, or breach a door to build
  the count. Each tool carries a serial; use *Read serial* on it to see the
  number.
- **Medic duty.** Clock in at the duty desk in the hospital (the punch clock,
  open to everyone, no roster). You swap to the medic kit (uniform, medical
  kit, dressings, saline, morphine, tourniquet, a pager) and your civilian
  inventory is kept until you clock out, when duty pay is handed over in cash.
  While you are clocked in, medic calls reach you: the hospital board opens
  for you, you can take a call there, and the pager beeps. Off duty the board
  tells you it is for on-duty staff.
- **Report to staff.** After something happens to you, type `/report` in chat.
  It flags the incidents from the last ten minutes that involved you, as either
  party, for admins to see in the Game Master incident view. You get a "Staff
  were told" toast; a second report within two minutes is refused.
- **Safe square.** On the spawn square you cannot hurt another player and they
  cannot hurt you. Weapons still fire; NPC, world, and self damage pass.
- **Hold up.** Two players face each other within 2 m; the one with a weapon in hand sees *Hold up* in the action menu. It raises their hand, beeps at both, and tells the other player. It changes nothing else: it only lands in the incident log (Game Master incident view, kind Hold-up). Demo: `node agent-box/session.mjs holdup-proof` (two clients).
- **Aim intent.** Hold a raised weapon on another player for 3 s: one "Weapon aimed" row in the Game Master incident view, then at most one more per 3 s of continuous aim. Needs two clients on a dedicated server to prove the aim direction replicates.
- **Lawless quarry.** The quarry south of the dirt lane (about 100 m north of the square) is the one marked lawless area. Two red *Lawless Area* signs stand at the lane and at the east edge; read one for the rules. For now the zone only labels incidents (`lawless` in the incident log) and keeps wanted on; there is no fence or gate yet. Demo: `node agent-box/session.mjs quarry-proof`.

### Known rough edges

- Esc does not close the wanted card or the organization ledger right after
  founding; use the button or walk away (#199).
- If an arrest leaves you standing where you were instead of in the cell, say so.
- Some NPCs may appear half-dressed.
- Sign titles can be hard to read from the road.
- Going down has no revive yet. Letting the bleed-out run out, giving up, and
  disconnecting while downed all end in death.
- The fence and crime scene are in the test annex behind the net fence, not on
  the main street; a village placement is still to come.

## Not in this build

Do not report these as broken. File them under "build next" if you want them.

- Buying a house
- A general crafting bench
- Revive while downed
- An ambulance
- Calling a medic or a taxi from the payphone
- Quarry fence, gate, or lawless-only rules (the zone and its signs are in)
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
