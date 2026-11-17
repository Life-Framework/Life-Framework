// Who a layout finding blames, and so whether it blocks (maintainer rule, 2026-11-15, docs/decisions.md): the
// town is the maintainer's and is hand-edited, so a layout finding on a TOWN entity is a warning on EVERY branch
// and never blocks anyone. A finding on anything else (the debug grid, a GridWorld layer, a prefab a PR edits)
// stays hard. The zone rule still stops a normal PR from changing a town layer (validate-world-zones.mjs), so a
// town warning is always one the PR did not cause.
// Pure; proven by tools/validation/town-warn-selftest.mjs.
import { zoneOfLayer } from "./world-zones.mjs";

// True for a layer path or an addon-relative path of a town layer: a DebugWorld village layer (not Grid_*, not a
// legacy annex layer) or any layer of the pack's town world.
export function isTownFile(file) {
  const p = String(file || "").replace(/\\/g, "/");
  if (/\/Everon1989Town_Layers\/[^/]+\.layer$/.test(p) || /^Everon1989Town_Layers\/[^/]+\.layer$/.test(p)) return true;
  const m = /(?:^|\/)DebugWorld_Layers\/([^/]+)\.layer$/.exec(p);
  return !!m && zoneOfLayer(m[1], "debug") === "town";
}

// A layer file name ("Street_ChopShop.layer") of the DebugWorld, as the placement audit records it.
export const isTownLayerName = (name) => zoneOfLayer(String(name || "").replace(/\.layer$/, ""), "debug") === "town";

// validate-prop-on-surface findings ({ file, ... }, file relative to the addon): { hard, warn }.
export function splitSurfaceFindings(findings) {
  const hard = [];
  const warn = [];
  for (const f of findings) (isTownFile(f.file) ? warn : hard).push(f);
  return { hard, warn };
}

// Placement audit --strict. `ranked` rows carry { layer }, `world` findings carry { text } (a duplicate names both
// layers as [Name.layer]; a cell budget names none). `hardLayers` are the layer file names this PR itself adds or
// moves in non-town layers; null means every non-town layer. A row is hard only when its layer is one of those;
// every town row, and every row of a layer the PR did not touch, is a warning. { hardRows, warnRows, hardWorld, warnWorld }.
export function splitAuditFindings(ranked, world, { hardLayers = null } = {}) {
  const isHardLayer = (layer) => {
    if (isTownLayerName(layer)) return false;
    return hardLayers === null || hardLayers.includes(layer);
  };
  const hardRows = [];
  const warnRows = [];
  for (const r of ranked) (isHardLayer(r.layer) ? hardRows : warnRows).push(r);
  const hardWorld = [];
  const warnWorld = [];
  for (const w of world) {
    const layers = [...String(w.text).matchAll(/\[([^\]]+\.layer)\]/g)].map((m) => m[1]);
    (layers.some(isHardLayer) ? hardWorld : warnWorld).push(w);
  }
  return { hardRows, warnRows, hardWorld, warnWorld };
}

// The verdict of `audit placement --strict`: { fail, lines }. Town rows print as WARN and never fail it; rows of a
// hard layer, and world findings that name one, fail it.
export function judgeStrict(ranked, world, { hardLayers = null, fmt = (r) => `${r.name} [${r.layer}] ${r.problems.map((p) => p.text).join("; ")}` } = {}) {
  const j = splitAuditFindings(ranked, world, { hardLayers });
  const lines = [];
  for (const r of j.warnRows) lines.push(`WARN  ${fmt(r)}`);
  for (const w of j.warnWorld) lines.push(`WARN  ${w.text}`);
  if (j.warnRows.length || j.warnWorld.length) {
    lines.unshift(`audit placement: ${j.warnRows.length + j.warnWorld.length} finding(s) in the town or in layers this branch did not change: warnings, never blocking (the town is hand-edited, 2026-11-15)`);
  }
  for (const r of j.hardRows) lines.push(`FAIL  ${fmt(r)}`);
  for (const w of j.hardWorld) lines.push(`FAIL  ${w.text}`);
  return { fail: j.hardRows.length + j.hardWorld.length > 0, lines, ...j };
}

// The layer file names of a diff that the PR itself adds or moves outside the town: the layers that stay hard.
export function hardLayersOf(files) {
  const out = new Set();
  for (const f of files) {
    const p = String(f).replace(/\\/g, "/");
    if (!p.startsWith("addons/") || !p.endsWith(".layer") || isTownFile(p)) continue;
    out.add(p.slice(p.lastIndexOf("/") + 1));
  }
  return [...out].sort();
}
