// tools/validation/validate-world-zones.mjs
// inputs: always
//
// The world zones (docs/architecture/world-zones.md, three-worlds.md).
//
// 1. Grid claims. Each tools/grid/cells/<slug>.json names a cell of the lattice, no two claim one
//    cell, each has its GridWorld_Layers/Grid_<slug>.layer, each Grid_*.layer has a claim, and every
//    entity a grid layer places stands inside its own cell (a feature never sprawls past its fence).
//    The frame (tools/grid/lattice.mjs, claim frame.json with cell "lattice") is the one claim that spans the
//    lattice: its entities need only stand inside the lattice rectangle.
//    A Grid_*.layer in DebugWorld_Layers fails: the grid lives in GridWorld.
// 2. Annex freeze. Until slice W6 deletes the annex from DebugWorld, no branch adds or changes an
//    entity inside the annex rectangle (x 18..114, z 18..114) in a DebugWorld layer; the copy in
//    GridWorld is the one to edit. Deleting is allowed. Entities are compared with the merge base.
// 3. Zone guard. A branch may change a town layer (a DebugWorld village layer, or a layer of the pack
//    world Everon1989Town) only when it is a town-placement pass: its worktree slug starts with
//    `town-` (branch ws/town-<chapter>). Slices put their world props in a grid cell with
//    `tools\cli grid place`. Branches that existed when the guard landed are listed in
//    tools/validation/zone-guard-baseline.json and keep working; the list never grows.
//    The branch is GITHUB_HEAD_REF in CI, else the checked-out branch; main and a detached HEAD
//    outside CI are not checked. The change set is the diff against the merge base with origin/main,
//    untracked files included.
// red-proof: tools/validation/world-zones-selftest.mjs plants a slice that edits Street_Hospital.layer,
// two claims on one cell, a grid prop outside its cell, a Grid_ layer in DebugWorld and a changed annex
// entity in DebugWorld, and asserts each is reported.

import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { FRAME_CELL, isFrameClaim, latticeRect } from "../grid/lattice.mjs";
import { DEBUG_LAYERS, GRID_LAYERS, allCells, cellBounds, inAnnexRect, layerOfSlug, parseCellId, readCells, zoneOfPath } from "../lib/world-zones.mjs";

export const BASELINE = "tools/validation/zone-guard-baseline.json";
export const TOWN_PASS = /^town-/;

// Top-level coords of a layer's entities (children carry parent-relative coords and are skipped).
export function topLevelCoords(text) {
  const out = [];
  let depth = 0;
  let name = null;
  for (const line of String(text).split("\n")) {
    const t = line.trim();
    if (depth === 0 && /\{\s*$/.test(t)) {
      const m = /^[A-Za-z_][A-Za-z0-9_]*\s+([A-Za-z_][A-Za-z0-9_]*)\s*:/.exec(t) || /^([A-Za-z_][A-Za-z0-9_]*)\s*:/.exec(t);
      name = m ? m[1] : t;
    }
    if (depth === 1) {
      const c = /^coords\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)/.exec(t);
      if (c) out.push({ name, x: Number(c[1]), z: Number(c[3]) });
    }
    depth += (t.match(/\{/g) || []).length - (t.match(/\}/g) || []).length;
  }
  return out;
}

