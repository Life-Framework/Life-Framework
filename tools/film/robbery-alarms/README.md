# Robbery alarms video (#740)

A 37 s clip of the per-type robbery alarms (#987) for Discord: the fuel station at golden hour, a robber at the kiosk
window, the clerk's voiced panic line, the intruder alarm and its wall beacon, the bank vault door swinging open to
the bell, the patrol car pulling up with its lights on, then an end card. Game audio only: no narration, music or
captions. Raw takes and the mp4 stay out of git (`tmp/robbery-video/`).

## Rebuild in one command

With a session up (below), from the worktree root:

```
node tools/film/robbery-alarms/shoot.mjs setup && node tools/film/robbery-alarms/shoot.mjs take all && node tools/film/robbery-alarms/cut.mjs
```

That writes `tmp/robbery-video/robbery-alarms.mp4`, `-contact.jpg` (one frame a second) and `-frames/` (one frame
every 0.5 s for the reviewer). `cut.mjs --out <file>` writes elsewhere; `--takes <dir>` reads other takes.

## Files

| File | What it holds |
|---|---|
| `shoot.mjs` | Session setup (register both clients, dress the robber), the shot list (`SHOTS`), the camera paths (`CAMS`), the robbery's clock (`TALK`, `DEMAND`), time of day (`HOUR`, 17.75) and where each body stands. `take <IDs\|all> [--as=_2]` records takes with game audio. |
| `costume.mjs` | The robber's 1989 civilian costume (denim jacket, dark trousers, black knitted cap) and what to strip. |
| `edit.json` | The cut: each shot's take and in and out points, the anchors that line the four robbery angles up, the audio for each shot, fades, loudness target, size cap, end card. |
| `cut.mjs` | The edit: filter graph, loudness (two-pass loudnorm then a limiter), two-pass H.264 sized under `maxBytes`, contact sheet, review frames. |
| `checklist.md` | What the reviewer checks on every frame, clothing rule included. |

## The session

```
node agent-box/session.mjs up --clients 2 --mission playtest --profile full --res 1920x1080 --sound --wait --hold 45
```

- `--sound` or every take is silent. The host's own volume and mute do not matter: the capture is the game client's
  process (`tools/film/audio.mjs`, process loopback), taken before the endpoint. Never change the host's audio.
- Client 1 is the robber; client 2 is the camera (HUD off, scripted `drone` moves). Client 2's body parks out of
  frame: at the payphone, or for the robbery at the FUEL STATION sign behind the kiosk, because a bystander only
  hears the clerk within the dialogue's 15 m hear range (`LF_Dialogue.Overhear`).
- The pool closes a session held over 45 min while others wait; run `node agent-box/session.mjs lease 40` between
  takes, prepare every edit before `session up`, and `session down` straight after.

## Shots

| ID | Seconds | What | Notes |
|---|---|---|---|
| `EST` | 8 | the station in the last sun, slow push | Shoot it twice (`take EST --as=_w`, then `take EST --as=_2`): the first pass streams the area in and stutters. |
| `WINDOW`, `INSIDE`, `SIDE`, `BEACON` | 24 | one robbery, shot four times from four angles | The demand is picked at 12.8 s: a reply picked while the 8.2 s panic line plays is refused. |
| `BANK` | 10 | the vault door swings open (3 s ease, `LF_BreachTargetComponent`), the bank bell | |
| `POLICE` | 14 | the patrol car (`robbery-police` film scene) pulls up head-on, lights on | The driver wears a dark plain-clothes outfit; there is no police uniform yet. |

## Re-cutting

`edit.json` places the four robbery angles on one clock: `anchors.voice` is the clerk's first word and
`anchors.alarm` the alarm's first frame in each take (video seconds). After re-shooting a robbery angle, read both
from the take's audio (the first loud 0.125 s window after silence) and update them. Every robbery shot plays the
SIDE take's audio, so the voice and the alarm run on without a seam across the cuts.

The end slate's beep is usually masked by the alarm; `audio.mjs` then places the audio by the start slate alone
(`chooseOffset`).

## Known limits

- The harness cannot walk a player smoothly or raise a weapon to aim, so the robber is already at the window with the
  pistol in hand.
- The hold countdown panel is not shown: the robber's own view carries the dialogue subtitle bar on every frame.
- The alarm beacon appears with the alarm (the alarm prefab spawns on the wall box when it sounds).
