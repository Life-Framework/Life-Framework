// tools/validation/mapper-selftest.mjs
// inputs: tools/mapper/**, tools/commands/mapper.mjs, tools/place/place.mjs, tools/test/fixtures/mapper/**, addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_blacklake_shore.layer, tools/grid/cells/mapper-junkyard-small.json, addons/LifeFramework/Worlds/GridWorld/GridWorld_Layers/Grid_mapper_junkyard_small.layer
//
// Proves `tools\cli mapper import` (docs/architecture/reforger-mapper-import.md) with the game, the grid tool and the
// placement tool replaced by fakes, so it needs no Workbench and no game install:
//   - the fixture (tools/test/fixtures/mapper/junkyard-small.json, Mapper's mapgen.json shape) reads as 5 entities and 1
//     shape; the shape is reported, not imported; a bad version, a shapes-only project, bad rows and `ycm` are reported;
//   - a prefab the resolver does not know is dropped and reported, never handed to the placement tool, and the layout's
//     centre is worked out from what is kept;
//   - cell mode claims the cell through `grid place --cell`, moves the layout's centre onto the cell's centre, keeps the yaw,
//     and sends every kept prop in ONE `place ground-many` run (a JSON plan with bare x,z), never a boot per prop, never a
//     quoted vector or a `coords` text of its own; a refused batch falls back to `place ground` per prop to name the refusal;
//   - the committed sample layer has no quoted vector (the writer is the placement tool's `appendEntity`);
//   - slot mode refuses a branch that is not a `ws/town-*` pass, a missing --at, a Grid_ layer and an unknown layer, and
//     with all of them right plans the props without writing in --dry;
//   - an island slot (`Place_<place>_<slot>.layer`): a branch that does not own the slot, a layer with no claim, props outside
//     the slot's ground, ground that is not level, a layer that already holds an import and a flagged audit are refused (the
//     layer restored byte for byte); a good run writes the props with the Mapper yaw as `angleY`, from ONE terrain probe,
//     through the strict island audit.
// red-proof: with the `ground-many` call in placeAll turned back into one `ground` per prop this printed "FAIL the 4 kept props should go in ONE ground-many run, got 0" (2026-11-19).
// red-proof: with the branch check of intoIslandSlot removed this printed "FAIL an island slot should refuse a branch that does not own it" (2026-11-19).
// red-proof: with the yaw left out of the written entity this printed "FAIL the bench keeps Mapper's yaw 270 as angleY -90" (2026-11-19).
// red-proof: with the `if (v.drop)` branch of importProject removed this printed "FAIL an unknown prefab should be dropped, not placed" (2026-11-19).
// red-proof: with isTownPass always true this printed "FAIL slot mode should refuse a branch that is not a town pass" (2026-11-19).
// red-proof: with the modulo left out of normYaw this printed "FAIL yaw -190 should read as 170, got -190" (2026-11-19).

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { centreOf, fit, parseProject, splitPrefab, translate } from "../mapper/convert.mjs";
import { main as mapperMain } from "../commands/mapper.mjs";
import { pinKey } from "../town/terrain.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIXTURE = join(ROOT, "tools", "test", "fixtures", "mapper", "junkyard-small.json");
const SAMPLE_LAYER = join(ROOT, "addons", "LifeFramework", "Worlds", "GridWorld", "GridWorld_Layers", "Grid_mapper_junkyard_small.layer");
let failed = 0;
const fail = (msg) => {
  console.log(`FAIL  ${msg}`);
  failed++;
};
const check = (cond, msg) => {
  if (!cond) fail(msg);
};

// Runs main with console.log captured.
async function run(argv, ctx) {
  const lines = [];
  const orig = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  try {
    code = await mapperMain(argv, ctx);
  } finally {
    console.log = orig;
  }
  return { code, text: lines.join("\n") };
}

const fixtureText = readFileSync(FIXTURE, "utf8");