export function checkClaims(root) {
  const errors = [];
  const claims = readCells(root);
  const seen = new Map();
  for (const c of claims) {
    const base = c.file.slice(c.file.lastIndexOf("/") + 1).replace(/\.json$/, "");
    if (c.error) {
      errors.push(`${c.file}: not JSON (${c.error})`);
      continue;
    }
    if (c.slug !== base) errors.push(`${c.file}: slug "${c.slug}" does not match the file name`);
    if (isFrameClaim(c)) {
      if (c.cell !== FRAME_CELL) errors.push(`${c.file}: the frame's cell is "${FRAME_CELL}"`);
      const frameFile = join(root, GRID_LAYERS, layerOfSlug(c.slug));
      if (!existsSync(frameFile)) {
        errors.push(`${c.file}: ${GRID_LAYERS}/${layerOfSlug(c.slug)} is missing (node tools/grid/lattice.mjs write)`);
        continue;
      }
      const r = latticeRect();
      for (const e of topLevelCoords(readFileSync(frameFile, "utf8"))) {
        if (e.x < r.x0 - 0.5 || e.x > r.x1 + 0.5 || e.z < r.z0 - 0.5 || e.z > r.z1 + 0.5) errors.push(`${layerOfSlug(c.slug)}: ${e.name} at ${e.x},${e.z} is outside the lattice (x ${r.x0}..${r.x1}, z ${r.z0}..${r.z1})`);
      }
      continue;
    }
    const p = parseCellId(c.cell);
    if (!p) {
      errors.push(`${c.file}: cell "${c.cell}" is not in the lattice (c0r0 to c6r6)`);
      continue;
    }
    if (seen.has(c.cell)) errors.push(`${c.file}: cell ${c.cell} is claimed by ${seen.get(c.cell)} too. Run tools\\cli grid free ${c.slug} and grid place again for a free cell`);
    else seen.set(c.cell, c.file);
    const layer = layerOfSlug(c.slug);
    if (c.layer !== layer) errors.push(`${c.file}: layer should be ${layer}`);
    const file = join(root, GRID_LAYERS, layer);
    if (!existsSync(file)) {
      errors.push(`${c.file}: ${GRID_LAYERS}/${layer} is missing (tools\\cli grid place ${c.slug} writes it)`);
      continue;
    }
    const b = cellBounds(p.col, p.row);
    for (const e of topLevelCoords(readFileSync(file, "utf8"))) {
      if (e.x < b.x0 - 0.5 || e.x > b.x1 + 0.5 || e.z < b.z0 - 0.5 || e.z > b.z1 + 0.5) {
        errors.push(`${layer}: ${e.name} at ${e.x},${e.z} is outside cell ${c.cell} (x ${b.x0}..${b.x1}, z ${b.z0}..${b.z1}); a grid feature stays in its cell`);
      }
    }
  }
  const claimed = new Set(claims.map((c) => layerOfSlug(c.slug ?? "")));
  const gridDir = join(root, GRID_LAYERS);
  if (existsSync(gridDir)) {
    for (const f of readdirSync(gridDir).filter((n) => /^Grid_.*\.layer$/.test(n))) {
      if (!claimed.has(f)) errors.push(`${GRID_LAYERS}/${f}: no claim in tools/grid/cells/ (write grid layers with tools\\cli grid place)`);
    }
  }
  const debugDir = join(root, DEBUG_LAYERS);
  if (existsSync(debugDir)) {
    for (const f of readdirSync(debugDir).filter((n) => /^Grid_.*\.layer$/.test(n))) {
      errors.push(`${DEBUG_LAYERS}/${f}: a Grid_ layer belongs in ${GRID_LAYERS}/ (the grid lives in GridWorld; tools\\cli grid place writes it there)`);
    }
  }
  return { errors, claims: claims.length, cells: allCells().length };
}

// Top-level entities of a layer with their exact text: [{ name, x, z, text }] (x, z null without coords).
export function topLevelBlocks(text) {
  const out = [];
  let depth = 0;
  let cur = null;
  for (const line of String(text).split("\n")) {
    const t = line.trim();
    if (depth === 0 && /\{\s*$/.test(t)) {
      const m = /^[A-Za-z_][A-Za-z0-9_]*\s+([A-Za-z_][A-Za-z0-9_]*)\s*:/.exec(t) || /^([A-Za-z_][A-Za-z0-9_]*)\s*:/.exec(t);
      cur = { name: m ? m[1] : t, x: null, z: null, lines: [] };
    }
    if (cur) {
      cur.lines.push(line);
      if (depth === 1) {
        const c = /^coords\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)/.exec(t);
        if (c) {
          cur.x = Number(c[1]);
          cur.z = Number(c[3]);
        }
      }
    }
    depth += (t.match(/\{/g) || []).length - (t.match(/\}/g) || []).length;
    if (cur && depth === 0) {
      out.push({ name: cur.name, x: cur.x, z: cur.z, text: cur.lines.join("\n") });
      cur = null;
    }
  }
  return out;
}

