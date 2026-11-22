# Law, police and prison: experience scope

Milestone: M7 (By the book, Behind bars, Making detective), with groundwork that can land in M6
Extends: [jailing](jailing.md), [ranks](ranks.md), [police missions](../later/police-missions.md),
[jailbreak](../crime/jailbreak.md), `Feature/Police/`, `Feature/Medic/`, `Feature/Qualifications/`,
`Feature/License/LF_LicenceSeizure*`, `Configs/Crime/LF_CrimeCatalog.conf`
Epics this plan feeds: #4151 (law book and charge sheet), #1027 (Behind bars), #798 (prison break),
#1035 (Making detective), #652 and #653 (medic ranks), #4155 (livery pipeline), #4140 (professions)

This is a design plan. It creates no issues and dispatches nothing. The integrator folds it into the
week plan; the maintainer approves before any card is cut.

## What exists today (0.2.x)

The police loop works end to end. Use it as the base and don't rebuild it.

- **Duty:** the duty desk in the Saint-Philippe police villa (b32, `Villa_E_2I01_Police`, 4553, 10748).
  Clocking in swaps clothes into the kit. Pay is cash at clock-out, by the second, and pauses while the
  officer stands idle (#3120). Five ranks from Cadet $900/h to Lieutenant $1,260/h, promoted by time on
  duty or by a higher rank at the armory.
- **Street work:** search with seizure, cuffs (on a downed, stunned, held-up or hands-up suspect), arrest
  of a wanted player, detain-and-escort in a UAZ469 police car, paper fine tickets at fixed amounts from
  the crime catalog (3 unpaid at most, doubling after 30 online minutes, then +1 wanted), spike strips,
  roadblocks, the taser.
- **Jail:** a holding cell in the villa. 60 s per wanted level, at most 5 min, online time only, kept
  through a restart. Three work stations (laundry tub, sink, broom) take time off. A bail phone pays
  from the bank at $150 a minute left, $500 minimum. Booking and a prisoner HUD clock (#4097).
- **Breaks:** two-charge jailbreak (outer door, then cells) with a 1 h cooldown. Prison-break rules
  (#3420), the break window (#3421) and escapee posting (#3422) are in-dev and ship off.
- **Calls:** payphones report a crime, an emergency or someone suspicious. Calls reach the dispatch
  board, the pager and the radio. Site alarms (bank, tills, cash van, armory) go to dispatch. A fine at
  the scene completes the call and pays (#2478).
- **Records:** wanted board, case board, conviction record (#1914), licence seizure on arrest (#3432),
  training desk courses (detective, traffic; #3426).
- **Medic:** 3 min bleed-out, revive, carry to the clinic, four ranks from Orderly $792/h to Doctor
  $1,224/h, a medic dispatch board.

What is missing is the reason to care: the fine is the same flat number whatever happened, the cell is a
5 minute wait, a call is a line on a board, every rank drives the same car, and nobody off duty knows the
police exist until they are arrested. This scope makes the law something the island can see, read and
feel.

## 1. Player fantasy and core loop

**Hour 1 (civilian or new officer).** A new player meets the law before they break it: they walk past
the Gendarmerie's lit fascia, the wanted posters on the board outside, and a patrol car with its light
bar parked in the yard. If they speed through the square, an officer with a radar gun waves them down
and writes a $75 ticket they pay at the front counter. That is a lesson, not a punishment. A player who
signs up as a Cadet that hour clocks in, gets a beige S105 patrol car with "POLICE" on the doors, and
the radio crackles with their first call. It's usually a payphone report on the main street that they
can reach in a minute. They close it with a ticket, hear the call-closed chime, and earn $40 on top of
shift pay. The first hour as an officer is short calls close to the station. The first hour as a
suspect is a ticket and a walk to the counter.

**Hour 10 (Patrol or Senior Patrol, or a repeat offender).** The officer knows the law book. A stop
becomes a real decision: search the trunk or let them go, add "resisting" when the suspect ran, read
the stacking rules on the charge sheet. They drive suspects to the booking desk, book them in the
paperwork screen, and watch the fine total and minutes add up. They have taken the traffic course and
run speed traps on the Meaux road with cones and a radar gun. On the other side, a player with three
arrests knows the charges by heart. They pay a $450 fine to avoid ten minutes in a cell, or they choose
the cell and work it off. At hour 10 both sides get the same thing: the choices have names and prices.

**Hour 50 (Sergeant or Lieutenant, a gang member, a prisoner with a long record).** The Sergeant runs
the shift: they sign off the charge overrides, call a roadblock at the Montignac junction, and take
the prisoner transport van to the Everon Correctional Center inland. A gang knows that van is the
weak point, and the prison break is the server's rare event. It is planned over the prison phone, and
the gang brings the clothes and the getaway car. The Lieutenant flies the police UH-1H over the hills
once they hold a pilot licence, and recaptures pay every officer on duty. A long-record criminal does
real time at the prison: work in the laundry or kitchen for commissary tokens, a cigarette trade in the
yard, and a phone call out. At hour 50 the law is a place on the map with its own society.

**Why it's addictive.**
- **Progression you can see.** Rank changes your car, your kit, your powers (fine cap, override,
  roadblock, transport, air unit) and the stripes on your sleeve. The police skill (#4140) adds perks
  along the way. Commendations go on a plaque at the station.
- **Variety.** No two calls are the same: the payphone report, a site alarm, a traffic stop, a stolen
  car found on the dispatch board, an escort, a transport, a break. They come at different distances,
  with different suspects and different players.
- **Social play.** Every police action has a player on the other end. Criminals need police on duty to
  start the big jobs (the bank needs 2, the break needs 3). A medic and an officer work the same scene.
  The prison is a social space where gang members meet.
- **Stakes both ways.** The suspect chooses between paying and serving time. The officer chooses
  between a clean booking and a chase. The gang chooses between hitting the van and hitting the prison.

## 2. Systems and how they fit

| Shared system | What this area uses or adds |
|---|---|
| **XP and skills** (#4140) | A `police` skill and a `medic` skill on the same curve as civilian jobs. XP comes from outcomes, never from time: a closed call, a booking, a recovered car, a recapture, a revive, a delivered patient. Perks are small and procedural (faster cuffing, longer radar range, a quicker booking, a longer track ping). Rank stays the whitelist ladder; the skill is separate and fills the "what next" card in F2 |
| **Reputation / standing** ([reputation.md](../../architecture/reputation.md)) | Police standing is how the gendarmerie sees a civilian (R5: not for officers). Paying a fine on time and giving a witness statement raise it. Unpaid fines, resisting and escapes lower it. High standing makes the officer's charge sheet suggest "warning" for minor road charges. Low standing makes the officer's pager flag a known face |
| **Wanted level** | Stays the warrant (1 to 5) that makes an arrest legal. The law book does not replace it: wanted says "may be arrested", the charge sheet says "for what, and for how much" |
| **Legal system** | New. The law book (codes) and the charge sheet, below. It reads the crime catalog and the open crime reports, so the charges a suspect faces are pre-ticked from what dispatch already knows |
| **Licences** | Driving, firearm, pilot (from the heli shop, wishlist 11) and truck licence (wishlist 11) can be seized from the sheet. They come back after a retake exam at the licence desk, with a fee. That fee is a sink |
| **Gangs** (gang scope) | Prison break as a gang event (#798). A jailed member calls out on the prison phone. Gang flags appear on prisoners' cell cards. The prisoner transport is an ambush target. Recruiting in the yard is allowed (scroll on a player) |
| **Crime** (crime scope) | Every robbery tier names its police minimum (till 0, cash van 1, bank 2, break 3, reserve 4). Each site alarm is a dispatch call with a type. Drugs feed the narcotics classes A/B/C by amount carried. VIN tampering comes from the chop shop |
| **Vehicles and cargo** | Police and medic fleets by rank, liveries (#4155), the impound lot and tow fee, trunk search, and the prisoner transport van. Stolen-car reports carry the plate (`LicensePlate`), so a stop finds them |
| **Economy and market** | Fines, bail, impound, retake exams and court costs are money sinks into the town treasury, never paid to the officer. Officer pay is the shift plus capped outcome bonuses (balance below) |
| **Map and run chains** | Dispatch calls are shared marks for on-duty police and medics (M10, #2238). The prison, impound and the stations are `LF_Places` kinds. The transport is a run with a start (booking desk) and an end (prison gate) |
| **HUD** | The prisoner clock (#4097), the dispatch toast with call type and distance, and a break timer for everyone involved. Nothing else new: rank is on the uniform, the charges are on paper |
| **F2 player menu** | "My file": open tickets, licences held or seized and when they return, time served, police standing in words. "What to do next" gains "Police duty" and "Medic duty" starter quests. An officer's F2 shows rank, hours to next rank, the courses taken and commendations |
| **Dialogue and voices** | Number-key dialogue with "0. Goodbye" for the desk sergeant, warden, property clerk, impound clerk and bondsman. Every line is voiced through the ElevenLabs pipeline |

## 3. Places

Island places come through the town pipeline (`town draft`, `town fit`, `audit placement`). Coordinates
are from [everon-world.md](../../architecture/everon-world.md). Places marked "scout" need a survey
pass first.

| Place | Where | What it looks like, what's there |
|---|---|---|
| **Saint-Philippe Gendarmerie** (exists) | Police villa b32, 4553, 10748, east of the square | Lit "Gendarmerie" fascia, front counter, wanted board, duty room, armory, evidence locker, case board, holding cell with its three work stations. **Adds:** a **booking desk** in the duty room (a desk with a typewriter (`Typewriter_01_grey`), an ink pad and a height chart on the wall), the **law book** on a lectern at the front counter, a **commendations plaque** in the hall, a **rank board** by the duty desk (names in rank order, printed). Short sentences (up to 10 min) stay here |
| **Meaux police post** | Vanilla police villa in Meaux, 4520, 9468 | Opens with Meaux (M6 state 2). A small post: duty desk, payphone outside, one holding cell, a parking spot for two cars. It covers the south road so a call from Meaux is not a 1.3 km drive from the square |
| **Everon Correctional Center** (new, scout) | A walled military compound inland. First pick: the site at 7447, 6705 between Montignac and Chotain (central, about 4.5 km from Saint-Philippe by road, so the transport is a real drive). Fallback: 5355, 10543 near the hub | Concrete wall and wire on the existing compound, two guard towers (`GuardTower_S_US_01` composition with the military insignia removed), a gate with a barrier and a gatehouse, floodlights. Inside: a cell block (an existing barracks building with `DoorSet_PrisonCell_L_EXT_B` doors), an exercise yard with benches, a **laundry**, a **kitchen**, a **workshop** (license plates, a nod to the period), the **property room** at the gate, the **visitor phone** and the **commissary hatch**. A `Restricted` gated zone on the map (everon-map.md). Sentences over 10 min are served here |
| **Impound lot** | Behind the Saint-Philippe districts garage (claim `saint-philippe-districts-garage`) or a fenced yard on the Meaux road (scout) | Chain-link fence, a gatehouse, a tow truck (`Ural4320` or `M923A1` flatbed, livery "TOWING"), a clerk at a window. Towed cars wait here for a fee |
| **Speed-trap lay-bys** | `road-sp-meaux` lay-bys (M5 state 1), the Montignac junction (state 2) | Pull-outs on the main roads with speed-limit signs (100 open road, 50 town, 30 near markets; US English signs) where an officer parks and runs the radar |
| **Courthouse / town hall** (proposal, see Suggested additions) | The municipal office on the Saint-Philippe square | One room set as a magistrate's office, a bench and a public gallery. Used only for appeals and bail hearings |
| **Clinic** (exists) | Converted town house north of the main street | Adds an **ambulance bay** with the `UAZ452_ambulance` parked and a medic call board |

The jail cell beside the police desk stays the template's `with:` pair (#3099). The prison is a new
`Prison` place kind in `LF_Places`. The core finds it by component and holds no coordinates (golden
rule 9).

## 4. People

All NPCs are base-game characters dressed with vanilla clothing prefabs through `Configs/NpcOutfits`.
Voices go through the ElevenLabs pipeline in US English. A French accent is allowed; the words are
English. Public clips use only voices cleared for public use (ljspeech).

| NPC | Where | Clothing (vanilla) | Voice lines (examples) | Behaviour |
|---|---|---|---|---|
| **Desk Sergeant Marchand** | Saint-Philippe front counter (extends #3875) | `Hat_PeakedCap_USSR_01` recolored police, `Shirt_Turtleneck_01_DarkBlue`, `Pants_Suit_01_black_V1`, `CombatBoots_US_01` | "Paying a ticket? Hand it over." / "You want to join? Fill this in." / "Nobody's on duty. Leave a report at the phone." | Takes ticket payments, takes police applications, says who is on duty. When nobody is on duty he reads the dispatch board aloud |
| **Dispatcher (radio voice only)** | No body. The radio and the pager | none | "All units, robbery at the fuel station, Saint-Philippe." / "Unit two, call closed." | Reads call types and place names. Generated per place name and call type, cached, never live TTS |
| **Warden Duval** | Prison yard | `Hat_PeakedCap_USSR_01`, `Shirt_CottonShirt_01_V2` (grey), `Pants_Trousers_01_DarkGrey`, a radio on the belt | "You've got eighteen minutes left. Keep your head down." / "Laundry needs hands. Ten minutes off if you work it." / "Visiting phone's free." | Patrols the yard on a loop. Talking to him gives the time left, the charges, and the work on offer. Never attacks; in a break he takes cover and radios it |
| **Property clerk** | Prison gatehouse | Same as the warden, no cap, glasses | "Sign here. Your things are in the bag." | On release hands back the stored civilian belongings, minus anything seized |
| **Prison cook** | Prison kitchen | `Shirt_CottonShirt_01_V1_Dirty`, `Pants_Fisherman_01_green` (apron look), `Hat_Knitted_01_black` | "Peel those, then you can sit down." | Gives the kitchen job, comments on the food |
| **Inmates (ambient)** | Prison yard, cell block | Prison issue: `Shirt_CottonShirt_01_V3` + `Pants_Trousers_01_DarkGrey`, or a recolored `Jacket_Denim_01` "work denim" (a pack variant: one color per prison) | "New fish." / "Got a smoke?" / "Warden's coming." | 3 to 5 resident NPCs sit, walk, play cards. They are the yard trader for the cigarette economy (below) and fill the place when only one player is jailed |
| **Impound clerk** | Impound window | `Jacket_Raincoat_01_green`, `Hat_TruckerCap_01_everon` | "That's $150 and the car's yours." | Releases towed cars for the fee, lists what's in the lot on a chalkboard |
| **Driving examiner** (exists as the licence desk, gets a body) | Saint-Philippe licence desk | `Jacket_Suit_01_Grey_V1`, `Pants_Suit_01_grey_V1` | "Your licence was seized. You'll need to retake the test." | Runs the retake: a short drive through the town's marked route with the examiner seated |
| **Bail bondsman Lucien** (proposal) | A back-office in the square (a building in the allocation plan) | `Jacket_Suit_01_Brown_V1`, `Hat_FlatCap_01_darkgrey` | "Short on bail? I'll front it. Twenty percent, paid back by Sunday." | Lends bail to a prisoner who can't pay; a missed repayment adds a debt flag |
| **Gang contact in the yard** (#798) | Prison yard | Inmate clothing | "You want out? Your people need to bring the car." | Starts the break and says what is missing (members, police, cooldown) |

Prisoner clothing on arrest: the cell gives a set of prison clothes and stores the civilian ones (the
duty-locker snapshot pattern, `LF_DutyLocker`). Officers' rank shows on the sleeve: a pack-built
armband or a patch on the turtleneck, one texture per rank (Blender, see Assets).

## 5. Assets

Order: vanilla first, CC0/CC-BY second, our Blender build scripts third. No Meshy here.

| Asset | Source | Prefab or plan | Why |
|---|---|---|---|
| Police car, Cadet and Patrol | Vanilla + livery | `Vehicles/Wheeled/S105/S105_beige` or `S105_darkblue` with a police livery (#4155) and the existing light bar | The Skoda is the period civilian car; a small beige patrol car fits a Cadet |
| Police car, Senior and up | Exists | `LF_UAZ469_Police.et` | Already has the escort actions |
| Prisoner transport | Vanilla + livery | `Vehicles/Wheeled/S1203/S1203_cargo_beige` repainted "POLICE", rear seats as cargo seats | A closed van is what a 1989 gendarmerie would use; the escort seat code already moves prisoners into cargo seats |
| Ambulance | Vanilla | `Vehicles/Wheeled/UAZ452/UAZ452_ambulance` (civilian livery via #4155: white with a red cross and "AMBULANCE") | Vanilla already has it; no build needed |
| Police helicopter | Vanilla + livery | `Vehicles/Helicopters/UH1H/UH1H` (unarmed) with a police livery and the vanilla searchlight `VehicleLight_UH1H_Searchlight` | The livery spike proves a heli first; the vanilla searchlight is the air unit's tool |
| Tow truck | Vanilla + livery | `Ural4320` or `M923A1` with "TOWING" lettering | The tow carries a car through the existing carry/attach system, or the impound teleports it after a tow action (decide in the card) |
| Typewriter, desks, cabinets | Vanilla | `Typewriter_01_grey`, `Desk_01_long`, `Cabinet_Folders_US_01` composition | The booking desk and the records room |
| Cell doors, prison doors | Vanilla | `Door_PrisonCell_E_A_Left_INT_ST_B`, `DoorSet_PrisonCell_L_EXT_B` | Already used by the jail |
| Guard towers, wire, floodlights | Vanilla | `Compositions/Slotted/SlotFlatSmall/GuardTower_S_US_01`, `Sandbag_Wall_BarbedWire_*` for the wire line, `FloodlightGenerator_S_*` | The prison perimeter. Remove military flags and insignia |
| Prison siren | Vanilla | `Structures/Military/Sirens/SirenMilitary_E_01` or `Firestation_siren_01` | The break alarm, heard across the valley |
| Cones, barriers, spike strip, roadblock | Exist | `ConeTraffic_01_red`, `CrashBarrier_E_01_3m`, `SpikeStrip_Item`, `Roadblock_Kit` | Already in the kit |
| Flashlight | Vanilla | `Flashlight_Soviet_01` in the kit | Night searches; a period torch |
| Binoculars | Vanilla | `Binoculars_B8` from Senior Patrol | Watching a suspect, spotting plates |
| Radio | Vanilla | `Radio_ANPRC68` as the carried police radio (#3155) | The dispatch voice comes out of it |
| Police control panel | Vanilla | `Props/Services/Police/PoliceControlPanel_01` | The dispatch desk at the station |
| **Law book** | Blender build | A thick bound book on a lectern, "EVERON PENAL CODE" on the cover in US English | The physical book players read at the counter. No vanilla book prop with a readable cover was found; a box mesh with a cover texture is a short Blender script |
| **Radar gun** | Blender build | Hand-held 1980s radar gun (pistol grip, a box head and a red LED panel) | Speed classes need a measured speed. Nothing vanilla fits. Simple hard-surface item |
| **Breathalyzer** (proposal) | Blender build | A small box with a mouthpiece | Drunk-driving charge, tied to the pub and moonshine |
| **Rank patches** | Blender texture | Armband or shoulder patch textures, one per rank | Rank visible at a glance |
| **Ink pad and fingerprint card** | Blender build | Small desk props | Booking flavor and the detective chapter's prints |
| **Evidence bag** | Blender build | Paper bag with a printed label | Seized items become a bag in the evidence locker |
| Charge sheet, booking card, release paper, commissary token | 2D (UI kit, paper) | Paper layouts on the existing ticket and slip pattern | Paperwork is one of the allowed full screens; the slip and ticket items exist |
| Wanted posters | Exist | `LF_WantedPosters` | Already printed |
| Livery decals | Pipeline (#4155) | POLICE (blue on white or beige), AMBULANCE, TOWING, CORRECTIONS | One template per vehicle UV; lettering in US English |
| **SFX** | ElevenLabs SFX | Typewriter clatter (booking), cell door slam, prison buzzer, distant prison siren, radio squelch and static, pager beep, radar beep, the prison yard ambience (voices, a ball, a radio playing), the kitchen ambience | Audio is always ElevenLabs. The radio squelch makes the dispatch voice read as a radio |
| **Ambience** | ElevenLabs SFX | Station interior: typewriter, phone ringing, a muffled radio. Prison at night: a dog barking, floodlight hum | Places should sound occupied |
| **Voice** | ElevenLabs TTS | All NPC lines above, the dispatcher per call type and place | Every line voiced (first-two-hours rule) |

UI elements, in-world first:
- **Law book:** read at the lectern. One action per section ("Read: Road traffic"), which opens the
  paper page (paperwork is an allowed full screen) and closes with Escape.
- **Rank board:** a printed board, refreshed when ranks change.
- **Commendations plaque:** names engraved after a commendation.
- **Impound chalkboard:** plates in the lot.
- **Prison cell card:** a card by the cell door with the prisoner's name, charges and release time
  (the sentence slip, posted).
- **Charge sheet:** the one new full screen. Paper on the UI kit (`LF_UIScreen`), not a menu.

## 6. Interactions

Every action is an entry on the object's action list (scroll or F). Full screens only where the house
rules allow: the charge sheet (paperwork), the law book page (paperwork), the map, F2, F3.

| Object | Actions | Who |
|---|---|---|
| Suspect (player) | Search · Search vehicle (at a car) · Cuff / Uncuff · Detain for escort · Write ticket · **Breath test** (proposal) · **Check licence** (reads the card, shows seized or valid) · **Give warning** (logs a warning, no fine, +XP for a clean stop) | On-duty police |
| Police car | Put in vehicle · Take out · Siren mode · **Radio: available / busy** (sets the unit's status on the dispatch board) · **Run plate** (the car in front: stolen or not) | Officer |
| Any parked car | **Tow** (illegal parking, abandoned, stolen) · **Inspect** (a road check, finds hidden compartments, #1035) | Officer (tow: Patrol and up) |
| Radar gun (held) | **Measure speed** (aim at a car, gives the speed class) | Traffic course |
| Booking desk | **Book suspect** (opens the charge sheet with the cuffed suspect within 3 m) · **Fingerprint** (flavor, adds to the record) · **Photograph** (mugshot on the wanted poster) | Officer |
| Charge sheet (paper screen) | Tick charges from the law book; the sheet totals fine and minutes and greys out what does not stack; **Offer: pay now** / **To the cell** / **To the prison** (over 10 min) / **Seize licence** (driving, firearm, pilot, truck) / **Sergeant override: jail only** | Officer; override Sergeant+ |
| Suspect's view of the offer | **Pay $X now (cash or bank)** / **Take the time** | Suspect |
| Law book lectern | Read: Criminal / Road traffic / Narcotics / Weapons | Everyone |
| Front counter (desk sergeant) | Pay ticket · Pay impound fee · Apply to the police · Report a crime (in person) · Ask who is on duty | Everyone |
| Payphone / carried radio | Report a crime · Report an emergency · Report someone suspicious · **Call a tow** | Everyone |
| Dispatch call (pager toast, map mark) | **Take the call** (radio action on the pager item) · **Close: resolved / unfounded / no one found** (at the scene) | Officer, medic |
| Cell | Work stations (laundry, sink, broom) · Bail phone · **Read cell card** | Prisoner |
| Prison yard | Talk to the warden (time left, charges, work) · Work: laundry, kitchen, workshop · **Commissary hatch** (spend tokens) · **Visitor phone** (call a player's pager, 1 a sentence) · Talk to the yard contact (#798) | Prisoner |
| Prison gate | **Transfer prisoner** (from the van, starts the prison sentence) · **Release** (property clerk returns belongings) | Officer / prisoner |
| Prison transport van | Put in · Take out · **Log transfer** at the booking desk | Officer, Sergeant+ |
| Impound window | **Pay $150: release <plate>** (one action per car the player owns there) | Owner |
| Licence desk examiner | **Retake: driving test ($100)** / firearm / pilot / truck | Player with a seized licence after the ban days |
| Commendations plaque | Read | Everyone |

### The 911 abuse loop

A payphone or radio call costs nothing and is rate-limited to one a minute. The officer who takes it
closes it at the scene with one of three outcomes:

- **Resolved** (a fine, an arrest, a revive): the caller's police standing +1, the officer gets XP and the
  call bonus.
- **No one found** (the suspect left): neutral for the caller, a small XP for the officer who went.
- **Unfounded** (nothing happened, the caller was trolling): the caller gets an "unfounded call" mark.
  Three unfounded calls in an in-game day file a crime report "Abuse of the emergency line" ($200 fine on
  the law book, +1 wanted when it goes unpaid). The caller's next calls that day are marked "low
  priority" on the board.

The caller's identity is server-side: the payphone records who called, so an officer can't mark a
call unfounded on a whim. "Unfounded" needs the officer at the call's position within 50 m for 30 s,
and one officer's unfounded rate over 50% in a week is flagged for staff. Medic calls use the same
three outcomes.

## 7. Balance

Numbers from `tools\cli economy` (2026-11-22, the Everon pack): civilian runs and chains earn **$400 to
$700 an hour** (parcel $400, postal $480, iron on foot $471, copper by car $514 steady and $701 the first
hour, taxi $600). The duty band rule is **1.5x to 2.5x the best civilian job**, so duty pay with
everything included lies between about **$1,050 and $1,750 an hour**.

### Officer pay

| Rank | Shift pay (today) | Outcome bonus cap | Most per hour | vs best civilian ($700) |
|---|---|---|---|---|
| Cadet | $900 | $150 | $1,050 | 1.5x |
| Patrol | $1,008 | $200 | $1,208 | 1.7x |
| Senior Patrol | $1,080 | $250 | $1,330 | 1.9x |
| Sergeant | $1,188 | $300 | $1,488 | 2.1x |
| Lieutenant | $1,260 | $350 | $1,610 | 2.3x |

Outcome bonuses (paid with the shift at clock-out, counted against the hourly cap):

| Outcome | Bonus | Police XP |
|---|---|---|
| Call resolved | $40 | 40 |
| Call: no one found | $0 | 10 |
| Clean stop with a warning | $0 | 15 |
| Booking (charge sheet signed) | $30 | 50 |
| Prison transport delivered | $60 | 80 |
| Stolen car recovered to the owner or impound | $50 | 50 |
| Escapee recaptured (break event) | $150 to every officer on duty, once per escapee | 150 |
| Escapee killed | $0 | 0 |

Medic outcome bonuses follow the same cap per rank, Orderly $150 to Doctor $300: revive $30 (already
half the $100 revive fee), delivered patient $40, a scene shared with police $20 extra.

The fine never goes to the officer: a fine paid to the arresting officer turns police into fine
farmers. Fines go to the town treasury.

### Fines and time

A civilian earns about **$8 a minute**. A fine is priced in minutes of that work, and jail time in
online minutes.

| Charge class | Fine | Jail option | Work-minutes |
|---|---|---|---|
| Minor road (speeding 1 to 15 over, parking, failed inspection) | $50 to $75 | none | 6 to 10 |
| Road (speeding 16 to 30, reckless) | $150 to $250 | none | 20 to 30 |
| Serious road (over 30, dangerous intervention, no licence) | $300 to $400 | none | 40 to 50 |
| Minor criminal (insult, threat, trespass, disobey, 911 abuse) | $50 to $200 | none | 6 to 25 |
| Property (theft, vehicle theft, VIN tampering) | $200 to $600 | or 5 to 10 min | 25 to 75 |
| Violent and heists (robbery, hostage, mayhem, murder) | none | 15 to 40 min | — |
| Narcotics A / B / C | $150 / $600 or 10 min / 25 min | | |
| Weapons (no licence, no-carry zone, discharge, illegal possession) | $250 to $800 + seizure | | |

These match the values in #4151, which hold the full code list. This scope changes three things:

1. **Two custody tiers.** Up to 10 minutes total: the Saint-Philippe holding cell. Over 10 minutes: the
   prisoner goes to the Correctional Center by transport, and the cap is **30 online minutes**, not 60.
   Thirty minutes of a 2-hour evening session is already a heavy cost; 60 invites logging off.
   (Question P2.)
2. **Repeat offending.** A second conviction for the same code in one in-game day doubles its fine and
   adds 5 minutes. A third is jail only. It reads `LF_ConvictionRecord`.
3. **Work time off.** In prison each job takes 1 minute off for about 45 s of work, up to 40% of the
   sentence. The holding cell keeps today's 8 to 10 s per chore. Work also pays **commissary tokens**
   (below).

### Bail, the bondsman and other sinks

- **Bail** stays $150 a minute left, $500 minimum, from the bank. No bail for murder or for a prisoner
  waiting for a break. Bail only works in the holding cell and the prison's first 5 minutes; after that
  the prisoner serves (bail must be a real decision made early).
- **Bondsman** (proposal): fronts bail for 20%, repaid within 3 in-game days; a missed payment files an
  "unpaid debt" (+1 wanted). A sink and a loan system in one.
- **Impound:** $150 release, $25 a day after the first, sold at auction after 7 days (feeds the car lot).
- **Licence retake:** driving $100, firearm $250, pilot $600, truck $300, after the ban days of
  `LF_LicenceSeizure_Everon1989.conf`.
- **Commissary:** tokens are earned in prison only (5 a job) and spent there (a coffee that halves the
  next work time, a phone card for a second call, a cigarette pack that buys a yard rumor from an inmate
  NPC: which cash-van stop is quiet tonight). Tokens never leave the prison and expire at release, so
  they are not money.

### Risk, reward and cooldowns

| Item | Value | Why |
|---|---|---|
| Call rate per caller | 1 a minute (today) | Flood guard |
| Unfounded marks to a charge | 3 a day | Trolls pay; an honest mistake doesn't |
| Ticket requests per officer | 3 per 10 s (today) | Rate limit |
| Unpaid tickets per person | 3 (today) | Pressure to pay |
| Jailbreak (two charges) | 1 h cooldown (today) | Keep |
| Prison break event | Server-wide 90 min, one at a time, needs 3 officers on duty, a jailed member with 25+ min left | Wishlist 12 |
| Prison transport ambush | Counts as a jailbreak for the cooldown; +4 wanted on the attackers | One break door, two places |
| Officer abuse | A booking needs the suspect cuffed and wanted, or a resolved call. A sheet over the rank cap is refused. An override is logged with the Sergeant's name | Authority is a money path (interrogate) |
| Killing a prisoner in transport | Murder charge for an officer too; on-duty status gives no immunity | No police death squads |

## 8. Slices

Six chapters, each playable and fun at its end. Chapter 1 and 2 can start in M6 if the maintainer
wants police depth earlier. The rest is M7.

1. **By the book** (#4151): the law book, the booking desk and the charge sheet. Result: a stop ends in a
   named charge and a choice to pay or serve.
2. **On the radio**: dispatch outcomes, the 911 abuse loop, unit status, the dispatcher voice. Result: a
   shift is a stream of real calls with a payoff each.
3. **Rank and file**: rank fleets, liveries, radar and traffic, tow and impound, rank patches, bonus
   cap, police skill. Result: an officer can see and feel their next rank.
4. **Behind bars** (#1027): the Correctional Center, transport, prison life, commissary, release.
   Result: jail is a place to be, not a timer.
5. **The break** (#798): the prison break as a gang event, plus the transport ambush. Result: the rare
   event the server talks about.
6. **The service** (#652, #653, #1035 tie-ins): medic fleet and outcomes, licence retakes, the
   detective road check. Result: medic and police careers stand beside each other.

### Cards

All cards: 30 to 45 minutes, at most 60 calls, file-disjoint within a chapter, acceptance written first
and seen red. Paths are under `addons/LifeFramework/Scripts/Game/Feature/` (core) or `addons/LF_Everon1989/`
(pack) unless written in full. Every world-prop card puts props in its own grid cell; each chapter ends with
one town card.

| Key | Chapter | Title | Type / tier / model | Owns | Depends | Acceptance | Risk |
|---|---|---|---|---|---|---|---|
| L1 | 1 | Law book codes: typed sections, stacking groups and jail-only flags on the crime catalog | system / T2 / sonnet | `Crime/LF_CrimeCatalog.c`, `Law/LF_LawCode*.c` | — | test `LF_Test_LawCodeStacking` | none |
| L2 | 1 | Pack law book: Everon codes and amounts from #4151, scaled to the economy | data / T1 / sonnet | `LF_Everon1989/Configs/Crime/LF_CrimeCatalog_Everon1989.conf`, pack `.st` rows | L1 | pack test `LF_Test_EveronLawBookAmounts` (amounts in band) | none |
| L3 | 1 | Charge sheet rules: total, stacking, rank cap, repeat-offense doubling, override | system / T2 / opus | `Law/LF_ChargeSheetRules.c` | L1 | test `LF_Test_ChargeSheetRules` | money |
| L4 | 1 | Charge sheet paper screen on the UI kit (tick, total, greyed rows) | visual / T2 / sonnet | `UI/Law/LF_ChargeSheet*.c`, `UI/layouts/Law/ChargeSheet.layout` | L3 | proof `charge-sheet` (session shot, full profile) | none |
| L5 | 1 | Booking desk: Book suspect action, server RPC, offer to the suspect (pay now / take the time) | capability / T2 / opus | `Law/LF_BookingDesk*.c`, `Prefabs/Props/Police/LF_BookingDesk.et` | L3 | test `LF_Test_BookingDeskOffer` | money, replication |
| L6 | 1 | Pay now from cash or bank; fine to the treasury; refusal goes to the cell | capability / T2 / sonnet | `Law/LF_ChargePayment.c` | L5 | test `LF_Test_ChargePayment` | money |
| L7 | 1 | Licence seizure from the sheet (driving, firearm; pilot and truck when they exist) | capability / T2 / sonnet | `License/LF_LicenceSeizureSheet.c` | L5 | test `LF_Test_SheetLicenceSeizure` | none |
| L8 | 1 | Law book lectern prop (Blender) and per-section Read actions | visual / T1 / sonnet | `tools/blender/lawbook/*`, `Prefabs/Props/Police/LF_LawBook.et`, `Grid_slice_L8.layer` | L2 | `look item` pictures | none |
| L9 | 1 | Booking SFX and the typewriter, ink pad props; desk sergeant lines for tickets and booking | visual / T1 / sonnet | `Sounds/Police/Booking*`, pack `Configs/Dialogue/DeskSergeant*` | L5 | `look grid:slice-L9`, voice files exist | none |
| L10 | 1 | Town: booking desk, lectern and sergeant into the Saint-Philippe Gendarmerie | town / T1 / sonnet | `Place_saint-philippe_police.layer` | L4–L9 | `audit placement --strict`, `look place:saint-philippe/police` | none |
| D1 | 2 | Call outcomes: resolved / no one found / unfounded, with presence checks at the scene | system / T2 / sonnet | `Police/LF_CallOutcome*.c` | — | test `LF_Test_CallOutcomes` | none |
| D2 | 2 | 911 abuse: unfounded marks, three a day file the charge, low-priority flag on the board | capability / T2 / sonnet | `Police/LF_CallAbuse.c` | D1, L2 | test `LF_Test_CallAbuse` | none |
| D3 | 2 | Unit status from the car radio (available / busy) on the dispatch board | capability / T2 / sonnet | `Police/LF_UnitStatus*.c` | — | test `LF_Test_UnitStatus` | replication |
| D4 | 2 | Outcome bonuses with the per-rank hourly cap, paid with the shift | system / T2 / opus | `Police/LF_DutyBonus.c`, `Medic/LF_MedicDutyBonus.c` | D1 | test `LF_Test_DutyBonusCap`; `tools\cli economy` prints the band | money |
| D5 | 2 | Dispatcher voice: lines per call type and place name, radio squelch | visual / T1 / sonnet | `Sounds/Dispatch/*`, pack `Configs/SoundSlots/Dispatch*` | D1 | session proof, audio in the log | none |
| D6 | 2 | `tools\cli economy` prints the duty band with bonuses and fines as work-minutes | tooling / T0 / haiku | `tools/commands/economy*.mjs` section file | D4 | node selftest | none |
| R1 | 3 | Rank fleet: the car pool hands each rank its vehicle (S105 Cadet, UAZ469 Patrol+) | capability / T2 / sonnet | `Police/LF_RankFleet.c`, pack `Configs/Police/LF_RankFleet_Everon1989.conf` | — | test `LF_Test_RankFleet` | none |
| R2 | 3 | Police S105 and S1203 transport liveries (needs #4155) | visual / T1 / sonnet | `Prefabs/Vehicles/Wheeled/S105/LF_S105_Police.et`, `S1203/LF_S1203_Police.et`, textures | #4155 | `look item` both | none |
| R3 | 3 | Rank patches: one texture per rank on the duty kit | visual / T1 / sonnet | `tools/blender/rank-patch/*`, pack kit conf rows | — | `look item` per rank | none |
| R4 | 3 | Radar gun item (Blender) and Measure speed action with speed classes | capability / T2 / sonnet | `Police/LF_RadarGun*.c`, `Prefabs/Items/Police/RadarGun.et` | L1 | test `LF_Test_RadarClasses` | none |
| R5 | 3 | Speed-limit signs in US English (100 / 50 / 30) and the speed-trap lay-bys | visual / T1 / sonnet | pack `Configs/Signs/SpeedLimit*`, `Grid_slice_R5.layer` | R4 | `look grid:slice-R5` | none |
| R6 | 3 | Tow action and impound lot rules (fee, daily rate, 7-day auction) | capability / T2 / opus | `Police/LF_Impound*.c` | — | test `LF_Test_ImpoundFees`; persistence test | persistence, money |
| R7 | 3 | Impound window and the clerk NPC (release per owned car) | capability / T2 / sonnet | `Police/LF_ImpoundWindow*.c`, pack NPC outfit row | R6 | test `LF_Test_ImpoundRelease` | money |
| R8 | 3 | Police skill on the professions curve: XP from outcomes, the first three perks | system / T2 / sonnet | `Jobs/Skills/LF_PoliceSkill*.c` (per #4140's shape) | D1, #4140 | test `LF_Test_PoliceSkillXp` | none |
| R9 | 3 | Rank board and commendations plaque (printed boards) | visual / T2 / sonnet | `Police/LF_RankBoard*.c`, `Police/LF_Commendation*.c` | — | test `LF_Test_RankBoardText` | none |
| R10 | 3 | Town: speed traps, impound lot, rank board into Saint-Philippe and the Meaux road | town / T1 / sonnet | the place layers | R1–R9 | `audit placement --strict`, looks | none |
| P1 | 4 | `Prison` place kind and the two-tier custody rule (10 min split) | system / T2 / sonnet | `Police/LF_CustodyTier.c`, `Places` kind row | L3 | test `LF_Test_CustodyTier` | none |
| P2 | 4 | Prison transport: log transfer, drive, Transfer at the gate starts the prison sentence | capability / T2 / opus | `Police/LF_PrisonTransfer*.c` | P1 | test `LF_Test_PrisonTransfer`; e2e restart mid-transfer | persistence, replication |
| P3 | 4 | Prison clothes on booking, civilian clothes stored, property clerk returns them | capability / T2 / opus | `Police/LF_PrisonProperty*.c` | P1 | persistence test `LF_Test_PrisonProperty` | persistence, inventory |
| P4 | 4 | Prison work stations: laundry, kitchen, workshop (1 min off, 40% cap) | capability / T2 / sonnet | `Police/LF_PrisonWork.c` (extends `LF_JailWork`) | P1 | test `LF_Test_PrisonWorkCap` | none |
| P5 | 4 | Commissary tokens: earned by work, spent at the hatch, expire at release | capability / T2 / sonnet | `Police/LF_Commissary*.c` | P4 | test `LF_Test_Commissary` | inventory |
| P6 | 4 | Visitor phone: one call to a player's pager per sentence | capability / T2 / sonnet | `Police/LF_VisitorPhone.c` | P1 | test `LF_Test_VisitorPhone` | none |
| P7 | 4 | Warden, cook, property clerk and inmate NPCs: outfits, voiced lines, yard loop | visual / T1 / sonnet | pack `Configs/Npcs/Prison*`, `Configs/NpcOutfits/Prison*`, `Configs/Dialogue/Warden*` | P1 | `look grid:slice-P7`, dialogue test | none |
| P8 | 4 | Correctional Center site: survey and kit (walls, towers, cell block, yard, gatehouse) | town / T1 / sonnet | `tools/town/kits/prison.json`, `tools/town/claims/<site>-prison.json` | survey | `town ready`, `audit placement --strict`, looks | none |
| P9 | 4 | Prison ambience and SFX (yard, buzzer, door slam, night hum) | visual / T1 / sonnet | `Sounds/Prison/*`, pack `SoundSlots/Prison*` | P8 | session proof | none |
| P10 | 4 | F2 "My file": tickets, licences, time served, standing in words | capability / T2 / sonnet | `PlayerMenu/LF_MyFile*.c` | L6, P1 | test `LF_Test_MyFile` | none |
| P11 | 4 | Town: prison stations and NPCs fitted on the island site | town / T1 / sonnet | the prison place layer | P2–P10 | `audit placement --strict`, looks | none |
| B1 | 5 | Prison break event state machine on the #3420 rules: prep, escape, extraction, outcomes, restart resume | system / T2 / opus | `Police/LF_PrisonBreakEvent*.c` | #3421, #3422, P1 | test `LF_Test_PrisonBreakEvent`; e2e restart | persistence, replication |
| B2 | 5 | Escapee tracking for police every 30 s and the "Take back to cell" action | capability / T2 / sonnet | `Police/LF_EscapeeTrack.c` | B1 | test `LF_Test_EscapeeTrack` | replication |
| B3 | 5 | Break HUD timer and the end summary card | visual / T2 / sonnet | `UI/Police/LF_BreakTimer*.c` | B1 | session proof, full profile | none |
| B4 | 5 | Extraction points and the contact NPC (pack data, 1 to 5 km from the prison) | data / T1 / sonnet | pack `Configs/Police/LF_PrisonBreak_Everon1989.conf` | B1, P8 | pack test `LF_Test_EveronExtractionPoints` | none |
| B5 | 5 | Transport ambush: attacking the van counts as a break, +4 wanted, shared cooldown | capability / T2 / sonnet | `Police/LF_TransportAmbush.c` | P2, B1 | test `LF_Test_TransportAmbush` | none |
| B6 | 5 | Prison siren and the break radio lines | visual / T1 / sonnet | `Sounds/Prison/Siren*`, dispatch lines | B1 | session proof | none |
| S1 | 6 | Medic outcome bonuses and the ambulance in the medic fleet | capability / T2 / sonnet | `Medic/LF_MedicFleet.c`, pack medic conf | D4 | test `LF_Test_MedicFleet` | money |
| S2 | 6 | Ambulance livery (UAZ452, US English "AMBULANCE") | visual / T1 / sonnet | `Prefabs/Vehicles/Wheeled/UAZ452/LF_UAZ452_Ambulance.et` | #4155 | `look item` | none |
| S3 | 6 | Licence retake exam at the licence desk (driving route, fee) | capability / T2 / sonnet | `License/LF_LicenceRetake*.c` | L7 | test `LF_Test_LicenceRetake` | money |
| S4 | 6 | Road check: Inspect action on a car finds a hidden compartment (#1035) | capability / T2 / sonnet | `Police/LF_RoadCheck.c` | R4 | test `LF_Test_RoadCheck` | none |
| S5 | 6 | Medic skill on the professions curve, XP from outcomes | system / T2 / sonnet | `Jobs/Skills/LF_MedicSkill*.c` | R8 | test `LF_Test_MedicSkillXp` | none |
| S6 | 6 | Town: ambulance bay at the clinic, Meaux police post | town / T1 / sonnet | clinic and Meaux layers | S1–S5 | `audit placement --strict`, looks | none |

51 cards. Critical path: L1 → L3 → L5 → P1 → P2 → B1. The livery pipeline (#4155) gates R2 and S2 only,
so a late spike doesn't hold anything else.

## 9. Risks and open questions

**Risks**

- **An empty police force.** All this is useless if nobody is on duty. On a small server, police on
  duty decide whether crime happens at all. Mitigations: a clear Cadet start quest in F2, the bonus,
  the S105 at once, and calls close to the station first. Measure who is on duty in the playtests.
- **Long jail time drives players off.** Even 30 minutes is a lot. Mitigation: the two tiers, work off
  up to 40%, bail early, and a prison worth being in. Watch logouts during sentences.
- **Officer abuse.** The charge sheet is a money and authority path. Every RPC re-checks duty, reach,
  cuffs, the wanted state, the rank cap, and logs the override. Interrogate before ship (#4151 B3).
- **Prison break across restarts.** A 15-minute event must survive a crash; `B1` has an e2e card.
  The event holds the auto-restart, so it needs a hard ceiling (20 min).
- **Island RAM.** A new walled site with towers and NPCs adds entities. Keep to the building budget
  (300 entities, 6 lights, 1 shadow caster per building).
- **Text on paper.** The charge sheet must stay short: section headers, one line per charge, a total.
  A legal wall of text is a menu by another name.

**Questions for the maintainer** (each with a recommendation; "accept all" takes them)

- **P1. Where is the prison?** Recommend the inland compound at 7447, 6705 (central, a real transport
  drive), after a scout. Fallback is 5355, 10543 by the hub.
- **P2. Jail cap.** Recommend 30 online minutes, not 60, with work off up to 40%.
- **P3. Custody split.** Recommend the holding cell up to 10 minutes and the prison over that.
- **P4. Fines to whom?** Recommend the town treasury (a sink), never the officer.
- **P5. Bonus cap.** Recommend the per-rank caps above, which keep Lieutenant at 2.3x the best civilian.
- **P6. Who signs the charge sheet?** Recommend any on-duty officer, override from Sergeant (as #4151 B2).
- **P7. Police helicopter.** Recommend Lieutenant with a pilot licence, after the heli shop and the
  livery spike. Not before M7's end.
- **P8. Unfounded-call charge.** Recommend 3 a day to a charge, checked by officer presence at the
  scene, with a staff flag on officers who mark too many.
- **P9. Prison clothes.** Recommend a pack color variant of vanilla work clothes (grey shirt and
  trousers). No orange jumpsuit; it's not a period European look.
- **P10. Chapters 1 and 2 in M6?** They improve every shift and need no new place. Recommend yes if M6
  has room; otherwise M7 first.

## Suggested additions (proposals for the maintainer)

Not on the wishlist. Each is a proposal; none is in the card table above.

| What | Why it helps the whole | Size | Touches |
|---|---|---|---|
| **Bail bondsman** (Lucien, a back office on the square) fronts bail at 20%, paid back in 3 days | Gives a broke prisoner a choice, adds a debt that can become a warrant, and a civilian who profits from crime without doing it | S (2 cards) | economy, crime, NPCs |
| **Magistrate's appeal** at the town hall: a prisoner can appeal one charge, a Lieutenant or staff rules on it within the sentence | A check on officer abuse that lives in the world, and roleplay for long sentences | M (3 cards) | legal, moderation, places |
| **Defense lawyer career** (whitelisted civilian job, paid per appeal and per bail hearing) | Gives a non-violent player a reason to be at the station, and a third side between police and crime | M (4 cards) | jobs, legal, standing |
| **Drunk driving**: breathalyzer (Blender), the pub and moonshine set a blood-alcohol value, a charge on the law book | Links the evening pub (M5) and moonshine to the road and gives traffic stops a second test | S (2 cards) | survival, crime, vehicles |
| **Stolen-car hotlist on the pager**: a report gives the plate; Run plate on any car finds it | Makes the chop shop and car theft a police game, not just a +1 | S (2 cards) | vehicles, crime, dispatch |
| **Community service** instead of a short jail term: sweep the square or pick up litter at the market, watched by an officer | Keeps minor offenders in the world and playing, and makes the town look looked after | S (2 cards) | jobs, places |
| **Police shooting range qualification**: the existing range (`saint-philippe-range`) gates the M16 for Senior Patrol | Makes rank kit earned, not handed out, and uses a place that exists | S (1 card) | qualifications, range |
| **Medic: fire and rescue** at the vanilla fire station (Saint-Philippe): car fires after crashes, a hose action | A third duty job sharing dispatch and the duty band, with a building that already exists and is unused | L (6 cards) | medic, vehicles, places |
| **Coroner run**: a medic picks up a body left after a death and earns a small fee; the body's items go to the evidence or the next of kin | Clears bodies with a job instead of a timer, and links murder to the detective chapter | M (3 cards) | medic, crime, evidence |
| **Witness statement at the counter**: a resident NPC or player who saw a crime gives a statement; it names the suspect on the case board | Uses the existing resident crime reaction (#2860) and standing R4, and gives the detective course real work | M (3 cards) | residents, standing, investigation |
| **Wanted posters in other towns** (Meaux post office, Montignac) when someone stays wanted for an in-game day | Makes a long chase visible beyond the station and makes the island feel connected | S (1 card) | wanted board, places |
| **Off-duty police perk**: an officer off duty can still Report a crime with priority and has their call answered first | Keeps the off-duty officer in the police world and makes them a witness | S (1 card) | dispatch |

## Proof

- Logic: each card's `LF_Test_*` as listed; charge-sheet totals, caps and stacking; bonus caps; tier split.
- Persistence: sentences, transport and break across a real restart (`wt e2e`); impound lot and property.
- World: `look` pictures of the booking desk, the lectern, the prison and every livery.
- Manual (in each PR): two clients on a dedicated server, an officer books a suspect who chooses pay or
  time; a three-officer break with a gang of two; a late joiner during a break sees the timer.
