// tools\cli building check <slug> [--file <prefab.et|layer>] [--json]: no small prop of a building floats, offline.
//
// For every child of the building's prefabs (addons/<addon>/<prefab_dir>/**/*.et: furniture, store, fixtures): a small prop
// (surface.mjs SMALL) must have its bottom within 2 cm of the top of the support under its footprint centre, or of a
// floor slab. A prop that hangs 2 cm or more over a top, sits in one, or has nothing under it is listed with the entity
// ID, where it is, and what to change. Hung kinds (posters, notices, clocks, ceiling lamps) are skipped. Needs only the
// paks (the same reader `grid place` sizes by), no Workbench: about a second warm.
//
// Supports outside the prefabs: the host building's colliders count, and spec.check_supports lists a counter a layer places
//   "check_supports": [{ "prefab": "Prefabs/...et", "at": [x, y, z], "yaw": 0, "why": "..." }]
// A prop that is meant to look so (a hanging pan, a prop leaning on a wall) goes in the spec:
//   "floaters_allow": [{ "match": "<entity ID or a regex on the prefab path>", "why": "<reason>" }]
// `floors` in the spec overrides the floor heights read from the host building's colliders.
// A kit whose rows live in a grid layer (the police station: systems, decor and lights in Grid_police_station_kit.layer) names it:
//   "check_layer": { "file": "<repo path of the .layer>", "host": "Grid_hostel_Main", "prefix": "HostelKit_" }
//   "check_layer": { "file": "...", "origin": [x, z], "y": 0, "yaw": 0, "prefix": "PoliceKit_", "skip": ["PoliceKit_Villa"] }
// The layer's entities whose name starts with `prefix` are judged in building space beside the prefabs' children, and stand as
// supports for each other (a phone on a desk the layer places). Building space is the host's own frame: `host` names the layer
// entity of the building and its coords, yaw (`angleY`, or `angles`) and tilt are undone, so a building standing at -90 degrees
// or on a plot raised 5 cm is judged like one at 0. `origin` (+ optional `y`, `yaw`) is the same frame given by hand.
//
// --fix moves each prop that floats over or sits in a top onto it (writes the prefab; a generator is the better home: use
// snapItems in it). Nothing under a prop is left for a person: a prop whose support was removed, or a hanging kind this list does not know.
// Exit: 0 clean, 1 a prop floats, 2 usage or the game files are not readable here (a runner without the game skips).
import { existsSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { addonDir } from "../../lib/addons.mjs";
import { parseTree } from "../../actions/prop-on-surface.mjs";
import { baseName, floaters, floorLevels, gameReader, prefabChildren, setHeights, specSupports } from "./surface.mjs";

// Every prefab of the building (its furniture, its store with the counters and fixtures): they share building space.
export function furnitureFiles(root, spec) {
  const dir = join(addonDir(root, spec.addon), spec.prefab_dir || "");
  if (!spec.prefab_dir || !existsSync(dir)) return [];
  const out = [];
  const walk = (d) => {
    for (const e of readdirSync(d, { withFileTypes: true })) {
      if (e.isDirectory()) walk(join(d, e.name));
      else if (e.name.endsWith(".et")) out.push(join(d, e.name));
    }
  };
  walk(dir);
  return out;
}

// Where the host building stands in the layer: { x, y, z, yaw, pitch, roll }. `check_layer.host` names the layer entity (its
// coords plus `angleY` / `angleX` / `angleZ`, or the base game's `angles x y z`: both forms); without it `origin: [x, z]` with an
// optional `y` and `yaw` gives the same frame by hand.
export function hostFrame(text, k) {
  const frame = { x: 0, y: k.y ?? 0, z: 0, yaw: k.yaw ?? 0, pitch: 0, roll: 0 };
  if (k.host) {
    const ent = parseTree(text).kids.find((b) => b.props.coords && b.header.replace(/^\$grp\s+/, "").trim().split(/\s+/)[1] === k.host);
    if (!ent) throw new Error(`check_layer.host ${k.host} is not an entity of ${k.file}`);
    const c = ent.props.coords.trim().split(/\s+/).map(Number);
    const ang = ent.props.angles ? ent.props.angles.trim().split(/\s+/).map(Number) : [0, 0, 0];
    return { x: c[0], y: c[1], z: c[2], pitch: Number(ent.props.angleX ?? ang[0] ?? 0), yaw: Number(ent.props.angleY ?? ang[1] ?? 0), roll: Number(ent.props.angleZ ?? ang[2] ?? 0) };
  }
  [frame.x, frame.z] = k.origin ?? [0, 0];
  return frame;
}

// A world point in the host's own frame: the inverse of Enfusion's yaw (about y), then pitch (x), then roll (z) placement.
// With yaw alone this is the `unrot` the prefab children already use; a tilted host also undoes its pitch and roll.
export function toBuildingSpace(f, x, y, z) {
  let p = [x - f.x, y - f.y, z - f.z];
  const D = Math.PI / 180;
  const turn = (a, i, j, v) => {
    const c = Math.cos(a * D);
    const s = Math.sin(a * D);
    const out = [...v];
    out[i] = c * v[i] + s * v[j];
    out[j] = -s * v[i] + c * v[j];
    return out;
  };
  // world = Ry(yaw) Rx(pitch) Rz(roll) local, so local = Rz(-roll) Rx(-pitch) Ry(-yaw) world
  p = turn(-f.yaw, 0, 2, p);
  if (f.pitch) p = turn(-f.pitch, 2, 1, p);
  if (f.roll) p = turn(-f.roll, 1, 0, p);
  return p;
}

// The kit rows of spec.check_layer in building space: [] when the spec names no layer. Each row is carried into the host's frame
// (position through the host's placement, yaw less the host's yaw), so a building placed at 90 or -90 degrees, or standing
// on a raised plot, is judged against its prefabs and colliders, which are in that frame.
export function layerEnts(root, spec) {
  const k = spec.check_layer;
  if (!k?.file) return [];
  const file = join(root, k.file);
  if (!existsSync(file)) throw new Error(`check_layer ${k.file} does not exist`);
  const text = readFileSync(file, "utf8");
  const frame = hostFrame(text, k);
  const skip = new Set(k.skip ?? []);
  if (k.host) skip.add(k.host);
  return prefabChildren(text)
    .filter((e) => e.name.startsWith(k.prefix ?? "") && !skip.has(e.name))
    .map((e) => {
      const [x, y, z] = toBuildingSpace(frame, e.x, e.y, e.z);
      return { ...e, id: e.id || e.name, x, y, z, yaw: e.yaw - frame.yaw, file: null };
    });
}

const hostPath = (spec) => String(spec.vanilla?.base ?? "").replace(/^\{[0-9A-Fa-f]{16}\}/, "");

// Floors of a building: the spec's own list, else the host's large slabs, else the heights several big props stand at.
export async function floorsOf(spec, ents, reader) {
  if (Array.isArray(spec.floors) && spec.floors.length) return spec.floors;
  if (hostPath(spec)) {
    const f = await floorLevels(hostPath(spec), { reader });
    if (f.length) return f;
  }
  const counts = new Map();
  for (const e of ents) {
    const info = e.path && reader.info(e.path);
    if (!info) continue;
    const k = e.scale ?? info.scale ?? 1;
    if ((info.bounds.max[1] - info.bounds.min[1]) * k <= 0.8) continue;
    const y = Math.round((e.y + info.bounds.min[1] * k) * 100) / 100;
    counts.set(y, (counts.get(y) ?? 0) + 1);
  }
  return [...counts].filter(([, n]) => n >= 3).map(([y]) => y);
}

const rel = (p) => p.replace(/\\/g, "/");

// A prefab of the building that another one places at an offset or a turn is a building of its own (the fuel station's
// canopy platform places the shop and the kiosk): its props are in ITS frame, on ITS floors and colliders, not the platform's.
// A prefab placed at the identity (a building's furniture set) shares its parent's frame. Returns [{ key, files }], the whole
// (root) first: files that nothing places, with every identity child; then one entry per offset child, with its own identity
// children. `key` is the part's file name.
export function buildingParts(files, addonRoot) {
  const byPath = new Map(files.map((f) => [rel(f).slice(rel(addonRoot).length + 1), f]));
  const placed = new Map(); // file -> [{ file, identity }]
  const isChild = new Set();
  for (const f of files) {
    const list = [];
    for (const e of prefabChildren(readFileSync(f, "utf8"))) {
      const c = e.path && byPath.get(e.path);
      if (!c || c === f || isChild.has(c)) continue;
      const identity = Math.abs(e.x) + Math.abs(e.y) + Math.abs(e.z) < 1e-6 && (e.yaw % 360 === 0) && !e.tilted;
      list.push({ file: c, identity });
      isChild.add(c);
    }
    placed.set(f, list);
  }
  const parts = [];
  const take = (f, part) => {
    part.files.push(f);
    for (const c of placed.get(f) ?? []) {
      if (c.identity) take(c.file, part);
      else {
        const sub = { key: baseName(rel(c.file)), files: [] };
        parts.push(sub);
        take(c.file, sub);
      }
    }
  };
  const root = { key: "", files: [] };
  parts.push(root);
  for (const f of files) if (!isChild.has(f)) take(f, root);
  return parts;
}

// The host collider of a part: spec.parts[<file name>].host (a vanilla key or a path), else spec.vanilla[<the file name less the root's
// name and "_", lower case>] (FuelStation_Building under FuelStation -> vanilla.building).
function partHost(spec, key, rootName) {
  const o = spec.parts?.[key]?.host;
  const name = key.startsWith(`${rootName}_`) ? key.slice(rootName.length + 1).toLowerCase() : key.toLowerCase();
  const v = o ? spec.vanilla?.[o] ?? o : spec.vanilla?.[name];
  return v ? String(v).replace(/^\{[0-9A-Fa-f]{16}\}/, "") : "";
}

export async function checkBuilding(root, slug, { file = null, reader = gameReader(root), allow: allowOverride = null } = {}) {
  const specFile = join(root, "tools", "buildings", slug, "spec.json");
  const spec = existsSync(specFile) ? JSON.parse(readFileSync(specFile, "utf8")) : { addon: "LF_Everon1989", prefab_dir: "" };
  const files = file ? [file] : furnitureFiles(root, spec);
  const kit = file ? [] : layerEnts(root, spec);
  if (!files.length && !kit.length) return { spec, files, ents: [], problems: [], floors: [], none: true };
  const load = (list) => list.flatMap((f) => prefabChildren(readFileSync(f, "utf8")).map((e) => ({ ...e, file: f })));
  const parts = file ? [{ key: "", files }] : buildingParts(files, addonDir(root, spec.addon));
  if (!parts.length) parts.push({ key: "", files: [] });
  const rootName = baseName(files[0] ?? "").split("_")[0];
  const allow = allowOverride ?? spec.floaters_allow ?? [];
  const ents = [];
  const problems = [];
  const partFloors = {};
  let floors = [];
  for (const part of parts) {
    const own = part.key === "" ? [...load(part.files), ...kit] : load(part.files);
    ents.push(...own);
    if (part.key === "") {
      floors = await floorsOf(spec, own, reader);
      problems.push(...(await floaters(own, { reader, floors, allow, extra: specSupports(spec) })));
      continue;
    }
    const host = partHost(spec, part.key, spec.prefab_dir ? baseName(spec.prefab_dir.replace(/\/$/, "")) : rootName);
    const pf = await floorsOf({ floors: spec.parts?.[part.key]?.floors, vanilla: { base: host } }, own, reader);
    partFloors[part.key] = pf;
    const extra = host ? [{ id: `host:${part.key}`, path: host, x: 0, y: 0, z: 0, yaw: 0, scale: null }] : [];
    problems.push(...(await floaters(own, { reader, floors: pf, allow, extra })));
  }
  return { spec, files, ents, problems, floors, partFloors, parts: parts.map((p) => p.key) };
}

const floorText = (r) => [r.floors.join(", ") || "none", ...Object.entries(r.partFloors ?? {}).map(([k, v]) => `${k} ${v.join(", ") || "none"}`)].join("; ");

export async function cmdCheck(root, args) {
  const slug = args.find((a) => !a.startsWith("--"));
  if (!slug) {
    console.log("usage: tools\\cli building check <slug> [--file <prefab.et>] [--json] [--fix]   no small prop floats: bottom within 2 cm of the top under it (offline)");
    return 2;
  }
  const at = args.indexOf("--file");
  const fix = args.includes("--fix");
  const t0 = Date.now();
  let r;
  try {
    r = await checkBuilding(root, slug, { file: at >= 0 ? args[at + 1] : null });
  } catch (e) {
    console.log(`SUMMARY building check skip ${slug}: ${e.message}`);
    return 2;
  }
  if (r.none) {
    console.log(`SUMMARY building check skip ${slug}: no prefab under ${r.spec.prefab_dir || "(no prefab_dir in the spec)"}`);
    return 2;
  }
  if (fix) {
    // Move every prop that floats over or sits in a top onto it (a prop with nothing under it needs a person). A stack settles
    // from the bottom up, so go again while something moves.
    let moved = 0;
    for (let round = 0; round < 6; round++) {
      const byFile = new Map();
      for (const p of r.problems.filter((q) => q.to != null && q.id && q.file)) byFile.set(p.file, [...(byFile.get(p.file) ?? []), p]);
      let n = 0;
      for (const [file, list] of byFile) {
        const res = setHeights(readFileSync(file, "utf8"), list);
        if (res.done) writeFileSync(file, res.text);
        n += res.done;
        for (const p of list) console.log(`  fixed ${baseName(p.path)} ${p.id} y ${p.y} -> ${p.to} (${p.support})`);
      }
      moved += n;
      if (!n) break;
      r = await checkBuilding(root, slug, { file: at >= 0 ? args[at + 1] : null });
    }
    console.log(`building check --fix: moved ${moved} prop(s) onto their surface`);
  }
  if (args.includes("--json")) console.log(JSON.stringify({ slug, floors: r.floors, problems: r.problems }, null, 1));
  for (const p of r.problems.slice(0, 60)) console.log(`  ${p.id || "(no id)"} ${baseName(p.path)} at ${p.x} ${p.y} ${p.z}: ${p.text}`);
  if (r.problems.length > 60) console.log(`  ... ${r.problems.length - 60} more`);
  const secs = ((Date.now() - t0) / 1000).toFixed(1);
  const props = r.ents.length;
  if (r.problems.length) {
    console.log(`SUMMARY building check fail ${slug}: ${r.problems.length} floating or sunk prop(s) of ${props} entities (floors ${floorText(r)}), ${secs}s. Stand a prop on its surface with surface.mjs (on: <id> or the auto snap), or list it in spec.json floaters_allow with a reason`);
    return 1;
  }
  console.log(`SUMMARY building check ok ${slug}: ${props} entities, every small prop stands on a top or a floor (floors ${floorText(r)}), ${secs}s`);
  return 0;
}
