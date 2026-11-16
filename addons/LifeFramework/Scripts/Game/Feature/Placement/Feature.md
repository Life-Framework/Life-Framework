# Placement

## Intent

A player puts something into the world (a spike strip, a roadblock, a stall, a chair) and sees
where it will go before it goes there. The ghost's colour is the server's answer, so nothing lands
somewhere the player did not expect. Design and the Conflict reuse verdict:
`docs/roadmap/building-foundation.md` (epic #927).

## Interaction pattern

In-world first: the player holds or stands at the thing they place, and the ghost lies in the world
in front of them. No sheet. Today the ghost appears while the folded spike strip's Deploy action is
selected; aimed placement with rotate, place and cancel is #1028.

## Placement rules and ghost (#928)

- **Player sees:** a blue ghost of the strip where it will lie, red when the spot is refused
  (no ground, a kerb or slope, something in the way, with the reason). Shipped for every on-duty officer on any world (#1939); the server refuses a confirm for a red spot. LF_Test_SpikeStripPreview.
- **Owner and side:** `LF_PlacementRules.Judge` is pure and runs on both sides on the same probe;
  the feature's server path (`LF_SpikeStrips.Deploy`) refuses anything but `VALID`.
  `LF_PlacementGhost` is client-local and never replicated.
- **Data:** no config yet; the step (0.25 m) and the clearance box are the feature's fixed geometry.
  String `#LF-Placement_Blocked`. Nothing persists (#1032 adds permanent placements).
- **Proof:** `placement/validity` (LOGIC), the clearance check in `police/spike-strip` (WORLD),
  `session.mjs placement-preview-proof` (pictures in `website/public/docs-images/928/`).

## Placement service (#1026)

- **Player sees:** the strip lands exactly where the ghost was, on every client. A refusal is a
  toast under "Placing": not allowed, no flat ground, something in the way, or "You moved" when the
  officer turned or stepped away between the ghost and the server seeing Deploy.
- **Owner and side:** `LF_PlaceableComponent` on the carried item is the data. The officer's machine
  sends the ghost's spot in the action's own data (`LF_SpikeStripDeployAction.OnSaveActionData`); the
  server (`LF_PlacementService.Confirm`) checks who, where the item is, `LF_RateLimit` SPAWN, the
  malformed-proposal check, the reach from where the player stands now (plus 1.5 m for walking;
  turning costs nothing), 15 degrees off straight across a road (only on a world with an AI road
  network: DebugWorld has none; `LF_Test_PlacementRoad` runs it on real Everon roads in `Worlds/CoreOnEveron`, #1129) and the probes at the proposed spot, then spawns
  the replicated prefab there and uses the item up. No new RPC; the ghost stays local.
- **Data:** the placeable's prefab attributes (footprint box, reach, snap, who, lifetime). Strings
  `#LF-Placement_Title`, `_NotAllowed`, `_NoGround`, `_Moved`, `_Blocked`. Nothing persists (#1032).
- **Proof:** `placement/confirm` (WORLD), the tolerance in `placement/validity` (LOGIC),
  `session.mjs placement-confirm-proof` (two clients; pictures in `website/public/docs-images/1026/`).

## Aimed placement (#1028)

- **Player sees:** the item's "Place where I aim" action starts a ghost that follows the camera aim within 4 m
  of their feet, lies on the ground, turns across a road when one is near, and is blue (valid) or red with the
  reason in a hint. Keys: Enter or right trigger place, Backspace or B cancel, `[` and `]` or left trigger and
  right shoulder turn 15 degrees (hold Shift or left stick for 1 degree). No new screen: the hint is the vanilla
  hint line. Dying, going down, getting in a vehicle or losing the item ends it (the vanilla placement ends the
  same way; the strip is not a weapon-slot item, so there is no weapon change to follow).
- **Owner and side:** the mode is client-only (`LF_PlacementAim`, keys listened to in
  `LF_PlacementPlayerController`). Place sends the ghost's origin and forward in one RPC
  (`RpcAsk_LF_PlaceAimed`) on the player's own controller; the server runs `LF_PlacementService.ConfirmAimed`,
  which is `Confirm` (who, where the item is, `LF_RateLimit` SPAWN, malformed proposal, reach, probes) plus the
  strip's deployer. The ghost is never replicated. The reach is the placeable's `m_fAimReach`.
- **Data:** `m_fAimReach` on `LF_PlaceableComponent` (the strip: 4 m). Input actions `LF_PlacementPlace`,
  `_Cancel`, `_TurnLeft`, `_TurnRight`, `_Fine` (keyboard plus `gamepad0:`). Strings `#LF-Placement_Aim`,
  `_AimHint`, `_AimTitle`, `_Key*`. Nothing persists.
- **Proof:** `placement/aim` (LOGIC: aim ray to spot, reach clamp, rotation steps, snap to a road axis, wall
  normal), `placement-aim-proof` (two clients on a dedicated server; pictures in
  `website/public/docs-images/1028/`).

## Composition placement (#1940)

- **Player sees:** a set of props (the police roadblock's cones and barriers, #647) is one placement: one
  ghost made of every piece, blue when every piece sits on valid ground and red when any piece does not, and one
  confirm that puts the whole set down with each piece on the ground. Picking up any piece takes the whole set
  away. No consumer wires it to an item yet, so nothing shows in the game until the roadblock item (#647).
- **Owner and side:** `LF_PlacementCompositions.Plan` runs on both sides (each piece probed like a single placement:
  ground at its ends and middle, the 0.25 m step, the box over it, what it lies on; the set's verdict is the first
  piece that is not VALID). `Confirm` is the server's: who (`m_eWho`), `LF_RateLimit` SPAWN, the malformed-proposal
  check, the reach from where the player stands now, then Plan again, then spawn every piece or none. `PickUp`
  (server, within 4 m of a piece) removes the set a piece belongs to. `LF_PlacementCompositionGhost` is
  client-local and never replicated. The service (`LF_PlacementService`) is untouched; the ground sampler is
  a small copy in the composition file because the service's is protected.
- **Data:** `LF_PlacementComposition` is a piece list (prefab, offset across and ahead, turn, footprint box), built in
  script or read from a composition prefab's child props (`FromPrefab`). Refusals reuse the toasts
  `#LF-Placement_NotAllowed`, `_Moved`, `_Blocked`, `_NoGround`. Nothing persists: a roadblock is temporary.
- **Proof:** `placement/composition` (WORLD: a three-cone set places as one with each piece on the ground, a set on
  a set, a malformed proposal and a far pick-up are refused, a pick-up removes every piece, a cone half on a table is
  `TOO_STEEP`).
