// tools/validation/grid-autosize-selftest.mjs
// inputs: tools/grid/autosize.mjs, tools/commands/grid.mjs, tools/lib/world-zones.mjs, tools/grid/lattice.mjs, tools/place/place.mjs
//
// Proves `tools\cli grid place <slug> <prefab>` sizes its own block (docs/architecture/world-zones.md "One command for
// a building"): the span is ceil((footprint + margin) / 12) per axis for the better of the two orientations, a 15.3 x 12.7 m
// villa takes 2x2, a 1 m crate 1x1, a 30 m building 3x1 or 1x3 by yaw; the front faces the yard's gap when a yaw lets it;
// the first free block is claimed and its centre goes to `place ground`; no free block names the largest free one and
// writes nothing; --span and --cell still override; open-PR claims carry their span. The bounds are fixtures
// (the grid command's `readPrefab` seam), the place tool and the audit are stubs: no server boots here.
// red-proof: with spanFor made `Math.floor((extent + margin) / CELL.size)` (no margin rounding up) this printed 22 FAILs, from
// "a 15.3 x 12.7 m villa takes a 2x2 block" to "a 2x2 in an open PR is avoided" (2026-11-16); restored, it passes.

import { mkdirSync, mkdtempSync, readFileSync, existsSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import * as P from "../place/place.mjs";
import { main as gridMain } from "../commands/grid.mjs";
import { MARGIN, chooseYaw, largestFree, noFitMessage, spanFor, spanOptions } from "../grid/autosize.mjs";
import { CELLS_DIR, GRID_LAYERS, allCells, blockers, claimCellIds, claimRect, gapSide, layerOfSlug, openPrClaims } from "../lib/world-zones.mjs";

let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

const box = (w, h, d, base = 0) => ({ min: [-w / 2, base, -d / 2], max: [w / 2, base + h, d / 2] });
const VILLA = box(15.3, 19.6, 12.7, -4.9);
const CRATE = box(1, 1, 1);
const LONG = box(30, 5, 6);
const spans = (b, margin) => spanOptions(P, b, 1, margin).map((o) => `${o.span.join("x")}@${o.yaw}`).join(",");

// ---- the maths
ok(spans(VILLA) === "2x2@0", `a 15.3 x 12.7 m villa takes a 2x2 block (got ${spans(VILLA)})`);
ok(spans(CRATE) === "1x1@0", `a 1 m crate takes a 1x1 block (got ${spans(CRATE)})`);
ok(spans(LONG) === "3x1@0,1x3@90", `a 30 x 6 m building is 3x1 at yaw 0 or 1x3 at yaw 90 (got ${spans(LONG)})`);
ok(spans(box(24.06, 20, 17.76)) === "3x2@0,2x3@90", `a 24 x 17.8 m office takes 3x2 or 2x3 (got ${spans(box(24.06, 20, 17.76))})`);
ok(spanFor(20) === 2 && spanFor(20.1) === 3 && spanFor(8) === 1 && spanFor(8.1) === 2, "a footprint that fills a block exactly with its margin stays in it (20 + 4 = 24 = 2 cells)");
ok(spanOptions(P, box(14, 3, 14), 1, 0).map((o) => o.span.join("x")).join() === "2x2", "a square footprint gives one option, not two");
const tall = spanOptions(P, { min: [-4, 0, -4], max: [4, 40, 4] }, 1, MARGIN);
ok(tall.length === 1 && tall[0].span.join("x") === "1x1", "height never changes the span");
const scaled = spanOptions(P, box(10, 3, 10), 2, MARGIN);
ok(scaled[0].span.join("x") === "2x2", "a root scale of 2 doubles the footprint (20 m + 4 = 2 cells)");

// ---- the yaw: front faces the gap when a yaw allows
const cellW = claimRect({ cell: "c3r3", span: [1, 3] });
const west = { out: [-1, 0] };
const long13 = chooseYaw(P, { bounds: LONG, block: cellW, out: west.out });
ok(long13 && long13.faces && long13.yaw === 270, `the long building in a 1x3 block with a west gap turns its front (+z) west: yaw 270 (got ${JSON.stringify(long13)})`);
const long31 = chooseYaw(P, { bounds: LONG, block: claimRect({ cell: "c3r3", span: [3, 1] }), out: west.out });
ok(long31 && !long31.faces && (long31.yaw === 0 || long31.yaw === 180), `in a 3x1 block with a west gap the front cannot face it (got ${JSON.stringify(long31)})`);
ok(chooseYaw(P, { bounds: LONG, block: claimRect({ cell: "c3r3", span: [1, 1] }), out: west.out }) === null, "no yaw fits a 30 m building in one cell");
ok(chooseYaw(P, { bounds: LONG, front: "+x", block: claimRect({ cell: "c3r3", span: [3, 1] }), out: west.out })?.yaw === 180, "--front +x: the +x face points west at yaw 180");

// ---- no block fits: the message names the largest free block
const gap = (c) => gapSide(c).out;
ok(gap(allCells()[0]) !== undefined, "gapSide gives a direction");
{
  const taken = new Set(allCells().map((c) => c.id));
  for (const id of claimCellIds({ cell: "c10r0", span: [2, 2] })) taken.delete(id);
  const big = largestFree({ taken, ents: [], blocked: (id, e) => blockers(allCells().find((c) => c.id === id), e).length > 0 });
  ok(big && big.w === 2 && big.h === 2 && big.id === "c10r0", `the largest free block is the one 2x2 left (got ${JSON.stringify(big)})`);
  const msg = noFitMessage(spanOptions(P, box(30, 8, 30)), big);
  ok(/no free 3x3/.test(msg) && /largest free block is 2x2 cells \(24 x 24 m\) at c10r0/.test(msg), `the no-fit message names what was tried and the largest free block (got ${msg})`);
  ok(/no free cell is left/.test(noFitMessage(spanOptions(P, box(30, 8, 30)), null)), "no free cell at all says so");
  ok(largestFree({ taken: new Set(allCells().map((c) => c.id)), ents: [], blocked: () => false }) === null, "a full grid has no free block");
}

// ---- grid place end to end, with the bounds, the place tool and the audit stubbed
const put = (root, path, body) => {
  mkdirSync(dirname(join(root, path)), { recursive: true });
  writeFileSync(join(root, path), body);
};
const claimFile = (slug, cell, span) => JSON.stringify({ slug, cell, ...(span ? { span } : {}), layer: layerOfSlug(slug), rows: 0, created: "2026-11-16" });
const prefabs = {
  villa: { ref: "{5CADC96916FF1CC4}Prefabs/Structures/Houses/Villa/Villa_E_2I01/Villa_E_2I01.et", info: { cls: "SCR_DestructibleBuildingEntity", bounds: VILLA, scale: 1 } },
  crate: { ref: "{AAAAAAAAAAAAAAAA}Prefabs/Props/Crate_01.et", info: { cls: "GenericEntity", bounds: CRATE, scale: 1 } },
  long: { ref: "{BBBBBBBBBBBBBBBB}Prefabs/Structures/Long_01.et", info: { cls: "SCR_DestructibleBuildingEntity", bounds: LONG, scale: 1 } },
  huge: { ref: "{CCCCCCCCCCCCCCCC}Prefabs/Structures/Huge_01.et", info: { cls: "SCR_DestructibleBuildingEntity", bounds: box(30, 8, 30), scale: 1 } },
};
const readPrefab = (arg) => prefabs[arg] ?? null;
const none = () => ({ claims: [], note: "" });
const real = console.log;
async function run(root, argv, extra = {}) {
  const out = [];
  console.log = (...a) => out.push(a.join(" "));
  try {
    const code = await gridMain(argv, { root, openPrClaims: none, worktreeClaims: none, readPrefab, ...extra });
    return { code, out };
  } finally {
    console.log = real;
  }
}
const tmp = mkdtempSync(join(tmpdir(), "lf-grid-autosize-"));
try {
  const fresh = (name) => {
    const r = join(tmp, name);
    put(r, `${GRID_LAYERS}/Annex.layer`, "");
    return r;
  };

  // the villa: 2x2, centred, front to the gap
  let root = fresh("villa");
  const calls = [];
  const placeMain = async (argv) => {
    calls.push(argv);
    return 0;
  };
  const auditCalls = [];
  const auditMain = async (v) => {
    auditCalls.push(v);
    return 0;
  };
  let g = await run(root, ["place", "villa-test", "villa", "--no-fetch"], { placeMain, auditMain });
  const claim = existsSync(join(root, CELLS_DIR, "villa-test.json")) ? JSON.parse(readFileSync(join(root, CELLS_DIR, "villa-test.json"), "utf8")) : null;
  ok(g.code === 0 && claim && JSON.stringify(claim.span) === "[2,2]", `grid place claims a 2x2 block for the villa (${g.out.join(" | ")})`);
  const rect = claim && claimRect(claim);
  ok(g.out.some((l) => /footprint 15\.3 x 12\.7 m, height 19\.6 m/.test(l) && /SCR_DestructibleBuildingEntity/.test(l)), `it prints the bounds and the entity class read from the prefab (${g.out[0]})`);
  ok(calls.length === 1 && calls[0][0] === "ground" && calls[0][2] === prefabs.villa.ref && calls[0].includes("--layer") && calls[0].includes(layerOfSlug("villa-test")), `it hands the villa to place ground with its full ref (${JSON.stringify(calls[0])})`);
  const at = calls[0]?.[1].split(",").map(Number);
  ok(rect && at && at[0] === (rect.x0 + rect.x1) / 2 && at[1] === (rect.z0 + rect.z1) / 2, `the footprint centre goes to the middle of the block (got ${at}, block ${rect && [rect.x0, rect.x1, rect.z0, rect.z1]})`);
  const yaw = Number(calls[0]?.[calls[0].indexOf("--yaw") + 1]);
  const facing = P.rot(yaw, 0, 1);
  const out = rect && gapSide(rect).out;
  ok(out && Math.abs(facing[0] - out[0]) < 1e-6 && Math.abs(facing[1] - out[1]) < 1e-6, `the villa's front faces the gap (yaw ${yaw}, gap out ${out})`);
  ok(auditCalls.length === 1 && auditCalls[0][0] === "audit-layer" && auditCalls[0][1] === layerOfSlug("villa-test"), `the cell's layer is audited once (${JSON.stringify(auditCalls)})`);
  ok(existsSync(join(root, GRID_LAYERS, layerOfSlug("villa-test"))), "the yard layer is written");

  // the place tool refuses: the claim and the yard are removed
  root = fresh("refused");
  g = await run(root, ["place", "refused", "villa", "--no-fetch"], { placeMain: async () => 1, auditMain });
  ok(g.code === 1 && !existsSync(join(root, CELLS_DIR, "refused.json")) && !existsSync(join(root, GRID_LAYERS, layerOfSlug("refused"))), `a building the place tool refuses leaves no claim or yard (${g.out.join(" | ")})`);

  // --dry writes nothing and prints the plan
  root = fresh("dry");
  g = await run(root, ["place", "villa-dry", "villa", "--dry", "--no-fetch"], { placeMain: async () => { throw new Error("placed in a dry run"); }, auditMain });
  ok(g.code === 0 && !existsSync(join(root, CELLS_DIR)) && !existsSync(join(root, GRID_LAYERS, layerOfSlug("villa-dry"))), "--dry writes no claim and no layer");
  ok(g.out.some((l) => /would place SCR_DestructibleBuildingEntity .*angleY/.test(l)) && g.out.some((l) => /\+2x2/.test(l)), `--dry prints the span, the class, the pose (${g.out.join(" | ")})`);

  // a 1 m crate stays a 1x1 cell with the rows of props
  root = fresh("crate");
  g = await run(root, ["place", "crate-test", "crate", "--dry", "--no-fetch"]);
  ok(g.code === 0 && g.out.some((l) => /would place 1 prop\(s\) in row 0/.test(l)) && !g.out.some((l) => /\+\d+x\d+/.test(l)), `a crate is a 1x1 cell with a row of props (${g.out.join(" | ")})`);

  // the long building: 3x1 or 1x3, whichever lets its front face the gap
  root = fresh("long");
  g = await run(root, ["place", "long-test", "long", "--dry", "--no-fetch"]);
  ok(g.code === 0 && g.out.some((l) => /\+(3x1|1x3)/.test(l)), `a 30 m building takes 3x1 or 1x3 (${g.out.join(" | ")})`);

  // explicit --span and --cell override the sizing
  root = fresh("explicit");
  g = await run(root, ["place", "villa-big", "villa", "--span", "3x3", "--cell", "c10r2", "--dry", "--no-fetch"]);
  ok(g.code === 0 && g.out.some((l) => /cell c10r2 \+3x3/.test(l)), `--span and --cell override (${g.out.join(" | ")})`);
  g = await run(root, ["place", "villa-small", "villa", "--span", "1x1", "--dry", "--no-fetch"]);
  ok(g.code === 1 && g.out.some((l) => /does not fit/.test(l)) && g.out.some((l) => /bigger --span/.test(l)), `an explicit span too small for the villa is said so (${g.out.join(" | ")})`);
  g = await run(root, ["place", "villa-cell", "villa", "--cell", "c10r2", "--dry", "--no-fetch"]);
  ok(g.code === 0 && g.out.some((l) => /cell c10r2 \+2x2/.test(l)), `--cell alone takes the sized span at that cell (${g.out.join(" | ")})`);

  // nothing fits: the message, no writes
  root = fresh("nofit");
  for (const c of allCells()) {
    if (claimCellIds({ cell: "c10r0", span: [2, 2] }).includes(c.id)) continue;
    put(root, `${CELLS_DIR}/fill-${c.id}.json`, claimFile(`fill-${c.id}`, c.id));
  }
  g = await run(root, ["place", "huge-test", "huge", "--no-fetch"], { placeMain, auditMain });
  ok(g.code === 1 && g.out.some((l) => /no free 3x3/.test(l) && /largest free block is 2x2 cells \(24 x 24 m\) at c10r0/.test(l)), `no block fits: the message names the largest free block (${g.out.join(" | ")})`);
  ok(!existsSync(join(root, CELLS_DIR, "huge-test.json")) && !existsSync(join(root, GRID_LAYERS, layerOfSlug("huge-test"))), "no block fits: nothing is written");
  g = await run(root, ["place", "villa-fits", "villa", "--dry", "--no-fetch"]);
  ok(g.code === 0 && g.out.some((l) => /cell c10r0 \+2x2/.test(l)), `the one free 2x2 is still found for the villa (${g.out.join(" | ")})`);

  // claims in other worktrees and open PRs count, with their spans
  root = fresh("claims");
  const natural = await run(root, ["place", "probe-a", "villa", "--dry", "--no-fetch"]);
  const nat = /cell (c\d+r\d+) \+2x2/.exec(natural.out.join("\n"))?.[1];
  ok(nat, "a natural 2x2 cell");
  const mine = new Set(claimCellIds({ cell: nat, span: [2, 2] }));
  g = await run(root, ["place", "probe-a", "villa", "--dry", "--no-fetch"], { worktreeClaims: () => ({ claims: [{ file: "x", slug: "other", cell: nat, span: [2, 2], worktree: "Life-Framework-ws-other", branch: "ws/other" }], note: "" }) });
  const got = /cell (c\d+r\d+) \+2x2/.exec(g.out.join("\n"))?.[1];
  ok(got && !claimCellIds({ cell: got, span: [2, 2] }).some((id) => mine.has(id)), `a 2x2 in a sibling worktree is avoided (took ${got}, sibling holds ${nat})`);
  g = await run(root, ["place", "probe-a", "villa", "--dry"], { openPrClaims: () => ({ claims: [{ file: "x", slug: "pr-claim", cell: nat, span: [2, 2], pr: 7, branch: "ws/pr" }], note: "" }) });
  const got2 = /cell (c\d+r\d+) \+2x2/.exec(g.out.join("\n"))?.[1];
  ok(got2 && !claimCellIds({ cell: got2, span: [2, 2] }).some((id) => mine.has(id)), `a 2x2 in an open PR is avoided (took ${got2}, the PR holds ${nat})`);

  // open-PR claims read from GitHub keep their span
  const gh = (cmd, args) => {
    if (args[0] === "pr") return { status: 0, stdout: JSON.stringify([{ number: 9, headRefName: "ws/bank", headRefOid: "abc", files: [{ path: `${CELLS_DIR}/bank.json` }] }]) };
    return { status: 0, stdout: claimFile("bank", "c3r1", [2, 2]) };
  };
  const pr = openPrClaims(tmp, { run: gh, cacheFile: join(tmp, "pr-cache.json") });
  ok(pr.claims.length === 1 && JSON.stringify(pr.claims[0].span) === "[2,2]", `an open PR's claim keeps its span (got ${JSON.stringify(pr.claims)})`);
} finally {
  console.log = real;
  rmSync(tmp, { recursive: true, force: true });
}

if (failed) {
  console.log(`grid-autosize-selftest: ${failed} FAIL(s)`);
  process.exit(1);
}
console.log("grid-autosize-selftest: ok");
