# Film pipeline: research and chosen approach

Research for #391, epic #390 (storyboard to YouTube). Measured 2026-11-10 on
this PC (RTX 3080, dedicated server and one game client on the same box,
DebugWorld "LF_PlaytestTown"), client at 1920x1080. The throwaway code behind
the tests is on the branch `ws/film-spike` (commit `c1299876`: a `spike`
harness verb, not for `main`); cherry-pick from it when building #392/#393.

## Answers in one table

| Question | Answer | Evidence |
|---|---|---|
| Can a script drive a free camera while another entity stays controlled? | Yes. `LF_FilmCameraRunner` (a `SCR_ManualCamera` with input off, `CameraManager.SetCamera`) orbited the plaza 6 m up while the player character stayed controlled and kept answering `stand`/`state`. | Clip frames below; log `camera pass started` / `camera pass done`. |
| Can base-game AI walk to a waypoint on cue? | Not in DebugWorld: the world has no AIWorld and no navmesh. Where a navmesh exists (MainWorld subscenes Eden) it should; not tested. | Server log `AI (E): No AIWorld is present in the map. Move handlers won't be created.` A runtime `SCR_AIWorld` made the agent move 0.2 m and stop for 40 s, 14 m short. |
| Can an actor walk and use one of our actions on cue without AI? | Yes, as a scripted puppet: a spawned character driven through `CharacterControllerComponent`, then `PerformAction` on our action. | 12.6 m in 3.9 s, then `LF_StoreTillAction` ran for it (below). |
| Capture: in-engine vs OS capture | In-engine frames for video (works on the hidden desktop, 53 fps at 1080p measured), a separate WASAPI capture for audio. | Numbers below. |
| 1080p60 cost | About 5.8 MB per frame (310 MB/s, 19 GB per minute of scratch), NVENC encode 1.75x real time. | Numbers below. |

## Cameras

**What a script can spawn and drive.** `api_search` (2026-11-10):
`CameraBase` (`SetTransform` via entity, `SetVerticalFOV`, `SetFOVDegree`,
near/far planes, `GetWorldCameraTransform`), `SCR_ManualCamera` (`AttachTo`,
`Detach`, `SetInputEnabled`, `TrySwitchToControlledEntityCamera`),
`CameraManager` (`SetCamera`, `CurrentCamera`, `SetOverlayCamera`,
`GetCamerasList`) and `ScriptCamera`. Vanilla ships camera prefabs, among them
`Prefabs/Editor/Camera/ManualCameraBase.et` and `ManualCameraPhoto.et`. There
is no spline class to script against; a path is our own keys.

**Already in the repo.** `LF_FilmCameraPath` keys (time, position, look-at,
FOV) and `LF_FilmCameraRunner` (spawn, hide HUD, move each frame, restore).
Pure sampling is under test; the runner had no harness verb. The `ws/shots-area`
work (uncommitted in that worktree when checked) poses the player body for a
clean shot (HUD hidden, hint hidden, clock hour, clear sky, a trace probe for
camera guarding). That is a good source of clean-frame and time/weather
helpers, but it does not move a free camera, so it is not the base of the
rig. `LF_ScreenshotCapture` already sets time of day and weather per shot
(`TimeOfDay24h`, `WeatherStateName`), which #394 reuses.

**Tested.** Verb `spike cam <x y z> <radius> <seconds>` plays an 8-segment
orbit through the runner while `film` records at 15 fps. Result: 275 shots in
18.4 s, `gameFps=30`, no stalls. Frame at 5 s shows the plaza from 6 m up; the
player had been stood at the general store (180, 244) and was moved to the
spawn sign (129.1, 0, 250) during the pass, so a second entity is controlled
and the view comes from the camera. Frame at 16 s (pass finished) is the
player's view again: the previous camera is restored.

Two findings for #392:

1. **A resource constant without a GUID crashed the client.** The runner's
   `"Prefabs/Editor/Camera/ManualCameraBase.et"` logs `Wrong GUID/name for
   resource @"{0000000000000000}..."` and the client died with `ENGINE (F):
   Crashed` right after `camera pass started`. With
   `{D6DE32D1C0FCC1C7}Prefabs/Editor/Camera/ManualCameraBase.et` the pass ran.
   The same happened on the server for an AI group prefab (`Attempting to
   insert a runtime entity without a prefab into replication`, then a crash).
   Rule for the whole video track: spawn constants are always `{GUID}path`.
   `LF_ScreenshotCapture.CAMERA_PREFAB` has the same path-only form.
