# Life Framework Dev Log 01: storyboard and script (for sign-off)

Status: **paper only.** Nothing is filmed, rendered, recorded or downloaded. The maintainer signs off
this file (the board, every line of the script, the slides and the music) before anyone runs
`session up`, `film-make`, Piper or a slide render. Edits after sign-off go in this file first.

This replaces the cinematic launch trailer board (`tools/film/trailer/storyboard.md`), rejected
2026-11-15. The new direction, in the maintainer's words: "more as a dev log video where we make the
harness rotate around a certain asset / demonstration of something and then explain it with some
richer dialog, then it can switch to a branded slide to explain more about it, what's planned etc. So
it's a little less janky and unnatural and easier to do."

The server is called **Life Framework | Everon 1989 (Alpha)** wherever it is named. Tone:
`docs/community/discord.md`, plain and a little dry. No hype words, no dates, no "coming soon".

## Why the earlier videos came out weak, and what this board does instead

No one wrote a post-mortem. These causes come from the pipeline notes (`docs/film-pipeline.md`), the
trailer v1 shoot driver (`video/trailer-v1/shoot.mjs`), the rejected board and the `look` stills in
`website/public/docs-images/`.

| What went wrong | Cause in the tools | This board |
|---|---|---|
| People moved like puppets | DebugWorld has no navmesh and no AI. An actor is a scripted puppet walking straight lines, or a session client standing still while a harness verb plays an action. | No acting. Characters in frame are placed NPCs standing at their posts. Nobody walks. |
| Cars looked wrong | `drive` pushes the body with `SetVelocity`: no engine, gears or brakes, and it fights the suspension on bumps. | No driving. Vehicles are parked props. |
| The camera went through walls | `camera orbit` has no collision. A 5 m orbit round the store passed through the building. | Every orbit is checked from scout stills taken at the start, middle and end of the arc. Interior orbits keep inside a radius measured from the plan. |
| Narration said nothing | 5 to 6 s shots at 2.5 words a second leave room for a slogan, not an explanation. | One 14 s orbit per segment with 25 to 35 words, then a slide that carries the detail. |
| Too many takes had to be perfect | 11 cuts of 3 to 8 s each, two clients acting on cue. | 8 orbits and one screen recording. Each take is a single camera move with nothing else moving. |
| Sets looked unfinished | Duty desks, boards and lockers stand on the lawn outside the police station and the hospital (`docs-images/area-6-police/wide.png`, `area-7-hospital/wide.png`). | Every shot is in the polished town and lists its town pass. Nothing is filmed in the debug grid. |

## What the camera can really do

From `addons/LifeFramework/Scripts/Game/Feature/Film/LF_FilmCameraPath.c`, `tools/film/storyboard.mjs`,
`tools/film/post.mjs`, `video/README.md` and `agent-box/README.md`.

- **Orbit:** harness `camera orbit x y z radius seconds [azimuth arc height]`, storyboard
  `{ "mode": "orbit", "point", "radius", "azimuth", "arc", "height" }`. It is an arc around a point
  in world metres. Azimuth is the side the camera starts on (0 = +z, 90 = +x, 180 = -z, 270 = -x), the
  arc is centred on it, `height` is the camera above the point, and a negative arc sweeps the other way.
  The camera looks at **point + 1.2 m** (fixed) with a **60 degree FOV** (fixed for orbit). The path has
  12 keys on a Catmull-Rom curve, eased as one pass: it starts from rest and stops at rest, which suits a
  cut to a slide.
- **Turntable:** there is no rotating plinth. A single-asset turntable is the same orbit with `arc 360`
  round an asset on a clear spot, so the camera moves and the asset stays put.
- **Static and drone** also exist; this board does not use them.
- **Clean frame:** `film-make` hides the HUD and hint pop-ups for every take.
- **Light:** scene presets `trailer-bank|square|store|police|hospital|farm` set 17.9 local (golden
  hour), Clear and a frozen day; `trailer-square-dusk` is the dusk preset; `scenes[]` overrides the hour.
- **Post:** `post.mjs` makes crossfades, ASS captions in the lower third plus an `.srt`, a Piper line per
  voice cue placed at its time, and a music bed that ducks under the voice, mixed to about -14 LUFS.
- **Not available:** no collision, no look-at-entity on an orbit, no FOV or look-height argument on
  the orbit verb. `BuildOrbit` already takes `lookHeight` and `fov`, so exposing them is two numbers in
  the verb's parser. That is optional, and the board works without it.

## Format and look

