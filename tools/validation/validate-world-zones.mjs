// tools/validation/validate-world-zones.mjs
// inputs: always
//
// The two DebugWorld zones (docs/architecture/world-zones.md).
//
// 1. Grid claims. Each tools/grid/cells/<slug>.json names a cell of the lattice, no two claim one
//    cell, each has its DebugWorld_Layers/Grid_<slug>.layer, each Grid_*.layer has a claim, and every
//    entity a grid layer places stands inside its own cell (a feature never sprawls past its fence).
// 2. Zone guard. A branch may change a town layer (a DebugWorld village layer, or a layer of the pack
//    world Everon1989Town) only when it is a town-placement pass: its worktree slug starts with
//    `town-` (branch ws/town-<chapter>). Slices put their world props in a grid cell with
//    `tools\cli grid place`. Branches that existed when the guard landed are listed in
//    tools/validation/zone-guard-baseline.json and keep working; the list never grows.
//    The branch is GITHUB_HEAD_REF in CI, else the checked-out branch; main and a detached HEAD
//    outside CI are not checked. The change set is the diff against the merge base with origin/main,
//    untracked files included.
// red-proof: tools/validation/grid-selftest.mjs plants a slice that edits Street_Hospital.layer, two
// claims on one cell and a grid prop outside its cell, and asserts each is reported.

import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { DEBUG_LAYERS, allCells, cellBounds, layerOfSlug, parseCellId, readCells, zoneOfPath } from "../lib/world-zones.mjs";

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
    const p = parseCellId(c.cell);
    if (!p) {
      errors.push(`${c.file}: cell "${c.cell}" is not in the lattice (c0r0 to c6r6)`);
      continue;
    }
    if (seen.has(c.cell)) errors.push(`${c.file}: cell ${c.cell} is claimed by ${seen.get(c.cell)} too. Run tools\\cli grid free ${c.slug} and grid place again for a free cell`);
    else seen.set(c.cell, c.file);
    const layer = layerOfSlug(c.slug);
    if (c.layer !== layer) errors.push(`${c.file}: layer should be ${layer}`);
    const file = join(root, DEBUG_LAYERS, layer);
    if (!existsSync(file)) {
      errors.push(`${c.file}: ${DEBUG_LAYERS}/${layer} is missing (tools\\cli grid place ${c.slug} writes it)`);
      continue;
    }
    const b = cellBounds(p.col, p.row);
    for (const e of topLevelCoords(readFileSync(file, "utf8"))) {
      if (e.x < b.x0 - 0.5 || e.x > b.x1 + 0.5 || e.z < b.z0 - 0.5 || e.z > b.z1 + 0.5) {
        errors.push(`${layer}: ${e.name} at ${e.x},${e.z} is outside cell ${c.cell} (x ${b.x0}..${b.x1}, z ${b.z0}..${b.z1}); a grid feature stays in its cell`);
      }
    }
  }
  const dir = join(root, DEBUG_LAYERS);
  const claimed = new Set(claims.map((c) => layerOfSlug(c.slug ?? "")));
  if (existsSync(dir)) {
    for (const f of readdirSync(dir).filter((n) => /^Grid_.*\.layer$/.test(n))) {
      if (!claimed.has(f)) errors.push(`${DEBUG_LAYERS}/${f}: no claim in tools/grid/cells/ (write grid layers with tools\\cli grid place)`);
    }
  }
  return { errors, claims: claims.length, cells: allCells().length };
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
  const errors = [...claims.errors, ...guard.errors];
  if (errors.length) {
    console.log("validate-world-zones: FAIL");
    for (const e of errors) console.log(`  ${e}`);
    process.exit(1);
  }
  console.log(`validate-world-zones: OK (${claims.claims} grid cell(s) of ${claims.cells}; guard ${guard.skipped ? `skipped: ${guard.skipped}` : `${branch}: no town layer changed`})`);
}
