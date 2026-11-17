# Map emulator

### `tools\cli map render` — `tools/commands/map.mjs`, `tools/map/`
- ✅ Draws the Everon map a given player would see to a PNG in about 0.3 s once the base layer is cached, with no
  game boot and no Workbench: `map render [--place saint-philippe|square|island|<catalog place>|--bbox x0,z0,x1,z1]
  [--zoom N] [--as new|known|police|org:<id>] [--night] [--size WxH] [--html] [--out f] [--fallback]`. View width is
  12800 m / 2^zoom, the same `--place`, `--bbox` and `--zoom` the in-game `map-shots` proof (M0b) takes.
- ✅ Base layer: the terrain colour (`worlds/Eden/Eden/.Data/Eden_<n>_supertexture.edds`, BC7, 2500 tiles) and heights
  (`Eden_<n>.ttile`) read once from the game paks by `map extract` (about 4 s, run automatically on first render) into
  `~/.life-framework/map-cache/everon-<build>/`, never committed. Contours (20/50/100 m by scale), coast and hillshade
  come from the heights. Heights: `raw * 1000/65535 - 100` m, sea level at raw 6500 (derived from the sea colour, not
  documented by the game).
- ✅ Fallback layer (no cache, or `--fallback`): a 100 m / 500 m / 1 km grid, a schematic road net (a minimum spanning
  tree between the named places, not real roads) and the 6,514 catalog buildings. No contours: the heights live only in
  the cache.
- ✅ Overlays read the game's confs: `LF_MapPoiKind` rows in `Configs/Map/LF_MapPois*.conf` and `LF_MapAreaRow` rows in
  `Configs/Map/LF_MapAreas*.conf` of the core and the pack (same Id: the pack row replaces; `m_sOp "remove"` drops).
  `tools/map/render/fixtures/map-overlay.fixture.conf` is the stand-in for `--fixture` only. Places and buildings come from `docs/everon/catalog-buildings.json`.
- ✅ `--as` personas use a built-in reading of D-M3 to D-M6 (`personaVisibility`); a `tools/map/rules/visibility.mjs`
  exporting `visible(poi, persona)` replaces it.
- ✅ `--html` writes the PNG with hover tooltips on POIs and areas and a live world coordinate.
- ✅ Rule loader: `tools/map/rules/index.mjs` imports whichever of `visibility`, `cluster`, `areas`, `discovery` exist
  and defines the case table (`addons/LifeFramework/Configs/Map/Rules/<rule>.cases.json`:
  `{ rule, version, cases: [{ name, input, expect }] }`) that a node selftest and an Enforce LOGIC test both run.
- ⚠️ No Enforce twin or case table ships here: each belongs to the card that writes the rule (M2, M4, M5, M13).
- Proof: `node tools/map/map-render.selftest.mjs`.
