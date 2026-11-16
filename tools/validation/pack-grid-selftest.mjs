// tools/validation/pack-grid-selftest.mjs
// inputs: tools/lib/worlds.mjs, tools/lib/world-zones.mjs, tools/commands/grid.mjs, tools/validation/validate-world-zones.mjs, tools/place/place.mjs, tools/lib/placement-gate.mjs, agent-box/session.mjs, addons/LF_Everon1989/Worlds/Everon1989Grid, addons/LF_Everon1989/Missions/LF_Everon1989_Test.conf
//
// Proves the pack grid (docs/architecture/world-zones.md "Pack cells"): a cell whose prefabs are pack content
// keeps its Grid_<slug>.layer in LF_Everon1989/Worlds/Everon1989Grid (a SubScene of GridWorld), still claims
// its cell in the one lattice (tools/grid/cells/<slug>.json with "pack": true), and every grid tool finds it.
//   1. Routing: isPackPrefab reads a `{GUID}path`, a path, a bare name and an addons/ path; packRouting sends a
//      new slug with a pack prefab or --pack to the pack, keeps a claimed cell in its world and refuses a pack
//      prefab on a core cell (a core layer must not name a pack prefab).
//   2. grid place --dry names the pack layer; grid place --pack writes it and the claim; grid free removes both.
//   3. validate-world-zones: a pack claim passes; a pack layer without "pack": true, a pack claim with its
//      layer in GridWorld and a layer in both worlds fail.
//   4. Paths: zoneOfPath calls a pack grid layer grid, not town; gridLayerDirs/gridWorldFor/hasPackGridLayers
//      and session's gridMissionFor pick the pack world only when a pack cell exists and the pack is loaded;
//      the gate treats a pack grid layer as a grid change; place's layerPath finds a pack layer.
//   5. The repo: Everon1989Grid.ent is a SubScene of GridWorld, has a meta GUID, and the pack test header boots it.
// Does not boot the game.
//
// red-proof: 2026-11-16, against the tree before the change (tools/ and agent-box/ stashed, the two new world
// files kept): this printed 21 FAIL lines, from "isPackPrefab reads a {GUID}path" to "session --mission grid
// boots the pack header when a pack cell exists", and exited 1. Green with the change: pack-grid-selftest: OK.

import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const repo = join(here, "..", "..");
const load = async (p) => {
  try {
    return await import(pathToFileURL(join(here, p)).href);
  } catch (e) {
    console.log(`(could not load ${p}: ${e.message.split("\n")[0]})`);
    return {};
  }
};
const W = await load("../lib/worlds.mjs");
const Z = await load("../lib/world-zones.mjs");
const G = await load("../commands/grid.mjs");
const V = await load("./validate-world-zones.mjs");
const PG = await load("../lib/placement-gate.mjs");
const PL = await load("../place/place.mjs");
const S = await load("../../agent-box/session.mjs");

const failures = [];
const ok = (cond, msg) => {
  if (!cond) failures.push(msg);
};
const fn = (o, n) => o[n] ?? (() => { throw new Error(`${n} is not exported`); });
const attempt = (f, msg) => {
  try {
    return f();
  } catch (e) {
    failures.push(`${msg}: ${e.message}`);
    return undefined;
  }
};
const put = (root, rel, text = "") => {
  mkdirSync(dirname(join(root, rel)), { recursive: true });
  writeFileSync(join(root, rel), text);
};
async function run(argv, root) {
  const out = [];
  const log = console.log;
  console.log = (...a) => out.push(a.join(" "));
  let code;
  try {
    code = await G.main(argv, { root, openPrClaims: () => ({ claims: [], note: "" }), worktreeClaims: () => ({ claims: [], note: "" }) });
  } finally {
    console.log = log;
  }
  return { code, out: out.join("\n") };
}