| | |
|---|---|
| Length | about 2:50 |
| Series | Life Framework Dev Log 01, posted on YouTube and Discord. Later chapters get their own episodes in the same format. |
| Shape | title slide, intro orbit, six segments (orbit, then slide), closing orbit, roadmap slide, end slide |
| Output | 1920x1080, 30 fps, 16:9, H.264 and AAC, captions burned in plus an `.srt` |
| Light | 17.9 local, Clear, frozen day (the `trailer-*` presets); interior 14.0; closing orbit at dusk |
| Filming | `--profile full --res 1920x1080`, one session for every orbit, one client, no `--sound` (no game audio in a dev log), `--priority low` |
| Voice | Piper `en_US-ljspeech` (high) only. It is public domain; other Piper voices are not safe for public video (`docs/credits.md`) |
| Cuts | 0.5 s crossfade from each orbit into its slide and from each slide into the next orbit |
| Where | The polished town, or a single-asset turntable. **Never the debug grid.** Each orbit's azimuth keeps the grid and the annex out of frame |
| Gate | `docs/marketing/trailer/quality-gate.md` (frames, audio, format, honesty), then the maintainer watches it |

## Running order

| # | Time | Part | Picture | Seconds |
|---|---|---|---|---|
| 0 | 0:00 | Title slide | Title layout | 4 |
| I | 0:04 | Intro orbit | High orbit over the town square | 14 |
| 1 | 0:18 | The supermarket | Interior orbit, then feature slide | 14 + 7 |
| 2 | 0:39 | Money, the bank and the cash van | Orbit round the van at the bank, then feature slide | 14 + 7 |
| 3 | 1:00 | Police and uniforms | Orbit at the station front, then feature slide | 14 + 7 |
| 4 | 1:21 | The hospital | Orbit across the ambulance bay, then feature slide | 14 + 7 |
| 5 | 1:42 | Saves that survive a restart | Orbit over the farm plots, then feature slide | 14 + 7 |
| 6 | 2:03 | Run your own: the config generator | Screen recording of the page, then feature slide | 14 + 7 |
| O | 2:24 | Closing orbit | Dusk orbit over the square | 10 |
| R | 2:34 | Roadmap slide | Roadmap layout | 9 |
| E | 2:43 | End slide | End layout | 7 |

Each part starts on the time given; the 0.5 s crossfades overlap the parts.

## Segments

Orbit numbers are a first pass from the layer coordinates (`agent-box/layers.mjs`). The town pass may
move things, so each orbit is re-checked from scout stills (start, middle and end of the arc,
`film-make --preview`) before the real take. Speed is degrees a second and the camera's metres a second.

### 0. Title slide (0:00, 4 s)

Title layout. No voice; the music starts. Slide text under "Slides" below.

### I. Intro orbit: the town (0:04, 14 s)

| Orbit | |
|---|---|
| Target | The town square, `Spawn_Town` (126, 0, 246) |
| Camera | `orbit 126 0 246 30 14 180 50 14`: radius 30 m, 14 m up, starting on the -z side (looking north into the town, with the annex behind the camera), 50 degree arc |
| Speed | 3.6 deg/s, 1.9 m/s |
| Light | `trailer-square`, 17.9, Clear, frozen |
| Staged | Townsfolk NPCs at their posts on the square, nobody walking. Parked cars only where they belong |

**Needs town pass:** the square from 30 m up (everything in a 40 m circle reads as a town: no test
signs, no stray crates, NPCs dressed by role); grid and annex out of frame on the north-facing arc.

**Narration:** "This is Life Framework, an open-source roleplay framework for Arma Reforger. This is our
test town: Everon, nineteen eighty-nine. Here is what we built, what is still rough, and what is next."

### 1. The supermarket (0:18)

| Orbit | |
|---|---|
| Target | `Village_Supermarket` (76, 0.1, 96.5), the sales hall between the gondolas |
| Camera | first pass `orbit 78 0.1 96.5 3.5 14 270 110 1.7`: radius 3.5 m, eye height, starting on the -x side, 110 degree arc that ends facing the checkouts and the shopkeeper (`Village_Supermarket_Shopkeeper`, 83.8, 96.8). The hall centre and radius are set from the plan section (`tools/buildings/supermarket`) so the arc stays inside the 18 x 14 m hall |
| Speed | 7.9 deg/s, 0.48 m/s |
| Light | `trailer-store` with the hour overridden to 14.0 (daylight through the glazed front) |
| Staged | The fully stocked shelves (nine lines, 248 shelf copies), the shopkeeper behind the till, no customers |

**Needs town pass:** interior only. Inside: every
shelf copy seated on its board, the checkout counters clear, the stockroom door shut. Outside is not
filmed.

