// tools/town/vegetation.mjs: `town vegetation <place> <slot>`: the island's own trees and bushes that crowd an open-ground slot (#4724).
//
// The island's forest is not a generator we can edit: every pine, birch and bush is a `Tree`-class entity of the base map with its own
// prefab (`PrefabLibrary/DefaultLibrary/Vegetation/Trees|Bushes/*.et`), so the same runtime hide that takes a vanilla sign off a wall
// (LF_BaseWorldClearComponent in box mode, docs/architecture/base-world-clear.md) takes a tree or a bush out of a camp. This step
// lists that vegetation inside the slot's clearing and writes the kit's boxed `clear` entries for it; `town fit --refit` turns them into carriers.
//
//   town vegetation <place> <slot>                 boot the island, scan, print what the clearing would hide (writes nothing)
//   town vegetation <place> <slot> --write         the same, then replace the kit's vegetation `clear` entries (run `town fit --refit` next)
//   town vegetation <place> <slot> --verify        after the fit: scan again, fail when a tree or bush of the clearing is still drawn
//   --margin <m>   metres the clearing reaches past the kit's footprint (default 1.5)
//   --reach <m>    metres the gate approach is cleared outward from the gate rows (default 8)
//   --gate <rx>    which kit rows make the gate line (default "gate")
//   --height <m>   box height above the ground under the carrier (default 12)
//   --below <m>    how far each box reaches below the ground under its carrier (default 4: the ground falls away from the box centre)
//   --scan-file f  read a saved `scan` answer (tmp/veg-scan-<place>-<slot>.txt) instead of booting the island
//
// The clearing is two boxes in the kit's frame: the footprint plus the margin, and a strip as wide as the gate rows reaching outward
// from the footprint edge nearest to them (the track a car takes to the gate). Only a hostless slot (an open plot, no building) is supported:
// its anchor is the plot centre at yaw 0, the way `town fit` seats it.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { toLocal, toWorld } from "./kit.mjs";

export const help = "vegetation <place> <slot> [--write] [--verify] [--margin m] [--reach m]   the island's trees and bushes in an open slot's clearing: list them, write the kit's boxed `clear` entries (#4724)";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;
const r3 = (n) => Math.round(n * 1000) / 1000;
const VALUED = new Set(["margin", "reach", "gate", "height", "below", "scan-file"]);
const DEFAULTS = { margin: 1.5, reach: 8, gate: "gate", height: 12, below: 4 };

// ------------------------------------------------------------------ pure parts

// One `scan` answer line of the placement audit: { prefab, name, x, y, z, top, cls, mesh, vis } or null. `class=` and `vis=` are in lines
// from a build that has them; an older line gives cls "" and vis 1.
export function parseScanLine(line) {
  const t = String(line).trim();
  if (!t.startsWith("scan ")) return null;
  const kv = {};
  for (const m of t.slice(5).matchAll(/(\w+)=(\S+)/g)) kv[m[1]] = m[2];
  const p = String(kv.p ?? "").split(",").map(Number);
  if (!kv.prefab || p.length !== 3 || p.some((n) => !Number.isFinite(n))) return null;
  return { prefab: kv.prefab, name: kv.name ?? "", x: p[0], y: p[1], z: p[2], top: kv.top === "1", cls: kv.class ?? "", mesh: kv.mesh ?? "", vis: kv.vis === undefined ? 1 : Number(kv.vis) };
}

export const parseScan = (text) => String(text).split(/\r?\n/).map(parseScanLine).filter(Boolean);

// Vegetation of the base map: a Tree-class entity (pines, birches, bushes alike) with a prefab under Vegetation/.
export const isVegetation = (e) => e.cls === "Tree" && /\/vegetation\//i.test(e.prefab.replace(/\\/g, "/"));