const root = mkdtempSync(join(tmpdir(), "lf-pack-grid-"));
try {
  // A tree with a core prop, a pack building and a pack prop.
  put(root, "addons/LifeFramework/Prefabs/Props/CoreCrate.et", "GenericEntity : \"{1111111111111111}Prefabs/Props/Base.et\" {\n}\n");
  put(root, "addons/LF_Everon1989/Prefabs/Buildings/Bank/Bank_Hall.et", "GenericEntity : \"{2222222222222222}Prefabs/Props/Base.et\" {\n}\n");
  put(root, "addons/LF_Everon1989/Prefabs/Props/Counter.et", "GenericEntity : \"{3333333333333333}Prefabs/Props/Base.et\" {\n}\n");
  mkdirSync(join(root, Z.GRID_LAYERS), { recursive: true });
  mkdirSync(join(root, Z.CELLS_DIR), { recursive: true });

  // 1. routing
  const is = (a) => attempt(() => fn(Z, "isPackPrefab")(root, a), `isPackPrefab(${a})`);
  ok(is("{2222222222222222}Prefabs/Buildings/Bank/Bank_Hall.et") === true, "isPackPrefab reads a {GUID}path");
  ok(is("Prefabs/Buildings/Bank/Bank_Hall.et") === true, "isPackPrefab reads a path");
  ok(is("Bank_Hall") === true, "isPackPrefab reads a bare name");
  ok(is("addons/LF_Everon1989/Prefabs/Props/Counter.et") === true, "isPackPrefab reads an addons/ path");
  ok(is("Prefabs/Props/CoreCrate.et") === false, "a core prefab is not pack content");
  ok(is("{1111111111111111}Prefabs/Props/CoreCrate.et") === false, "a core {GUID}path is not pack content");
  ok(is("{4444444444444444}Prefabs/Weapons/Vanilla.et") === false, "a base-game prefab is not pack content");
  const route = (o) => attempt(() => fn(G, "packRouting")(root, o), "packRouting");
  ok(route({ prefabs: ["Bank_Hall"] })?.pack === true, "a new slug with a pack prefab is a pack cell");
  ok(route({ prefabs: ["Bank_Hall"] })?.auto === true, "…routed automatically");
  ok(route({ flags: { pack: true } })?.pack === true && route({ flags: { pack: true } })?.auto === false, "--pack alone makes a pack cell");
  ok(route({ prefabs: ["Prefabs/Props/CoreCrate.et"] })?.pack === false, "a core prefab stays in the core grid");
  ok(route({ entry: { slug: "a", pack: true }, prefabs: ["Prefabs/Props/CoreCrate.et"] })?.pack === true, "a pack cell may hold a core prop");
  ok(/core cell/.test(route({ entry: { slug: "a" }, prefabs: ["Bank_Hall"] })?.error ?? ""), "a pack prefab on a core cell is refused");
  ok(/core cell/.test(route({ entry: { slug: "a" }, flags: { pack: true } })?.error ?? ""), "--pack on a core cell is refused");

  // 2. grid place / free
  const dry = await run(["place", "bank-hall", "Bank_Hall", "--dry", "--no-fetch"], root);
  ok(dry.code === 0 && /pack cell/.test(dry.out) && /Everon1989Grid_Layers\/Grid_bank_hall\.layer/.test(dry.out), `grid place --dry names the pack layer (${dry.out})`);
  ok(!existsSync(join(root, Z.PACK_GRID_LAYERS)), "--dry wrote nothing");
  const placed = await run(["place", "bank-hall", "--pack", "--no-fetch", "--no-verify"], root);
  ok(placed.code === 0, `grid place --pack succeeds (${placed.out})`);
  const layer = join(root, Z.PACK_GRID_LAYERS, "Grid_bank_hall.layer");
  ok(existsSync(layer), "the layer is in the pack grid world");
  ok(!existsSync(join(root, Z.GRID_LAYERS, "Grid_bank_hall.layer")), "…and not in GridWorld_Layers");
  const claim = existsSync(join(root, Z.CELLS_DIR, "bank-hall.json")) ? JSON.parse(readFileSync(join(root, Z.CELLS_DIR, "bank-hall.json"), "utf8")) : {};
  ok(claim.pack === true && /^c\d+r\d+$/.test(claim.cell ?? ""), `the claim names a lattice cell and "pack": true (${JSON.stringify(claim)})`);
  const again = await run(["place", "bank-hall", "Prefabs/Props/CoreCrate.et", "--dry", "--no-fetch"], root);
  ok(again.code === 0 && /pack cell/.test(again.out) && !/\(new\)/.test(again.out), "a second call stays on the pack cell");
  const refused = await run(["place", "crate-yard", "--no-fetch", "--no-verify"], root);
  ok(refused.code === 0 && existsSync(join(root, Z.GRID_LAYERS, "Grid_crate_yard.layer")), "a core slug still writes GridWorld_Layers");
  const bad = await run(["place", "crate-yard", "Bank_Hall", "--dry", "--no-fetch"], root);
  ok(bad.code === 1 && /grid free crate-yard/.test(bad.out), "a pack prefab on the core cell is refused with the fix");

  // 3. validate
  const errs = () => V.checkClaims(root).errors;
  ok(errs().length === 0, `a pack claim with its layer in the pack world passes (${errs().join("; ")})`);
  const cl = JSON.parse(readFileSync(join(root, Z.CELLS_DIR, "bank-hall.json"), "utf8"));
  const withoutFlag = { ...cl };
  delete withoutFlag.pack;
  writeFileSync(join(root, Z.CELLS_DIR, "bank-hall.json"), `${JSON.stringify(withoutFlag, null, 2)}\n`);
  ok(errs().some((e) => /Grid_bank_hall\.layer.*is missing/.test(e)) && errs().some((e) => /Everon1989Grid_Layers\/Grid_bank_hall\.layer: no "pack": true claim/.test(e)), `a pack layer whose claim lacks "pack": true fails (${errs().join("; ")})`);
  writeFileSync(join(root, Z.CELLS_DIR, "bank-hall.json"), `${JSON.stringify(cl, null, 2)}\n`);
  writeFileSync(join(root, Z.GRID_LAYERS, "Grid_bank_hall.layer"), readFileSync(layer, "utf8"));
  ok(errs().some((e) => /in both/.test(e)), `a layer in both worlds fails (${errs().join("; ")})`);
  rmSync(join(root, Z.GRID_LAYERS, "Grid_bank_hall.layer"));
  rmSync(layer);
  writeFileSync(join(root, Z.GRID_LAYERS, "Grid_bank_hall.layer"), "// empty\n");
  ok(errs().some((e) => /Everon1989Grid_Layers\/Grid_bank_hall\.layer is missing/.test(e)), `a pack claim whose layer is in GridWorld fails (${errs().join("; ")})`);
  rmSync(join(root, Z.GRID_LAYERS, "Grid_bank_hall.layer"));
  put(root, `${Z.PACK_GRID_LAYERS}/Grid_bank_hall.layer`, readFileSync(join(root, Z.GRID_LAYERS, "Grid_crate_yard.layer"), "utf8"));

  // 4. paths
  const zp = (p) => attempt(() => fn(Z, "zoneOfPath")(p), "zoneOfPath");
  ok(zp(`${Z.PACK_GRID_LAYERS}/Grid_x.layer`) === "grid", "a pack grid layer is in the grid zone, not the town");
  ok(zp("addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers/Village_Roads.layer") === "town", "a pack town layer is still the town");
  ok(PG.gridRelevant?.("addons/LF_Everon1989/Worlds/Everon1989Grid/Everon1989Grid_Layers/Grid_x.layer") === true, "the placement gate reads a pack grid layer as a grid change");
  ok(PL.layerPath?.(root, "Grid_bank_hall.layer") === join(root, Z.PACK_GRID_LAYERS, "Grid_bank_hall.layer"), "place finds a pack cell's layer");
  ok(PL.layerPath?.(root, "Grid_crate_yard.layer") === join(root, Z.GRID_LAYERS, "Grid_crate_yard.layer"), "place finds a core cell's layer");
  const dirs = attempt(() => fn(W, "gridLayerDirs")(root), "gridLayerDirs") ?? [];
  ok(dirs.length === 2 && dirs[0].endsWith("GridWorld_Layers") && dirs[1].endsWith("Everon1989Grid_Layers"), "gridLayerDirs lists GridWorld's folder, then the pack's");
  ok(W.gridWorldFor?.(root) === W.PACK_GRID_WORLD, "gridWorldFor picks the pack world when a pack cell exists");
  const lone = mkdtempSync(join(tmpdir(), "lf-pack-grid-core-"));
  try {
    mkdirSync(join(lone, Z.GRID_LAYERS), { recursive: true });
    ok(W.gridWorldFor?.(lone) === W.GRID_WORLD && W.gridLayerDirs?.(lone).length === 1, "with no pack cell the world is GridWorld and one folder is read");
    ok(W.hasPackGridLayers?.(lone) === false, "hasPackGridLayers is false without a pack layer");
    ok(S.gridMissionFor?.(["LifeFramework", W.PACK_ADDON], lone) === W.GRID_MISSION, "session --mission grid stays on LF_GridWorld.conf with no pack cell");
  } finally {
    rmSync(lone, { recursive: true, force: true });
  }
  ok(S.gridMissionFor?.(["LifeFramework", W.PACK_ADDON], root) === W.PACK_GRID_MISSION, "session --mission grid boots the pack header when a pack cell exists");
  ok(S.gridMissionFor?.(["LifeFramework"], root) === W.GRID_MISSION, "…and never on a core-only launch");

  // free
  const freed = await run(["free", "bank-hall"], root);
  ok(freed.code === 0 && !existsSync(join(root, Z.PACK_GRID_LAYERS, "Grid_bank_hall.layer")) && !existsSync(join(root, Z.CELLS_DIR, "bank-hall.json")), `grid free removes a pack cell's layer and claim (${freed.out})`);
} finally {
  rmSync(root, { recursive: true, force: true });
}

