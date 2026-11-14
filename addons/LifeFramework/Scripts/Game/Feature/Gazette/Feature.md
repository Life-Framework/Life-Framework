# Gazette

## Intent

TODO(scaffold): what a player gets from this area, in two sentences.

## Interaction pattern

TODO(scaffold): in-world first (docs/decisions.md). What object does the player walk up to, what does the F key offer, and what, if anything, opens a sheet?

## Gazette (#792)

- **Player sees:** TODO(scaffold)
- **Owner and side:** `LF_Gazette` decides on the server; `LF_GazetteSheet` is client only.
- **Data:** TODO(scaffold): which config section and which string keys drive it; what is persisted (nothing, or the payload name).
- **Proof:** `gazette/gazette` (LOGIC), `gazette/gazette-world` (WORLD), `node agent-box/session.mjs gazette-proof`.