**Narration (orbit):** "The supermarket is the first building we made ourselves, from the floor plan
up. Everything on these shelves is a real item: pick it up, carry it to the till, and pay."

**Slide (feature):** kicker `ECONOMY` · title `THE SUPERMARKET` · stamp `IN ALPHA`
- What works: Built in Blender from a 1989 floor plan · 248 real items on the shelves · Pay at the till; the door alarm catches the rest
- Next: The same build and polish for every building in town

**Narration (slide):** "Next: every building in town gets the same treatment."

### 2. Money, the bank and the cash van (0:39)

| Orbit | |
|---|---|
| Target | The cash van at its bank stop, `Roadmap_Cash_Van` (196, 0, 252.4), strongbox `Roadmap_Cash_Van_Box` (197.4, 0, 253.8) |
| Camera | `orbit 196.5 0 253.2 7 14 180 90 1.8`: radius 7 m, 1.8 m up, from the street side (-z), so the bank front and its fascia sign (190, 259.7) stand behind the van |
| Speed | 6.4 deg/s, 0.79 m/s |
| Light | `trailer-bank`, 17.9, Clear, frozen |
| Staged | The van parked at the stop, doors shut, the strongbox at its rear door, the bank front behind. No robber, no drill |

**Needs town pass:** the strongbox on the kerb at the van's rear doors, not loose on the grass, and the
paper on the verge cleared (`docs-images/925/cash-van-box-after.png`); the bank front dressed (the
vault hood, rob-kit stand and alarm box neat or out of the arc).

**Narration (orbit):** "Money here is physical. Cash is an item in your pocket, the bank keeps it
behind the teller's glass, and this van carries it across town in a locked strongbox."

**Slide (feature):** kicker `MONEY AND CRIME` · title `THE BANK AND THE CASH VAN` · stamp `IN ALPHA`
- What works: Cash is an item you carry · A teller behind glass, a vault inside · The van runs between the bank and the fuel station
- Next: More robbery targets and dirty money (M6, M7)

**Narration (slide):** "Drill the box, take the bags, and the police get the call."

### 3. Police and uniforms (1:00)

| Orbit | |
|---|---|
| Target | The police station, villa `Villa_E_2I01_Police` (234, 0, 234.2), its front door |
| Camera | `orbit 234 0 241 10 14 0 80 2.2`: radius 10 m, 2.2 m up, from the street side (+z), 80 degree arc |
| Speed | 5.7 deg/s, 1.0 m/s |
| Light | `trailer-police`, 17.9, Clear, frozen |
| Staged | One officer NPC (`LF_Npc_Police_2`, our shirt, trousers and cap) at the door; the police car (`Roadmap_Police_Car`, 243.5, 246) parked at the kerb |

**Needs town pass:** the duty desk, case board, armory, armory store, evidence locker, handcuffs and
taser moved inside the station (they stand on the lawn now, `docs-images/area-6-police/wide.png`); the
dispatch and wanted boards on the front wall by the door; the spike strip (234, 245.4) off the road;
a uniformed officer NPC placed at the door.

**Narration (orbit):** "The police are players. You apply at the station, and once you are accepted
you clock in at the duty desk and the uniform goes on. The shirt, trousers and cap are our own."

**Slide (feature):** kicker `LAW AND ORDER` · title `POLICE` · stamp `IN ALPHA`
- What works: Apply at the station; staff approve · Clock in at the duty desk for the uniform and kit · Case board, evidence locker, wanted board
- Next: Patrols, then detective work (M5, M7)

**Narration (slide):** "Next: patrol routes, then proper detective work."

### 4. The hospital (1:21)

| Orbit | |
|---|---|
| Target | The ambulance bay in front of the hospital (`House_Town_E_2I03_Base`, 262, 0, 264.4) |
| Camera | `orbit 265 0 255 11 14 180 80 2.0`: radius 11 m, 2 m up, from the street side (-z), so the ambulance (`Roadmap_Hospital_Ambulance`, 266, 250) is in the foreground and the red cross front behind |
| Speed | 5.7 deg/s, 1.1 m/s |
| Light | `trailer-hospital`, 17.9, Clear, frozen |
| Staged | The ambulance in the bay, its four cones; the doctor NPC (`Hospital_Doctor`) at the door. Siren off |

**Needs town pass:** the duty desk outside the hospital (259.4, 256.4) moved inside, as the maintainer
called out; with it the dispatch board, waiting bench, cabinet, till and supply crates inside; the
downed-patient prop (268.2, 254.6) inside on a bed or removed; the beds checked to be inside the
building; the doctor at the door.

