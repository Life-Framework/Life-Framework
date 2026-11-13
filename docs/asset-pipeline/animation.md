# Animation for an asset

`tools\cli asset anim <slug>` is the stage that makes an asset move: parts of the model that turn,
the animation the character plays while using it, sparks at a socket, a looped sound. It runs after
`prefab`, and `tools\cli asset run` runs it last; an asset with no `anim` block skips it. What the
first run (the vault drill) taught us is in
[`docs/architecture/asset-anim-spike.md`](../architecture/asset-anim-spike.md).

```
tools\cli asset anim <slug>          clips, workspace, effect; checks the prefab's component
tools\cli asset anim <slug> --fix    also rewrites the prefab's LF_AssetAnimComponent from the spec
tools\cli asset run <slug> --only clean    after changing anim.bones or anim.sockets (then --only import)
```

## What the spec declares

```json
"anim": {
  "prefab": "Prefabs/Items/Fence/LF_Drill.et",
  "bones":   [{ "name": "chuck", "motion": "spin", "axis": "z", "speed": 1440,
                "pivot": [-0.034, 0.075, 0.08], "box": [-0.064, 0.046, 0.045, -0.004, 0.105, 0.32] }],
  "sockets": [{ "name": "fx_bit", "pos": [-0.034, 0.075, 0.3] }],
  "effects": [{ "socket": "fx_bit", "sparks": { "rate": 60, "speed": 1.8, "life": 0.3, "size": 0.012, "spread": 75 } }],
  "sounds":  [{ "event": "SOUND_LF_DRILL_MOTOR_LP", "project": "{GUID}Sounds/Items/Drill/Items_Drill_Use.acp" }],
  "character": { "workspace": "LF_DrillPush", "dir": "Anims/Workspaces/Work", "control": 562,
                 "clips": { "in": "clip:push_in", "loop": "clip:push_loop", "out": "clip:push_out" } }
}
```

| Key | Means | Becomes |
|---|---|---|
| `bones` | A part that moves by itself. `spin` turns at `speed` degrees a second while the asset is active; `hinge` swings `angle` degrees in `seconds` and back; `slide` moves `distance` metres in `seconds` and back. `axis` is model space (x right, y up, z forward), `pivot` the point it turns about, `box` the faces that move with it | A skinned bone in the model (clean stage) and a row in the prefab's `LF_AssetAnimComponent` |
| `sockets` | A named point: where sparks come out, where a hand grips. `parent` puts it on a bone so it moves with the part | An empty in the model, a node in the `.xob` |
| `effects` | A particle effect at a socket while active: `sparks { ... }` (the vfx hook writes `Particles/LF/<Name>_<socket>.ptc`) or `particle`, a `{GUID}path` to any `.ptc` | A component row |
| `sounds` | A looped event of a project `tools\cli asset sound` wired | A component row |
| `character` | The character's animation while using the asset: in, loop, out, each a vanilla clip (`{GUID}path.anm`) or a custom one (`clip:<name>`) | A workspace (`.agr .agf .ast .asi .aw`) that plays as a custom loiter when `Gestures == control` |
| `hold` | How it is carried: a vanilla carry `profile` (`jerrycan`: one hand, arm down at the side) and the `grip` socket, which must sit where that profile's hand closes | The `ItemAnimationAttributes` block for the prefab (`tmp/assets/<slug>/anim/hold.txt`) |
| `actions` | `place` and `throw`: a clip and the second the item leaves the hand | Checked only, so far; the runtime comes with the first carried asset |

`prefab` names the prefab when it is not `<prefab_dir>/<Name>.et`. The anim block is checked with
the rest of the spec; the messages say what to change.

## The runtime half

`LF_AssetAnimComponent` (`Feature/AssetAnim/`) is on the asset's prefab. The server calls
`SetActive(true|false)`, and `Pose(position, angles)` / `Release(position, angles)` to hold the asset
still in the air and set it down again. Both are replicated properties, so a player who joins
mid-use sees the same. Every machine that renders turns the bones (`Animation.SetBone`, per frame,
only while something moves), spawns the effects on the sockets and starts the sound loops; a machine
out of earshot retries every two seconds. Feature code decides when: `LF_DrillScene` is the example
(the vault drill: `Begin` when the work animation starts, `End` from `LF_WorkAnim.End` on every
exit).

The component block in the prefab is generated. Do not edit it by hand: change the spec and run
`asset anim <slug> --fix`. `tools\cli validate` (`asset-anim-selftest`) fails when the prefab, a
`.txa`, the workspace or the effect differ from what the spec generates.

