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
  (no ground, a kerb or slope, something in the way). Only in dev-tools worlds until #928 ships.
- **Owner and side:** `LF_PlacementRules.Judge` is pure and runs on both sides on the same probe;
  the feature's server path (`LF_SpikeStrips.Deploy`) refuses anything but `VALID`.
  `LF_PlacementGhost` is client-local and never replicated.
- **Data:** no config yet; the step (0.25 m) and the clearance box are the feature's fixed geometry.
  String `#LF-Placement_Blocked`. Nothing persists (#1032 adds permanent placements).
- **Proof:** `placement/validity` (LOGIC), the clearance check in `police/spike-strip` (WORLD),
  `session.mjs placement-preview-proof` (pictures in `website/public/docs-images/928/`).