// The clearing, as boxes in the kit's frame: { id, offset: [cx, cz], half: [hx, hz] }.
// kit.anchoring.footprint (min, max) plus `margin` on every side is the camp; the gate rows' x-extent reaching `reach` metres outward from the
// footprint edge nearest to them is the approach.
export function clearingBoxes(kit, { margin = DEFAULTS.margin, reach = DEFAULTS.reach, gate = DEFAULTS.gate } = {}) {
  const fp = kit.anchoring?.footprint;
  if (!fp?.min || !fp?.max) throw new Error(`kit ${kit.kit}: anchoring.footprint is missing`);
  const [x0, z0] = [fp.min[0] - margin, fp.min[1] - margin];
  const [x1, z1] = [fp.max[0] + margin, fp.max[1] + margin];
  const boxes = [{ id: "camp", offset: [r3((x0 + x1) / 2), r3((z0 + z1) / 2)], half: [r3((x1 - x0) / 2), r3((z1 - z0) / 2)] }];
  const rx = new RegExp(gate);
  const gateRows = (kit.rows ?? []).filter((r) => rx.test(r.id) && Array.isArray(r.offset));
  if (gateRows.length && reach > 0) {
    const gx0 = Math.min(...gateRows.map((r) => r.offset[0])) - 0.5;
    const gx1 = Math.max(...gateRows.map((r) => r.offset[0])) + 0.5;
    const gz = gateRows.reduce((s, r) => s + r.offset[2], 0) / gateRows.length;
    // The footprint edge nearest to the gate rows says which way is outward.
    const gcx = (gx0 + gx1) / 2;
    const edges = [
      { d: Math.abs(gcx - x0), side: "-x" },
      { d: Math.abs(x1 - gcx), side: "+x" },
      { d: Math.abs(gz - z0), side: "-z" },
      { d: Math.abs(z1 - gz), side: "+z" },
    ].sort((a, b) => a.d - b.d);
    const side = edges[0].side;
    let rect;
    if (side === "-z") rect = [gx0, z0 - reach, gx1, z0];
    else if (side === "+z") rect = [gx0, z1, gx1, z1 + reach];
    else {
      const gzs = gateRows.map((r) => r.offset[2]);
      const [lo, hi] = [Math.min(...gzs) - 0.5, Math.max(...gzs) + 0.5];
      rect = side === "-x" ? [x0 - reach, lo, x0, hi] : [x1, lo, x1 + reach, hi];
    }
    boxes.push({ id: "approach", offset: [r3((rect[0] + rect[2]) / 2), r3((rect[1] + rect[3]) / 2)], half: [r3((rect[2] - rect[0]) / 2), r3((rect[3] - rect[1]) / 2)], side });
  }
  return boxes;
}

// A world point in the kit's frame.
export function localOf(anchor, wx, wz) {
  const [lx, lz] = toLocal(wx - anchor.x, wz - anchor.z, anchor.yaw ?? 0);
  return [r3(lx), r3(lz)];
}

export const inBox = (box, [lx, lz]) => Math.abs(lx - box.offset[0]) <= box.half[0] && Math.abs(lz - box.offset[1]) <= box.half[1];

// The vegetation of a scan inside each box: [{ box, found: [entity + local] }].
export function selectVegetation(entries, boxes, anchor) {
  return boxes.map((box) => ({
    box,
    found: entries
      .filter(isVegetation)
      .map((e) => ({ ...e, local: localOf(anchor, e.x, e.z) }))
      .filter((e) => inBox(box, e.local)),
  }));
}

// The kit's boxed clear entries for those finds: one per box and distinct prefab, like the bank's door.
export function clearEntries(selected, height = DEFAULTS.height, below = DEFAULTS.below) {
  const out = [];
  for (const { box, found } of selected) {
    for (const prefab of [...new Set(found.map((e) => e.prefab))].sort()) {
      out.push({ prefab, box: { offset: [...box.offset], half: [...box.half], height, below } });
    }
  }
  return out;
}

export const isVegetationEntry = (c) => c && typeof c === "object" && /\/vegetation\//i.test(String(c.prefab ?? "").replace(/\\/g, "/")) && Boolean(c.box);

// The kit with its vegetation entries replaced; other `clear` entries (a door, a sign) stay. No entries and none before leaves the kit unchanged.
export function withVegetation(kit, entries) {
  const keep = (kit.clear ?? []).filter((c) => !isVegetationEntry(c));
  const next = { ...kit };
  const merged = [...keep, ...entries];
  if (merged.length) next.clear = merged;
  else delete next.clear;
  return next;
}