## Step 1: the moving part

1. Run the clean stage once without an anim block and read `clean/review/side_left.png` and
   `top.png`. A render is 1100 px wide over 1.5 times the model's longest side; the centre of the
   picture is the model's centre. That gives the part's position to a centimetre.
2. Write the bone: the axis it turns about, a pivot on that axis, a box around the faces that move.
   Keep the box tight: a face whose centre is inside moves, whatever it belongs to.
3. Rerun `--only clean` and read `review/part_<bone>.png` and `part_<bone>_side.png`. The part is
   drawn pulled out of the model (a spin, a slide) or swung open (a hinge). **Only the part may have
   left.** A piece of the body that went with it means the box is too big; a piece of the part left
   behind means it is too small.
4. A spinning part must be round about its axis, or the spin shows as a wobble. Put the pivot on
   the centre of the part's cross-section, not on the model's centre line (the drill's bit is
   3.4 cm off-centre).
5. Speed: at 30 frames a second anything over about 900 degrees a second reads as a blur, which is
   what a motor should do. A part the player should see turn (a crank, a wheel on a safe) wants 90
   to 360.

A socket for an effect belongs to the root, not to the spinning bone, or the effect spins with it.

## Step 2: the character's clip, vanilla or custom

Decide in this order.

1. **Does a vanilla clip show the action?** List the candidates
   (`docs/research/animation-pipeline.md` section 1.4: CPR, kneeling treatment, repair and
   demolition work loops, push and pull, one-hand pick and place, the grenade throw, the jerrycan
   carry). A vanilla clip is free, has real motion capture in it and blends with the engine's own
   states. Use it when the body does what a player would call the action from ten metres away,
   even if the hands are a few centimetres off.
2. **Is the mismatch in what the hands hold?** Then reuse still wins if the asset can be shaped to
   the clip: put the grip socket where the vanilla item's handle is (`hold.profile`).
3. **Otherwise write a custom clip.** The base game has no drill, saw, hammer, lockpick, carry,
   surrender or cuffed pose. A custom clip here is a pose script, not a hand animation: good for a
   braced working pose with a small repeating motion, a hold, a reach, a lean. It is not good for
   a walk, a fall, or anything where weight shifts through several steps; that still needs an
   animator.