// ---- parse
{
  const p = parseProject(fixtureText);
  check(!p.error, `the fixture should parse (${p.error})`);
  check(p.entities.length === 5, `the fixture holds 5 entities, got ${p.entities.length}`);
  check(p.shapes === 1 && p.notes.some((n) => /1 shape\(s\) not imported/.test(n)), "the fixture's wall shape should be counted and reported, not imported");
  check(p.entities[0].prefab.guid === "002B8C09F0E7E371" && p.entities[0].prefab.path.endsWith("S1203_wreck_static.et"), "an entity's prefab splits into GUID and path");
  check(splitPrefab("Prefabs/Props/X.et")?.guid === null, "a bare path has no GUID");
  check(splitPrefab("{12}Prefabs/X.et") === null, "a short GUID is not a prefab ref");
  check(splitPrefab("Prefabs/X.conf") === null, "only .et is a prefab");
  const y = parseProject(JSON.stringify({ version: 0, entities: [{ prefab: "Prefabs/A.et", x: 1, z: 2, yaw: 450 }, { prefab: "Prefabs/B.et", x: 1, z: 2, yaw: -190 }] }));
  check(y.entities[0].yaw === 90, `yaw 450 should read as 90, got ${y.entities[0].yaw}`);
  check(y.entities[1].yaw === 170, `yaw -190 should read as 170, got ${y.entities[1].yaw}`);
  check(parseProject("{nope").error?.startsWith("not JSON"), "text that is not JSON is an error");
  check(/version/.test(parseProject('{"version":1,"entities":[]}').error ?? ""), "another project version is an error");
  check(/shapes only/.test(parseProject('{"version":0,"shapes":[{"id":"a"}],"entities":[]}').error ?? ""), "a project of shapes only says so");
  const bad = parseProject(JSON.stringify({ version: 0, entities: [{ id: "a", prefab: "nope", x: 1, z: 1 }, { id: "b", prefab: "Prefabs/B.et", x: "1", z: 1 }, { id: "c", prefab: "Prefabs/C.et", x: 1, z: 1, ycm: 250 }] }));
  check(bad.entities.length === 1 && bad.entities[0].id === "c", "rows with a bad prefab or non-number coordinates are dropped, the good one kept");
  check(bad.notes.some((n) => /entities\[0\] a: prefab/.test(n)) && bad.notes.some((n) => /ycm/.test(n)), "dropped rows and ignored heights are reported");
}

// ---- fit and translate
{
  const p = parseProject(fixtureText);
  const f = fit(p.entities);
  check(f.w === 6.8 && f.d === 5, `layout of the fixture is 6.8 x 5 m, got ${f.w} x ${f.d}`);
  check(f.span[0] === 1 && f.span[1] === 1, `the fixture needs one cell, got ${f.span}`);
  const t = translate(p.entities, [100, 50]);
  const g = fit(t);
  check(Math.abs(g.centre[0] - 100) < 0.01 && Math.abs(g.centre[1] - 50) < 0.01, "the translated layout's centre lands on the target");
  check(t.every((e, i) => e.yaw === p.entities[i].yaw), "translate keeps every yaw");
  const wide = fit([{ x: 0, z: 0 }, { x: 30, z: 5 }]);
  check(wide.span[0] === 3 && wide.span[1] === 1, `a 30 x 5 m layout needs 3x1 cells, got ${wide.span}`);
  const rect = { x0: 58, z0: 58, x1: 82, z1: 82 };
  check(centreOf(rect)[0] === 70 && centreOf(rect)[1] === 70, "centreOf is the rectangle's middle");
}

// ---- the fakes
const KNOWN = new Set(["S1203_wreck_static.et", "S105_wreck_static.et", "Pallet_01_Pile.et", "Cardboard_Pile_03.et"]);
const inspect = async (prefab) => {
  const name = prefab.path.split("/").pop();
  if (KNOWN.has(name)) return { ref: `{${prefab.guid ?? "0000000000000001"}}${prefab.path}` };
  if (/Decal/.test(name)) return { drop: "no mesh the placement tool can measure (no MeshObject in its chain)" };
  return { drop: "unknown prefab (no such prefab)" };
};

