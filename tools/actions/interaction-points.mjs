// tools/actions/interaction-points.mjs
//
// An interaction point (a UserActionContext PointInfo Offset) should sit above the prop's own top surface, where a
// player looking down at a desk, counter or bench puts the aim ray (#4803: the booking desk's point was 0.9 m, its
// top 0.904 m, and a ray landing on the top offered nothing until the point went to 1.05 m).
// What the island session showed (#4871 follow-up, `interaction-points-proof`): a point under the top is not always
// hidden. 12 of 15 props with a point 0.006 to 0.25 m under their top were still offered at -45 degrees, and the
// law-book lectern, moved to the booking desk's old corner point (0 0.9 0, 6 mm under the top), was offered at -30,
// -45 and -60. So the rule is not "the collider blocks the point". A buried point is farther from a ray that stops on
// the top than one above it, which is the likely loser when two contexts overlap (the desk's own row and the kit
// props on it); that is the one #4803 hit. Raising the point a few cm costs nothing, so a new prop starts above its top.
//
// The geometry comes from tools/actions/interaction-surfaces.json: per model, its up-facing collider surfaces
// (y, x range, z range, prefab space), written from the .xob files by
//   node tools/actions/interaction-points.mjs --update      (needs the game install, like `tools\cli measure`)
// so the check runs in CI, which has none. Docs: docs/action-anchors.md "Interaction points".
//
//   node tools/actions/interaction-points.mjs --check       the findings (validate-interaction-points runs this)
//   node tools/actions/interaction-points.mjs --fix [--dry] move each flagged point to CLEARANCE above its surface
//   node tools/actions/interaction-points.mjs --update      rewrite interaction-surfaces.json from the game files

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import {
  ROOT, addonFileOf, addonOf, directComponents, entitySpans, isDoorDefault, isHandheld, matchBlock,
  parseContext, resolveChain, textCache, walk,
} from "./place-action-contexts.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
export const TABLE_FILE = join(HERE, "interaction-surfaces.json");

// A fixed point goes this far above the top it is over. 5 cm: above rounding in the table and a few mm of
// collider skin, still well inside the context radius (a player's ray lands on the top surface, not the air).
export const CLEARANCE = 0.05;
// A point this far or less above a surface still counts as flush with it.
export const FLUSH = 0.02;
// How far a point may sit outside a surface's edge and still count as over it.
export const EDGE = 0.02;
// A top more than this above the point is a cabinet's or a register's body, not the surface the player looks down at.
export const MAX_BURIED = 0.2;
// A surface narrower than this in x or z is a rim, a door edge or a rung, not a top the player looks down at.
export const MIN_TOP = 0.15;
const KEEP_SURFACES = 24;

export function loadTable(file = TABLE_FILE) {
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return { meshes: {} };
  }
}

// The up-facing surface that buries the point, or null: the highest top over the point's x and z (a lower shelf board
// inside a bookcase is not what a ray lands on) with the point under it, or within FLUSH above it. Surfaces are
// [y, x0, x1, z0, z1].
export function buriedBy(point, surfaces) {
  const [px, py, pz] = point;
  let top = null;
  for (const [y, x0, x1, z0, z1] of surfaces) {
    if (x1 - x0 < MIN_TOP || z1 - z0 < MIN_TOP) continue;
    if (px < x0 - EDGE || px > x1 + EDGE || pz < z0 - EDGE || pz > z1 + EDGE) continue;
    if (!top || y > top.y) top = { y, x: [x0, x1], z: [z0, z1] };
  }
  if (!top || py > top.y + FLUSH || top.y - py > MAX_BURIED) return null;
  return top;
}

// tools/actions/interaction-points-allow.json: { "<repo path or path prefix>": "<why it stays>" }. A reason is required.
export function loadAllow(file = join(HERE, "interaction-points-allow.json")) {
  try {
    return Object.entries(JSON.parse(readFileSync(file, "utf8"))).filter(([k]) => !k.startsWith("_"));
  } catch {
    return [];
  }
}

export function allowedBy(rel, allow) {
  const hit = allow.find(([prefix]) => rel.startsWith(prefix));
  return hit ? hit[1] : null;
}