The drill went through this list: the vanilla demolition loop (what #513 shipped) has the robber
bent over the ground with empty hands, which does not read as drilling a wall, and no vanilla clip
holds a tool at chest height. So it got a custom clip.

### Writing a custom clip

`tools/assets/<slug>/clips/<name>.json`:

```json
{ "name": "push_loop", "fps": 30, "frames": 40, "loop": true, "bones": "body",
  "preview": { "wall_z": 0.95, "props": [{ "asset": "drill", "name": "Drill", "pos": [0.135, 1.23, 0.63] }] },
  "keys": [
    { "frame": 0,
      "hips": [0, -0.05, 0.02],
      "turn": { "Spine1": [7, 0, 0], "Neck1": [-8, 0, 0] },
      "legs": { "left": { "foot": [-0.16, 0.1, 0.24], "yaw": 5 }, "right": { "foot": [0.19, 0.1, -0.3], "yaw": 25 } },
      "arms": { "right": { "hand": [0.1, 1.2, 0.42], "elbow": [0.45, 1.0, 0], "fingers": [0, -0.25, 1], "palm": [-1, 0, 0] } },
      "curl": { "right": 75 } },
    { "frame": 20, "...": "the other end of the motion" }
  ] }
```

Everything is in the character's own space: metres, x right, y up, z forward, the origin between
the feet. A key says where things are, and the tool solves the joints (`pose.mjs`):

| Key field | Does |
|---|---|
| `hips` | Moves the hips from where they rest (0.93 m up). Lower them before stepping a foot out |
| `turn` | Turns a bone by degrees about x (forward lean is positive), y, z |
| `legs` | Puts an ankle at a point (it rests 0.10 m above the ground) with the sole flat, toes out by `yaw` |
| `arms` | Puts a wrist at `hand`, bends the elbow towards `elbow`, points the `fingers`, faces the `palm` |
| `curl` | Closes the fingers: 0 open, 75 a fist round a handle |
| `prop` | Where a held item's origin sits, for a clip that carries one |

`bones` is `"body"` for a full-body clip (a bone the keys do not pose rests, so name the legs or
the character stands in its bind pose) or a list of bone names for a part
(`["LeftShoulder"]` writes that arm and leaves the rest to the locomotion graph). A loop needs no
last key; it closes on its first. Frames between keys are eased.

The stage refuses a clip with a hand or foot target more than 3 cm out of reach, a foot under the
ground or in the air, or a loop that does not close, and says which key.

### Reading the preview

The stage renders `tmp/assets/<slug>/anim/preview-<clip>/f<frame>_{side,front,back}.png`: a stick
figure (left limbs blue, right limbs orange, a nose on the head), the floor, the wall at
`preview.wall_z`, and the props. Read every picture.

- **Balance.** The head is over the space between the feet, or over the front foot when leaning on
  something. A head ahead of both feet with nothing to lean on is falling.
- **Feet.** Flat on the floor, a shoulder width apart or more for work. A braced pose has one foot
  forward and one back.
- **The line of force.** For a push: back foot, hips, shoulders and hands in one slanted line
  towards the thing pushed. Arms straight up or elbows behind the back break it.
- **Hands on the prop.** The right hand's wrist is at the grip, the left on something that exists
  on the model. Compute the prop's place from the hand and the model's own grip point; do not move
  the prop until it looks right.
- **Readable from the side.** Would a silhouette tell you what the person is doing? If the arms
  hide in front of the chest in the side view, lower or raise the hands.
- **The motion.** Compare the two key frames. Under 2 cm of travel is invisible in the game; over
  10 cm in a working loop looks like exercise. Three to five centimetres at the hips, a loop of 1.2
  to 1.6 seconds, reads as effort.

Then film it in the game. The stick figure has no shoulders, clothes or backpack, and only the game
shows whether the hands close on the handle.

### Timing

- A working loop: 1.2 to 1.6 s (36 to 48 frames at 30). Shorter looks frantic.
- In and out: 0.4 to 0.5 s (12 to 15 frames). The graph blends 0.2 s on top.
- The work's own length belongs to the feature (`LF_WorkAnim.DRILL_SECONDS`), not to the clip.

### Where the asset is while the clip plays

A custom loiter holsters what the character holds, so the asset is not in the hand. Hold it in the
world instead: `LF_AssetAnimComponent.Pose` at the hand's position, which the clip fixes. For the
drill, `LF_DrillScene.HOLD_OFFSET` is the right hand's target minus the model's grip point. When you
move a hand in the clip, move that constant with it.

## Step 3: the effect

`sparks` is one template: hot streaks with gravity and a flicker of light, looping until stopped.
Start from the drill's numbers and change one at a time.

| Field | Drill | Raise it when |
|---|---|---|
| `rate` (streaks a second) | 60 | the effect looks thin from five metres |
| `speed` (m/s) | 1.8 | sparks should fly further; 2.5 already looked like a firework |
| `life` (s) | 0.3 | they should fall to the ground instead of dying in the air |
| `size` (m, streak width) | 0.012 | they vanish at distance; 0.025 looked like tracer |
| `spread` (cone half-angle) | 75 | a tighter jet: lower it |

The cone opens upwards from the socket. Judge it in a film frame, at the distance a second player
stands. Anything else (smoke, dust, a welding arc) is a `.ptc` of its own: copy a vanilla one's text
(`Particles/`), or name a vanilla effect with `particle`.

## Step 4: the sound

A loop per source, each its own generation ([sounds.md](sounds.md)): the drill has a motor loop and
a grinding loop, both `range` `voice70`. List each event under `anim.sounds` with the project the
sound stage wired, then `asset anim <slug> --fix`.

## Proof

- `tools\cli validate`: `asset-anim-selftest` (the rules, the solver, and the committed files
  against the specs).
- An `LF_Test` that spawns the prefab, finds the bones and sockets on the model, switches the
  component on and off and poses it (`LF_Test_AssetAnim`).
- A session with two clients, `--profile full --sound`: the harness verb `animseen` reports, for
  the nearest animated asset, what this client shows (running, held, effects and sounds live, the
  moving bone's axis, which differs between two calls while it spins); `animhold` / `animoff` keep
  one running so a client can join late. `session.mjs drill-scene-proof --audio` is the example and
  films the scene from the second client.

## What the stage does not do

- It does not decide that a pose looks human. That is the preview and the film.
- It does not animate a weapon's vanilla skeleton (anim bones on a `weapon` are refused).
- It does not write feature code: who may use the asset, for how long, and what ends it.
- It does not mirror a clip left to right or cut a vanilla clip; a vanilla `.anm` cannot be edited.