// ---- cell mode through the fakes, in a scratch root with its own claim folder
{
  const root = scratchDir("lf-mapper-selftest-");
  const cellsDir = join(root, "tools", "grid", "cells");
  const gridCalls = [];
  const placeCalls = [];
  const auditCalls = [];
  const gridMain = async (argv) => {
    gridCalls.push(argv);
    if (argv[0] === "place") {
      mkdirSync(cellsDir, { recursive: true });
      const slug = argv[1];
      writeFileSync(join(cellsDir, `${slug}.json`), `${JSON.stringify({ slug, cell: argv[argv.indexOf("--cell") + 1] ?? "c3r3", layer: `Grid_${slug.replace(/-/g, "_")}.layer`, rows: 0, created: "2026-11-19" })}\n`);
    }
    return 0;
  };
  const plansSeen = [];
  const placeMain = async (argv) => {
    placeCalls.push(argv);
    if (argv[0] === "ground-many") plansSeen.push(JSON.parse(readFileSync(argv[1], "utf8")));
    return 0;
  };
  const auditMain = async (argv) => {
    auditCalls.push(argv);
    return 0;
  };
  const noPlace = { layerPath: () => "", prefabInfo: () => ({}) };
  const proj = join(root, "mini.json");
  writeFileSync(proj, fixtureText);
  const r = await run(["import", proj, "--cell", "c3r3", "--no-fetch"], { root, inspect, gridMain, placeMain, auditMain, P: noPlace });
  check(r.code === 0, `cell mode should succeed through the fakes, got ${r.code}: ${r.text}`);
  check(gridCalls[0]?.slice(0, 2).join(" ") === "place mapper-mini" && gridCalls[0].includes("--cell") && gridCalls[0].includes("--no-verify") && gridCalls[0].includes("1x1"), `grid place should claim the cell (got ${JSON.stringify(gridCalls[0])})`);
  const many = placeCalls.filter((a) => a[0] === "ground-many");
  check(many.length === 1 && placeCalls.filter((a) => a[0] === "ground").length === 0, `the 4 kept props should go in ONE ground-many run, got ${many.length}`);
  const rows = plansSeen[0] ?? [];
  check(rows.length === 4, `the batch plan should hold the 4 kept props, got ${rows.length}`);
  check(!JSON.stringify(rows).includes("Decal_OilStain"), "an unmeshed prefab should be dropped, not placed");
  check(rows.every((r) => Array.isArray(r.at) && r.at.length === 2 && r.at.every(Number.isFinite)), "each batch row gets a bare x,z");
  check(placeCalls.every((a) => a.every((x) => !/["']/.test(x) && x !== "coords")), "no argument carries a quote or a coords text: vectors are the placement tool's to write");
  check(rows[0]?.at.join(",") === "64,61.5", `the layout centre should land on c3r3 centre (64,64); the first wreck sits 2.5 m south of it at 64,61.5, got ${rows[0]?.at}`);
  check(rows[0]?.yaw === 90, "the wreck keeps its yaw of 90");
  check(Boolean(many[0]?.includes("--layer") && many[0].includes("--no-verify") && many[0].includes("--y-hint")), "the batch is written unaudited at y-hint 0 (settle and audit follow once for the cell)");
  check(!existsSync(many[0]?.[1] ?? ""), "the plan file is removed after the run");
  check(placeCalls.some((a) => a[0] === "settle") && auditCalls.some((a) => a.includes("--strict") && a.includes("--grid")), "the cell is settled and audited strict at the grid bar");
  check(/dropped JY1_oil/.test(r.text) && /1 dropped/.test(r.text), "the dropped decal is reported");
  const again = await run(["import", proj, "--cell", "c3r3"], { root, inspect, gridMain, placeMain, auditMain, P: noPlace });
  check(again.code === 1 && /already has a cell/.test(again.text), "a second import of the same slug is refused until the cell is freed");
  const dry = await run(["import", FIXTURE, "--cell", "c5r5", "--dry", "--slug", "dry-run"], { root, inspect, gridMain: async () => fail("a dry run must not claim a cell"), placeMain: async () => fail("a dry run must not place"), P: noPlace });
  check(dry.code === 0 && /nothing written/.test(dry.text) && !existsSync(join(cellsDir, "dry-run.json")), "--dry writes nothing");
}

// ---- unknown prefabs
{
  const root = scratchDir("lf-mapper-selftest-");
  const text = JSON.stringify({ version: 0, entities: [{ id: "a", prefab: "{AAAAAAAAAAAAAAAA}Prefabs/Props/Nope/Nope.et", x: 1, z: 1 }] });
  const proj = join(root, "nothing.json");
  writeFileSync(proj, text);
  const r = await run(["import", proj, "--dry"], { root, inspect, P: {} });
  check(r.code === 1 && /nothing left to place/.test(r.text) && /unknown prefab/.test(r.text), `a project whose every prefab is unknown places nothing and says so: ${r.text}`);
  const text2 = JSON.stringify({ version: 0, entities: [{ id: "a", prefab: "{AAAAAAAAAAAAAAAA}Prefabs/Props/Nope/Nope.et", x: 1, z: 1 }, { id: "b", prefab: "{0000000000000001}Prefabs/Props/Industrial/Pallets/Pallet_01_Pile.et", x: 4, z: 1 }] });
  writeFileSync(proj, text2);
  const placed = [];
  const r2 = await run(["import", proj, "--cell", "c4r4", "--slug", "unk", "--no-verify"], {
    root, inspect, P: {},
    gridMain: async (argv) => {
      if (argv[0] === "place") {
        mkdirSync(join(root, "tools", "grid", "cells"), { recursive: true });
        writeFileSync(join(root, "tools", "grid", "cells", "unk.json"), `${JSON.stringify({ slug: "unk", cell: "c4r4", layer: "Grid_unk.layer", rows: 0, created: "2026-11-19" })}\n`);
      }
      return 0;
    },
    placeMain: async (argv) => {
      placed.push(...JSON.parse(readFileSync(argv[1], "utf8")));
      return 0;
    },
  });
  check(r2.code === 0 && placed.length === 1 && placed[0].prefab === "{0000000000000001}Prefabs/Props/Industrial/Pallets/Pallet_01_Pile.et", `an unknown prefab should be dropped, not placed (placed ${placed.length}): ${r2.text}`);
  const refused = await run(["import", proj, "--cell", "c4r4", "--slug", "unk2", "--no-verify"], { root, inspect, P: {}, gridMain: async (argv) => {
    if (argv[0] === "place") writeFileSync(join(root, "tools", "grid", "cells", "unk2.json"), `${JSON.stringify({ slug: "unk2", cell: "c4r4", layer: "Grid_unk2.layer", rows: 0, created: "2026-11-19" })}\n`);
    return 0;
  }, placeMain: async () => 1 });
  check(refused.code === 1 && /1 refused by the placement checks/.test(refused.text) && /claim unk2/.test(refused.text), `when the placement tool refuses every prop the claim is freed and the exit is 1: ${refused.text}`);
}

// ---- slot mode guards
{
  const root = scratchDir("lf-mapper-selftest-");
  const base = { root, inspect, P: { layerPath: (_r, n) => (n === "Street_Police.layer" ? FIXTURE : join(root, "none", n)), prefabInfo: () => ({}) } };
  const args = ["import", FIXTURE];
  let r = await run([...args, "--slot", "Street_Police.layer", "--at", "10,20", "--dry"], { ...base, branch: "ws/slice-3839" });
  check(r.code === 1 && /town-placement pass/.test(r.text), "slot mode should refuse a branch that is not a town pass");
  r = await run([...args, "--slot", "Street_Police.layer", "--dry"], { ...base, branch: "ws/town-draft-x" });
  check(r.code === 1 && /--at x,z/.test(r.text), "slot mode needs --at");
  r = await run([...args, "--slot", "Grid_foo.layer", "--at", "1,1"], { ...base, branch: "ws/town-draft-x" });
  check(r.code === 1 && /use --cell/.test(r.text), "a Grid_ layer is not a slot");
  r = await run([...args, "--slot", "Nope.layer", "--at", "1,1"], { ...base, branch: "ws/town-draft-x" });
  check(r.code === 1 && /no layer Nope.layer/.test(r.text), "an unknown layer is refused");
  // An island slot layer holds a hyphen in its place or slot word (Place_coalmans-brow_mine.layer); the name check must let it through (#4713).
  r = await run([...args, "--slot", "Place_coalmans-brow_mine.layer", "--dry"], { ...base, branch: "ws/town-draft-coalmans-brow-mine", islandLayerExists: () => false });
  check(r.code === 1 && /no layer Place_coalmans-brow_mine.layer/.test(r.text) && !/takes a layer file name/.test(r.text), `a hyphenated island slot layer name should pass the name check: ${r.text}`);
  r = await run([...args, "--slot", "Bad name.layer", "--at", "1,1"], { ...base, branch: "ws/town-draft-x" });
  check(r.code === 1 && /takes a layer file name/.test(r.text), "a layer name with a space is still refused");
  r = await run([...args, "--slot", "Street_Police.layer", "--at", "10,20", "--dry"], { ...base, branch: "ws/town-draft-x" });
  check(r.code === 0 && /centre 10,20/.test(r.text) && /Mapper_Street_Police_M1/.test(r.text) && /nothing written/.test(r.text), `slot mode on a town branch plans the props: ${r.text}`);
  r = await run([...args, "--slot", "Street_Police.layer", "--cell", "c1r1", "--at", "1,1"], { ...base, branch: "ws/town-draft-x" });
  check(r.code === 1 && /two targets/.test(r.text), "--cell and --slot together are refused");
}

// ---- a refused batch falls back to one prop at a time, keeping the props the checks accept
{
  const root = scratchDir("lf-mapper-selftest-");
  const proj = join(root, "fall.json");
  writeFileSync(proj, fixtureText);
  const calls = [];
  const r = await run(["import", proj, "--cell", "c6r6", "--slug", "fall", "--no-verify", "--no-fetch"], {
    root, inspect, P: {},
    gridMain: async (argv) => {
      if (argv[0] === "place") {
        mkdirSync(join(root, "tools", "grid", "cells"), { recursive: true });
        writeFileSync(join(root, "tools", "grid", "cells", "fall.json"), `${JSON.stringify({ slug: "fall", cell: "c6r6", layer: "Grid_fall.layer", rows: 0, created: "2026-11-19" })}\n`);
      }
      return 0;
    },
    placeMain: async (argv) => {
      calls.push(argv[0]);
      if (argv[0] === "ground-many") return 1;
      return argv[argv.indexOf("--yaw") + 1] === "-90" ? 1 : 0; // the second wreck is the one the checks refuse
    },
  });
  check(calls[0] === "ground-many" && calls.filter((c) => c === "ground").length === 4, `a refused batch should fall back to one place ground per prop (calls ${calls})`);
  check(r.code === 1 && /3 placed/.test(r.text) && /1 refused by the placement checks/.test(r.text), `the refused prop is counted and the others kept: ${r.text}`);
}

// ---- island slots, through fakes: the place tool's geometry is real; the game, the terrain probe and the audit are not
{
  const realP = await import("../place/place.mjs");
  const LAYER = "Place_lakeplace_shore.layer";
  const BOX = { min: [-1, 0, -0.5], max: [1, 1, 0.5] };
  const P = { ...realP, ROOT: "", prefabInfo: () => ({ bounds: BOX, scale: 1, cls: "GenericEntity", dynamic: false }), modelOf: () => [] };
  const proj = join(scratchDir("lf-mapper-selftest-"), "lake.json");
  writeFileSync(proj, JSON.stringify({ version: 0, entities: [
    { id: "bench", prefab: "{5BAA975FE04E94EF}Prefabs/Props/Civilian/BenchStreet/StreetBench_01/BenchStreet_01_green.et", x: 10, z: 10, yaw: 270 },
    { id: "bucket", prefab: "{BCE07A58672955B9}Prefabs/Props/Civilian/Bucket_01.et", x: 13, z: 10, yaw: 40 },
    { id: "crate", prefab: "{6FC9E85153AE41B1}Prefabs/Props/Civilian/WoodKindling_Crate_01.et", x: 10, z: 13, yaw: 90 },
  ] }));
  // `place ground-many` refuses a bad plan before any boot: no file, not JSON, empty, a row without x,z, a name twice
  const bad = join(scratchDir("lf-mapper-selftest-"), "plan.json");
  const many = async (body) => {
    if (body != null) writeFileSync(bad, body);
    const lines = [];
    const orig = console.log;
    console.log = (...a) => lines.push(a.join(" "));
    let code;
    try {
      code = await realP.main(["ground-many", body == null ? join(bad, "..", "missing.json") : bad, "--layer", "Street_Police.layer"]);
    } finally {
      console.log = orig;
    }
    return { code, text: lines.join("\n") };
  };
  check((await many(null)).code === 1 && /no plan file/.test((await many(null)).text), "ground-many with no plan file says so");
  check(/is not JSON/.test((await many("{nope")).text), "ground-many refuses a plan that is not JSON");
  check(/holds no rows/.test((await many("[]")).text), "ground-many refuses an empty plan");
  check(/needs at: \[x, z\]/.test((await many(JSON.stringify([{ prefab: "X.et" }]))).text), "ground-many refuses a row with no at");
  check(/names A twice/.test((await many(JSON.stringify([{ at: [1, 2], prefab: "X.et", name: "A" }, { at: [3, 4], prefab: "X.et", name: "A" }]))).text), "ground-many refuses one name twice");
  const known = async (p) => ({ ref: `{${p.guid}}${p.path}` });
  const MARKER = "GenericEntity Fit_lakeplace_shore_Place {\n coords 100 50 200\n}\n";
  const setup = () => {
    const root = scratchDir("lf-mapper-selftest-");
    const dir = join(root, "addons", "LF_Everon1989", "Worlds", "MainWorld", "MainWorld_Layers");
    mkdirSync(dir, { recursive: true });
    writeFileSync(join(dir, LAYER), MARKER);
    mkdirSync(join(root, "tools", "town", "claims"), { recursive: true });
    const claim = { schema: 1, place: "lakeplace", slot: "shore", plot: "p1", hostless: true, layer: `addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/${LAYER}`, rows: 1, date: "2026-11-19" };
    writeFileSync(join(root, "tools", "town", "claims", "lakeplace-shore.json"), `${JSON.stringify(claim)}\n`);
    const placeData = () => ({
      place: "lakeplace",
      dossier: { freePlots: { plots: [{ id: "p1", x: 100, z: 200, width: 14, depth: 80 }] } },
      mapping: { slots: { shore: { plot: "p1", hostless: true } } },
      template: { slots: [{ id: "shore" }] },
      claims: { "lakeplace-shore.json": claim },
      layers: { [LAYER]: readFileSync(join(dir, LAYER), "utf8") },
    });
    return { root, file: join(dir, LAYER), placeData };
  };
  const ctxOf = (s, over = {}) => {
    const probes = [];
    const audits = [];
    const ctx = {
      root: s.root, inspect: known, P, placeData: s.placeData, branch: "ws/town-draft-lakeplace-shore-mapper",
      probe: async (points) => {
        probes.push(points);
        return { points: Object.fromEntries(points.map((p) => [pinKey(p[0], p[1]), 50])), missing: [] };
      },
      auditMain: async (argv) => {
        audits.push(argv);
        return 0;
      },
      ...over,
    };
    return { ctx, probes, audits };
  };
  const args = ["import", proj, "--slot", LAYER];

  let s = setup();
  let c = ctxOf(s, { branch: "ws/town-draft-other-slot" });
  let r = await run(args, c.ctx);
  check(r.code === 1 && /town-draft-lakeplace-shore/.test(r.text) && readFileSync(s.file, "utf8") === MARKER, `an island slot should refuse a branch that does not own it: ${r.text}`);
  r = await run(args, ctxOf(s, { branch: "ws/town-draft-lakeplace" }).ctx);
  check(r.code === 1 && /town-draft-lakeplace-shore/.test(r.text), "the place alone is not the slot: the branch names both");
  rmSync(join(s.root, "tools", "town", "claims", "lakeplace-shore.json"));
  r = await run(args, ctxOf(s).ctx);
  check(r.code === 1 && /has no claim/.test(r.text), `a layer nobody fitted has no claim: ${r.text}`);

  s = setup();
  c = ctxOf(s);
  r = await run([...args, "--dry"], c.ctx);
  check(r.code === 0 && /slot's ground centre/.test(r.text) && /nothing written/.test(r.text) && c.probes.length === 0 && readFileSync(s.file, "utf8") === MARKER, `--dry plans from the slot's ground centre and boots nothing: ${r.text}`);
  r = await run([...args, "--dry", "--at", "100,300"], c.ctx);
  check(r.code === 1 && /outside plot p1/.test(r.text), `props outside the plot are refused: ${r.text}`);
  r = await run([...args, "--at", "100,300"], c.ctx);
  check(r.code === 1 && /outside plot p1/.test(r.text) && c.probes.length === 0 && readFileSync(s.file, "utf8") === MARKER, "an outside layout stops before the probe boot and writes nothing");

  // a good run: ONE probe, the Mapper yaw as angleY, the strict island audit
  r = await run(args, c.ctx);
  const text = readFileSync(s.file, "utf8");
  check(r.code === 0, `an island import should succeed through the fakes, got ${r.code}: ${r.text}`);
  check(c.probes.length === 1 && c.probes[0].length === 18, `ONE terrain probe for every prop (3 props x 5 footprint points + the pivot, for a terrain-frame prefab), got ${c.probes.length} probe(s) of ${c.probes[0]?.length}`);
  check((text.match(/^GenericEntity Mapper_lakeplace_shore_M\d+ : "\{[0-9A-F]{16}\}Prefabs\//gm) || []).length === 3, "the three props are written with their {GUID}path refs");
  check(text.startsWith(MARKER), "the slot's own entities are kept byte for byte");
  check(/Mapper_lakeplace_shore_M1 : "[^"]*" \{\n coords [\d.]+ 50 [\d.]+\n angleY -90\n/.test(text), "the bench keeps Mapper's yaw 270 as angleY -90, standing on the probed terrain (mesh base at the pivot)");
  check(c.audits.length === 1 && ["--strict", "--world", "MainWorld", "--layer", LAYER, "--hard-layer"].every((x) => c.audits[0].includes(x)), "the layer is audited strict on the island");
  check(!/(coords|angleY)\s+"/.test(text), "no quoted vector");
  r = await run(args, c.ctx);
  check(r.code === 1 && /already holds an import/.test(r.text) && c.probes.length === 1, "a second import into the same slot is refused");

  // a flagged audit restores the layer byte for byte
  s = setup();
  r = await run(args, ctxOf(s, { auditMain: async () => 1 }).ctx);
  check(r.code === 1 && /restored/.test(r.text) && readFileSync(s.file, "utf8") === MARKER, `a flagged audit restores the layer: ${r.text}`);

  // ground that is not level refuses the props: nothing placed, nothing written
  s = setup();
  r = await run(args, ctxOf(s, { probe: async (points) => ({ points: Object.fromEntries(points.map((p) => [pinKey(p[0], p[1]), 50 + p[0] * 0.5])), missing: [] }) }).ctx);
  check(r.code === 1 && /not level/.test(r.text) && /nothing was placed/.test(r.text) && readFileSync(s.file, "utf8") === MARKER, `sloping ground refuses the props: ${r.text}`);

  // a prop whose probe points got no answer is refused, the others kept
  s = setup();
  r = await run(args, ctxOf(s, { probe: async (points) => ({ points: Object.fromEntries(points.slice(5).map((p) => [pinKey(p[0], p[1]), 50])), missing: [] }) }).ctx);
  check(r.code === 1 && /no terrain height/.test(r.text) && (readFileSync(s.file, "utf8").match(/Mapper_lakeplace_shore_M\d+ :/g) || []).length === 2, `a prop with no probe answer is refused, the others kept: ${r.text}`);
}

// ---- the committed sample layer: its vectors are bare
{
  check(existsSync(SAMPLE_LAYER), "the sample layer Grid_mapper_junkyard_small.layer should be committed");
  const layer = existsSync(SAMPLE_LAYER) ? readFileSync(SAMPLE_LAYER, "utf8") : "";
  check(!/(coords|angles?|Position)\s+"/.test(layer), "no vector in the sample layer is quoted");
  check((layer.match(/^GenericEntity Grid_mapper_junkyard_small_M\d+ : "\{[0-9A-F]{16}\}Prefabs\//gm) || []).length === 4, "the sample layer holds the 4 imported props, each with a {GUID}path ref");
  check(!/Decal_OilStain/.test(layer), "the dropped decal is not in the sample layer");
}

// ---- the committed island import: the four props of lakeshore-small.json in the Black Lake slot, bare vectors, the bench's yaw
{
  const island = join(ROOT, "addons", "LF_Everon1989", "Worlds", "MainWorld", "MainWorld_Layers", "Place_blacklake_shore.layer");
  const text = existsSync(island) ? readFileSync(island, "utf8") : "";
  check((text.match(/^GenericEntity Mapper_blacklake_shore_M\d+ : "\{[0-9A-F]{16}\}Prefabs\//gm) || []).length === 4, "the Black Lake slot should hold the 4 imported props, each with a {GUID}path ref");
  check(!/(coords|angleY)\s+"/.test(text), "no vector in the island slot is quoted");
  check(/Mapper_blacklake_shore_M1 : "[^"]*BenchStreet_01_green[^"]*" \{\n coords [\d.]+ [\d.]+ [\d.]+\n angleY -90\n/.test(text), "the committed bench keeps Mapper's yaw 270 as angleY -90");
}

if (failed) {
  console.log(`SUMMARY mapper-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY mapper-selftest ok");