// The annex freeze on a change set: { files: [path], baseText(path) -> text|null at the merge base,
// nowText(path) -> text|null in the tree } -> { errors }. Only DebugWorld layers are checked. An entity
// inside the annex rectangle whose exact text is not in the base layer is new or changed.
export function checkFreeze({ files, baseText, nowText }) {
  const errors = [];
  for (const f of files) {
    const path = f.replace(/\\/g, "/");
    if (!path.endsWith(".layer") || !path.startsWith(`${DEBUG_LAYERS}/`)) continue;
    const now = nowText(path);
    if (now == null) continue; // deleted: W6 removes annex entities
    const base = new Set(topLevelBlocks(baseText(path) ?? "").map((b) => b.text));
    for (const b of topLevelBlocks(now)) {
      if (b.x == null || !inAnnexRect(b.x, b.z) || base.has(b.text)) continue;
      errors.push(`${path}: ${b.name} at ${b.x},${b.z} is new or changed inside the annex rectangle in DebugWorld. The annex is frozen there (docs/architecture/three-worlds.md); edit its copy in ${GRID_LAYERS}/ instead`);
    }
  }
  return { errors };
}

// The guard on a change set: { branch, files: [path], baseline: [branch] } -> { skipped?, town, errors }.
export function checkGuard({ branch, files, baseline }) {
  if (!branch || branch === "main" || branch === "HEAD") return { skipped: `branch ${branch || "unknown"}`, town: [], errors: [] };
  const slug = branch.replace(/^ws\//, "");
  const town = files.filter((f) => zoneOfPath(f) === "town");
  if (TOWN_PASS.test(slug)) return { skipped: `town-placement pass (${slug})`, town, errors: [] };
  if (baseline.includes(branch)) return { skipped: `${branch} predates the guard (zone-guard-baseline.json)`, town, errors: [] };
  const errors = town.map((f) => `${f}: a town layer. Slices place world props in a grid cell (tools\\cli grid place <slug> <prefab>...); town layers change only in a town-placement pass, a worktree whose slug starts with town- (docs/architecture/world-zones.md)`);
  return { town, errors };
}

function git(root, args) {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8" });
  return r.status === 0 ? r.stdout : null;
}

export function changedFiles(root) {
  const mb = git(root, ["merge-base", "origin/main", "HEAD"]);
  if (!mb) return null;
  const diff = git(root, ["diff", "--name-only", mb.trim()]);
  const un = git(root, ["ls-files", "--others", "--exclude-standard"]);
  if (diff == null || un == null) return null;
  return [...new Set([...diff.split(/\r?\n/), ...un.split(/\r?\n/)].filter(Boolean))];
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const claims = checkClaims(root);
  const branch = process.env.GITHUB_HEAD_REF || (git(root, ["rev-parse", "--abbrev-ref", "HEAD"]) || "").trim();
  const baseline = existsSync(join(root, BASELINE)) ? JSON.parse(readFileSync(join(root, BASELINE), "utf8")).branches : [];
  const files = changedFiles(root);
  const guard = files ? checkGuard({ branch, files, baseline }) : { skipped: "no merge base with origin/main", town: [], errors: [] };
  const mb = files ? (git(root, ["merge-base", "origin/main", "HEAD"]) || "").trim() : "";
  const freeze = mb && branch !== "main" && branch !== "HEAD"
    ? checkFreeze({
        files,
        baseText: (path) => git(root, ["show", `${mb}:${path}`]),
        nowText: (path) => (existsSync(join(root, path)) ? readFileSync(join(root, path), "utf8") : null),
      })
    : { errors: [] };
  const errors = [...claims.errors, ...freeze.errors, ...guard.errors];
  if (errors.length) {
    console.log("validate-world-zones: FAIL");
    for (const e of errors) console.log(`  ${e}`);
    process.exit(1);
  }
  console.log(`validate-world-zones: OK (${claims.claims} grid cell(s) of ${claims.cells}; guard ${guard.skipped ? `skipped: ${guard.skipped}` : `${branch}: no town layer changed`})`);
}