// After a fit: what of the clearing is still drawn (the carrier has not hidden it), and what outside it was hidden along the way.
export function verifyClearing(entries, boxes, anchor) {
  const veg = entries.filter(isVegetation).map((e) => ({ ...e, local: localOf(anchor, e.x, e.z) }));
  const inside = (e) => boxes.some((b) => inBox(b, e.local));
  return {
    shown: veg.filter((e) => inside(e) && e.vis !== 0),
    hiddenInside: veg.filter((e) => inside(e) && e.vis === 0),
    hiddenOutside: veg.filter((e) => !inside(e) && e.vis === 0),
  };
}

// ------------------------------------------------------------------ the command

const readJson = (f) => JSON.parse(readFileSync(f, "utf8"));
const base = (p) => String(p).replace(/^\{[0-9A-Fa-f]{16}\}/, "").split("/").pop().replace(/\.et$/, "");

function parseArgs(args) {
  const o = { _: [], flags: {} };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a.startsWith("--")) {
      const k = a.slice(2);
      o.flags[k] = VALUED.has(k) ? args[++i] : true;
    } else o._.push(a);
  }
  return o;
}

// The anchor of a hostless slot: the dossier plot centre at yaw 0 (fit.mjs plans it the same way).
export function hostlessAnchor(mapping, dossier, slot) {
  const entry = mapping.slots?.[slot];
  if (!entry) throw new Error(`slot ${slot} is not in the mapping`);
  if (!entry.hostless) throw new Error(`slot ${slot} has a building: only a hostless (open-ground) slot is supported`);
  const plot = (dossier.freePlots?.plots ?? []).find((p) => p.id === entry.plot);
  if (!plot) throw new Error(`plot ${entry.plot} is not in the dossier`);
  return { x: plot.x, z: plot.z, yaw: 0 };
}

