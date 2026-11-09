# M2 Living Town (outline)

M2 leads with four themes (a fifth, Underworld, was added on 2026-11-09) the maintainer picked on 2026-11-08: moderation,
medic, comms, and investigation. This file is an outline. Each theme is broken
into sub-issues on GitHub, listed under its section; they get full scope cards
after the M1 playtests show what players need. Read it with
[direction](../direction.md), [decisions](../decisions.md), and the
[life/RP benchmark](../research/life-rp-benchmark.md), which ranks these four as
its top candidates.

**Where M2 happens.** The four themes are built and proven in the M1 playtest
village. M2 closes by placing the loops in one Everon town on MainWorld from
reusable site prefabs, with a filmed tour. That step was all of M2 ("Flagship
town") until 2026-11-08; it now comes last, so the prefabs carry the new
systems too.

**Rules that apply to every theme.**

- In-world first: a real item, a place, a sound before a screen.
- Animation and sound are part of done. Each scope names its animation and
  sound (vanilla, a recorded dependency, or ours).
- Gamepad-friendly, PC-first: amounts from presets, short lists, typing rare.
  Every scope says how a gamepad player completes it.
- Framework, then flagship: each system is configurable; the flagship default
  is what 1989 Everon ships.
- Officers apply fixed penalties from config (no court in M2).
- Overthrow code may be copied with its header (`THIRD_PARTY.md`); check
  `docs/ecosystem.md` pull #2 before writing a system from scratch.

Epics on GitHub: milestone **M2**, labels `m2` and `epic`, one issue per theme: #145 moderation, #146 medic,
#147 comms, #148 investigation.

---

## 1. Moderation: incident log and Game Master tools

Epic: #145.

Moderation decides whether a life server survives; every burnout story in the
benchmark is about RDM and admin conduct, not missing features.

**Player journey.** A player is shot outside the bank. They report it from the
payphone or to an admin. The admin opens Game Master, sees the incident at that
spot: who fired, who was hit, whether either held up the other first, whether
it happened in the lawless area. The admin rules in a minute, from facts, and
the ruling is recorded. Players see moderation as fast and fair; nobody sees an
admin panel.

**In-world design.** The log is invisible to players. Its effects are in the
world: the hold-up is a visible gesture with a sound, the lawless area has signs
and a fence line, and the New Life Rule cue appears as a hospital wristband
(see Medic). Admins work inside vanilla Game Master, not a custom menu.

**Framework vs flagship.**

| Setting | Framework | Flagship |
| --- | --- | --- |
| Logged event kinds | config list (deaths, damage, cash moves, intent, zone crossings, NLR returns) | all |
| Retention | config (sessions or days) | a week |
| Zones | any number, each lawless / safe / normal | one lawless area (quarry or islands), the spawn plaza safe |
| Hold-up | aim window, range, aim-alone counts | hold-up action plus aimed weapon |

**Depends on M1.** Server-authoritative crime and kill hooks (M1-27, #125), the
safe zone (#125), the interaction menu (M1-12, #98), the restart-safe save
(M1-04, #89).

**Sub-issues** (epic #145, in order; the first is the playable core):

1. #239 Incident log records deaths, damage, and cash moves
2. #240 Admins read incidents in Game Master
3. #241 A player report flags recent incidents for staff
4. #242 Zone service with safe, lawless, and normal rule sets
5. #243 The quarry is the one marked lawless area (flagship)
6. #244 Hold-up action marks hostile intent
7. #245 Aiming a weapon at a player is logged as intent
8. #246 Admin actions are logged too

**Risks.** Log volume on a busy server (bound it, sample damage). Aim
detection cost per frame (check on a timer, only for drawn weapons). Game
Master API surface for custom panels is unknown; research before planning.
Privacy: the log is for admins only and never reaches a client.

## 2. Medic: downed state, EMS, ambulance

Epic: #146.

A third role with its own loop, so the town is more than cops and criminals.
Reforger already simulates bleeding, tourniquets, morphine, and saline.

**Player journey.** A courier crashes on the coast road and goes down instead
of dying. Someone calls the hospital from a payphone; the medic's pager
beeps with the location. The medic drives the ambulance out, stabilises the
patient with a real kit, loads them, and drives them to a hospital bed. The
patient wakes there with a smaller bill. If nobody comes in time, the patient
dies, wakes in hospital with the full bill, and the New Life Rule applies.

**In-world design.** The downed state uses vanilla unconsciousness with a
bleed-out timer the patient can feel (vision, sound), not a countdown HUD. The
medic kit is real vanilla medical items. The ambulance is a vanilla vehicle
with a stretcher slot. Paying the medic is cash, handed over or billed on paper.
Polish: carry and drag animations (ACE Carrying, under evaluation in
`docs/ecosystem.md`), stretcher load sound, siren.

**Framework vs flagship.**

| Setting | Framework | Flagship |
| --- | --- | --- |
| Downed state | on / off, bleed-out time | on, a few minutes |
| Revive | medic only / anyone with a kit | medic stabilises; anyone can apply first aid |
| Bills | hospital bill, reduced bill when brought in | small bill, smaller when a medic brings you |
| New Life Rule | radius, window, cue | about 15 minutes, the death scene's area |
| Medic job | whitelisted / open, duty pay rate | open, clocked in at the hospital desk |

**Depends on M1.** Wake up in hospital (M1-26, #121), duty desk pattern and
duty pay (M1-11, #96; M1-07, #92), dual inventory for duty gear (#96).

**Sub-issues** (epic #146, in order; the first is the playable core):

1. #255 Downed state on lethal damage
2. #256 Bleed-out timer and the give-up option
3. #257 Anyone with first aid slows the bleed-out
4. #258 Medic job clocked in at the hospital desk
5. #259 A medic revives a downed patient
6. #260 Treatment bill on paper, paid at the hospital desk
7. #261 Carry and drag a downed player
8. #262 Ambulance delivers a patient to a hospital bed for the reduced bill
9. #263 Siren and lights on service vehicles
10. #264 New Life Rule: wristband on waking and a reminder near the scene
11. #265 Village hospital: medic desk, ambulance bay, and 1989 bills (flagship)

**Risks.** Intercepting vanilla death without breaking respawn, loot on the
body, and the murder hook. Downed players as hostages or loot pinatas (decide
what a downed player can lose). A medic role with nothing to do on a quiet
server (pair it with a simple NPC call or a towing side job).

## 3. Comms: payphones, pagers, dispatch board

Epic: #147.

Every service (police, medic, taxi, mechanic) needs a way to be called. No
phone UI; 1989 comms only.

**Player journey.** A shop owner sees a robbery and walks to the payphone. They
pick up, press the police preset, and give a short description from a list.
On duty, the officer's pager beeps; at the station, the dispatch board shows
the call with time and place. A criminal with a scanner hears the police radio
and leaves early.

**In-world design.** Payphones are placed props with a coin slot and a handset
animation. The call is a short preset menu (service, then what you saw); free
text is optional. Pagers are carried items that beep and show a one-line
message. The dispatch board is a corkboard or teleprinter at the station.
Radios are vanilla, on frequencies. Polish: dial tone, DTMF, coin drop, pager
beep, teleprinter clatter.

**Framework vs flagship.**

| Setting | Framework | Flagship |
| --- | --- | --- |
| Services | config list (name, who receives, pager or board) | police, medic, taxi, mechanic |
| Call cost | config | one coin |
| Anonymous tips | on / off | on |
| Scanner | item and legality config | illegal, from the fence |

**Depends on M1.** Alarm and dispatch on the vanilla radio (M1-20, #105), the
wanted board (#96), the interaction menu patterns (#98).

**Sub-issues** (epic #147, in order; the first is the playable core):

1. #247 Call the police from a payphone
2. #248 Calls route to any service in config
3. #249 Pager item beeps with the call
4. #250 Dispatch board at each service's station
5. #251 A responder takes a call and the caller hears back
6. #252 Service radios on fixed frequencies
7. #253 Police scanner as an illegal crime tool
8. #254 Payphones and boards placed around the village (flagship)

**Risks.** Spam calls (cost and cooldown, logged). Message text on a
dedicated server is fine, but rendering on a prop needs the RT-texture pattern
from the signs (#119). Vanilla radio API limits for scanning.

## 4. Investigation: witnesses and evidence

Epic: #148.

Gives police something to do between chases and makes the vague-description
wanted system real (decision "Wanted is automatic; police get a description").

**Player journey.** A cash van is robbed at the quarry road. A passing
civilian sees a green jacket and a tan car with a plate starting "B-" and
reports it from the payphone. Police find casings and a dropped drill at the
scene, bag them, and log them in the evidence locker. The drill came from the
fence; its serial ties to a buyer. A stop on the coast road finds a man in a
green jacket in a tan car. An ID check names him and the wanted entry becomes
"identified".

**In-world design.** Witness statements come from the payphone and from
talking to a civilian, using presets drawn from what that witness could see.
Evidence is physical: casings, dropped items, a bag, the evidence locker.
Plates are read by eye. No database terminal; the board and paper files are the
record.

**Framework vs flagship.**

| Setting | Framework | Flagship |
| --- | --- | --- |
| Description fields | config (clothing, vehicle, plate fragment, place) | all four |
| Witness range and accuracy | config | short range, partial plates |
| Evidence kinds | config (casings, dropped items, tool serials) | casings, items, fence-tool serials |
| Fence | thresholds, tool list, prices | gated on a few completed smaller jobs |

**Depends on M1.** Description snapshots and the identified flag (decision
2026-11-08, wanted board #96, dispatch #105), the paper ticket (#104), jail
(#106), plates (`LF_LicensePlateManager`).

**Sub-issues** (epic #148, in order; the first is the playable core):

1. #266 Witness description from the payphone
2. #267 Officers question witnesses in person, including NPCs
3. #268 Casings and dropped items become scene evidence
4. #269 Evidence locker at the station
5. #270 Hidden crime record per account
6. #271 Fence NPC sells tools to proven criminals only
7. #272 Fence tools carry serials that lead back to the buyer
8. #273 The bank vault needs fence tools
9. #274 Cash van robbery
10. #275 Police armory heist
11. #276 Fence, cash van route, and evidence locker in the village (flagship)

**Risks.** Witness quality on a low-population server (NPC witnesses may be
needed). Evidence that never gets used turns into clutter (expiry). The fence
must not become a grind wall; thresholds are small and come from playtests.

---

## 5. Underworld: lockpicking, chop shops, illicit chains

Epic: #410. Scope cards: [lockpicking](scopes/crime/lockpicking.md),
[chop shops](scopes/crime/chop-shop.md),
[illicit chains](scopes/crime/drug-chains.md). Added 2026-11-09.

Crime beyond robberies. A player picks a lock by hand, steals a car, and sells
it to a chop shop that pays less than it is worth and asks nothing. A player
runs a production chain of a chosen difficulty: step count, a skill or licence
gate, equipment cost, and police heat all scale with the tier. Every chain can
be run solo, but the hard ones are very slow without a team, because the work
is physical and spread over several stations (several barrels, several people).

**In-world design.** No loading bars or menus: each step is a held action at a
real object with animation and sound. Heat is a smell and smoke trace at a
running site and officer searches at stops; both feed the investigation theme.

**Framework vs flagship.** M2 builds the systems and the easy chain (moonshine).
The other chains are flagship pack content in M5 (see the M5 outline).

**Sub-issues** (epic #410, in order):

1. #411 Pick a door lock with a lockpick item
2. #412 Pick a vehicle lock and hotwire an unkeyed vehicle
3. #414 Stolen vehicles: report, wanted board, police stops
4. #415 Chop shop yard dismantles a vehicle for cash and parts
5. #416 Tiered chain config: steps, stations, gates, equipment, heat
6. #417 Hands-on stations: several barrels, many workers, no progress bars
7. #418 Smell and smoke from running sites reach officers and witnesses
8. #419 Officers search players and vehicles for chain goods at stops
9. #420 Moonshine chain on hands-on stations (the easy tier, flagship)

**Risks.** Chop shops can turn into an endless money tap (cap and cooldown).
Hands-on stations need good animations; the animation pipeline (#340) may be
needed. Lockpicking noise must not make crime trivially visible.

---

## Not in M2

- Scheduled and organised events (decision "Scheduled events are parked").
- A player-staffed bail or appeal desk (decision "Officers apply fixed
  penalties from config").
- Permanent character death.
- Cosmetic donations (flagship, M4 at the earliest).