// 5. the repo
const read = (rel) => (existsSync(join(repo, rel)) ? readFileSync(join(repo, rel), "utf8") : "");
const pk = "addons/LF_Everon1989";
ok(/Parent "Worlds\/GridWorld\/GridWorld\.ent"/.test(read(`${pk}/Worlds/Everon1989Grid/Everon1989Grid.ent`)), "Everon1989Grid.ent is a SubScene of GridWorld");
const guid = /Name "\{([0-9A-F]{16})\}Worlds\/Everon1989Grid\/Everon1989Grid\.ent"/.exec(read(`${pk}/Worlds/Everon1989Grid/Everon1989Grid.ent.meta`))?.[1];
ok(Boolean(guid), "Everon1989Grid.ent has a .meta GUID");
ok(new RegExp(`World "\\{${guid}\\}Worlds/Everon1989Grid/Everon1989Grid\\.ent"`).test(read(`${pk}/Missions/LF_Everon1989_Test.conf`)), "the pack test header boots Everon1989Grid");
ok(existsSync(join(repo, pk, "Worlds/Everon1989Grid/Everon1989Grid_Layers/default.layer")), "the pack grid world has its default.layer");
ok(W.PACK_GRID_MISSION === W.PACK_TEST_MISSION, "the pack grid header is the pack test header");

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  console.log(`pack-grid-selftest: ${failures.length} failure(s)`);
  process.exit(1);
}
console.log("pack-grid-selftest: OK (pack cells route to Everon1989Grid, claim the shared lattice, and every grid tool finds them)");
