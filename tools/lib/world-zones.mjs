// tools/lib/world-zones.mjs: the two world zones (docs/architecture/world-zones.md).
//
// Grid: the net-fenced annex (x 20..208, z 20..112; it was 20..112 before the lattice grew east), which lives in GridWorld
// (Worlds/GridWorld/GridWorld_Layers, docs/architecture/three-worlds.md); until slice W6 deletes
// it, the legacy annex layers also sit in DebugWorld. One feature per cell, low look bar, written
// by `tools\cli grid`. Town: every other layer the playtest town loads
// (the DebugWorld village layers and the pack world Everon1989Town). Polished; written only by a
// town-placement pass (a `town-` worktree slug).
//
// Pure functions, no I/O except where a function says it reads the tree. Used by
// tools/commands/grid.mjs, tools/validation/validate-world-zones.mjs, agent-box/place-audit.mjs,
// tools/validation/validate-player-props.mjs and tools/visual/look.mjs.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { CORE_ADDON, GRID_WORLD, PLAYTEST_WORLD, layersOf } from "./worlds.mjs";
import { CELL, GRID_CENTRE, GRID_FENCE, allCells, cellBounds, cellId, claimCellIds, claimRect, claimSpan, isFrameClaim, parseCellId } from "../grid/lattice.mjs";

// The lattice constants and cell maths live in tools/grid/lattice.mjs (with the grid frame).
export { CELL, GRID_CENTRE, allCells, cellBounds, cellId, claimCellIds, claimRect, claimSpan, parseCellId };

export const DEBUG_LAYERS = `addons/${CORE_ADDON}/${layersOf(PLAYTEST_WORLD)}`;
// Where Grid_<slug>.layer files are written and claims are checked: GridWorld, not the playtest world.
export const GRID_LAYERS = `addons/${CORE_ADDON}/${layersOf(GRID_WORLD)}`;
export const PACK_TOWN_LAYERS = "addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers";
export const CELLS_DIR = "tools/grid/cells";

// The annex fence (Annex.layer): the grid zone.
export const GRID = { ...GRID_FENCE };
// The rectangle the DebugWorld freeze watches: the fence plus 2 m (three-worlds.md "What counts as grid").
export const ANNEX_RECT = { x0: 18, z0: 18, x1: 114, z1: 114 };
// Ground no cell may take: the open ground world tests spawn their fixtures on
// (docs/roadmap/m1-25-village.md "Test annex"), and the track in through the gap in the north fence.
export const RESERVED = [
  { why: "world-test fixture ground", x0: 60, z0: 60, x1: 90, z1: 91 },
  { why: "world-test fixture spot 48,64", x0: 47, z0: 63, x1: 49, z1: 65 },
  { why: "the track in through the north gate", x0: 66, z0: 100, x1: 82, z1: 112 },
];

// Annex layers before the grid existed (validate-annex-fixtures.mjs keeps the list for tests).
// They are grid-zone layers: the legacy stations, with the low look bar.
export const LEGACY_ANNEX = new Set([
  "Annex", "Crimes", "HousesKeys", "Jobs", "NPCs", "PRFeatures", "Police", "Tests", "WhitelistChat",
  "Factions", "Gamemode", "default", "Shops",
]);
// Layers with no props: world settings and managers. Neither zone; nobody's look bar.
export const SYSTEM_LAYERS = new Set(["default", "Gamemode", "Factions"]);

export const SLUG_RX = /^[a-z0-9][a-z0-9-]{1,40}$/;
export const layerOfSlug = (slug) => `Grid_${slug.replace(/-/g, "_")}.layer`;
// The slug is in the claim file (its `layer` field); a layer name maps back only with dashes unknown.
// Entity names written into a grid layer: Grid_<slug with _>_<n>.
export const entityPrefix = (slug) => `Grid_${slug.replace(/-/g, "_")}`;

// "grid", "town" or "system" for a layer file name (with or without .layer) and the world it is in.
export function zoneOfLayer(file, world = "debug") {
  const name = String(file).replace(/\.layer$/, "");
  if (world === "pack") return "town";
  if (SYSTEM_LAYERS.has(name)) return "system";
  if (/^Grid_/.test(name) || LEGACY_ANNEX.has(name)) return "grid";
  return "town";
}

// The zone of a repo path, or null when it is not a world layer.
export function zoneOfPath(path) {
  const p = String(path).replace(/\\/g, "/");
  if (!p.endsWith(".layer")) return null;
  const base = p.slice(p.lastIndexOf("/") + 1);
  if (p.startsWith(`${DEBUG_LAYERS}/`) || p.startsWith(`${GRID_LAYERS}/`)) return zoneOfLayer(base, "debug");
  if (p.startsWith(`${PACK_TOWN_LAYERS}/`)) return zoneOfLayer(base, "pack");
  return null;
}