**Narration (orbit):** "Going down is not the end. A medic can revive you where you fell, or bring you
here by ambulance. If nobody comes, you wake in a hospital bed with a bill."

**Slide (feature):** kicker `MEDICS` · title `THE HOSPITAL` · stamp `IN ALPHA`
- What works: Downed, not dead: a medic can revive you · Ambulance to the hospital · Wake in a bed with a bill; pay at the desk
- Next: The "Downed, not dead" chapter goes to playtest (M2)

**Narration (slide):** "You pay the bill at the hospital desk. Medics are players too."

### 5. Saves that survive a restart (1:42)

| Orbit | |
|---|---|
| Target | The farm plots, `FarmPlot_1` and `FarmPlot_2` (145 and 148.5, 0, 299.5) |
| Camera | `orbit 146.75 0 299.5 7 14 180 90 2.5`: radius 7 m, 2.5 m up, from the -z side, 90 degree arc |
| Speed | 6.4 deg/s, 0.77 m/s |
| Light | `trailer-farm`, 17.9, Clear, frozen |
| Staged | Both plots ripe (cabbage), the seed crate, the chalkboard and the wheelbarrow |

**Needs town pass:** the farm corner tidy (stand crates, wheelbarrow, signs upright and seated). The
plots are grown ripe for the take, which is a staging step and not a placement.

**Narration (orbit):** "A town only works if it remembers. Crops like these keep growing through a
restart, and so do your cash, your gear and your I D card."
(Caption: "...and so do your cash, your gear and your ID card.")

**Slide (feature, terminal panel):** kicker `FOR SERVER OWNERS` · title `SAVES THAT SURVIVE` · stamp `IN ALPHA`
- Terminal panel: `tools\cli host backup` · `tools\cli host restore` · `tools\cli host wipe --confirm <host>`
- What works: Every save carries its build; a load from another build warns first · A wipe moves the old save aside and never deletes it
- Next: The upgrade path, so a framework update keeps your saves (M3)

**Narration (slide):** "Owners get backup and restore. A wipe moves the old save aside, never deletes it."

### 6. Run your own: the config generator (2:03)

This picture is a **screen recording**, not an orbit (open question 2).

| Screen | |
|---|---|
| Target | `life-framework.org/config`, the Shops section |
| Recording | 1920x1080, 14 s, a scripted pass (proposed: Playwright `recordVideo`, the same Playwright the share images use): type 70 into `defaultBuyPrice`, type -3 into `defaultSellPrice` and hold on "must be at least 0", fix it to 25, the `config.json` panel updates, the pointer rests on DOWNLOAD CONFIG.JSON |
| Look | Day theme; no browser chrome; slow typing (120 ms a key) |

**Needs town pass:** none.

**Narration (orbit):** "If you run your own server, you should not have to read our code to change a
price. The config generator builds the file and checks each value as you type."

**Slide (feature):** kicker `RUN YOUR OWN` · title `THE CONFIG GENERATOR` · stamp `IN ALPHA`
- What works: One `config.json` for prices, zones, police and persistence · Built from the same schema the server reads · Each value checked against its range
- Next: The framework on the Workshop as its own addon (M3)

**Narration (slide):** "Next is the framework release on the Workshop, so your changes survive our updates."

### O. Closing orbit (2:24, 10 s)

| Orbit | |
|---|---|
| Target | The town square, `Spawn_Town` (126, 0, 246) |
| Camera | `orbit 126 0 246 18 10 160 40 6`: radius 18 m, 6 m up, from the south-south-east side, 40 degree arc |
| Speed | 4 deg/s, 1.3 m/s |
| Light | `trailer-square-dusk`, street lamps on |
| Staged | The square at dusk, NPCs at their posts |

**Needs town pass:** the same square as the intro, seen lower, and lamps that light at dusk.

**Narration:** "Everything you saw is in alpha. It is merged and it runs, but it has not met enough
players yet."

### R. Roadmap slide (2:34, 9 s)

**Narration:** "That is the next step: playtests on our server, fixing what breaks, then the framework release."

### E. End slide (2:43, 7 s)

**Narration:** "Join us on Everon, or take the framework and run your own."

## The whole script, start to finish

Times are the cut's. A voice line starts 0.8 s into its picture. The captions are the spoken words,
except where the voice text is spelled for Piper (marked). Piper text: numbers as words, "I D" for ID.
Budget: 2.3 words a second (`lengthScale` 1.1, a little slower than default).