// The test worlds hold fixtures (Annex_*, Grid_*) that only the harness and the tests use.
export const TEST_WORLD = /\/Worlds\/(?:DebugWorld|GridWorld)\//;

export function clearY(surface) {
  return Math.round((surface.y + CLEARANCE) * 1000) / 1000;
}

const relOf = (root, file) => relative(root, file).replace(/\\/g, "/");

// Every own context with a fixed point and a model: { label, file, rel, mesh, point, radius }.
export function collectPoints(root = ROOT) {
  textCache.clear();
  const out = [];
  for (const dir of [addonOf(root), join(root, "addons", "LF_Everon1989")]) {
    if (!existsSync(dir)) continue;
    for (const file of walk(dir)) {
      const rel = relOf(root, file);
      if (TEST_WORLD.test(`/${rel}`)) continue;
      const text = readFileSync(file, "utf8");
      for (const span of entitySpans(text, true)) {
        if (isHandheld(rel, span.text)) continue;
        const comps = directComponents(span.text)?.body ?? "";
        const own = parseContext(comps);
        if (!own || !own.pointGuid) continue; // no point of its own: the ancestor's is checked at its source
        const resolved = resolveChain(root, span.text);
        if (!resolved.found || isDoorDefault(resolved) || !resolved.mesh) continue;
        const name = (span.text.split(/\r?\n/, 1)[0].match(/^[ \t]*\w+\s+([A-Za-z0-9_]+)\s*(?::|\{)/) ?? [])[1] ?? "(root)";
        out.push({ label: `${rel}#${name}`, file, rel, name, mesh: resolved.mesh, point: resolved.offset, radius: resolved.radius, ctx: own.contextGuid });
      }
    }
  }
  return out;
}

export function checkPoints(root = ROOT, table = loadTable()) {
  const findings = [];
  let checked = 0;
  const unknown = new Set();
  const allow = loadAllow();
  let allowed = 0;
  for (const p of collectPoints(root)) {
    const entry = table.meshes?.[p.mesh];
    if (!entry) { unknown.add(p.mesh); continue; }
    checked++;
    const hit = buriedBy(p.point, entry.surfaces);
    if (!hit) continue;
    if (allowedBy(p.rel, allow)) { allowed++; continue; }
    findings.push({ ...p, surface: hit, to: clearY(hit) });
  }
  return { findings, checked, allowed, unknown: [...unknown].sort() };
}

const fmt = (v) => v.map((n) => Math.round(n * 1000) / 1000).join(" ");

export function describe(f) {
  const where = f.point[1] <= f.surface.y ? `${(f.surface.y - f.point[1]).toFixed(3)} m inside` : `${(f.point[1] - f.surface.y).toFixed(3)} m above (flush with)`;
  return `${f.label}: context point ${fmt(f.point)} is ${where} the top at y ${f.surface.y.toFixed(3)} of ${f.mesh.split("/").pop()} (x ${f.surface.x.map((n) => n.toFixed(2)).join("..")}, z ${f.surface.z.map((n) => n.toFixed(2)).join("..")}). A ray landing on the top ends nearer the top than the point (#4803). Move the Offset y to ${f.to} or higher, or list the prefab in tools/actions/interaction-points-allow.json with the session that proved it offered.`;
}

// ---- --fix: rewrite the flagged Offset's y -------------------------------------------------------------

// The text with the flagged context's Offset y set to f.to, or null when the entity or context is not found.
export function fixOne(text, f) {
  const spans = entitySpans(text, true);
  const named = (s) => new RegExp(`^[ \\t]*\\w+\\s+${f.name}\\s*(?::|\\{)`).test(s.text.split(/\r?\n/, 1)[0]);
  const span = f.name === "(root)" ? spans[0] : spans.find(named);
  const comps = span && directComponents(span.text);
  if (!comps) return null;
  const head = new RegExp(`UserActionContext\\s*"\\{${f.ctx}\\}"\\s*\\{`, "i").exec(comps.body);
  if (!head) return null;
  const block = matchBlock(comps.body.slice(head.index), /^UserActionContext\s*"\{[0-9A-Fa-f]{16}\}"\s*\{/);
  if (!block) return null;
  const ctx = comps.body.slice(head.index, head.index + block.end);
  const next = ctx.replace(/(Offset[ \t]+\S+[ \t]+)\S+([ \t]+\S+)/, `$1${f.to}$2`);
  if (next === ctx) return null;
  const body = comps.body.slice(0, head.index) + next + comps.body.slice(head.index + block.end);
  const spanText = span.text.slice(0, comps.start) + body + span.text.slice(comps.end);
  return text.slice(0, span.start) + spanText + text.slice(span.end);
}

// ---- --update: the surface table ---------------------------------------------------------------------------

export async function updateTable(root = ROOT) {
  const { gameVfs, readBytes } = await import("../lib/game-prefab.mjs");
  const { measureXob } = await import("../lib/xob.mjs");
  const vfs = await gameVfs();
  const old = loadTable();
  const meshes = {};
  const missing = [];
  for (const mesh of [...new Set(collectPoints(root).map((p) => p.mesh))].sort()) {
    const buf = readBytes(root, vfs, mesh);
    if (!buf) { missing.push(mesh); if (old.meshes?.[mesh]) meshes[mesh] = old.meshes[mesh]; continue; }
    const m = measureXob(buf, { scale: 1, minSize: 0.04 });
    const r = (n) => Math.round(n * 1000) / 1000;
    const surfaces = m.surfaces
      .sort((a, b) => b.area - a.area)
      .slice(0, KEEP_SURFACES)
      .sort((a, b) => b.y - a.y)
      .map((s) => [r(s.y), r(s.x[0]), r(s.x[1]), r(s.z[0]), r(s.z[1])]);
    meshes[mesh] = { surfaces };
  }
  const table = {
    _doc: "Up-facing collider surfaces ([y, x0, x1, z0, z1], prefab space, scale 1) of every model that carries an interaction point. Written by node tools/actions/interaction-points.mjs --update; read by validate-interaction-points so CI needs no game install.",
    meshes,
  };
  writeFileSync(TABLE_FILE, JSON.stringify(table, null, 1).replace(/\[\n\s+([-\d.]+),\n\s+([-\d.]+),\n\s+([-\d.]+),\n\s+([-\d.]+),\n\s+([-\d.]+)\n\s+\]/g, "[$1, $2, $3, $4, $5]") + "\n");
  return { count: Object.keys(meshes).length, missing };
}

// ---- cli ---------------------------------------------------------------------------------------------------

async function main(argv) {
  const flag = (n) => argv.includes(n);
  if (flag("--update")) {
    const r = await updateTable();
    console.log(`interaction-points: wrote ${r.count} model(s) to tools/actions/interaction-surfaces.json${r.missing.length ? `; ${r.missing.length} not found (kept as they were): ${r.missing.slice(0, 5).join(", ")}` : ""}`);
    return 0;
  }
  const r = checkPoints();
  if (flag("--fix")) {
    let n = 0;
    const byFile = new Map();
    for (const f of r.findings) {
      if (!byFile.has(f.file)) byFile.set(f.file, []);
      byFile.get(f.file).push(f);
    }
    for (const [file, fs] of byFile) {
      let text = readFileSync(file, "utf8");
      for (const f of fs) {
        const next = fixOne(text, f);
        if (!next) { console.log(`could not rewrite ${f.label}`); continue; }
        console.log(`${flag("--dry") ? "would set" : "set"} ${f.label} y ${f.point[1]} -> ${f.to}`);
        text = next;
        n++;
      }
      if (!flag("--dry")) writeFileSync(file, text);
    }
    console.log(`interaction-points: ${n} point(s) ${flag("--dry") ? "to move" : "moved"}`);
    return 0;
  }
  for (const f of r.findings) console.log(`ERROR ${describe(f)}`);
  console.log(`interaction-points: ${r.findings.length} finding(s), ${r.checked} point(s) checked, ${r.unknown.length} model(s) not in the table`);
  return r.findings.length ? 1 : 0;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  process.exit(await main(process.argv.slice(2)));
}