2. **`SCR_HUDManagerComponent.SetVisible(false)` left the hint toast** ("New in
   town") on screen in the 5 s frame. A clean frame also needs
   `SCR_HintManagerComponent.HideHint()` (what shots-area does).

**Shot kinds.**

- Static, orbit, drone/path: the free camera on a keyed path. Add easing and
  look-at-an-entity (resolve the entity each frame) in the rig.
- Follow: `SCR_ManualCamera.AttachTo(actor)` or move the camera each frame from
  the actor's position and heading. Not tested.
- First person of an actor: place the camera at the actor's head each frame.
  Not tested. For the controlled player, the normal player camera is already
  first person, and `SwitchCameraType` is a harness input action.

## Actors

**Base-game AI.** The API is all there: `AIGroup.AddAgent/AddWaypoint`,
`AIWaypoint_Move.et` (`{750A8D1695BD6998}`), `SCR_BoardingWaypoint`
(get in a vehicle as driver/cargo), `SCR_SmartActionWaypoint`,
`SCR_AIAnimationWaypoint`, `SCR_TransportWaypoint`. Spawning worked once the
constants carried GUIDs (agent, group, waypoint all created; `control=1
agent=1`). It cannot path in DebugWorld, because that world is a custom
terrain with no `SCR_AIWorld` and no baked navmesh (`MainWorld.ent` is a
subscene of Eden, which has both). Baking a navmesh for DebugTerrain is a
Workbench job that has not been tried. So AI stays an option only where a
navmesh exists.

**Scripted puppet (recommended).** Spawn `Character_Roleplay.et`
(`{9B5BB216CC7FF18E}`), then every 100 ms call
`CharacterControllerComponent.SetHeadingAngle(atan2(dx, dz))` and
`SetMovement(2, "0 0 1")` (2 is a run) toward an authored waypoint. Server log:

```
puppet t=3.23 pos=<183.4,0,244.1> dist=4.39
puppet stopped t=3.93 pos=<181.3,0,244.95> dist=2.41
action LF_StoreTillAction canPerform=1 uid=''
[LFDebug:Store] store general_store: till refused 0x20000000000003E0 {}, nothing to pay for
till action performed by the AI on cue
```

It covered 12.6 m in 3.9 s with the character's own animation, stopped on cue
and ran our action's `PerformAction(till, puppet)` on the server. Limits:

- No obstacle avoidance. A waypoint list must be authored and checked (the
  shots-area trace probe is the tool for that). The puppet stalled 2.0 m from
  the till because the counter blocks it; stop radius 2.5 m worked.
- Our actions that need a player identity refuse an actor:
  `LF_Utils.GetPlayerUid(puppet)` is empty (`LF_JobSignUpAction` answers
  `#EL-Job_NotSignedUp`). Actions that act on the entity itself (the till)
  work. #393 should give an actor a stand-in identity for the demo path
  instead of changing every action.
- Driving is not tested. Candidates: `SCR_BoardingWaypoint` with AI where a
  navmesh exists, or a script-driven vehicle controller. #393 should spike a
  driver first, since the acceptance clip needs a drive.

**Replay of recorded player input** needs one game client per actor, and a
client is a full game process (0.7 to 1.5 GB RAM each, per `docs/worktrees.md`).
Rejected for anything but the one hero player.

## Capture

All runs: 1920x1080 window, harness verb `film start <name> <fps>`, BMP frames
to `$logs:el-film/<name>/`, encoded by `tools/film/engine.mjs` (NVENC).

| Run | Game cap | Shots | Captured | Game fps | Frame ms (plain / after a shot) | Stalls |
|---|---|---|---|---|---|---|
| 1080p, 15 fps | 30 | 275 in 18.4 s | 15 | 30 | 33.4 / 33.3 | 0 |
| 1080p, 30 fps | 30 | 366 in 12.2 s | 30 | 30 | n/a / 33.3 | 0 |
| 1080p, 60 fps requested | 60 | 672 in 12.6 s | 53.2 | 53.7 | 15.4 / 18.6 | 0, 0 failed |

- The 60 fps row needed the recorder and harness limits raised from 30 to 60
  and the client `-maxFPS 60` (`ws/film-spike`). A shot costs about 3 ms of
  frame time. The recorder shoots at most once per rendered frame, so the
  ceiling is the game's frame rate, 53.7 here, not 60. Record at the
  requested rate and resample in post (`ffmpeg -vf fps=60`) for a constant
  frame rate; the clips are variable-rate by real timestamp today.
- A 1080p frame is 5.8 MB: 3.9 GB for the 672 frames (310 MB/s while
  recording). NVENC encode of those 672 frames took 22.1 s; 275 frames took
  7.4 s. The 12.6 s clip is 12.6 MB.
- No motion blur comes from this path; each shot is one rendered frame.
- OS-level capture (`ddagrab`, `gdigrab`) cannot see the hidden `LF-Agents`
  desktop, and an unfocused window stops presenting (`agent-box/README.md`,
  "Screen capture": one frame held for 68 s on 2026-11-08). It stays the tool
  for a focused window only. Its 1080p60 numbers were not measured:
  `lf-testbox` filming is broken (#482: clips show only the Steam window and
  the audio endpoint is muted), so every capture figure on that box, and
  every audio figure, depends on #482. The only measured screen capture is
  the 2026-11-08 one in `docs/worktrees.md`: 2560x1440 at 28 fps with audio.

**Audio.**

- The session starts every client with `-noSound` (`clientArgs`, `sound`
  defaults to false), so the in-engine films are silent by construction.
  `api_search` found no engine API that records audio (`AudioSystem`,
  `SoundWorld`, `AudioHandle` are playback classes), so game audio has to be
  captured outside the process.
- The repo's route is `tools/remote/desktop/lf-loopback.ps1`, a WASAPI
  loopback of the default output written as raw PCM into ffmpeg. It records
  everything that plays on that endpoint, after volume and mute. That is why
  #482 gives silent clips, and it is why the laptop's output is the silent
  "Steam Streaming Speakers" endpoint.
- Recommended for #396: video from in-engine frames, audio from a separate
  capture of the game process only (Windows process loopback,
  `AUDCLNT_PROCESS_LOOPBACK` with the client's pid, which leaves other host
  sounds out), written next to the frames as its own WAV track. Align both
  with the engine clock: `index.txt` already holds each frame's real time, so
  the audio starts at the `film start` wall time and a one-off marker sound
  at start proves the offset. Untested here: whether a client on the hidden
  desktop has an audio device, and drift over 60 s.

## Cost and machine needs (1080p60)

- GPU: an RTX 3080 ran game plus capture at 53.7 fps with the server on the
  same machine. Expect one such GPU per shooting client, NVENC for the encode.
- Disk: 310 MB/s sustained, 19 GB scratch per minute of 1080p60 (10.4 GB at 30
  fps, 5.2 GB at 15). Frames are deleted after the clip is written. 4K would be
  about 4x that (23 MB per frame); render 1080p and upscale, or do not do 4K.
- Time: shoot in real time, then encode at 1.75x real time: about 1 min shoot
  and 1 min 45 s encode per 60 s of footage, per take. Shots can be encoded
  while the next shoots.
- Machine: this PC. The laptop (`lf-testbox`) is only for OS-level capture and
  waits for #482; the game slot is shared, so a shoot is a session job
  (`--priority low`, session down between takes).

## Recommendations by issue

| Issue | Do this |
|---|---|
| #392 Directed camera rig | Build on `LF_FilmCameraRunner`. First: `{GUID}` camera prefab (and the same fix in `LF_ScreenshotCapture`), a `camera` harness verb (the `spike` verb is the starting point), HUD plus hint hiding, easing, look-at-entity, follow via `AttachTo`. Take clean-frame, time and weather helpers from `ws/shots-area` when it merges instead of writing new ones. First person of an actor and follow need a short spike. |
| #393 Autonomous actors | Scripted puppet with authored waypoint lists, not AI and not input replay. Gives walk/run/use today in DebugWorld. Needs a stand-in identity for actions that read a player uid, and a driving spike first (the acceptance clip drives). The "crowd or traffic" extra stays out of DebugWorld: no navmesh. Either bake one (Workbench) or shoot crowd scenes in MainWorld. |
| #394 Scene setup and reset | Reuse the `TimeOfDay24h`/`WeatherStateName` setup in `LF_ScreenshotCapture`. A scene is: clock, weather, puppet spawn points, camera keys. Reset by despawning puppets and restoring the clock; the world itself is the DebugWorld mission start. |
| #395 Storyboard and film-make | Shots map to (camera keys, puppet steps, duration). Output size and fps are recorder parameters; vertical 9:16 needs a 1080x1920 client window (`--res`), untested. Default 1080p30, 1080p60 for hero shots. |
| #396 Game audio in sync | Can start now, in parallel with #392/#393 (it depends only on #391). Process-loopback capture per the Audio section; first run needs `--sound` clients and #482 settled for the laptop. |
| #397 Post-production | Plan for variable-rate frame input: resample to constant fps, then join. Scratch disk 19 GB per minute at 60 fps; encode is 1.75x real time. |
| #398 First real video | No change; use 1080p30 for the first video. |

**Epic build order.** The order #392, #393, #394, #395, #396, #397, #398 stands,
with two changes: #396 runs in parallel from the start (its only dependency
is this research), and #393 begins with the driving spike because that is the
largest unproven item.

## Biggest risks

1. Driving (vehicles) has no tested path and the first video needs one.
2. Audio: no in-engine capture exists, and the only working capture
   (WASAPI loopback) records the whole endpoint; process loopback and sync
   are untested, and the laptop is blocked on #482.
3. Frame rate: the game's own frame rate bounds capture (53.7 fps at 1080p
   here with a server on the same box), so true 60 fps is not guaranteed.