| Time | Part | Voice (Piper text) | Caption |
|---|---|---|---|
| 0:00 | Title slide | (music only) | none |
| 0:05 | Intro orbit | This is Life Framework, an open-source roleplay framework for Arma Reforger. | This is Life Framework, an open-source roleplay framework for Arma Reforger. |
| 0:10 | | This is our test town: Everon, nineteen eighty-nine. | This is our test town: Everon, 1989. |
| 0:14 | | Here is what we built, what is still rough, and what is next. | Here is what we built, what is still rough, and what is next. |
| 0:19 | 1 orbit | The supermarket is the first building we made ourselves, from the floor plan up. | The supermarket is the first building we made ourselves, from the floor plan up. |
| 0:25 | | Everything on these shelves is a real item: pick it up, carry it to the till, and pay. | Everything on these shelves is a real item: pick it up, carry it to the till, and pay. |
| 0:33 | 1 slide | Next: every building in town gets the same treatment. | Next: every building in town gets the same treatment. |
| 0:40 | 2 orbit | Money here is physical. | Money here is physical. |
| 0:42 | | Cash is an item in your pocket, the bank keeps it behind the teller's glass, and this van carries it across town in a locked strongbox. | Cash is an item in your pocket, the bank keeps it behind the teller's glass, and this van carries it across town in a locked strongbox. |
| 0:54 | 2 slide | Drill the box, take the bags, and the police get the call. | Drill the box, take the bags, and the police get the call. |
| 1:01 | 3 orbit | The police are players. | The police are players. |
| 1:03 | | You apply at the station, and once you are accepted you clock in at the duty desk and the uniform goes on. | You apply at the station, and once you are accepted you clock in at the duty desk and the uniform goes on. |
| 1:09 | | The shirt, trousers and cap are our own. | The shirt, trousers and cap are our own. |
| 1:15 | 3 slide | Next: patrol routes, then proper detective work. | Next: patrol routes, then proper detective work. |
| 1:22 | 4 orbit | Going down is not the end. | Going down is not the end. |
| 1:24 | | A medic can revive you where you fell, or bring you here by ambulance. | A medic can revive you where you fell, or bring you here by ambulance. |
| 1:29 | | If nobody comes, you wake in a hospital bed with a bill. | If nobody comes, you wake in a hospital bed with a bill. |
| 1:36 | 4 slide | You pay the bill at the hospital desk. Medics are players too. | You pay the bill at the hospital desk. Medics are players too. |
| 1:43 | 5 orbit | A town only works if it remembers. | A town only works if it remembers. |
| 1:45 | | Crops like these keep growing through a restart, and so do your cash, your gear and your I D card. | Crops like these keep growing through a restart, and so do your cash, your gear and your ID card. (spelled) |
| 1:57 | 5 slide | Owners get backup and restore. A wipe moves the old save aside, never deletes it. | Owners get backup and restore. A wipe moves the old save aside, never deletes it. |
| 2:04 | 6 screen | If you run your own server, you should not have to read our code to change a price. | If you run your own server, you should not have to read our code to change a price. |
| 2:10 | | The config generator builds the file and checks each value as you type. | The config generator builds the file and checks each value as you type. |
| 2:18 | 6 slide | Next is the framework release on the Workshop, so your changes survive our updates. | Next is the framework release on the Workshop, so your changes survive our updates. |
| 2:25 | Closing orbit | Everything you saw is in alpha. It is merged and it runs, but it has not met enough players yet. | Everything you saw is in alpha. It is merged and it runs, but it has not met enough players yet. |
| 2:35 | Roadmap slide | That is the next step: playtests on our server, fixing what breaks, then the framework release. | That is the next step: playtests on our server, fixing what breaks, then the framework release. |
| 2:44 | End slide | Join us on Everon, or take the framework and run your own. | Join us on Everon, or take the framework and run your own. |
| 2:50 | | (music fades out over 3 s, then the cut ends) | none |

A long caption is split at the comma into two cues of no more than 42 characters a line and two lines.
On slides the caption sits in the reserved band at the bottom (see "Slides").

## Slides

### Brand tokens (from `website/src/index.css` and `website/BRAND.md`)

Slides use the **day** palette: printed paper, which stands apart from the game footage around it.

