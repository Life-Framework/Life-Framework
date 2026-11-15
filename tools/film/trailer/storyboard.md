> **Rejected 2026-11-15, superseded.** The maintainer rejected this cinematic trailer board. The
> dev-log video in `tools/film/devlog/storyboard.md` replaces it. This file is kept for reference only;
> do not film from it.

# Launch trailer: storyboard and script (for sign-off)

Status: **paper only.** Nothing is filmed, no session is up, no music is bought and no voice is
recorded. The maintainer signs off this file (the storyboard and the whole script) before #1293, #1294,
#1351 or #1352 shoot or render anything. Edits after sign-off go in this file first.

Covers slices #1293 (shots 1 to 4), #1294 (shots 5 and 6), #1351 (shots 7 and 8, see "Open questions"
for a mismatch) and #1352 (the edit: `video/m4-trailer.json`, cues, music, end cards). Source of truth
for the shot list is `tools/film/scenes/m4-trailer/m4-trailer.json` (#1292, format `lf-storyboard/1`);
this file adds the script and the edit, and when it is signed off the JSON and the cue files follow it.

## The story in five lines

1. A new arrival on Everon, 1989: a small village, a clerk's desk, and a rules quiz before you may play.
2. Then the town does what a roleplay town does: a cash van gets drilled, a car runs down the main street.
3. The law answers: police recruits, in uniform, applied for and approved.
4. The town looks after its own: a medic brings a downed player back; staff keep the incident log and keep it fair.
5. A full square, then the invitation, twice: join our server, or run your own from the framework.

Tone: `docs/community/discord.md`. Plain and a little wry. No hype, no dates, no "coming soon".
Captions say only what the footage shows and the game does (`docs/marketing/trailer/quality-gate.md`, item 4).
The server is called **Life Framework | Everon 1989 (Alpha)** wherever it is named.

## Format and look

| | |
|---|---|
| Length | about 1:04 (the card for #1352 allows 60 to 90 s) |
| Output | 1920x1080, 30 fps, 16:9, H.264 and AAC, captions burned in plus an `.srt` |
| Look | 17.9 local (golden hour), Clear, day frozen; HUD, hints and toasts off except where the UI is the picture (shots 5 and 6) |
| Filming | `--profile full`, one session for all shots of a slice, `--sound`, `--priority low`; this PC or `lf-cloud`, the pool places it |
| Voice | Piper `en_US-ljspeech` only (public domain). No other voice is safe for public video |
| Transitions | hard cuts on the beat; 0.3 s crossfade into and out of the cards; 4-frame dip to white once, into the heist |
| Gate | the cut passes `docs/marketing/trailer/quality-gate.md` and the maintainer signs the file before it is posted (#1295 renders, nothing posts from here) |

## Shot list in edit order

Shot ids are the storyboard's (filming order and slice split); the edit order is the story's.
Times are the cut's. `Seconds` is the on-screen length, which is also the board's `duration`.

| Edit | Time | Shot | Slice | Seconds | Picture | Voice-over | Caption (lower third) |
|---|---|---|---|---|---|---|---|
| 1 | 0:00 | T01 | G4E | 3 | Title card, logo | "Everon. Nineteen eighty-nine." | LIFE FRAMEWORK / Arma Reforger roleplay, built in the open |
| 2 | 0:03 | G01 | #1293 | 6 | Drone rises over the spawn square | "A small village. Everyone knows everyone. Nobody has told you the rules." | Our server: Everon 1989 (Alpha) |
| 3 | 0:09 | G05 | #1294 | 6 | First person: the rules quiz at the clerk's desk | "Before you sign the register, you pass a short rules quiz." | New arrivals pass a short rules quiz |
| 4 | 0:15 | G02 | #1293 | 6 | Low orbit: a player drills the cash van's box | "Honest work pays. Some people bring a drill instead." | Heists: the cash van |
| 5 | 0:21 | G03 | #1293 | 5 | Follow camera behind a car on the main street | "Then, the getaway." | none |
| 6 | 0:26 | G07 | #1351 | 6 | Police recruits in uniform at the station | "The police are recruited, one application at a time." | Police: apply at the station desk |
| 7 | 0:32 | G04 | #1293 | 6 | Orbit the hospital: ambulance, a medic over a downed player | "Go down, not out. A medic brings you back." | Medics: revive a downed player |
| 8 | 0:38 | G06 | #1294 | 6 | First person: a moderator at the incident panel | "And staff keep it fair. Every incident is logged." | Moderators: the incident panel |
| 9 | 0:44 | G08 | #1351 | 8 | Slow dusk orbit over the square with a crowd | "It plays best when the square is full." | Life Framework | Everon 1989 (Alpha) |
| 10 | 0:52 | End A | G4E | 6 | End card: join the server | "Join our server." | (card text below) |
| 11 | 0:58 | End B | G4E | 6 | End card: run your own | "Or run your own. It is open source." | (card text below) |

Voice-over budget is 2.5 words a second. Every line below fits its shot; the longest is shot 2
(12 words, 4.8 s in 6 s). A line may straddle a cut by half a second rather than squeeze the picture.

## Per shot

Camera numbers are the board's (`m4-trailer.json`); a pose that changes after the scouting stills is
changed here and in the JSON together. Orbit azimuth: 0 is the +z side, 90 +x, 180 -z, 270 -x.

### G01 village: the spawn square (#1293)

- **On screen:** the cobbled square, the welcome sign, the street running east, golden light.
- **Camera:** `drone`, from (110, 3, 252) looking at (130, 1, 250) to (118, 14, 250) looking at (170, 0, 250), fov 60, eased, 6 s.
- **Location:** spawn square (125, 0, 252). Scene `trailer-square`.
- **Props and NPCs:** none staged. The clerk and his desk are in the square; nobody stands in the lens path.
- **Reuse:** the M2 drone keys of `video/trailer-571.json` S05 (they held up).
- **Fallback:** still `area-1-spawn-square/wide-after` with a slow push in.

### G05 quiz: the clerk's desk (#1294)

- **On screen:** a fresh character at the clerk's desk; the rules quiz screen with one question and a chosen answer, then the register form R-89 behind it.
- **Camera:** `firstPerson`, client 1, HUD on (the screen is the picture). The UI sits high in the frame so the caption band stays clear.
- **Location:** the clerk `Quest_Clerk` and desk at (134, 0, 243.5). Scene `trailer-square`.
- **Props and NPCs:** the clerk; the quiz from `LF_Quiz_Everon.conf`. Needs a fresh account each take (a returning one skips the quiz), so the take is scripted with `quiz-proof` and the typed answers are timed (see "Production notes").
- **Fallback:** still of the quiz screen from `tools\cli look`.

### G02 heist: the cash van (#1293)

- **On screen:** the cash van at the bank stop; a player bent over the steel box on the rear step with the drill, sparks, the alarm beacon on the box, sacks spilling.
- **Camera:** `orbit` round (196, 0, 252.4), radius 4, azimuth 90 (the rear), arc 80, height 1.2, 6 s. Low, so the van reads at natural scale (the v3 cold open lesson: stand back far enough that the van, robber and sacks read at real size).
- **Location:** the bank stop, the van facing west. Scene `trailer-bank`. The van drives its route, so frame from the robber's live position as `O3` did.
- **Props and NPCs:** client 1 as the driller (`bank-kit-proof` for the drill, then `use` the van box); client 2 off duty holds the camera so no dispatch toast lands on the recorded screen.
- **Reuse:** none; M2 S01 and S02 were never shot.
- **Fallback:** orbit the bank front with no actor.

### G03 chase: the main street (#1293)

- **On screen:** a car driving east along the main street, wheels turning, the driver in the seat, the street behind.
- **Camera:** `follow`, near (128, 0, 250), distance 10, 5 s.
- **Location:** main street. Scene `street`, which spawns the `S105_beige` and drives it along z 250 (`LF_FilmActor`).
- **Props and NPCs:** one clothed puppet driver. Honesty: the drive is a physics-body drive with no engine or brake model (`docs/film-pipeline.md`, "Driving"), so the caption claims nothing and the voice says "the getaway", which is only what the picture shows: a car leaving.
- **Reuse:** the `follow` call of `video/camera-test.json` (proven in game 2026-11-10).
- **Fallback:** static at (132, 2.2, 250) looking east, the car passes through.

### G07 recruits: the police station (#1351)

- **On screen:** three police recruits in uniform stand in a row at the station front; the jeep with its light bar flashing behind them.
- **Camera:** `static` at (236, 1.7, 240) looking at (244, 1.4, 246), fov 50, 6 s, with a 1 m dolly toward the line over the shot.
- **Location:** police station (243.5, 0, 246). Scene `trailer-police`.
- **Props and NPCs:** three puppets in the police outfits (`tools\cli outfits generate`), placed with `tools\cli place`; the `LF_UAZ469_Police` at `Roadmap_Police_Car` with siren mode 1.
- **Optional insert (2 s, only if S9 has merged):** the Game Master panel with one open application and the approve button, cut in before the line of recruits. If S9 has not merged the shot stands alone and the caption stays as above.
- **Fallback:** orbit the jeep with no recruits (`video/trailer-571.json` S04); the voice line then becomes "The police keep a station on the square." and the caption is dropped.

### G04 ambulance: the hospital (#1293)

- **On screen:** the hospital front; the ambulance in its bay under the sign; a medic kneeling by a downed player doing CPR (#512), then the player stands.
- **Camera:** `orbit` round (262, 0, 252), radius 6, azimuth 180 (the front faces -z), arc 100, height 1.8, 6 s.
- **Location:** hospital (262, 0, 252). Scene `trailer-hospital`.
- **Props and NPCs:** client 1 as the medic (clock in at the hospital duty desk, then hold Revive; `revive-anim-proof` flow), client 2 downed by `shoot`; the `LF_M997_Ambulance` in the bay. Blocked by #512 and #625 being closed; the driven-ambulance part of the board is dropped here (the ambulance sits in its bay, nothing claims a run).
- **Fallback:** still of the ambulance in its bay, slow push in.

### G06 moderator: the incident panel (#1294)

- **On screen:** the Game Master incident panel open over the plaza: markers per incident, one flagged `!!`, the list on the right.
- **Camera:** `firstPerson`, client 2 (the moderator, an administrator), HUD on. The panel sits high in the frame.
- **Location:** spawn square (125, 0, 250). Scene `trailer-square`.
- **Props and NPCs:** client 1 stands in the square as the player who `/report`ed; the incident exists because client 2 staged one first (a hold-up between the two clients), so the panel has a real row. Nothing is faked in the panel.
- **Fallback:** still of the panel from `tools\cli look`.

### G08 crowd: the square at dusk (#1351)

- **On screen:** a slow dusk orbit over the square with a real crowd of players on it.
- **Camera:** `orbit` round (125, 0, 250), radius 16, azimuth 270, arc 120, height 7, 8 s. Scene `trailer-square-dusk` (19.2).
- **Props and NPCs:** twelve or more real players. A session of three clients is not a crowd, so this shot is filmed last, at a playtest or the opening, with the maintainer's players.
- **Fallback:** `video/trailer-571.json` S28, the main street at dusk with no people; the line then becomes "The square is waiting." and no crowd is claimed.

## The full script, start to finish

Read aloud by Piper `en_US-ljspeech`. Numbers are spelled out; the URL is read as it is spoken. If Piper
mispronounces "Everon", respell it in the TTS line (EV-er-on) without changing the caption.

| # | Time | Voice-over (caption `.srt` shows this) | TTS (what Piper reads) |
|---|---|---|---|
| 1 | 0:00 | Everon. Nineteen eighty-nine. | Everon. Nineteen eighty-nine. |
| 2 | 0:03 | A small village. Everyone knows everyone. Nobody has told you the rules. | A small village. Everyone knows everyone. Nobody has told you the rules. |
| 3 | 0:09 | Before you sign the register, you pass a short rules quiz. | Before you sign the register, you pass a short rules quiz. |
| 4 | 0:15 | Honest work pays. Some people bring a drill instead. | Honest work pays. Some people bring a drill instead. |
| 5 | 0:21 | Then, the getaway. | Then, the getaway. |
| 6 | 0:26 | The police are recruited, one application at a time. | The police are recruited, one application at a time. |
| 7 | 0:32 | Go down, not out. A medic brings you back. | Go down, not out. A medic brings you back. |
| 8 | 0:38 | And staff keep it fair. Every incident is logged. | And staff keep it fair. Every incident is logged. |
| 9 | 0:44 | It plays best when the square is full. | It plays best when the square is full. |
| 10 | 0:52 | Join our server. | Join our server. |
| 11 | 0:58 | Or run your own. It is open source. | Or run your own. It is open source. |

Captions (lower third, at most 42 characters a line, at most two lines, white on a dark band), in order:

1. LIFE FRAMEWORK / Arma Reforger roleplay, built in the open (full frame, the title card)
2. Our server: Everon 1989 (Alpha)
3. New arrivals pass a short rules quiz
4. Heists: the cash van
5. (none)
6. Police: apply at the station desk
7. Medics: revive a downed player
8. Moderators: the incident panel
9. Life Framework | Everon 1989 (Alpha)
10. and 11. the end cards, full frame, below

Every claim is built and in `docs/features.md`: the quiz (#915, `LF_Quiz_Everon.conf`), the heist (#513), the
revive and CPR (#512), the duty desk and police applications (#128), the incident panel (#240) and
`/report` (#241). Caption 6 is true today; the approval half of "one application at a time" needs S9 and the
line stays only if S9 has merged when G07 is shot (otherwise use the fallback line in the G07 section).

## The end cards (the run-your-own close)

Two full-frame cards over a slow push on the square at dusk (the G08 picture, or `S28` if there is no
crowd), a dark overlay, the logo centred. They say what the join page says
(`website/src/components/RunYourOwn.tsx`: the example server, the config generator, the install guide).

**Card A, 0:52 to 0:58: join our server**

```
Life Framework | Everon 1989 (Alpha)
Find it in the server browser
life-framework.org/join
Discord: discord.gg/MjMexJteqz
```

**Card B, 0:58 to 1:04: run your own** (the three steps, one line each)

```
Run your own: life-framework.org/join
1  Start from the example server
2  Set your numbers: life-framework.org/config
3  Install it from zero: the install guide
Source and install guide: github.com/Life-Framework/Life-Framework
Open source, MIT. Built in the open with AI agents.
```

Small credit lines under card B (the ones `README.md` and `docs/marketing/trailer/script.md` already
approve): "Built on Everon Life by Arkensor", "Code from Overthrow and LNGCLifeRP_Public", "Game assets by
Bohemia Interactive". The video description carries "Voice: Piper en_US-ljspeech (LJ Speech, public
domain)" and the music credit (`docs/credits.md`). Fade to black over the last second.

`post.mjs` takes one `endCard`; #1352 either lets card A and card B be two clips (a still of the square
with the card text) or one card with both blocks. Content is as above in either case.

## Music bed

One instrumental track, about 100 BPM (a bar is 2.4 s), warm and a little tense, nothing that sounds
like a stock corporate bed. It ducks 6 dB under every voice line (`sidechaincompress`, as `post.mjs`
does) and sits under the game sound, which is kept: the drill, the sacks, the siren, the engine. Mixed
to about -14 LUFS, true peak under -1 dBFS.

| Time | Energy (of 5) | Cue |
|---|---|---|
| 0:00 | 1 | Low pad, no beat, room tone; the title card |
| 0:03 | 2 | Light pulse as the drone rises over the square |
| 0:15 | 3 | A beat comes in under the heist's drill |
| 0:21 | 4 | Full bed on the getaway; cuts land on the bar |
| 0:32 | 2 | Break: the beat drops out for the medic, a single sustained note |
| 0:38 | 3 | Back to the pulse for the moderator |
| 0:44 | 4 | Builds under the crowd orbit |
| 0:52 | 3 | Drums out, the melody stays under the join card |
| 0:58 | 3 | Same bed, holds for the run-your-own card |
| 1:03 | 1 | Ring out to the fade |

Choice for sign-off. Reuse **Kevin MacLeod "Hitman"** (CC BY 4.0, already in `docs/credits.md`, used in
the v1 cut; the credit line goes in the description), or pick another track under the licence rule in
`docs/credits.md` (Kevin MacLeod, Pixabay Music, CC0 from the Free Music Archive; never the YouTube Audio
Library). The ElevenLabs tracks in the mod are licensed for use inside the mod only, so they are out. The
track is chosen and recorded in `docs/credits.md` before #1352 writes the cue.

## Production notes

- Placement first: `tools\cli audit placement --strict` clean from a worktree merged with `origin/main`, then `tools\cli shots area <name> --eye-only --annotate` for each location and look at the contact sheets (quality gate, step 0).
- Prepare every edit before `session up`, film every shot of a slice in one session, `session down` straight after. G01 to G04 in one session (client 1 hero, client 2 the patient or off-duty camera, a third for the van if needed); G05 and G06 in one (fresh account for G05, then client 2 on the moderator roster for G06); G07 and G08 later.
- The camera paths and setup above go into `tools/film/scenes/m4-trailer/m4-trailer.json` as `setup` lists and `pending` lines are removed as each cause is gone (`tools/film/storyboard.mjs`). The edit is `video/m4-trailer.json` and `video/cues/m4-trailer/**`, rendered with `tools\cli film-make video/m4-trailer.json`; clips land in `website/public/media/m4-trailer/{a,b,c}/`.
- Each slice's PR carries one still per clip. The final cut is reviewed frame by frame by an agent that did not cut it, then watched by the maintainer.

## Open questions for the maintainer

1. **Shots 7 and 8.** #1351's card says "an administrator approving a police recruit in the Game Master panel, and the rules board at the plaza with the siren behind it". The #1292 board says "police recruits in uniform" and "the crowd". This file follows the board (they match the chapter's beat "the first police recruits in uniform") and keeps the GM approval as an optional 2 s insert. The rules board with the siren is not in this cut; say if you want it instead of the crowd, or in addition, as a 4 s insert before the end cards.
2. **The crowd.** It needs twelve or more real players and a playtest first. Fallback is the empty dusk street with the changed line. Accept the fallback if the crowd never happens before launch?
3. **The chase.** The car drive is a scripted physics-body drive. It looks like a car driving down the street, nothing more; the voice and caption claim nothing. OK to ship that?
4. **Beat 1 of the chapter** ("the server in the browser with real players") is not a shot here, because it needs the server live. Card A carries the server name. If you want it as a shot, it is filmed after opening and replaces the empty frame of card A.
5. **Length.** 1:04 against the card's 60 to 90 s. Room for one more 6 s shot (the rules board, or the bank teller) if you want it.
6. **Links on the end cards**: `life-framework.org/join`, `life-framework.org/config`, the GitHub repository and the Discord invite from `docs/marketing/trailer/script.md`. Confirm the invite is the one to publish.

## Sign-off

- [ ] The five-line story
- [ ] The shot list and per-shot pictures
- [ ] The full voice-over and caption script
- [ ] The music direction and the track choice
- [ ] The end cards
- [ ] The open questions above answered