export async function main(args, { root = ROOT } = {}) {
  const { _: pos, flags } = parseArgs(args);
  const [place, slot] = pos;
  if (!place || !slot || !SLUG_RX.test(place) || !SLUG_RX.test(slot)) {
    console.log(`usage: tools\\cli town ${help}`);
    return 1;
  }
  const num = (k) => (flags[k] === undefined ? DEFAULTS[k] : Number(flags[k]));
  const opts = { margin: num("margin"), reach: num("reach"), gate: flags.gate ?? DEFAULTS.gate };
  const height = num("height");
  const below = num("below");
  if (![opts.margin, opts.reach, height, below].every(Number.isFinite)) {
    console.log("town vegetation: --margin, --reach, --height and --below are numbers of metres");
    return 1;
  }

  const placeDir = join(root, "docs", "everon", "places", place);
  const mappingFile = join(placeDir, "mapping.json");
  const dossierFile = join(placeDir, "dossier.json");
  if (!existsSync(mappingFile) || !existsSync(dossierFile)) {
    console.log(`town vegetation: ${place} has no mapping.json and dossier.json under docs/everon/places`);
    return 1;
  }
  const mapping = readJson(mappingFile);
  const templateFile = join(root, "tools", "town", "templates", `${mapping.template}.json`);
  const kitName = mapping.slots?.[slot]?.kit ?? (existsSync(templateFile) ? (readJson(templateFile).slots ?? []).find((s) => s.id === slot)?.kit : null) ?? null;
  const kitFile = kitName ? join(root, "tools", "town", "kits", `${kitName}.json`) : null;
  if (!kitFile || !existsSync(kitFile)) {
    console.log(`town vegetation: no kit for ${place}/${slot}`);
    return 1;
  }
  const kit = readJson(kitFile);
  let anchor;
  let boxes;
  try {
    anchor = hostlessAnchor(mapping, readJson(dossierFile), slot);
    boxes = clearingBoxes(kit, opts);
  } catch (e) {
    console.log(`town vegetation: ${e.message}`);
    return 1;
  }

  // Where to scan: a circle round the clearing, every box inside it.
  const corners = boxes.flatMap((b) => [[-1, -1], [-1, 1], [1, -1], [1, 1]].map(([sx, sz]) => [b.offset[0] + sx * b.half[0], b.offset[1] + sz * b.half[1]]));
  const world = corners.map(([lx, lz]) => toWorld(lx, lz, anchor.yaw)).map(([dx, dz]) => [anchor.x + dx, anchor.z + dz]);
  const cx = r3(world.reduce((s, p) => s + p[0], 0) / world.length);
  const cz = r3(world.reduce((s, p) => s + p[1], 0) / world.length);
  const radius = Math.ceil(Math.max(...world.map((p) => Math.hypot(p[0] - cx, p[1] - cz)))) + 4;

  const scanFile = join(root, "tmp", `veg-scan-${place}-${slot}.txt`);
  let text;
  if (flags["scan-file"]) text = readFileSync(resolve(flags["scan-file"]), "utf8");
  else {
    const audit = await import(`file://${join(root, "agent-box", "place-audit.mjs").replace(/\\/g, "/")}`);
    const layerName = `Place_${place}_${slot}.layer`;
    const run = audit.measureTown(audit.auditRecords({ layers: [layerName], island: true }), { island: true, extra: [`scan ${cx} ${cz} ${radius}`], preferLocal: true, log: console.log });
    if (run.outText == null) {
      console.log("town vegetation: the island's test server wrote no measurements (lines above)");
      return 1;
    }
    text = run.outText.split("\n").filter((l) => l.startsWith("scan ")).join("\n");
    mkdirSync(dirname(scanFile), { recursive: true });
    writeFileSync(scanFile, `${text}\n`);
  }
  const entries = parseScan(text);
  if (!entries.some(isVegetation)) {
    console.log(`town vegetation: the scan round ${cx}, ${cz} (r ${radius}) shows no vegetation: nothing to clear, or a build whose scan prints no Tree class`);
    return flags.verify ? 1 : 0;
  }

  if (flags.verify) {
    const v = verifyClearing(entries, boxes, anchor);
    console.log(`town vegetation --verify ${place}/${slot}: ${v.hiddenInside.length} hidden in the clearing, ${v.shown.length} still drawn, ${v.hiddenOutside.length} hidden outside it`);
    for (const e of v.shown) console.log(`  STILL DRAWN  ${base(e.prefab)} at ${e.x}, ${e.z} (kit ${e.local.join(", ")})`);
    for (const e of v.hiddenOutside.slice(0, 20)) console.log(`  outside      ${base(e.prefab)} at ${e.x}, ${e.z} (kit ${e.local.join(", ")}): hidden by a box edge`);
    console.log(`SUMMARY town-vegetation verify ${v.shown.length ? "fail" : "ok"} shown=${v.shown.length} hidden=${v.hiddenInside.length} outside=${v.hiddenOutside.length}`);
    return v.shown.length ? 1 : 0;
  }

  const selected = selectVegetation(entries, boxes, anchor);
  for (const { box, found } of selected) {
    const counts = new Map();
    for (const e of found) counts.set(base(e.prefab), (counts.get(base(e.prefab)) ?? 0) + 1);
    console.log(`box ${box.id}: kit offset ${box.offset.join(", ")} half ${box.half.join(" x ")}${box.side ? ` (outward ${box.side})` : ""}: ${found.length} trees and bushes`);
    for (const [name, n] of [...counts].sort((a, b) => b[1] - a[1])) console.log(`  ${String(n).padStart(3)}  ${name}`);
  }
  const wanted = clearEntries(selected, height, below);
  if (!flags.write) {
    console.log(`SUMMARY town-vegetation list ok boxes=${boxes.length} entries=${wanted.length} trees=${selected.reduce((s, x) => s + x.found.length, 0)} (add --write to put them in tools/town/kits/${kitName}.json)`);
    return 0;
  }
  const next = withVegetation(kit, wanted);
  writeFileSync(kitFile, `${JSON.stringify(next, null, 2)}\n`);
  console.log(`town vegetation: wrote ${wanted.length} boxed clear entries to tools/town/kits/${kitName}.json; next: tools\\cli town fit ${place} ${slot} --refit --no-look, then --verify`);
  console.log(`SUMMARY town-vegetation write ok entries=${wanted.length}`);
  return 0;
}
