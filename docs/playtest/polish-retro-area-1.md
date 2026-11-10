# Polish pass retro: area 1 (spawn square)

Written by the area 1 builder so areas 2 to 17 go faster. About 150 tool calls in total.

## 1. Where the time went

| Part | Share | Notes |
|---|---|---|
| Shot setup (golden-hour clock, camera framing, HUD, converting to PNG) | about 40% | The biggest cost by far. Five session restarts, four of them for the clock or a layer change. |
| Session and test queueing | about 25% | `session.mjs up` and `test --only` waited on other agents. The test waited 20 minutes behind my own idle session. |
| Finding entity positions | about 10% | A layer scan with `awk` by radius did it. Fine, but I wrote it by hand. |
| Ground snap and placement | about 8% | Terrain is flat at about y 0.1, so there was nothing to snap. The real defects (payphone beside a tree, a notice board with no legs) only showed in close shots. |
| Choosing prefabs | about 7% | `asset_search` returns paths with no GUIDs ("0 GUIDs"). I reused GUIDs found in existing layers. |
| Ship | about 10% | Gate and PR were smooth. |

## 2. Biggest wastes, and what would remove them

1. **Golden hour needs a server clock, and the harness cannot set it.** `shot world` calls `SetTimeOfTheDay` on the client, which the server overrides, so my first two sets of shots were noon-ish and then pitch black. I found out that the world sits at latitude -4, longitude 71, so the game clock runs about 4.7 h behind local time. Fix: a harness verb `time <hour>` (or `shot world ... --hour 17.5`) that sends an authority-side RPC, like `LF_AskHarnessPose` does for pose. It takes an hour in local terms and returns the clock it set. Until then, the layer override is in `agent-area-polish.md`.
2. **Every layer change needs a full session restart (3 to 5 minutes, often queued).** Two restarts were only to take before shots and then after shots. Fix: `session.mjs shots --before-after <script>` that snapshots shots, applies the working-tree layers, restarts once, and shoots again. Cheaper: take all before shots first, in one session, before editing anything.
3. **Remote vs local session confusion.** `up --machine any` landed on the test box. `harness.mjs --client 1` then said `no_live_session`, and `run <script>` could only run files that existed at `up` time. Fix: `session.mjs shot <name> x y z ox oy oz` that always works against the recorded session (local or remote), pulls the BMP, and returns a PNG path.
4. **Screenshots are BMP, and a plain PNG is 1.8 MB.** The 500 KB rule meant writing a 128-colour quantiser (`tmp/quant-spawn-polish.mjs`, not committed). Fix: `harness.mjs shot ... --png --max-kb 500` doing the quantise in `agent-box/bmp.mjs`.
5. **Blind camera framing.** The pose clamps pitch, so overhead views failed (`a_top` looked at the sky). Three of eight shots had a roof or tree canopy filling the frame. Fix: `harness.mjs frame <EntityName> [--wide|--detail]` that picks an offset with a clear line of sight (a raycast from the camera to the target) and prints it, so the same cameras can be reused for the before and after shots.

## 3. `docs/agent-area-polish.md`

- Missing: how to set the clock (added in PR #483), the hour offset, and the 15 s wait for the quest toast after signing form R-89.
- Missing: shots hide the HUD only because I changed the harness; the doc said "no HUD" but nothing did it.
- Unhelpful: "ground snap and surface height" suggests checking each entity with `wb_read_props`. Terrain is flat here and the real problems are visual. Say "close shot first, from the side the player approaches".
- Missing: a list of known-good vanilla prop GUIDs. `asset_search` has none; a table of 20 props (desk, chair, cart, crates, barrels, cones) would save a lot.
- Wrong order: the doc says before shots, then fix. In practice take all before and detail shots in one session, then edit once and restart once.
- Missing: say that sessions must be down while tests run (the coordinator had to remind me).

## 4. Bugs worth an issue

- **Harness `shot world` sets time on the client only.** The log prints `set time of day to 18.3` and nothing changes. It should say it was ignored, or use an RPC.
- **`session.mjs up --help` starts a session.** I booted a two-client session by asking for help.
- **`NoticeBoardCork_01` placed on the ground renders as a bare plank.** It is a wall-mount prop; not a bug, but worth a note in the prop table.
- **A queued `session.mjs up` kept running after its tool call timed out** and later took the slot back from me, so a stale background `up` can start a session you thought was gone.

## 5. Camera work the reviewer failed (follow-up)

The reviewer failed area 1 on framing alone: a trunk filling the wide shot, a payphone that read as a notice column, a detail shot of the sign instead of the clerk. All three are camera choices, and most of the 40% shot-setup time above was spent on them. `tools\cli shots area <n>` should do this automatically: take a list of subject entities (the main interaction, the prop that names the area), try a ring of candidate cameras around each at eye level and at about 8 m up, reject any whose line to the subject crosses a tree, wall or roof (raycast), pick the one that keeps every subject in frame, and take before and after from the same cameras. It should also set the golden-hour clock, hide the HUD, wait out the quest toast and quantise to under 500 KB. The follow-up PR for these three shots was blocked for over an hour by the single game slot (another agent's session held it, the test box was down), which a one-shot `shots area` run would shorten.