| Token | Value | Use on a slide |
|---|---|---|
| `--paper` | `#f0ebe1` | Slide background |
| `--card` | `#fbf8f2` | Cards (roadmap columns, the "Next" box base) |
| `--inset` | `#e5ddcd` | The "Next" box fill; behind a still |
| `--ink` | `#1b1d19` | Text, 3 to 4 px rules and borders |
| `--ink-2` | `#54564d` | Kickers and labels |
| `--green` | `#2d4a33` | End slide background; "Next" box left rule |
| `--on-green` | `#fbf8f2` | Text on green |
| `--stamp` | `#a3302a` | The one status stamp per slide |
| `--sign` | `#e3b23c` | The top bar; the one "Run your own" plate. Never text on paper |
| `--terminal` / `--terminal-ink` | `#1b1d19` / `#e4dfcf` | The terminal panel on slide 5 |

Fonts (OFL, self-hosted in `website/public/fonts/`): **Barlow Condensed** 700 for titles (uppercase),
**IBM Plex Sans** 400 and 600 for anything read, **IBM Plex Mono** 500 for kickers, labels, the stamp
and commands. Logo and mark: `website/public/brand/` (`logo-1024.png`, `mark.svg`, `wordmark.svg`).
Red and yellow are spice, as on the site: one stamp per slide and at most one yellow plate.

### Common frame (1920x1080)

- 96 px side margins; the bottom 160 px stays empty so the caption never covers slide text.
- A 18 px road-sign yellow bar across the top, with a 4 px ink rule under it (the share images do the same).
- The mark at 72 px and `LIFE FRAMEWORK` in Barlow 700, 44 px, top left; `DEV LOG 01` in Plex Mono, 24 px,
  `--ink-2`, top right.
- Status stamp (feature slides): Plex Mono 500, 30 px, uppercase, `--stamp` text, a 4 px border with a
  2 px outline 4 px out, rotated -3 degrees, on paper at 90%. One of three words:
  - `SHIPPED`: verified by a human after a playtest (issue label `verified`);
  - `IN ALPHA`: merged on `main` and on the alpha server, waiting for a playtest (`in-dev`, `needs-playtest`);
  - `PLANNED`: on the roadmap, not built.
  All six features say `IN ALPHA` today (their issues carry `needs-playtest`, checked 2026-11-15:
  #1143, #931, #925, #148, #586). The stamps are re-checked against the labels the day the slides render.

### Title slide

- Left 55%: kicker `ARMA REFORGER` (Plex Mono 500, 28 px, 0.08em tracking, `--ink-2`);
  title `LIFE FRAMEWORK` over `DEV LOG 01` (Barlow 700, 168 px, line height 0.93, two lines); line under it in Plex Sans 400, 44 px:
  "Open-source roleplay. Test town: Everon, 1989."
- Right 45%: the first frame of the intro orbit in a 4 px ink border, saturation 0.85, with the stamp
  `ALPHA` on its top-left corner.

### Feature slide (segments 1 to 6)

- Left 1080 px: the **last frame of that segment's orbit** (or screen recording) in a 4 px ink border.
  The cut from the orbit comes to rest on that frame, so it carries straight onto the slide.
- Right column, 648 px: kicker (Plex Mono 500, 28 px, `--ink-2`), title (Barlow 700, 96 px, up to two
  lines), the status stamp, then the label `WHAT WORKS NOW` (Plex Mono 500, 24 px) over three lines of
  Plex Sans 400, 34 px, each eight words or fewer, then a "Next" box: `--inset` fill, 6 px `--green`
  left rule, label `NEXT` in Plex Mono, the line in Plex Sans 600, 32 px.
- Slide 5 swaps the still's lower third for a terminal panel (`--terminal`, Plex Mono 500, 28 px,
  `--terminal-ink`) with the three `host` commands.

### Roadmap slide

Three columns of `--card` with 3 px ink borders, a Plex Mono header each and up to four Plex Sans lines:

| `IN ALPHA NOW` | `NEXT: M3 FRAMEWORK RELEASE` | `THEN: M4 FLAGSHIP LIVE` |
|---|---|---|
| The supermarket | Two addons: framework and Everon 1989 | The public Everon 1989 server |
| Bank, cash van, robberies | An upgrade path that keeps your saves | Opening day |
| Police, medics, the hospital | The framework on the Workshop | Then: settling in (M5) |
| Saves, backup, the config generator | "Your town in five minutes" | |

The middle column gets the one stamp, `PLANNED`, and the first column a green tick per line. Rows come
from `docs/roadmap/README.md` and are re-checked the day the slides render.

### End slide

- Background `--green`, text `--on-green`. The logo centred at the top.
- Left half, top: `LIFE FRAMEWORK DEV LOG 01` (Plex Mono 500, 28 px, 0.08em tracking, `--on-green`), the series name as on the title slide.
- Left half: `JOIN LIFE FRAMEWORK | EVERON 1989 (ALPHA)` (Barlow 700, 88 px, two lines), then
  `discord.gg/MjMexJteqz` in Plex Mono 500, 34 px.
