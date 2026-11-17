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
- ✅ Overlays read the game's confs: `LF_MapPoi` and `LF_MapArea` blocks in `Configs/Map/LF_MapPois.conf` and
  `LF_MapAreas.conf` of the core and the pack. Until M2 and M5 land, `tools/map/render/fixtures/map-overlay.fixture.conf`
  stands in. Places and buildings come from `docs/everon/catalog-buildings.json`.
- ✅ `--as` personas use a built-in reading of D-M3 to D-M6 (`personaVisibility`); a `tools/map/rules/visibility.mjs`
  exporting `visible(poi, persona)` replaces it.
- ✅ `--html` writes the PNG with hover tooltips on POIs and areas and a live world coordinate.
- ✅ Rule loader: `tools/map/rules/index.mjs` imports whichever of `visibility`, `cluster`, `areas`, `discovery` exist
  and defines the case table (`addons/LifeFramework/Configs/Map/Rules/<rule>.cases.json`:
  `{ rule, version, cases: [{ name, input, expect }] }`) that a node selftest and an Enforce LOGIC test both run.
- ⚠️ No Enforce twin or case table ships here: each belongs to the card that writes the rule (M2, M4, M5, M13).
- Proof: `node tools/map/map-render.selftest.mjs`.

### Map icon set — `tools/map/icons/`, `tools/map/lib/icons.mjs` (card M3, #2231)
- ✅ The icon manifest (`tools/map/lib/icons.mjs`) names every quad of the two `LF_MapIcons` imagesets: 8 category
  fallbacks `cat_<category>`, 20 pack pictograms `poi_<Kind>` (bank coin, police star, clinic cross in a square, fuel
  pump, handset, crossed hammer and pick, fish, flag, ...), 5 pencil marks, 4 shared pins (police blue, org, taxi
  yellow, waypoint) and 4 stamps (town cluster, no-police, restricted, you). A POI row's icon is its `m_sIcon`, else
  `poi_<Kind>` in the pack imageset, else `cat_<category>`.
- ✅ `node tools/map/icons/build-icons.mjs` draws them (ink palette, 1-2 px stamped edge, red plate a pixel off) and
  writes the `.tif` sources, `.edds.meta`, `.imageset` and `.imageset.meta` for the core placeholders
  (`LifeFramework/UI/Imagesets/LF_MapIcons`, neutral dark grey), the 1989 set
  (`LF_Everon1989/UI/Imagesets/LF_MapIcons_Everon1989`) and the hatch tiles (core white-ink `UI/Textures/Map/LF_MapHatch_*`,
  pack inked `LF_MapIcons_Everon1989_Hatch{Lawless,Gated}`). It is deterministic, and writes a contact sheet to
  `tmp/map/icons-<side>.png`.
- ✅ `validate-ui-kit` checks the set once an imageset is committed: every quad of the manifest, every quad a POI conf
  row names (core `LF_MapPois.conf` and a pack `LF_MapPois*.conf` overlay), the quads inside the texture, the
  texture and hatch files with matching GUIDs. Proof: `node tools/validation/map-icons-selftest.mjs`.
- ✅ `tools\cli ui-image --batch <list.txt>` rebuilds a list of `.tif` files into `.edds` in one Workbench boot
  (`tools/map/icons/icons-batch.txt` is the icon set's list).
- ⚠️ The textures are not built yet: two Workbench boots on lf-testbox failed at the render device ("Couldn't build
  device list for any adapter"). The imagesets and `.edds` are committed together by the follow-up; until then the
  validator has nothing to check.
