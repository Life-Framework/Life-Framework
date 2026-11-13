# Playtest: Life Framework

The playtest is a small 1989 Everon town, published on the Arma Reforger
Workshop and hosted from the maintainer's PC. Play it, then tell us what broke.
Feedback goes to #playtest-feedback on [Discord](https://discord.gg/MjMexJteqz) or to a GitHub issue.

Licence: the Workshop build is under the Arma Public License (APL); the source on GitHub is MIT.

Decision 2026-11-11 (#567): outsiders get the build from the Workshop upload (Unlisted, 0.1.0, see
[playtest/workshop-publish.md](playtest/workshop-publish.md)), and from the website zip until that item is published.
Until the Workshop item exists, the playtest is a download, and the Workshop steps below do not apply yet.

## Download and play

1. Press **Download the playtest** on [life-framework.org](https://life-framework.org/playtest). It is
   `LifeFramework-Playtest.zip` from the newest GitHub release:
   <https://github.com/Life-Framework/Life-Framework/releases/latest/download/LifeFramework-Playtest.zip>.
2. Unzip it anywhere.
3. Double-click **Play Life Framework**. It finds Arma Reforger through Steam and asks:
   - **Play on my own**: the playtest town (`Missions/LF_PlaytestTown.conf`) hosted by your own game on
     `127.0.0.1:2302`.
   - **Join the community server**: reads <https://life-framework.org/playtest-server.json> and joins the
     address in it. With `"address": null` it says nobody is hosting and points to Discord. When the file
     names a `release` other than the download's `version.txt`, it asks the player to download again.

The launcher logs to `%LOCALAPPDATA%\LifeFramework\launcher.log`.

### Releasing and hosting (maintainer)

- **Cut a release:** in a worktree at `origin/main`, `tools\cli playtest release` (next `playtest-m2-<n>`, or
  `--tag`). The game refuses every resource `resourceDatabase.rdb` does not register (`Wrong GUID/name`, and
  the world does not load; seen 2026-11-10), and only Workbench writes that file, so this step runs where the
  tools are: it refreshes the rdb, creates the tag's release as a pre-release with the rdb attached, and starts
  `.github/workflows/playtest-release.yml`. The workflow checks the rdb against the tag's `.meta` files, packs
  the zip with `tools/release/pack-playtest.mjs` (the tracked files under `addons/LifeFramework`, nothing
  generated, plus the rdb and the launcher from `tools/release/launcher/`), makes the release **latest**, and
  rebuilds the website so the button shows the new version and size. Any other release must stay a
  pre-release, or the stable link moves to it.
- **Point the launcher at a host:** `tools\cli playtest server <address[:port]>` when the host is up,
  `tools\cli playtest server off` when it stops. It opens an auto-merge PR that rewrites
  `website/public/playtest-server.json`; the site serves it a few minutes after the merge. The host must run the
  release's commit: a different build has a different script checksum and the game refuses the join.

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
   Talking to the clerk is a conversation scene (#896): the camera holds one
   over-the-shoulder shot the whole time, the lips and jaw follow the voice, and
   he blinks and nods. Demo: `node agent-box/session.mjs dialogue-proof` (debug mission).
2. **Job.** Sign up at the post office counter and deliver the parcel. Five
   parcel runs open fuel delivery, fifteen open the taxi.
3. **Bank.** You start with $1,000 in the bank. Deposit and withdraw at the bank teller: walk to the west counter window on the bank front at (176, 259), where Edith stands behind the counter under the *Teller* sign, and use *Talk to the teller*; it opens the same deposit and withdraw screen the ATM did (demo: `node agent-box/session.mjs bank-teller-proof`). The old ATM entity is kept hidden inside the bank for the tests; the east counter at (187, 259) is a plain counter.
4. **Shop.** The general store sells food, drink, and a dressing from a price
   sheet.
5. **Vehicle.** Read the price sheet at the car lot (desk at 211, 262; four display cars in a row at z 261, each with a chalkboard price board in front, x 216 to 230) and buy a car. Your character
   also starts with an S105 in the garage and a UAZ at the impound lot (playtest
   only): read the garage ledger, retrieve one, and collect the other at the
   impound desk for the fee. The key goes straight into your inventory; drag it and your ID card between pockets to check nothing hangs (#620).
6. **Police.** On a fresh save the first player to join is on the police roster.
   Clock in at the station desk; the kit includes the key to the station car. A
   second player robs the fuel till: the officer gets the radio dispatch, reads
   the wanted board, arrests the robber, and the robber goes to the cell. Clock
   out at the same desk to swap back.
   Then walk to the back room (black-market stall, 307, 279) on duty and use "Search back room"; the stock goes to the evidence locker and the stall shuts for 10 min. Demo: `session.mjs backroom-police-proof` (2 clients).
   Rank ladder (#754a): `LF_Police.conf` `m_aRanks`. Promote a cop (a second officer, or 3600 s on duty) and check the fine cap on the ticket form and the armory kit change; pay at clock-out uses the rank row.
   The duty kit includes a radio tuned to the police frequency (41000 kHz; the
   medic kit is on 42000). Two officers hear each other on it; a civilian
   does not.
7. **Organization and hideout.** Found an organization at the registry desk,
   deposit cash, and buy the processing upgrade. Take the alley yard lease, open
   the hideout door, and store something in the stash. Run contraband from the
   barn to the drop (in the debug game mode a second crate cage 27 m south of the barn pallet at `466 0 304`; the flagship keeps the pub).
   Where: the walled yard at the end of the cobbled alley (about 302, 288, door on the alley); inside are the machine, the stash, a workbench, crates, barrels and a parked van. The registry desk is at 289, 256 and carries a phone, files, notebooks and a clock, with an office chair behind it (props on the desk top at y 0.907). Demo: `session.mjs org` (proof `agent-box/proofs/org.mjs`) (2 clients: found the org, take the lease, open the gate).
   Courier parcel: take a run at the post counter, then open the inventory; the parcel's description says where to deliver it (the porch past the brick-walled lock-up). Demo: `session.mjs parcel-proof`.
8. **Restart.** Leave and rejoin: your cash, bank, job, organization, and cars
   should be back.

### M2 additions

The M2 systems run in the same playtest build. The money ledger and incident
log that back them stay on the server and invisible.

- **Mining.** The miner stand at (176, 167) has ore rocks and a display crate. Swing a pickaxe at a rock: it shakes, shrinks a step per piece, and drops nuggets that rest on the ground; the last piece makes it vanish and it grows back after its cooldown. Open the inventory to check the nugget and pickaxe previews against the gold bar. The rock sinks as it shrinks, so tip your aim down a little after each piece. Demo: `node agent-box/session.mjs mining-proof` (equips the pickaxe, mines the iron rock piece by piece until it vanishes, then smelts four nuggets at the boiler; add `--regrow` to wait the 5 minute cooldown and see it grown back).
- **Payphone.** The booth by the square takes one coin from the cash you carry. Four booths stand in town (square beside the handcart, fuel station at the kiosk, hospital and police station against their front walls). A booth's open side is local -Z, so each one is turned with its back to the wall or the lawn and its door to the street. Area 8 and 17 polish: the hospital and police booths were facing their walls (now back to the wall, door to the street); the Red Cross banner was buried in the hospital wall (only its ropes showed) and now hangs in front of it, the *Ambulance bay* sign no longer stands in the ambulance's lane, and the police whitelist plaque is on the station wall under the *Police Station* sign (236.6, 241.2) instead of lying on the grass.
  Pick *Report a crime*, *Report an emergency*, or *Report someone suspicious*.
  An on-duty officer hears it on the radio and sees the line on the wanted
  board. One player can call once a minute. With nobody on duty nothing
  connects and no coin is taken.
- **Where the investigation gear stands.** The fence (Vince) is behind the barn at (186, 311), north-west of the bank, away from the street. The bank vault door (a steel door set on the bank's east wall, with an alarm bell box above it) is at (193, 265) and opens only to a fence tool. The cash van starts at the bank doorstep, in the north lane at (196, 252.4) facing west, then visits the general store (162) and the fuel station (255) in the south lane (z 248.3) along the main street. Its cash box (a vanilla steel trunk) stands on the ground at the van's kerb-side door; it opens to a drill and sets three money sacks down in a row beside it, on the side away from you. A robbery sounds a small beacon on the bank's bell box or on the van roof. The evidence locker (235.65, 238.15, between the two doors), the armory store (231, 238.15), wanted board and dispatch board are at the police station. The payphone witness and the officer's "Ask what you saw" are covered under Comms and Evidence above.
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
- **Jail work (#127).** Get arrested (the jail-me board, or a robbery and an officer), then in the cell use *Sort the laundry* (basket) or *Scrub the floor* (bucket) and hold still 5 to 6 seconds: the job takes 20 or 30 seconds off the remaining sentence and a toast shows the time left (also on the slip in your inventory). A repeat of the same job inside its cooldown is refused with the wait time. Demo: `node agent-box/session.mjs jail-work-proof` (run `session.mjs roster 2` before `up --clients 2`).
- **Steal a car.** A beige car parked east of the lock-up (about 312, 246) is locked and will not start without its key. With a lockpick, use *Pick the lock* at its door, get in, and use *Hotwire the ignition* at the wheel. It takes 6 to 15 seconds and can fail; a failure may be heard. Once it works, start the engine as usual and drive off. The car is marked stolen on the server. If you own an S105 (the starter garage car) a thief can take it the same way: with its key in your pocket, use *Report the car stolen* at it and the plate goes on the police wanted board as a stolen car. The chop shop yard is a walled back lot north-east of the lock-up, behind the chain barrels (about 322, 300), with a blue engine crane (walls on three sides, open at the south gate; a stripped jeep shell, doors, tyres, drums and a tool bench stand around it): park a stolen car beside it, get out, and use *Take this car apart*, hold *Strip it down* until it is bare (more hands are faster), then *Take the cash and parts*. It pays 30 percent of the car value (an S105 pays $300) and two spare parts a trader will buy; the car is gone for good, the crime is reported, and the yard takes nothing for five minutes. It will not take a car that is not stolen, or one you hold the key to; the toast says so ("Only stolen cars. Lockpick or hotwire a car that is not yours and bring it here.") and the car is left untouched. Hot means the engine was started without its key on a locked-ignition car (the hotwire above); a base-game road car with no ignition lock, or one you simply got into and drove, is not hot (#601). An on-duty officer can use *Check the plate* at any car (a reported one reads STOLEN) and *Recover stolen vehicle* at a reported, empty one, which returns it to the owner's garage. A witness can describe the thief.
- **Crime penalties are one table (#685a).** What each crime adds to your wanted level, the ticket fines and the repeat rule are in `Configs/Crime/LF_CrimeCatalog.conf` and the owner's `crime` section, not in the town. To see it: rob the fuel till (wanted 1), steal a weapon (2) or a car by force (3), kill someone (3) and read your wanted card; an owner file with `"crime": {"crimes": {"#LF-Offence_Robbery": {"wantedAdd": 2, "repeatThreshold": 2, "repeatWantedAdd": 1}}}` makes the third robbery add 3. `/configreload` as an admin applies it.
- **Medic duty.** Clock in at the duty desk in the hospital (the punch clock,
  open to everyone, no roster). The *Clock in* / *Clock out* row sits on the clock itself, first in the list (it flips by your state), above *Pay bill*. You swap to the medic kit (uniform, medical
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
- **Spawn hub (#607).** The small table on the spawn square (about 119, 243.5, a few steps from where you spawn) carries a globe: use *Travel to a playtest area* to open a scrollable list (police station, bank, jail, fence, farm, orchard, market, mining, quarry, garage, car lot, chop shop, fuel station, hospital and more) and pick *Go* to be teleported there. The same globe still sets day, night and rain or clear weather (moved here from the test annex), and the survival food and water trees stand at 112, 262. Playtest game mode only: the server refuses it elsewhere. New areas are one row in `Configs/Debug/LF_SpawnHub.conf` plus a `LF-SpawnHub_Pt_<id>` string. Demo: `node agent-box/session.mjs spawn-hub-proof` (add `--shot` for screenshots).
- **Safe square.** On the spawn square you cannot hurt another player and they
  cannot hurt you. Weapons still fire; NPC, world, and self damage pass.
- **Hold up.** Two players face each other within 2 m; the one with a weapon in hand sees *Hold up* in the action menu. It raises their hand, beeps at both, and tells the other player. It changes nothing else: it only lands in the incident log (Game Master incident view, kind Hold-up). Demo: `node agent-box/session.mjs holdup-proof` (two clients).
- **Admin actions.** As Game Master, teleport and heal a player, or type `!wadd POLICE <uid>`: each appears as an "Admin action" row with your name in the Game Master incident view. Non-admins never get that view.
- **Aim intent.** Hold a raised weapon on another player for 3 s: one "Weapon aimed" row in the Game Master incident view, then at most one more per 3 s of continuous aim. Needs two clients on a dedicated server to prove the aim direction replicates.
- **Lawless HUD tag (#591).** Walk into the quarry (zone centre 178 0 172, radius 32): a red LAWLESS AREA tag shows top centre and a toast says you entered; walk out and both go (exit toast). Spawn or join inside and it shows at once.
- **Lawless quarry.** The quarry south of the dirt lane (about 100 m north of the square) is the one marked lawless area. Two red *Lawless Area* signs stand at the lane and at the east edge; read one for the rules. For now the zone only labels incidents (`lawless` in the incident log) and keeps wanted on; there is no real gate yet, only a lane barrier of fence rails and cones with the Mine Hazard sign, a gatehouse shed, three granite cliff walls round the pit, stones, spoil heaps, gravel and sand piles, tyres and barrels (area 11 polish). The lane sign now stands at the lane (172, 200). Demo: `node agent-box/session.mjs quarry-proof`.
- **Moonshine camp.** A hidden camp under a stand of spruce east of the chop shop (about 380 to 388, 300 to 305, in the field's treeline): three mash barrels in a row, the still (a base-game upright boiler tank) behind them with a dark furnace beside it, with a fire pit, small sack piles (scaled to sit lower than the barrels), a crate and bottles around it. Walk up to the barrels from the south and use *Load* / *Work* / *Collect* on each, then the still.

- **Farm plots (#614).** At the farm (about 148, 299) stand three dirt plots, a seed crate and a *Vegetable plots* chalkboard (read it). *Take Cabbage seeds* at the crate (four packets, then it refills after two minutes), walk to an empty plot and *Plant Cabbage seeds*. Six sprouts appear; after a minute they are half grown and after two minutes full cabbages (the prompt reads *Growing: sprouts* / *Growing: half grown* until then). *Harvest Cabbage* gives three cabbages and empties the plot. Sell them at the general store (*Sell these*). A planted plot keeps its growth across a server restart (growth only counts while the server runs). Demo: `node agent-box/session.mjs farm-proof`.

- **Evidence.** After a crime, shots near it leave casings and dropped items get a paper marker. Clock in, pick "Bag as evidence" on one, then use "Log evidence bags" at the locker beside the armory in the police station. If the piece was a serial'd fence tool, the buyer's wanted card now shows their name. "Read case file" lists what is on file.
- **Photo identity card (#902).** Sign form R-89, then talk to the town clerk on the spawn square (Quest_Clerk, about 134, 243) and pick *I am new in town.*: you are handed your identity card. Open the inventory, hover the card and press Inspect: a card sheet shows your head and shoulders in the clothes you wore at the desk, your name, age, date of issue, card number and a violet registry stamp. Face another player and pick *Show ID*: the same card opens on their screen for about 8 s. As an on-duty officer, pick *Request ID* on someone: they get an ID CHECK sheet and can show or refuse. Drop the card (or let someone take it), talk to the clerk again and pick *I have lost my identity card.*: 50 in cash buys a new card with a new number; the old one now reads VOID - REPLACED. Demo: `node agent-box/session.mjs photo-id-card-proof`.

### Known rough edges

- Esc may not close a sheet the server opened (wanted card, ledger after
  founding); press it again or use the close button. #199
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
- **Directed camera rig (dev tooling, #392).** With a session up (`session.mjs up --clients 1 --mission playtest --profile full --res 1920x1080 --wait`, then `harness.mjs --client 1 flow ui-intake`), `tools\cli film-make video/camera-test.json --no-vo` shoots five 4 s shots of the town square and street (first person, follow, orbit, drone, static) with no HUD and joins them into `agent-box/reports/film/camera-test.mp4`; `harness.mjs --client 1 camera orbit 125 0 250 16 8 270 140 7` plays one move by hand.
- **Shot picker (dev tooling, issue 515).** `tools\cli shots area hospital --pick` takes its wide from the area's `front` heading in `agent-box/areas.json` and ranks each candidate by how well the area's interactables show (`focus` in the same file aims it at named entities) and ray-scores the best six; the `problems:` list measures clips, sunk and reach against the mesh and the pivot; `--wide-only` skips the entity checks and close-ups. Scores are in `tmp/shots/<area>/index.json` (`wideCandidates`, `shots[].frame`).
- **Shot baseline (dev tooling).** `tools\cli shots baseline` shoots every area in one session into the main checkout's `tmp/shots-baseline/<area>/`; `tools\cli shots area bank --baseline` prints the freshest one and its age. Polish builders use it as their before shots (`docs/agent-area-polish.md`).
- **Fuel station (area 5).** The till (rob it with the `Rob the till` action, reward 150) sits on a counter at the kiosk's south service window, about (283.6, 237.2), under the canopy (the kiosk walls are solid, so the old spot inside was unreachable, #729). Stand south of the counter facing the till: the prompt shows there with the till in front of you and the clerk behind the window mesh. `node agent-box/qa-m2.mjs` / `harness.mjs stand Roadmap_Fuel_Till s` then `look` lists Rob the till. The fuel payphone is at (256.5, 237) and the FUEL STATION sign at the kiosk corner (264.6, 242.7). The pumps are the map station's own (x 268 and 279, z 235). Try it with `session.mjs` witness-proof or the `crime/rob-reward` test. The fuel payphone is at (256.5, 237) and the FUEL STATION sign at the kiosk corner (264.6, 242.7). The pumps are the map station's own (x 268 and 279, z 235). Try it with `session.mjs` witness-proof or the `crime/rob-reward` test.
- **Police car light bar (#595).** The police car at the station (about 243.5, 246) is a covered UAZ469; its blue light bar sits on the roof. Walk round it and check the bar touches the roof. Sirens: as an on-duty officer drive it and use the siren key (mouse button 2).
- **Police station look (polish pass 6).** The post is the loggia of the police villa (233, 239); the duty desk, armory cabinet, armory store (231, 238.15) and evidence locker (235.65, 238.15, between the two doors) stand along its back wall with the doors kept clear, the wanted and dispatch boards stand on the lawn facing the main street, and the payphone, a bin and a flowerpot stand on the front lawn. No setup needed; walk up from the main street.
- **Sign cards (printed notices).** Look at any sign and press the interact key (*Read: <title>*). The card now has a large title with a rule under it, then labelled sections (WHAT IT IS, HOW, YOU NEED, RULES, NEXT) with bulleted lists, action names in colour, and the frame colour of a painted sign. Try the *Town Board* at the spawn square, the *Post office*, and the long police station sign; Esc or *Put it back* closes the card. Demo: `node agent-box/session.mjs up --machine lf-testbox --clients 1 --mission playtest --wait`, then `node agent-box/harness.mjs stand Sign_TownMap`, `node agent-box/harness.mjs use LF_SignReadAction Sign_TownMap`, `node agent-box/harness.mjs ui-shot card`.
- **Farm (polish pass 12).** The farm field is north-west of the square at about (150, 298), reached by the dirt track from the south past the farm sign (144.5, 292.5). Three dirt plots on the west side (`FarmPlot_1..3`, see *Farm plots* above), a greenhouse and hay bales on the east, a produce stand with crates, a cart and cabbages on its table by the sign, and a fence along the north edge. The apple orchard is to the west (the gather tree at 130, 310); its juicer (`AppleProcessor`) now stands on a press table at (141, 311) with crates. Try it: `node agent-box/harness.mjs flow chain-apple` (gather, trade, press), or grow cabbages in the plots. The apple and plum traders stay on the market table row (area 3).
- Debug money top-up: in the playtest town you never run out; bank refills under 100k to 1M and cash under 5k to 20k on spawn and every 60 s (ledger reason debug-topup; flag m_bDebugMoneyTopUp).
- **Car lock polish.** With a keyed car: Get in is first in the car's menu, then Lock/Unlock (label follows the state), then Store in garage (owner on foot within 15 m of the garage doors, about 238, 259). Press U seated or within 6 m on foot to toggle the lock (a click plays). The garage counter now stands at the doors (238, 256.6): Store, Retrieve and Read the ledger show from the doorway.
- **Bank robbery kit (#605).** A chest against the bank's east wall, at 193.5, 262.3 (just south of the vault door). Use *Take bank robbery kit* to get a drill, keycard and charge in one go (a second press adds nothing; with a full inventory the item lands at your feet), then rob the vault at 193, 265 or the cash van. Only works in the playtest game mode (`m_bDebugMoneyTopUp`). Demo: `node agent-box/session.mjs bank-kit-proof` on a one-client session.
- **Smelter furnace.** The DebugWorld mining yard furnace is now a base-game boiler (was a wood chipper) at 169.8, 171.5, beside the mining sign. Bring two iron nuggets and use *Smelt Iron Nuggets* at its front; the bar drops 1.6 m in front. Also at 298.4, 276.5 (crime chain) and in St Philippe.
- Send me to jail (debug): the red *Playtest: jail board* just outside the jail door (about 235.5, 228.5). Use "Send me to jail (playtest)" to serve 60 s solo and try the cell and release (flag m_bDebugJailMe; `node agent-box/session.mjs up --mission playtest`, then `harness.mjs stand Sign_DebugJailMe`).
- **Civilians and witnesses (#610, #615).** Two civilians stand by the fuel station (about 258, 243 and 266, 244): shoot one dead and you are wanted for murder; the clerk and quest givers cannot be hurt. Clock in as police and use *Ask what you saw* on a civilian or the clerk: they give their name and either a statement or a noncommittal line. Demo: `node agent-box/session.mjs npc-witness-proof` (debug mission).
- **Fenced crime site (#608).** In the fenced test annex at about (30 to 48, 50) a row of five stops, four with a chalk board to read: 1 the safe (*Rob the safe*, $200), 2 the weapon rack (*Rob the weapon rack*, an AK-74), 3 the tyre pile (*Steal the vehicle*, an armoured car), 4 the evidence table with the wanted suspect, 5 the practice police desk. Walk the row west to east. The actions read in English (#609). Demo: `tools\cli shots area 40,50 --radius 14`.
- **Quarry works (#618).** Cement, gravel, sand and wood now all sit inside the quarry pit (about 100 m north of the square). From the gate: the *Quarry Works* info board (184, 190) to read the layout; the gravel stand (173, 181), cement stand (162, 181) with the cement plant behind it at the west wall, the sand stand (189, 183), the miner stand and furnace in the middle, and the lumberjack stand, sawmill and three pines on the south side (177, 158.5). Nothing overlaps the cliffs; the stand bags lie side by side on their tables, the cement pile stands clear behind its stand, and the third pine is at (152, 163), west of the pit, so no canopy hangs over the sawmill or the sand dune. Demo: `node agent-box/session.mjs quarry-proof`, or walk the pit and use each stand.
- **Inventory previews (#612).** Open your inventory with an item of each kind (Debug menu kit, or the traders) to see the previews; for all of them at once run `tools\cli shots previews --out tmp/previews/<label>` against a playtest session and read `previews-1.png` and `previews-2.png` (before and after sheets for #612 are in `website/public/docs-images/612/`).
- **Dispatch area and solo test call (debug game mode only).** Between the police station and the fuel station, behind the main street (about 249 to 257, 229 to 233) stands the dispatch area: a police board, a medic board and the dispatch console (a desk with a radio). Clock in as police or medic (a service with nobody on duty answers "No answer"), walk to the console and pick *Test police call* (reported from the fuel till) or *Test medic call* (from the fuel payphone). The call goes out the way a payphone call does: the radio line for police, the pager for a medic, and the board. Then read the board and use *Take the call*. It works only in the debug game mode (GameMode_Roleplay_Debug, `m_bDebugTestDispatch`); anywhere else the server answers "Test calls are off in this game mode".
- **Town sound (#897).** Walk the town with sound on and the places answer. A terrace radio stands on a table right of the pub door (about 122, 287) playing the pub music (10:00 to 02:00; *Switch off* / *Switch on* on it; with it off the muffled music from inside the pub plays instead). The bank counters (177 and 187, 258.7) have a quiet murmur and a clock tick with a till drawer; the hospital has equipment hum at the beds (267 to 270, 261) and a quiet ward at the duty desk (265, 258); each payphone hums faintly while its receiver is up (pick up, or place a call; the hum stops on hang-up); the fuel pumps (273, 235) hum while a vehicle is fuelled; the cash van idles at its stop (the sound comes from its cash box beside the door). Demo: `node agent-box/session.mjs atmosphere-town-proof` on `session up --profile full --sound` (every start and stop is a `[LFDebug:Atmosphere]` line in the client log).
- **Test till (debug).** A counter with a till at (244.5, 230.5), west of the dispatch area, is for trying the till robbery alone: *Rob the till* pays 150 every 5 seconds, needs no police on duty, and only a civilian can use it (clock out first). The robbery raises wanted and radios any on-duty officer, so a second player (or the test call) covers the officer side.
- **Key models (#613).** House keys are a brass key; vehicle keys (including the police car key) are a black remote fob with a steel blade. In a running session `node agent-box/session.mjs keys-proof` creates a character, puts one of each in your pockets and one of each on the ground ahead of you, and shoots the ground view and the open inventory. By hand: buy a house or a car and open the inventory (I) to check the icons.
- **Game Master (#617).** On the debug mission every player can open Game Master; in the town it is administrators only. Try: `node agent-box/session.mjs up --mission debug` then `node agent-box/harness.mjs gm`. See `docs/game-master-spike.md`.
- **Pay at the register (#582) and car lot price boards (#540).** At the general store (about 179, 244) take something off the counter, then walk up to the cash register on the counter and use *Pay for goods* on it (the register itself, not the counter). Try it with `node agent-box/session.mjs store-proof` (two clients on the debug mission). At the car lot the four chalkboards beside the display cars (about 215 to 228, 259) show each car's price from the lot's price list; the board follows the vehicle prices config (`Configs/Vehicles/LF_VehiclePrices.conf`, or a `vehicles` section in `$profile:LifeFramework/config.json`: `{"vehicles":{"vehicles":{"54C3CC22DEBD57BE":{"basePrice":1200}}}}` reprices the S105 at the lot sheet after a restart; #283). Read one to see the card title. The register sits centred on the counter: stand at the counter and check it is wholly on the top. The police duty desk clock and the dispatch console radio sit on their desks the same way; `toolscli validate` runs `validate-prop-on-surface` for all of them.
- **Police application (#128).** At the police station duty desk (the clock) a civilian sees *Apply to the police*: a short form (name, why, availability); *Hand it in* gives a receipt toast and writes `$profile:LifeFramework/police_applications.txt`. On the playtest town the application is approved on submit (`m_bLF_InstantPoliceApproval`), so the *Clock in* works at once; the first player is already police, so try it as the second player, or delete that line from `whitelist.txt` first. Elsewhere an administrator picks *Review applications (admin)* at the desk to approve the oldest one. Demo: `node agent-box/session.mjs up --clients 2 --mission playtest` then `police-application-proof` (fresh session; it needs client 2 off the roster).
- **Key previews and teller (QA #679).** The house key and vehicle key inventory previews now fill their cell (a brass key, a black fob with red buttons); check them with `node agent-box/session.mjs keys-proof` (debug mission) and open the inventory. The bank teller stands centred behind the west counter and *Talk to the teller* reaches her from the front of the counter (radius 2.5 m); the police car's rear plate sits left of the spare tyre.
- **Vehicle prices (#688).** No new object: `Configs/Vehicles/LF_VehiclePrices.conf` holds the same prices the car lot sheet (211, 262), the chop shop ($300 for an S105) and the impound desk ($250) already show; to see it run, read those three. The test `vehicles/price-model` proves the equality.
- **Core no longer names the village (#323 batch 0d).** Nothing new to look at: the town behaves as before. The two things worth a glance on the playtest town: a new character still gets the garaged and the impounded test car (car lot at 215 to 228, 259), and `node agent-box/session.mjs up --mission playtest` then the `journey` stash and garage steps still find the hideout stash and the garage (`tools/cli test --no-build --only LF_Test_HideoutStashFind,LF_Test_JailPlacedCell` proves the lookups). The new annex props (hospital till and cabinet at 57, 40 to 46; convoy markers at 57 to 63, 70) are test fixtures only.
- **Owner config check (#281).** Not in the town; it is a tool. Run `tools\cli config-check docs\config\config.example.json` (clean, exit 0), then on a copy with a price set to `"cheap"` or `-5` to see `line:col` errors the server would log; `node tools/validation/config-check-selftest.mjs` runs the fixtures.
- **Config batch 1 (#284).** Nothing new to look at: behaviour is unchanged. To see an owner knob work, edit `addons/LifeFramework/Configs/Organizations/LF_Org.conf` (`m_iFoundingFee`) and read the Organization Registry desk in the town: the founding price on its button follows it. `tools\cli test --no-build --only LF_Test_ConfigConstantsBatch1` pins every moved default.
- **Per-proof reset (dev tooling, #867).** With a playtest session up, rob the fuel till with `session.mjs witness-proof`, then `node agent-box/harness.mjs --client 1 resetcrime` (or `session.mjs reset`): wanted, posters, dispatch log and till and vault cooldowns clear, so the next proof runs in the same session.
- **Taser sounds and stun buzz (#664, #341).** Needs ears. Take the taser from the police desk, draw it, and fire at a second player within 10 m: a pop and about a second of electric crackle, not a pistol shot, and the stunned player buzzes for the 8 s they are down, heard by both and fading with distance. Pull the trigger on the spent cartridge (a plastic click), then reload (the cartridge out, a new one in, no slide sounds). The files: `addons/LifeFramework/Sounds/Weapons/Handguns/Taser/Samples/`. `session.mjs taser-sound-proof` on `up --clients 2 --profile full --sound` replays it without ears and records client 1.
- **Taser v3 model (#664, #341).** On the police desk at the station (`Roadmap_Police_Taser`, 232.9, 240): pick it up and draw it (handgun slot); the model is the boxy black stun gun with the yellow dart cartridge (picked from 10 concept images, then Meshy image-to-3D) and the fingers close around its grip. `node agent-box/session.mjs up --clients 2 --profile full`, then `session.mjs taser-proof` replays take, stun, give-up refusal and wake-up and shoots the held weapon.
- **Material classes (#664).** The taser, handcuffs and spike strip maps come from a `materials` block in each spec instead of a tone-map script: the taser reads matte black plastic with a yellow nose. Try: `node agent-box/session.mjs up --clients 2 --profile full` then `taser-proof` (held taser) and `harness.mjs previews show <prefab>`; the strip lies at 234, 245.4 (`toolsi look 234,245.4`). A new asset declares its materials in its spec ("Materials" in `docs/asset-pipeline/README.md`).
- **Held cuffs and spike strip look (#664, #646).** Handcuffs: clock in at the police desk (`Roadmap_Police_Desk`, 232.9, 240; the kit carries them), take them into the left hand from the quick slots; they hang from the fingers by the chain. Demo: `node agent-box/session.mjs roster 1`, `up --clients 2 --profile full`, `cuffs-held-proof`. Spike strip: lies on the grass at 234, 245.4 beside the station, black rubber with steel spikes (`tools\cli look 234,245.4`).
