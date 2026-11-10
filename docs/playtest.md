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
3. **Bank.** You start with $1,000 in the bank. Deposit and withdraw at the ATM: two teller-window counters flank the bank door (about 177 and 187, 259), under the ATM sign.
4. **Shop.** The general store sells food, drink, and a dressing from a price
   sheet.
5. **Vehicle.** Read the price sheet at the car lot (desk at 211, 262; four display cars in a row at z 261, each with a chalkboard price board in front, x 216 to 230) and buy a car. Your character
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
   Where: the walled yard at the end of the cobbled alley (about 302, 288, door on the alley); inside are the machine, the stash, a workbench, crates, barrels and a parked van. The registry desk is at 289, 256. Demo: `session.mjs org` (proof `agent-box/proofs/org.mjs`) (2 clients: found the org, take the lease, open the gate).
8. **Restart.** Leave and rejoin: your cash, bank, job, organization, and cars
   should be back.

### M2 additions

The M2 systems run in the same playtest build. The money ledger and incident
log that back them stay on the server and invisible.

- **Mining.** The miner stand at (176, 167) has ore rocks and a display crate. Swing a pickaxe at a rock: it shakes, shrinks a step per piece, and drops nuggets that rest on the ground; the last piece makes it vanish and it grows back after its cooldown. Open the inventory to check the nugget and pickaxe previews against the gold bar. No proof verb yet.
- **Payphone.** The booth by the square takes one coin from the cash you carry. Four booths stand in town (square beside the handcart, fuel station at the kiosk, hospital and police station against their front walls). A booth's open side is local -Z, so each one is turned with its back to the wall or the lawn and its door to the street. Area 8 and 17 polish: the hospital and police booths were facing their walls (now back to the wall, door to the street); the Red Cross banner was buried in the hospital wall (only its ropes showed) and now hangs in front of it, the *Ambulance bay* sign no longer stands in the ambulance's lane, and the police whitelist plaque is on the station wall under the *Police Station* sign (236.6, 241.2) instead of lying on the grass.
  Pick *Report a crime*, *Report an emergency*, or *Report someone suspicious*.
  An on-duty officer hears it on the radio and sees the line on the wanted
  board. One player can call once a minute. With nobody on duty nothing
  connects and no coin is taken.