- Right half: the one yellow plate (`--sign` fill, `--on-sign` text, 4 px ink border): `RUN YOUR OWN`
  (Barlow 700, 88 px), then `github.com/Life-Framework/Life-Framework` in Plex Mono 500, 30 px.
- Bottom line, Plex Mono 400, 22 px: the music credit and "Voice: Piper en_US-ljspeech (LJ Speech, public domain)".

### How slides are rendered

The repo already renders HTML to PNG: `website/scripts/make-og.mjs` uses Playwright 1.49.1 (installed
with `--no-save`, Chromium or an installed Edge) and loads the brand fonts from `website/public/fonts/`
by file URL. The slides use the same route. **Proposed, not built:**

1. `tools/film/devlog/slides.json`: one entry per slide (type, kicker, title, stamp, lines, next, still path).
2. `tools/film/devlog/slides.mjs`: one HTML template per slide type built from the tokens above,
   screenshot at 1920x1080 by Playwright into `tmp/devlog/slides/<id>.png`. The still is the take's last
   frame (`ffmpeg -sseof -0.1 -i <take>.mp4 -frames:v 1 <id>-still.png`).
3. Each PNG becomes a clip with one command (`ffmpeg -loop 1 -t <seconds> -i <id>.png -r 30 -pix_fmt yuv420p <id>.mp4`)
   and goes in `video/devlog-01.json` as an ordinary `clips[]` entry, so `post.mjs` needs no change.
   The title and end slides are clips too, and `titleCard`/`endCard` are left out.

Captions in `post.mjs` use Segoe UI. Captions in IBM Plex Sans would need its TTF (the IBM Plex
release, OFL) in the fonts folder and a font option in `post.mjs`. That is a small change, made only if
the maintainer wants it.

## Music and voice

**Music:** one calm, steady instrumental bed under the whole cut, at -18 dB, ducked a further 10 dB
under the voice, with a 2 s fade in and a 3 s fade out. Chosen: **"Hitman" by Kevin MacLeod**
(incompetech.com, CC BY 4.0), already in `docs/credits.md`. Only CC BY 4.0, Pixabay licence or
CC0 music (`docs/credits.md` "Music"); never the YouTube Audio Library.

Description text:

```
Music: "Hitman" by Kevin MacLeod (incompetech.com)
Licensed under Creative Commons: By Attribution 4.0 License
http://creativecommons.org/licenses/by/4.0/
Voice: Piper en_US-ljspeech (LJ Speech, public domain)
```

**Voice:** Piper `en_US-ljspeech` (high), the MIT 2023 Piper binary that `tools\cli film-make --setup`
installs. No other Piper voice: libritts_r, lessac and the Ryan and HiFi-TTS voices carry research-only or
non-commercial terms (`docs/credits.md` "Voice-over"). One sentence per Piper call, `lengthScale` 1.1.
Every line is listened to once before the cut; a word Piper says wrongly is respelled in the voice text
only, never in the caption.

## Production route (after sign-off)

1. The town-placement agent does the town pass below and runs `tools\cli audit placement --strict`.
2. One `--profile full` session, one client: scout stills of every orbit (`film-make --preview`), then the
   real takes (`video/devlog-01.json` in `lf-storyboard/1`, `--group` per place). `session down` straight after.
3. Screen recording of the config generator; slides rendered; slide clips made.
4. `film-make video/devlog-01.json --no-shoot` posts it: crossfades, captions, voice, music.
5. The quality gate, a frame review by a second agent, then the maintainer watches the file.

## Town pass for the town-placement agent

One list, so the spots are ready before anyone films. Every orbit is in the polished town; none uses
the debug grid.

| Segment | Place | Needs town pass |
|---|---|---|
| I, O | Town square (126, 246) | Clean from 30 m up and at 6 m: no test signs or stray props, NPCs dressed by role at their posts, lamps that light at dusk; the grid and annex out of frame on a north-facing arc |
| 1 | Supermarket (76, 96.5), interior only | Shelf copies seated, counters clear, stockroom door shut, shopkeeper behind the till |
| 2 | Cash van at the bank (196, 252.4) | Strongbox at the van's rear doors on the kerb, verge paper cleared, the bank front's vault hood, rob-kit stand and alarm box neat |
| 3 | Police station (234, 234) | Duty desk, case board, armory, armory store, evidence locker, handcuffs and taser inside; dispatch and wanted boards on the front wall; spike strip off the road; a uniformed officer NPC at the door |
| 4 | Hospital (262, 264) | Duty desk inside (called out by the maintainer), with the dispatch board, waiting bench, cabinet, till and supply crates; the downed-patient prop on a bed inside or removed; the beds inside; the doctor at the door |
| 5 | Farm plots (145 to 148.5, 299.5) | Stand crates, wheelbarrow and signs seated and upright (the plots are grown ripe at filming, a staging step) |
| 6 | (website) | None |

