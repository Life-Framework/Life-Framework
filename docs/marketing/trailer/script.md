# Trailer script: Life Framework (hero video, #571)

The hero demo for M2.5 Showcase and Outreach (#571, epic #566). Length 1:45.
It hooks with a heist, introduces Life Framework, runs a fast montage of real
M2 systems, asks people to join the playtest, then shows the road to v1.

Companion files:

- [storyboard.json](storyboard.json): every shot for the film pipeline
  (location, scene, actors, camera, duration, what blocks it).
- [shot-checklist.md](shot-checklist.md): the shots grouped by location for one
  filming session, with a look check per shot.
- [cutdowns.md](cutdowns.md): the 15 s cut and the 6 s bumper.
- `quality-gate.md` (written by the post-production agent, #397): the bar the
  cut must clear before the maintainer signs it off.

## When it is shot

Last. The maintainer decided on 2026-11-10 that the trailer is filmed and cut
after the M2 polish fixes and the visual QA sweep (#679) land, then passes the
quality gate and the maintainer's sign-off before it is posted. Every shot
that depends on an open issue names it (`blockedBy` in the storyboard, the
"Blocked by" column below). The shoot starts when those are closed or the shot
is swapped for its fallback.

## Rules this script follows

- **Only what is built.** Every system in the montage is in `docs/features.md`
  and `docs/playtest.md` as of 2026-11-10. Roadmap items are captioned
  "Coming" and carry no dates and no promises.
- **Tone.** It follows `docs/community/discord.md` and the outreach kit. No
  hype, no dates, no "the best", no "coming soon". Wry and plain. The joke is the
  honesty ("tell us what broke").
- **AI.** The outreach kit says to mention that AI agents build it under the
  maintainer's direction, without making it the headline. It is one small line
  on the end card.
- **Credits.** Only what `README.md` already states. That is Everon Life by Arkensor
  (the code this grew from), code from Overthrow (with its owner's permission)
  and LNGCLifeRP_Public, base-game assets by Bohemia Interactive. #480 (the
  credits page) is still open, so nobody else is named.
- **"v1".** No repo doc defines v1. This script reads it as M3 Framework
  Release, the first build a server owner can host. The maintainer confirms
  this at sign-off.

## Voice-over for TTS (Kokoro or Piper, #397)

- Short sentences, one breath each. No abbreviations in the spoken text.
- The `TTS` line under each VO line is what the engine reads. The VO line is
  what the caption file (`.srt`) shows.
- Numbers are spelled out: "nineteen eighty-nine", "version one".
- The URL is read as "life framework dot org slash join".
- Pronunciation hints. Kokoro (misaki) takes inline IPA as `[word](/ipa/)`.
  Piper has no inline hint in its default voices, so if it says a word wrong,
  respell the word in the TTS line with the "Say" column.

| Word | Say | Kokoro hint |
| --- | --- | --- |
| Everon | EV-er-on | `[Everon](/ˈɛvəɹɒn/)` |
| Reforger | ree-FOR-jer | `[Reforger](/ɹiˈfɔɹdʒɚ/)` |
| Arma | AR-muh | `[Arma](/ˈɑɹmə/)` |
| Edith | EE-dith | none needed |

## Captions

Lower third, at most 42 characters per line, at most two lines, white on a
dark band. The title card and end card are full-frame, not lower third.

## Pacing

The VO budget is about 2.5 words a second, the pace Kokoro and Piper read at
their default speed. Every line fits its shot at that rate. A line may run
half a second past its cut in the cold open. Post lets it straddle the cut
rather than squeezing the picture.

## Beat list

| # | Beat | Time | Shots |
| --- | --- | --- | --- |
| B1 | Cold open | 0:00 to 0:09 | S01 to S04 |
| B2 | Title | 0:09 to 0:11 | card |
| B3 | What Life Framework is | 0:11 to 0:26 | S05 to S08 |
| B4 | Showcase montage | 0:26 to 1:10 | S09 to S22 |
| B5 | Join the playtest | 1:10 to 1:22 | S23 |
| B6 | Road to v1 and the roadmap | 1:22 to 1:38 | R1 to R4 |
| B7 | End card | 1:38 to 1:45 | S28 |

## Music

One track, about 120 BPM (a bar is 2 s, so montage cuts land on bars).
Licence recorded in the #397 music library.

| Time | Energy | Cue |
| --- | --- | --- |
| 0:00 | 1 of 5 | Low pad, no beat. Room tone and the drill under it |
| 0:07 | 2 | A riser under the siren |
| 0:09 | 4 | One hard hit on the title card, then 1 s of silence |
| 0:11 | 2 | Pad returns, light pulse |
| 0:19 | 3 | Beat comes in under the store orbit |
| 0:26 | 5 | Full track on the first montage cut. Cuts on the bar |
| 0:52 | 2 | Break. Drop to a single stab as the medic beat ends |
| 0:54 | 5 | Back in on the drill |
| 1:10 | 3 | Drums out, melody stays under the call to action |
| 1:22 | 3 | Slow build under the roadmap cards |
| 1:38 | 4 | Final hit on the end card, ring out to 1:45 |

Game sound (#396) sits under the music. Keep the drill, the sacks hitting the
ground, the coin in the payphone, the siren, the CPR and the engine crane. Duck
the music 6 dB under every VO line.

## The script

### B1 Cold open (0:00 to 0:09)

**S01, 0:00 to 0:03.** Low static shot beside the cash van at the bank stop.
A player bends over the steel box on the rear step with a drill.

- VO: "Everon. Nineteen eighty-nine."
- TTS: "Everon. Nineteen eighty-nine."
- Caption: none.
- Transition in: fade from black (0.5 s).

**S02, 0:03 to 0:05.** Ground level behind the van. Three money sacks drop
onto the road.

- VO: "The cash van keeps a schedule."
- TTS: "The cash van keeps a schedule."
- Caption: none.
- Transition: hard cut on the sacks' sound.

**S03, 0:05 to 0:07.** First person at the fuel station payphone. A coin goes
in, the call menu opens on "Police".

- VO: "Somebody always finds a payphone."
- TTS: "Somebody always finds a payphone."
- Caption: none.
- Transition: hard cut.

**S04, 0:07 to 0:09.** Orbit round the police jeep at the station, light bar
flashing, siren up.

- VO: none. The siren carries it.
- Caption: none.
- Transition: hard cut to black on the music hit.

### B2 Title (0:09 to 0:11)

Black card, logo and wordmark, centred.

- Caption (full frame): "LIFE FRAMEWORK"
- VO: none.
- Transition: cut in, 0.3 s fade out.

### B3 What Life Framework is (0:11 to 0:26)

**S05, 0:11 to 0:16.** Drone rises over the spawn square at golden hour:
cobbles, the welcome sign, the payphone, the street running east.

- VO: "This is Life Framework. Open-source roleplay for Arma Reforger."
- TTS: "This is Life Framework. Open-source role-play for Arma Reforger."
- Caption: "Open-source roleplay for Arma Reforger"
- Transition in: fade from black (0.3 s).

**S06, 0:16 to 0:19.** First person at the clerk's desk on the square. Form
R-89, then the ID card.

- VO: "Arrive. Sign the register. Get your papers."
- TTS: "Arrive. Sign the register. Get your papers."
- Caption: none (the form is the picture).
- Transition: hard cut.

**S07, 0:19 to 0:23.** Orbit round the general store front: sign, shop
window, counter, the market table.

- VO: "Any server can run it. Ours is Everon, nineteen eighty-nine."
- TTS: "Any server can run it. Ours is Everon. Nineteen eighty-nine."
- Caption line 1: "An open framework"
- Caption line 2: "+ our 1989 Everon server"
- Transition: hard cut.

**S08, 0:23 to 0:26.** First person reads the Town Board. The printed notice
card fills the screen.

- VO: "Every sign tells you what happens inside."
- TTS: "Every sign tells you what happens inside."
- Caption: "Life in the world, not in a menu"
- Transition: whip cut into the montage on the downbeat.

### B4 Showcase montage (0:26 to 1:10)

Cuts land on the bar. Captions change with the system, not with every shot.

| Shot | Time | Picture | VO (TTS reads the same unless noted) | Caption | Blocked by |
| --- | --- | --- | --- | --- | --- |
| S09 | 0:26 to 0:28 | Farm at golden hour. First person, *Gather Cabbage* at the beds | "Work the fields." | "Jobs and chains" | #614 |
| S10 | 0:28 to 0:32 | Orchard: an apple off the tree, then the juicer on the press table | "Pick apples. Press them into juice." | "Farm and orchard" | #612 (no inventory in frame) |
| S11 | 0:32 to 0:35 | Quarry mining yard. A pickaxe swing; the rock shakes, shrinks, nuggets fall | "Swing a pickaxe." | "Mining and smelting" | #592, #618, #679 |
| S12 | 0:35 to 0:37 | The smelter boiler. *Smelt Iron Nuggets*; the bar drops in front | "Smelt what you dig up." | (same) | #593, #618, #679 |
| S13 | 0:37 to 0:40 | General store. Pay at the till, cash leaves the hand | "Cash is an item. You carry it." | "Cash is an item" | #582 |
| S14 | 0:40 to 0:43 | Bank front. *Talk to the teller*; Edith behind the west counter, the terminal opens | "Bank it with Edith. Or risk it." | "The bank teller" | #590, #679 |
| S15 | 0:43 to 0:46 | Police station. Clock in at the duty desk, then the wanted board card | "Police clock in. The wanted board keeps score." | "Police and the wanted board" | #495 (no stolen-car row in frame) |
| S16 | 0:46 to 0:48 | Payphone call, then the dispatch board line at the station | "One coin. Dispatch hears it." | "Payphones and dispatch" | #626 |
| S17 | 0:48 to 0:52 | Hospital front. A downed player; a medic does CPR and they get up | "Go down, not out. A medic brings you back." | "Downed, not dead" | #512, #625 |
| S18 | 0:52 to 0:54 | The ambulance in its bay under the sign, slow push in | "The bill comes on paper." | (same) | none |
| S19 | 0:54 to 0:58 | Bank vault door on the east wall. A player bent over it with the drill; it opens | "Or skip the job. Bring a drill." | "Heists: the vault and the cash van" | #513 |
| S20 | 0:58 to 1:02 | Chop shop yard at sunset. Hold *Strip it down* beside the engine crane | "Stolen cars go to the chop shop. Hot ones only." | "The chop shop" | #601, #415 |
| S21 | 1:02 to 1:06 | The barn pallet. Crate taken, van pulls away down the lane, follow cam | "Organizations run crates out of town." | "Contraband convoys" | #616, #679 |
| S22 | 1:06 to 1:10 | First person at the spawn table. The globe's travel list opens; pick Farm | "Lost? The globe on the square takes you anywhere in town." | "Playtest travel globe" | #607, #679 |

The TTS reads the VO column as written. If #606 has not merged, S14 says
"Bank it at the teller. Or risk it." and drops the name.

Transitions: hard cuts throughout. One whip pan between S12 and S13 (country
to town). A 4-frame dip to white on the S18 to S19 break, where the music
drops.

### B5 Join the playtest (1:10 to 1:22)

**S23, 1:10 to 1:22.** Slow drone or orbit over the spawn square at golden
hour. Two players on the square, one at the payphone and one reading the
Town Board. A third client holds the camera.

- VO: "The playtest town is open. Download it, play it, and tell us what broke."
- TTS: "The playtest town is open. Download it. Play it. And tell us what broke."
- VO, second line at 1:17: "Life framework dot org, slash join."
- TTS: "Life framework, dot org, slash join."
- Caption, 1:11 to 1:16: "Join the playtest"
- Caption, 1:16 to 1:22, line 1: "life-framework.org/join"
- Caption, 1:16 to 1:22, line 2: "Discord: #playtest-feedback"
- Transition out: crossfade (0.5 s) into the roadmap.

### B6 Road to v1 and the roadmap (1:22 to 1:38)

Four cards over golden-hour stills with a slow push in. Each card is a still
plus a caption, so this beat needs no new filming. The word "Coming" leads
every card.

| Card | Time | Picture | VO | Caption |
| --- | --- | --- | --- | --- |
| R1 | 1:22 to 1:27 | The general store counter (gallery `area-3-store/counter`) | "Next stop, version one. The framework release, for server owners." | "Coming: v1, the framework release" |
| R2 | 1:27 to 1:31 | The spawn square wide (gallery `area-1-spawn-square/wide-after`) | "Then our Everon server goes live." | "Coming: our 1989 Everon server, live" |
| R3 | 1:31 to 1:34 | The hideout bench in the evening sun (gallery `area-13-hideout/bench`) | "After that, a story of its own." | "Coming: the flagship story" |
| R4 | 1:34 to 1:38 | The farm in the evening light (gallery `area-12-farm/wide`) | "Then homes and land. No dates. It ships when it plays." | "Coming: homes and land" |

TTS reads the VO column as written. On screen under R1, a second caption line
names what v1 holds: "Two addons, one config file, owner docs" (40).

Transitions: crossfade (0.4 s) between cards.

### B7 End card (1:38 to 1:45)

**S28.** The main street from the square at dusk, static, behind a dark
overlay. Logo centred.

- VO: "Life Framework. Come break it with us."
- TTS: "Life Framework. Come break it, with us."
- Full-frame text:
  - "life-framework.org/join"
  - "Discord: discord.gg/MjMexJteqz"
  - "Open source, MIT."
  - "Built in the open with AI agents."
- Small credit lines, bottom:
  - "Built on Everon Life by Arkensor"
  - "Code from Overthrow and LNGCLifeRP_Public"
  - "Game assets by Bohemia Interactive"
- Transition out: fade to black over the last second.

## Footage status in short

The storyboard lists every shot's path. In short:

- **Can be shot with today's tools:** player first-person shots driven by the
  harness and the existing proofs (S03, S06, S08, S09, S10, S13, S15, S16),
  orbits on fronts that face +z (S04, S07), and stills for the roadmap cards
  and S18 (`tools\cli shots area`).
- **Needs the camera rig (#392):** static and drone shots (S01, S02, S05,
  S19, S23, S28), an orbit that takes an azimuth so it can circle a front that
  does not face +z (S17, S20, S23), and a clean first-person frame (HUD and
  hints off on the player view) for every first-person shot.
- **Needs actors (#393):** a clothed outfit for puppets (they are in underwear
  today), which S21's van driver needs. Every other shot uses real player
  clients so it does not need puppets.
- **Needs scenes (#394):** one scene per location (`trailer-*` in
  `storyboard.json`) so each take starts at golden hour in clear weather.
  Only `street` exists today.
- **Needs a flow:** mining (S11) has no proof verb yet; the convoy (S21) has
  no proof.