- **Where the investigation gear stands.** The fence (Vince) is behind the barn at (186, 311), north-west of the bank, away from the street. The bank vault door (a steel door on the bank's east wall) is at (194, 265) and opens only to a fence tool. The cash van starts at the bank doorstep, in the north lane at (196, 252.4) facing west, then visits the general store (162) and the fuel station (255) in the south lane (z 248.3) along the main street. Its cash box (a vanilla steel trunk) sits on the van's rear step; it opens to a drill and drops three money sacks on the ground behind the van. Two traffic cones and a pair of crates dress the bank stop. The evidence locker (234.4, 237.6, against the back wall beside the door), the armory store (231, 237.6), wanted board and dispatch board are at the police station. The payphone witness and the officer's "Ask what you saw" are covered under Comms and Evidence above.
- **Downed, not dead.** A lethal hit knocks a player unconscious instead of
  killing them. You lie on the ground, cannot move or act, take no further
  damage, and others can search you. A downed player bleeds out after three
  minutes unless someone uses a bandage, tourniquet, or saline bag on them, and
  each one adds time (morphine adds none). Hold X (pad Y) after 30 seconds to
  give up, which is death. Disconnecting while downed is death too. NPCs still
  die as before. A medic on duty can revive a downed player.
- **The fence.** Vince, a dealer behind the old barn at the north-west edge of
  the village (about 186, 311, a walk up the lane behind the bank), sells crime
  tools only to proven criminals: a drill after 3 completed crimes, a breaching charge after
  5, a keycard after 8. Rob a till, steal a vehicle, or breach a door to build
  the count. Each tool carries a serial; use *Read serial* on it to see the
  number. His yard has a workbench, crates, a barrel, a tyre pile and a parked old car; stand south of the barn doors to trade.
  A police scanner ($900, after 2 crimes) chirps and shows a toast whenever a
  police or medic call goes out. It is illegal: an officer who searches a wanted
  suspect takes it and files a charge.
- **Lockpick.** The fence sells a lockpick (no record needed, $250, a stack: each opened lock or snapped pick wears one use). The lock-up door just east of the yard gate (about 306, 241) is locked: stand at it and use *Pick the lock*. Stay close for 6 to 20 seconds (your level shortens it). You bend over the lock the whole time and other players see it (#513; the bank vault drill and the cash van box drill do the same for 6 s, and picking the locked car beside the door bends you the same way; demo: `node agent-box/session.mjs work-anim-proof`, which drives all four). It opens, or the pick slips, or snaps. An opened lock files a crime report that nearby witnesses can describe; the police are not called by it. The yard gate, the bank vault and the jail door cannot be picked.
- **Steal a car.** A beige car parked east of the lock-up (about 312, 246) is locked and will not start without its key. With a lockpick, use *Pick the lock* at its door, get in, and use *Hotwire the ignition* at the wheel. It takes 6 to 15 seconds and can fail; a failure may be heard. Once it works, start the engine as usual and drive off. The car is marked stolen on the server. If you own an S105 (the starter garage car) a thief can take it the same way: with its key in your pocket, use *Report the car stolen* at it and the plate goes on the police wanted board as a stolen car. The chop shop yard is a walled back lot north-east of the lock-up, behind the chain barrels (about 322, 300), with a blue engine crane (walls on three sides, open at the south gate; a stripped jeep shell, doors, tyres, drums and a tool bench stand around it): park a stolen car beside it, get out, and use *Take this car apart*, hold *Strip it down* until it is bare (more hands are faster), then *Take the cash and parts*. It pays 30 percent of the car value (an S105 pays $300) and two spare parts a trader will buy; the car is gone for good, the crime is reported, and the yard takes nothing for five minutes. It will not take a car that is not stolen, or one you hold the key to; the toast says so ("Only stolen cars. Lockpick or hotwire a car that is not yours and bring it here.") and the car is left untouched. Hot means the engine was started without its key on a locked-ignition car (the hotwire above); a base-game road car with no ignition lock, or one you simply got into and drove, is not hot (#601). An on-duty officer can use *Check the plate* at any car (a reported one reads STOLEN) and *Recover stolen vehicle* at a reported, empty one, which returns it to the owner's garage. A witness can describe the thief.
- **Medic duty.** Clock in at the duty desk in the hospital (the punch clock,
  open to everyone, no roster). You swap to the medic kit (uniform, medical
  kit, dressings, saline, morphine, tourniquet, a pager) and your civilian
  inventory is kept until you clock out, when duty pay is handed over in cash.
  While you are clocked in, medic calls reach you: the hospital board opens
  for you, you can take a call there, and the pager beeps. Off duty the board
  tells you it is for on-duty staff.
- **The village hospital.** The two-storey house on the east side of the square road (about 262, 256) is the Everon Village Hospital. Out front, left to right: the payphone, the duty desk, the medic dispatch board, the medicine cabinet and till, with benches and flowerpots for the waiting room. The ambulance waits in its marked bay on the road below, between four cones, under an *Ambulance bay* sign. The three beds are in the ground-floor ward behind the front door. Read the *Everon Village Hospital* sign by the door for the short version. Polish (area 7): the payphone moved off the wall to (254, 258), the pots, bench and beds sit clear of the walls, and two open supply boxes and a bin stand by the east end of the front, the till (a post-office counter) is at (268, 257) and the medicine cupboard stands against the wall at (266.5, 258). Treatment bills are on 1989 paper, in cash at the desk: $100 for a medic's revive, $150 for waking up in hospital after a death. Both prices live in config (`LF_Medic.conf` `m_iReviveBillAmount`, `LF_Respawn.conf` `m_iBillAmount`).
- **Revive.** Clocked in as a medic, face a downed player within about two metres
  and use *Revive* (the same list as Introduce / Show ID). They get up with a
  sliver of health and keep what they carry, and the bleed-out stops. A civilian
  does not get the entry. *Revive* is now a 4-second hold: the medic does CPR for
  everyone to see, and it stops if you let go or walk away (demo: `node agent-box/session.mjs revive-anim-proof`, #512).
- **Report to staff.** After something happens to you, type `/report` in chat.
  It flags the incidents from the last ten minutes that involved you, as either
  party, for admins to see in the Game Master incident view. You get a "Staff
  were told" toast; a second report within two minutes is refused.
- **Spawn square dressing (area 1 polish).** The payphone stands on the square beside the handcart, front to the west, toward the spawn.5, 240.5). The clerk has a desk and chair beside him, and a handcart stands by the benches on the north side.
- **Safe square.** On the spawn square you cannot hurt another player and they
  cannot hurt you. Weapons still fire; NPC, world, and self damage pass.
- **Hold up.** Two players face each other within 2 m; the one with a weapon in hand sees *Hold up* in the action menu. It raises their hand, beeps at both, and tells the other player. It changes nothing else: it only lands in the incident log (Game Master incident view, kind Hold-up). Demo: `node agent-box/session.mjs holdup-proof` (two clients).
- **Admin actions.** As Game Master, teleport and heal a player, or type `!wadd POLICE <uid>`: each appears as an "Admin action" row with your name in the Game Master incident view. Non-admins never get that view.
- **Aim intent.** Hold a raised weapon on another player for 3 s: one "Weapon aimed" row in the Game Master incident view, then at most one more per 3 s of continuous aim. Needs two clients on a dedicated server to prove the aim direction replicates.
- **Lawless HUD tag (#591).** Walk into the quarry (zone centre 178 0 172, radius 32): a red LAWLESS AREA tag shows top centre and a toast says you entered; walk out and both go (exit toast). Spawn or join inside and it shows at once.
- **Lawless quarry.** The quarry south of the dirt lane (about 100 m north of the square) is the one marked lawless area. Two red *Lawless Area* signs stand at the lane and at the east edge; read one for the rules. For now the zone only labels incidents (`lawless` in the incident log) and keeps wanted on; there is no real gate yet, only a lane barrier of fence rails and cones with the Mine Hazard sign, a gatehouse shed, three granite cliff walls round the pit, stones, spoil heaps, gravel and sand piles, tyres and barrels (area 11 polish). The lane sign now stands at the lane (172, 200). Demo: `node agent-box/session.mjs quarry-proof`.
- **Moonshine camp.** A hidden camp under a stand of spruce east of the chop shop (about 380 to 388, 300 to 305, in the field's treeline): three mash barrels in a row, the still (a base-game metal boiler tank) behind them with a dark furnace beside it, with a fire pit, sack piles, a crate and bottles around it. Walk up to the barrels from the south and use *Load* / *Work* / *Collect* on each, then the still.

- **Evidence.** After a crime, shots near it leave casings and dropped items get a paper marker. Clock in, pick "Bag as evidence" on one, then use "Log evidence bags" at the locker beside the armory in the police station. If the piece was a serial'd fence tool, the buyer's wanted card now shows their name. "Read case file" lists what is on file.

### Known rough edges

- Esc does not close the wanted card or the organization ledger right after
  founding; use the button or walk away (#199).
- If an arrest leaves you standing where you were instead of in the cell, say so.
- Some NPCs may appear half-dressed.
- Sign titles can be hard to read from the road.
- Letting the bleed-out run out, giving up, and disconnecting while downed
  all end in death.

## Not in this build

Do not report these as broken. File them under "build next" if you want them.

- Buying a house
- A general crafting bench
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
- **Shot picker (dev tooling, issue 515).** `tools\cli shots area hospital --pick` takes its wide from the area's `front` heading in `agent-box/areas.json` and ranks each candidate by how well the area's interactables show (`focus` in the same file aims it at named entities) and ray-scores the best six; the `problems:` list measures clips, sunk and reach against the mesh and the pivot; `--wide-only` skips the entity checks and close-ups. Scores are in `tmp/shots/<area>/index.json` (`wideCandidates`, `shots[].frame`).
- **Shot baseline (dev tooling).** `tools\cli shots baseline` shoots every area in one session into the main checkout's `tmp/shots-baseline/<area>/`; `tools\cli shots area bank --baseline` prints the freshest one and its age. Polish builders use it as their before shots (`docs/agent-area-polish.md`).
- **Fuel station (area 5).** The till (rob it with the `Rob the till` action, reward 150) is on the counter at (263.2, 240.4) beside the kiosk wall; the clerk stands behind it facing the street. The fuel payphone is at (256.5, 237) and the FUEL STATION sign at the kiosk corner (264.6, 242.7). The pumps are the map station's own (x 268 and 279, z 235). Try it with `session.mjs` witness-proof or the `crime/rob-reward` test.
- **Police car light bar (#595).** The police car at the station (about 243.5, 246) is a covered UAZ469; its blue light bar sits on the roof. Walk round it and check the bar touches the roof. Sirens: as an on-duty officer drive it and use the siren key (mouse button 2).
- **Police station look (polish pass 6).** The post is the loggia of the police villa (233, 239); the duty desk, armory cabinet, armory store and evidence locker stand along its back wall with the door kept clear, the wanted and dispatch boards stand on the lawn facing the main street, and the payphone, a bin and a flowerpot stand on the front lawn. No setup needed; walk up from the main street.
- **Farm (polish pass 12).** The farm field is north-west of the square at about (150, 298), reached by the dirt track from the south past the farm sign (144.5, 292.5). Cabbage beds and plants on the west side (`CabbageCrop_Debug`, gather), a greenhouse and hay bales on the east, a produce stand with crates, a cart and cabbages on its table by the sign, and a fence along the north edge. The apple orchard is to the west (the gather tree at 130, 310); its juicer (`AppleProcessor`) now stands on a press table at (141, 311) with crates. Try it: `node agent-box/harness.mjs flow chain-apple` (gather, trade, press), or walk up and use *Gather Cabbage*. The apple and plum traders stay on the market table row (area 3).
- Debug money top-up: in the playtest town you never run out; bank refills under 100k to 1M and cash under 5k to 20k on spawn and every 60 s (ledger reason debug-topup; flag m_bDebugMoneyTopUp).
- **Smelter furnace.** The DebugWorld mining yard furnace is now a base-game boiler (was a wood chipper) at 169.8, 171.5, beside the mining sign. Bring two iron nuggets and use *Smelt Iron Nuggets* at its front; the bar drops 1.6 m in front. Also at 298.4, 276.5 (crime chain) and in St Philippe.
- **Game Master (#617).** On the debug mission every player can open Game Master; in the town it is administrators only. Try: `node agent-box/session.mjs up --mission debug` then `node agent-box/harness.mjs gm`. See `docs/game-master-spike.md`.