export const inGrid = (x, z) => x >= GRID.x0 && x <= GRID.x1 && z >= GRID.z0 && z <= GRID.z1;
export const inAnnexRect = (x, z) => x >= ANNEX_RECT.x0 && x <= ANNEX_RECT.x1 && z >= ANNEX_RECT.z0 && z <= ANNEX_RECT.z1;

// ------------------------------------------------------------------ cells

const overlaps = (a, b) => a.x0 < b.x1 && b.x0 < a.x1 && a.z0 < b.z1 && b.z0 < a.z1;

// The side of a cell its gap opens on: the one facing the annex centre along the larger offset.
// Returned as the compass word and the unit vector pointing out of the cell through the gap.
export function gapSide(cell) {
  const dx = GRID_CENTRE[0] - (cell.x0 + cell.x1) / 2;
  const dz = GRID_CENTRE[1] - (cell.z0 + cell.z1) / 2;
  if (Math.abs(dx) > Math.abs(dz)) return dx > 0 ? { side: "east", out: [1, 0] } : { side: "west", out: [-1, 0] };
  return dz >= 0 ? { side: "north", out: [0, 1] } : { side: "south", out: [0, -1] };
}

// Why a cell cannot be used, from the reserved ground and from entities that are not the grid's own
// (an entity is { x, z, layer }). Fence runs of the annex itself never block a cell.
export function blockers(cell, ents) {
  const why = RESERVED.filter((r) => overlaps(cell, r)).map((r) => r.why);
  const pad = { x0: cell.x0 + 0.5, z0: cell.z0 + 0.5, x1: cell.x1 - 0.5, z1: cell.z1 - 0.5 };
  const hits = ents.filter((e) => !/^Grid_/.test(e.layer) && !/NetFence/.test(e.prefab || "") && e.x >= pad.x0 && e.x <= pad.x1 && e.z >= pad.z0 && e.z <= pad.z1);
  if (hits.length) why.push(`${hits.length} existing entit${hits.length === 1 ? "y" : "ies"} (${[...new Set(hits.map((h) => h.layer))].join(", ")})`);
  return why;
}

