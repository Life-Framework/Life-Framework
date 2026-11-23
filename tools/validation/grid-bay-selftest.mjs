// tools/validation/grid-bay-selftest.mjs
// inputs: tools/grid/lattice.mjs, tools/lib/world-zones.mjs, tools/commands/grid.mjs, tools/town/author.mjs, tools/validation/validate-world-zones.mjs
//
// Proves the authoring bays (docs/architecture/world-zones.md "Authoring bays"): the east end of the lattice (c25 to c28, rows
// r0 to r5) is six 2 x 2 bays that only `town author` (`grid place <slug> <prefab> --bay`) claims; every other claim keeps out of
// them; the claim carries "authoring": true; validate-world-zones refuses a normal claim on a bay and an authoring claim that
// is not exactly one bay; `town author extract` releases the bay its layer sits in (the kit is the source of truth) and
// `town author --reopen <kit>` plans the rebuild from the kit alone. The bounds, the place tool and the audit are stubs (no server).
// red-proof: 2026-11-21, with allocate not skipping authoring cells for a normal claim (`|| isAuthoringCell(id)` removed) this printed
// "FAIL a normal claim never takes an authoring bay cell (got c25r2)" and "FAIL a normal 2x2 claim never takes a bay"; restored, OK.
import { existsSync, mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { main as gridMain } from "../commands/grid.mjs";
import { CELL, authoringBays, isAuthoringCell, isBayClaim, claimCellIds, allCells } from "../grid/lattice.mjs";
import { CELLS_DIR, GRID_LAYERS, PACK_GRID_LAYERS, allocate, layerOfSlug } from "../lib/world-zones.mjs";
import { checkClaims } from "./validate-world-zones.mjs";
import { bayClaimOfLayer, planAuthor, releaseBay } from "../town/author.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

// ---- the lattice: six bays, inside c25..c28 rows r0..r5, none on a legacy cell
const bays = authoringBays();
ok(CELL.cols === 32 && bays.length === 6, `the lattice has 32 columns and six authoring bays (got ${CELL.cols}, ${bays.length})`);
ok(bays.every((b) => b.span.join("x") === "2x2" && b.cells.length === 4 && b.cells.every((id) => claimCellIds({ cell: id }).length === 1)), "every bay is a 2x2 of lattice cells");
ok(bays.map((b) => b.id).join() === "c25r0,c27r0,c25r2,c27r2,c25r4,c27r4", `the bays are c25r0, c27r0, c25r2, c27r2, c25r4, c27r4 (got ${bays.map((b) => b.id)})`);
const used = bays.flatMap((b) => b.cells);
ok(new Set(used).size === 24 && used.every((id) => Number(/^c(\d+)r/.exec(id)[1]) >= 25 && Number(/r(\d+)$/.exec(id)[1]) <= 5), "the 24 bay cells are distinct, east of c24, below row r6");
ok(!isAuthoringCell("c24r2") && !isAuthoringCell("c25r6") && isAuthoringCell("c28r5"), "legacy cells and row r6 are not bay cells");

// ---- allocate: a normal claim never takes a bay, --bay takes only one
const nonBay = new Set(allCells().filter((c) => !isAuthoringCell(c.id)).map((c) => c.id));
ok(allocate("shop", { span: [1, 1], ents: [], taken: nonBay }) === null, `a normal claim never takes an authoring bay cell (got ${allocate("shop", { span: [1, 1], ents: [], taken: nonBay })?.id})`);
ok(allocate("shop", { span: [2, 2], ents: [], taken: nonBay }) === null, "a normal 2x2 claim never takes a bay");
const first = allocate("butcher", { ents: [], taken: nonBay, authoring: true });
ok(first && bays.some((b) => b.id === first.id) && first.w === 2 && first.h === 2, `--bay takes a free bay (got ${first?.id})`);
const allButOne = new Set([...allCells().map((c) => c.id)]);
for (const id of bays[3].cells) allButOne.delete(id);
ok(allocate("butcher", { ents: [], taken: allButOne, authoring: true })?.id === bays[3].id, "--bay takes the one free bay left");
allButOne.add(bays[3].cells[0]);
ok(allocate("butcher", { ents: [], taken: allButOne, authoring: true }) === null, "a bay with one claimed cell is not free");

// ---- grid place --bay, end to end (stubs for the bounds, the place tool and the audit)
const box = (w, h, d, base = 0) => ({ min: [-w / 2, base, -d / 2], max: [w / 2, base + h, d / 2] });
const prefabs = {
  house: { ref: "{38A5F3E4578087AB}Prefabs/Structures/Houses/Town/House_Town_E_2I01/House_Town_E_2I01.et", info: { cls: "SCR_DestructibleBuildingEntity", bounds: box(12.36, 9, 16.76, -3), scale: 1 } },
  office: { ref: "{363A616E6C64BB7F}Prefabs/Structures/Municipal/MunicipalOffice_E_01.et", info: { cls: "SCR_DestructibleBuildingEntity", bounds: box(24.06, 20, 17.76), scale: 1 } },
  crate: { ref: "{AAAAAAAAAAAAAAAA}Prefabs/Props/Crate_01.et", info: { cls: "GenericEntity", bounds: box(1, 1, 1), scale: 1 } },
};
const readPrefab = (arg) => prefabs[arg] ?? null;
const none = () => ({ claims: [], note: "" });
const real = console.log;
async function run(root, argv, extra = {}) {
  const out = [];
  console.log = (...a) => out.push(a.join(" "));
  try {
    const code = await gridMain(argv, { root, openPrClaims: none, worktreeClaims: none, readPrefab, placeMain: async () => 0, auditMain: async () => 0, ...extra });
    return { code, out };
  } finally {
    console.log = real;
  }
}
const put = (root, path, body) => {
  mkdirSync(dirname(join(root, path)), { recursive: true });
  writeFileSync(join(root, path), body);
};
const readClaim = (root, slug) => (existsSync(join(root, CELLS_DIR, `${slug}.json`)) ? JSON.parse(readFileSync(join(root, CELLS_DIR, `${slug}.json`), "utf8")) : null);

const tmp = scratchDir("lf-grid-bay-");
try {
  const fresh = (name) => {
    const r = join(tmp, name);
    put(r, `${GRID_LAYERS}/Annex.layer`, "");
    return r;
  };

  let root = fresh("bay");
  let g = await run(root, ["place", "author-butcher", "house", "--pack", "--bay", "--no-fetch"]);
  let claim = readClaim(root, "author-butcher");
  ok(g.code === 0 && claim && bays.some((b) => b.id === claim.cell) && claim.authoring === true && claim.pack === true && JSON.stringify(claim.span) === "[2,2]", `grid place --bay claims a 2x2 authoring bay and records authoring + pack (${g.out.join(" | ")}; ${JSON.stringify(claim)})`);
  ok(claim && isBayClaim(claim) && existsSync(join(root, PACK_GRID_LAYERS, layerOfSlug("author-butcher"))), "the claim is exactly one bay and its layer is in the pack grid world");

  // the second author takes another bay; each --bay claim stays out of the first one's cells
  g = await run(root, ["place", "author-bakery", "house", "--pack", "--bay", "--no-fetch"]);
  const second = readClaim(root, "author-bakery");
  ok(g.code === 0 && second && claim && second.cell !== claim.cell && !claimCellIds(second).some((id) => claimCellIds(claim).includes(id)), `a second --bay claim takes another bay (${g.out.join(" | ")})`);

  // a bay holds only a 2x2: a 24 x 18 m office needs 3x2 and is refused
  g = await run(root, ["place", "author-office", "office", "--pack", "--bay", "--no-fetch"]);
  ok(g.code === 1 && !readClaim(root, "author-office") && g.out.some((l) => /more than the 2x2 authoring bay/.test(l)), `a building bigger than a bay is refused (${g.out.join(" | ")})`);

  // a normal claim never lands in a bay, even with --cell
  g = await run(root, ["place", "plain-crate", "crate", "--cell", "c25r4", "--no-fetch"]);
  ok(g.code === 1 && !readClaim(root, "plain-crate") && g.out.some((l) => /authoring bay/.test(l)), `grid place --cell on a bay cell without --bay is refused (${g.out.join(" | ")})`);
  g = await run(root, ["place", "author-cell", "house", "--pack", "--bay", "--cell", "c26r0", "--no-fetch", "--dry"]);
  ok(g.code === 1 && g.out.some((l) => /not an authoring bay/.test(l)), `--bay --cell wants a bay's corner cell (${g.out.join(" | ")})`);

  // six bays fill; the seventh says so
  for (const n of ["a", "b", "c", "d"]) await run(root, ["place", `author-fill-${n}`, "house", "--pack", "--bay", "--no-fetch"]);
  g = await run(root, ["place", "author-seventh", "house", "--pack", "--bay", "--no-fetch"]);
  ok(g.code === 1 && g.out.some((l) => /no free authoring bay \(6 of 6/.test(l)) && !readClaim(root, "author-seventh"), `a full set of bays says so and writes nothing (${g.out.join(" | ")})`);

  // validate-world-zones
  ok(checkClaims(root).errors.length === 0, `the bay claims validate (${checkClaims(root).errors.join("; ")})`);
  put(root, `${CELLS_DIR}/sneaky.json`, JSON.stringify({ slug: "sneaky", cell: "c24r5", span: [2, 1], layer: layerOfSlug("sneaky"), rows: 0, created: "2026-11-21" }));
  put(root, `${GRID_LAYERS}/${layerOfSlug("sneaky")}`, "");
  ok(checkClaims(root).errors.some((e) => /sneaky\.json/.test(e) && /authoring bay/.test(e)), "a normal claim that touches a bay fails validate-world-zones");
  put(root, `${CELLS_DIR}/sneaky.json`, JSON.stringify({ slug: "sneaky", cell: "c26r0", span: [2, 2], authoring: true, layer: layerOfSlug("sneaky"), rows: 0, created: "2026-11-21" }));
  ok(checkClaims(root).errors.some((e) => /sneaky\.json/.test(e) && /exactly on one 2x2 authoring bay/.test(e)), "an authoring claim off a bay's corner fails validate-world-zones");
  unlinkSync(join(root, CELLS_DIR, "sneaky.json"));
  unlinkSync(join(root, GRID_LAYERS, layerOfSlug("sneaky")));

  // town author: extract releases the bay, --reopen plans from the kit
  const layerPath = join(root, PACK_GRID_LAYERS, layerOfSlug("author-butcher"));
  ok(bayClaimOfLayer(root, layerPath)?.slug === "author-butcher", "the layer of an authoring claim is found as a bay");
  ok(bayClaimOfLayer(root, join(root, GRID_LAYERS, "Grid_sneaky_other.layer")) === null, "a layer with no authoring claim is not a bay");
  const freed = [];
  ok(releaseBay(root, layerPath, (slug) => { freed.push(slug); return 0; }) === "author-butcher" && freed.join() === "author-butcher", "releaseBay frees the claim's slug");
  ok(releaseBay(root, join(root, GRID_LAYERS, "Grid_slice_x.layer"), () => { throw new Error("freed a non-bay"); }) === null, "a normal grid layer is never freed by extract");
  put(root, `${CELLS_DIR}/normal.json`, JSON.stringify({ slug: "normal", cell: "c3r3", layer: layerOfSlug("normal"), rows: 0, created: "2026-11-21" }));
  ok(releaseBay(root, join(root, GRID_LAYERS, layerOfSlug("normal")), () => { throw new Error("freed a normal cell"); }) === null, "a claim without authoring: true is never freed by extract");
  g = await run(root, ["free", "author-butcher"]);
  ok(readClaim(root, "author-butcher") === null && !existsSync(layerPath), "grid free removes the bay's claim and its pack layer");
  g = await run(root, ["place", "author-butcher", "house", "--pack", "--bay", "--no-fetch"]);
  ok(g.code === 0 && readClaim(root, "author-butcher")?.authoring === true, `a released bay can be claimed again (${g.out.join(" | ")})`);

  const kits = join(tmp, "kits");
  put(tmp, "kits/butcher.json", JSON.stringify({ kit: "butcher", building: { vanilla: prefabs.house.ref }, stamp: { prefab: prefabs.house.ref }, rows: [] }));
  const re = planAuthor("butcher", { reopen: true, placesDir: join(tmp, "places"), templatesDir: join(tmp, "templates"), kitsDir: kits });
  ok(re.reopen && re.seed && re.prefab === prefabs.house.ref && re.slug === "author-butcher", `--reopen plans from the kit alone, seeded (${JSON.stringify(re).slice(0, 120)})`);
  let err = "";
  try {
    planAuthor("nokit", { reopen: true, placesDir: join(tmp, "places"), templatesDir: join(tmp, "templates"), kitsDir: kits });
  } catch (e) {
    err = e.message;
  }
  ok(/no kit .*to rebuild from/.test(err), `--reopen with no kit is refused (${err})`);
} finally {
  console.log = real;
}

if (failed) {
  console.log(`grid-bay-selftest: ${failed} FAILED`);
  process.exit(1);
}
console.log("grid-bay-selftest: OK (six 2x2 authoring bays, --bay only, normal claims keep out, extract frees, --reopen rebuilds)");