## Sign-off packet (#1422)

This section is the sign-off packet. It takes about 15 minutes. Read the parts named here, tick each
box, and write the answer next to each open question. A box left unticked means "change this", with a
note saying what to change. Nothing is filmed, recorded or rendered until every box in A to D is ticked.

**What to read:** "Why the earlier videos came out weak" (1 min), "Running order" (1 min), "The whole
script" (5 min, best read aloud at an even pace), "Slides" (3 min), "Town pass" (2 min), and the open
questions.

**A. Content**
- [ ] A1. The shape: title, intro orbit, six orbit-and-slide segments, closing orbit, roadmap, end; about 2:50.
- [ ] A2. The six subjects and their order: supermarket, bank and cash van, police, hospital, saves, config generator.
- [ ] A3. Every voice line and caption in "The whole script", word for word. Nothing claims more than the game does today.
- [ ] A4. The statuses: `IN ALPHA` for all six, and the meanings of `SHIPPED` / `IN ALPHA` / `PLANNED`.
- [ ] A5. The "Next" line on each feature slide and the three roadmap columns.
- [ ] A6. The end slide: "Join Life Framework | Everon 1989 (Alpha)" with the Discord link, and "Run your own" with the GitHub link.

**B. Look and sound**
- [ ] B1. Slides: the day palette, Barlow Condensed, IBM Plex Sans and Plex Mono, one stamp and at most one yellow plate per slide.
- [ ] B2. The four layouts (title, feature, roadmap, end), with each feature slide built on the last frame of its orbit.
- [ ] B3. The voice: Piper `en_US-ljspeech` at `lengthScale` 1.1. No other voice.
- [ ] B4. The music track (open question 3), at -18 dB and ducked under the voice.
- [ ] B5. Captions in Segoe UI as now, or IBM Plex Sans (that needs a small `post.mjs` change).

**C. Camera and places**
- [ ] C1. Orbits only: no acting, no driving, no first-person shots. Every orbit is in the polished town and none is in the debug grid.
- [ ] C2. The orbit numbers are a first pass, to be fixed from scout stills before the real take.
- [ ] C3. The town-pass list, including the hospital desk moving inside.

**D. Route**
- [ ] D1. The town pass is done and passes `tools\cli audit placement --strict` before any session.
- [ ] D2. Slides go Playwright HTML to PNG (as `make-og.mjs` does) and become plain clips. Small tooling (`slides.mjs`, the screen recording) is built only after this sign-off.
- [ ] D3. The finished cut passes `docs/marketing/trailer/quality-gate.md`, gets a frame review by a second agent, and the maintainer watches it before it is posted.

**E. Open questions answered** (next section): 1 ___ 2 ___ 3 ___ 4 ___ 5 ___

**After sign-off:** the town-placement agent takes the town pass. Then one agent builds the slide
renderer and the screen recording, scouts the orbits, shoots, and posts the cut. Its PR carries one
still per orbit and the slides as PNGs for a second look before the full render.

## Open questions

1. **The supermarket's place.** It stands at (76, 96.5), next to the annex test area, not on the town
   streets. In the two-zone DebugWorld is it part of the polished town? If not, the board films only its
   interior (as written), or the town pass moves it (a large job).
   [x] answered 2026-11-15: film the interior only, with no move.
2. **Segment 6 is not an orbit.** It is a 14 s screen recording of the config generator. Keep it, or
   make segment 6 a slide only (a still of the page) and add 7 s to another orbit?
   [x] answered 2026-11-15: keep the screen recording for segment 6.
3. **Music.** "Hitman" (already in credits) or "Carefree" (credited, brighter)?
   [x] answered 2026-11-15: "Hitman" by Kevin MacLeod, already in `docs/credits.md`. No new credits row.
4. **The website on the end slide.** The end slide shows Discord and GitHub only, because
   life-framework.org expires on 2026-11-29 and its transfer is pending. Add the site once the transfer
   is done?
5. **Series name.** Is this "Dev log 01" of a series, posted on YouTube and in Discord, or a one-off
   with another title?
   [x] answered 2026-11-15: a series. This is "Life Framework Dev Log 01", posted on YouTube and Discord.