// FNV-1a: the same slug starts its search at the same cell on every machine.
export function slugHash(slug) {
  let h = 0x811c9dc5;
  for (const ch of String(slug)) {
    h ^= ch.charCodeAt(0);
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  return h >>> 0;
}

// The cell for a slug: the first usable cell from hash(slug) in lattice order. Two branches with
// different slugs rarely land on one cell; validate-world-zones fails if they do.
// `taken` is a Set of cell ids claimed in tools/grid/cells/.
// `span` is [cols, rows] for a big item: the first free block of that size, returned as the claim's rectangle
// (its low corner's id, so a one-cell span is the same cell as before).
export function allocate(slug, { ents = [], taken = new Set(), span = [1, 1] } = {}) {
  const cells = allCells();
  const start = slugHash(slug) % cells.length;
  for (let k = 0; k < cells.length; k++) {
    const c = cells[(start + k) % cells.length];
    const rect = claimRect({ cell: c.id, span });
    if (!rect) continue;
    const ids = claimCellIds({ cell: c.id, span });
    if (ids.some((id) => taken.has(id))) continue;
    if (ids.some((id) => blockers(cells.find((x) => x.id === id), ents).length)) continue;
    return rect;
  }
  return null;
}

// ------------------------------------------------------------------ the yard a cell gets

// Rotate local (+x along the run) into the world direction `d`: the yaw whose rot(yaw, 1, 0) is d.
// `rot` is place.mjs's convention (passed in so this file stays free of the tool's imports).
export function yawAlong(d, rot) {
  for (const y of [0, 90, 180, 270]) {
    const [x, z] = rot(y, 1, 0);
    if (Math.abs(x - d[0]) < 1e-6 && Math.abs(z - d[1]) < 1e-6) return y;
  }
  throw new Error(`no right-angle yaw maps +x to ${d}`);
}

// The fence runs of a cell's yard: 5 m segments (pivot at one end, extending along local +x),
// two per side, one left out on the gap side. Returns [{ at: [x, z], dir: [dx, dz] }], plus the
// sign spot (outside the open end of the gap) and the facing the sign and the props take.
// A claim of several cells (a rectangle from claimRect) gets one yard around the whole block: each side is
// ceil(length / 5) runs spaced evenly (a 10 m side is two runs at 5 m, as for one cell); a side 20 m or more
// leaves two runs out for the gap, a shorter one leaves one.
export function yardPlan(cell) {
  const i = CELL.inset;
  const a = { x0: cell.x0 + i, z0: cell.z0 + i, x1: cell.x1 - i, z1: cell.z1 - i };
  const sides = {
    south: { from: [a.x0, a.z0], dir: [1, 0] },
    east: { from: [a.x1, a.z0], dir: [0, 1] },
    north: { from: [a.x1, a.z1], dir: [-1, 0] },
    west: { from: [a.x0, a.z1], dir: [0, -1] },
  };
  const gap = gapSide(cell);
  const runs = [];
  let signAt = null;
  for (const [name, s] of Object.entries(sides)) {
    const len = Math.abs(s.dir[0]) ? a.x1 - a.x0 : a.z1 - a.z0;
    const n = Math.max(2, Math.ceil(len / CELL.fence - 1e-9));
    const L = len / n;
    const open = name === gap.side ? (len >= 20 ? 2 : 1) : 0;
    for (let k = 0; k < n; k++) {
      const at = [s.from[0] + s.dir[0] * L * k, s.from[1] + s.dir[1] * L * k];
      if (k === n - open) {
        // The open end: the sign stands just outside its far end, clear of the opening.
        const end = [s.from[0] + s.dir[0] * len, s.from[1] + s.dir[1] * len];
        signAt = [end[0] - s.dir[0] * 0.6 + gap.out[0] * 0.8, end[1] - s.dir[1] * 0.6 + gap.out[1] * 0.8];
      }
      if (k >= n - open) continue;
      runs.push({ at, dir: s.dir });
    }
  }
  return { runs, signAt, gap, yard: a };
}

// Where the k-th call's row of props goes: a line parallel to the gap side, deeper rows first
// (row 0 at the back), facing the gap. Returns the two end points and the compass word.
export function rowLine(cell, k) {
  const { gap, yard } = yardPlan(cell);
  const across = gap.side === "north" || gap.side === "south" ? yard.z1 - yard.z0 : yard.x1 - yard.x0;
  const depth = across - 2.5 - 2.5 * k; // from the gap side: 7.5 m for the first row of a 10 m yard
  if (k < 0 || depth < 1.5) return null;
  const edge = 2; // keep row ends 2 m off the side fences
  if (gap.side === "north" || gap.side === "south") {
    const z = gap.side === "north" ? yard.z1 - depth : yard.z0 + depth;
    return { a: [yard.x0 + edge, z], b: [yard.x1 - edge, z], facing: gap.side };
  }
  const x = gap.side === "east" ? yard.x1 - depth : yard.x0 + depth;
  return { a: [x, yard.z0 + edge], b: [x, yard.z1 - edge], facing: gap.side };
}

// ------------------------------------------------------------------ the registry (reads the tree)

// Every claimed cell: one JSON file per feature in tools/grid/cells/ (golden rule 10).
export function readCells(root) {
  const dir = join(root, CELLS_DIR);
  if (!existsSync(dir)) return [];
  return readdirSync(dir)
    .filter((f) => f.endsWith(".json"))
    .sort()
    .map((f) => {
      const file = `${CELLS_DIR}/${f}`;
      try {
        return { file, ...JSON.parse(readFileSync(join(dir, f), "utf8")) };
      } catch (e) {
        return { file, error: String(e.message || e) };
      }
    });
}

// The look target of a claimed cell: the centre of its yard, seen from the gap side.
export function lookArea(entry) {
  // The frame claims the whole lattice: its picture is the gate end, the map board seen from the spawn.
  if (isFrameClaim(entry)) return { name: "grid-frame", x: 74, y: 0, z: 100, radius: 22, front: 0 };
  const c = claimRect(entry);
  if (!c) return null;
  const front = { north: 0, east: 90, south: 180, west: 270 }[gapSide(c).side];
  return { name: `grid-${entry.slug}`, x: (c.x0 + c.x1) / 2, y: 0, z: (c.z0 + c.z1) / 2, radius: 8 * Math.max(c.w, c.h), front };
}

// The claims origin/main holds, read from git, so a second worktree sees a cell the first one took
// after this branch was cut (every worktree has its own copy of tools/grid/cells/). Fetches
// origin/main first (bounded to 20 s) unless { fetch: false }. A failed fetch is not fatal: it is
// `note`, and the claims come from the last origin/main this clone has; no origin/main is no claims.
export function mainClaims(root, { fetch = true, ref = "origin/main" } = {}) {
  const git = (args, timeout = 20000) => spawnSync("git", args, { cwd: root, encoding: "utf8", timeout });
  let note = "";
  if (fetch) {
    const f = git(["fetch", "--quiet", "origin", "main"]);
    if (f.status !== 0) note = `could not fetch origin/main (${String(f.stderr || f.error?.message || "timed out").trim().split("\n")[0]}); using the last one this clone has`;
  }
  const ls = git(["ls-tree", "--name-only", "-r", ref, "--", `${CELLS_DIR}/`]);
  if (ls.status !== 0) return { claims: [], note: note || `no ${ref} here; claims on main were not checked` };
  const claims = [];
  for (const file of ls.stdout.split(/\r?\n/).filter((f) => f.endsWith(".json"))) {
    const show = git(["show", `${ref}:${file}`]);
    if (show.status !== 0) continue;
    try {
      claims.push({ file, ...JSON.parse(show.stdout) });
    } catch (e) {
      claims.push({ file, error: String(e.message || e) });
    }
  }
  return { claims, note };
}

// ------------------------------------------------------------------ claims in flight (open PRs)

// A PR's claim is invisible to origin/main until it merges, so two slices open at once can each take a
// free-looking cell (#2014 `jobs` and #2016 `shooting-range` both took c0r6, 2026-11-16). This reads
// the claim files of every OPEN PR from GitHub: one `gh pr list --json files` call, then one raw read
// per cell file a PR touches. { claims: [{ file, slug, cell, pr, branch }], note, cached }.
//
// Fail soft: no `gh`, no token or an unreadable answer is `note` and no claims, never an error.
// The answer is cached for `ttlMs` (default 60 s) in the OS temp dir so a burst of `grid place` and
// `grid list` calls is one round of API calls. `exclude.branch` leaves out the caller's own PR;
// `run(cmd, args)` returns spawnSync's shape and is the test seam (selftest stubs it).
export const PR_CLAIMS_TTL_MS = 60000;
const defaultPrCache = () => join(tmpdir(), "lf-grid-open-pr-claims.json");

export function openPrClaims(root, { run, cacheFile = defaultPrCache(), ttlMs = PR_CLAIMS_TTL_MS, now = Date.now, exclude = {} } = {}) {
  const exec = run ?? ((cmd, args) => spawnSync(cmd, args, { cwd: root, encoding: "utf8", timeout: 30000, maxBuffer: 64 * 1024 * 1024 }));
  const keep = (claims) => claims.filter((c) => !(exclude.branch && c.branch === exclude.branch));
  try {
    const hit = JSON.parse(readFileSync(cacheFile, "utf8"));
    if (cacheFile && now() - hit.at < ttlMs && Array.isArray(hit.claims)) return { claims: keep(hit.claims), note: "", cached: true };
  } catch {
    // no cache, or an unreadable one: ask GitHub
  }
  const fail = (why) => ({ claims: [], note: `could not read the claims of open PRs (${why}); cells claimed in flight were not checked`, cached: false });
  const ls = exec("gh", ["pr", "list", "--state", "open", "--limit", "200", "--json", "number,headRefName,headRefOid,files"]);
  if (!ls || ls.status !== 0) return fail(String(ls?.stderr || ls?.error?.message || "gh failed").trim().split("\n")[0]);
  let prs;
  try {
    prs = JSON.parse(ls.stdout);
  } catch (e) {
    return fail(`gh answered with ${String(e.message).slice(0, 60)}`);
  }
  const claims = [];
  for (const pr of Array.isArray(prs) ? prs : []) {
    for (const f of pr.files ?? []) {
      const path = String(f.path ?? "");
      if (!path.startsWith(`${CELLS_DIR}/`) || !path.endsWith(".json")) continue;
      const raw = exec("gh", ["api", "-H", "Accept: application/vnd.github.raw", `repos/{owner}/{repo}/contents/${path}?ref=${pr.headRefOid || pr.headRefName}`]);
      if (!raw || raw.status !== 0) continue; // the PR deletes the file (grid free), or the read failed
      try {
        const j = JSON.parse(raw.stdout);
        if (j.slug && j.cell) claims.push({ file: path, slug: j.slug, cell: j.cell, pr: pr.number, branch: pr.headRefName });
      } catch {
        // not a claim file we can read; validate-world-zones reports it on that PR
      }
    }
  }
  try {
    writeFileSync(cacheFile, JSON.stringify({ at: now(), claims }));
  } catch {
    // an unwritable temp dir only costs the cache
  }
  return { claims: keep(claims), note: "", cached: false };
}
