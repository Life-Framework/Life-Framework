// tools/buildings/pipeline/surface.mjs - props stand on the real top of what is under them, read offline.
//
// A building generator used to type each prop's height (`put(P.phone, 3.5, 1.238, -0.75)`): the counter top was
// guessed, so phones, lamps and small items hovered over desks. This helper snaps y at write time:
//
//   y = the top of the support under the prop's footprint centre - the prop's bottom offset (bounds.min.y * scale)
//
// Bounds come from the placement tool's `prefabInfo` (the measured list, else the .xob header in the paks: what
// `grid place` sizes by); the support tops come from the .xob colliders (`tools\cli measure --surfaces`: what a trace
// and a foot hit), so a counter's top, a shelf board and a table are the surface a thing really stands on. No game.
//
// Generator use (gun-store decor.mjs is the model):
//   const items = [];                                         // { path, x, y, z, yaw, scale, id?, on?, free? }
//   items.push({ id: "desk", path: P.desk, x, y: F, z, yaw });
//   items.push({ path: P.phone, x, y: 1.1 /* a hint, replaced */, z, on: "desk" });   // stands on the desk named "desk"
//   items.push({ path: P.lampTable, x, y: 1.1, z });          // no `on`: the support under it, if one is within the band
//   const report = await snapItems(items, { fixed: prefabChildren(furnitureText) });  // mutates item.y
//
// `on` is an item `id`, a 16-hex entity ID of a child already in the prefab, or an item index. A named support must
// have a surface under the prop's centre or the generator stops with the name of the prop and the support.
// Without `on`, a prop within BAND of a surface under it snaps to that surface when it is more than TOL off; a wall-hung kind (posters, notices,
// clocks, ceiling lamps, see HUNG) and a prop with `free: true` are left where the generator put them.
// `tools\cli building check <slug>` (floaters.mjs) judges the written prefab with the same tops.
import { parseTree, prefabOf } from "../../actions/prop-on-surface.mjs";
import { ROOT, prefabInfo, rot, unrot } from "../../place/place.mjs";

export const TOL = 0.02; // `building check`: a small prop's bottom may be this far from the top under it
export const BAND = { down: 0.4, up: 0.1 }; // an unnamed prop snaps when its typed bottom is this far above / below a top
export const SMALL = { height: 0.8, plan: 1.2 }; // a "small prop" for the check: it stands on something, it is not furniture
export const RIM = { maxHeight: 0.5, recess: 0.08 }; // a support this low whose highest collider is 8 cm under its bounds top is open: stack on the rim
const SLACK = 0.05;
export const EDGE = 0.03; // a prop whose centre is this close to a top's edge still counts as standing on it (a carton across a rail) // plan slack when asking which entities could be under a point

// Kinds that hang on a wall or a ceiling (or are lights): never snapped without `on`, never flagged by the check.
export const HUNG = /Poster|Notice|Painting|Skull|Chandelier|Banner|Curtain|Mirror|Trophy|Wall(?!et)|LampFluorescent|LampIndustrial|Ceiling|Sign|ClockUniversal|Garland|Radiator|Switch|Socket|Valve|Pipe|Vent|Window|Door|Hook|Pennant|Flag|Sink|Faucet|Basin|Tap_|Toilet|Towel|ElectricityBox|LightIndustrial|ExhaustFan|ClockPunch|ClockWall|FirstAid/i;
export const baseName = (path) => String(path || "").split("/").pop().replace(/\.et$/, "");
export const isHung = (path) => HUNG.test(baseName(path));

// The reader the generators and the check use: bounds from prefabInfo, surfaces from the model colliders.
// Both are memoised; a reader of another shape (the selftest's fixtures) passes the same two methods:
//   info(path) -> { bounds: { min: [x,y,z], max: [x,y,z] }, scale } | null
//   surfaces(path) -> Promise<[{ y, x: [a, b], z: [a, b] }] | null>   in mesh space at scale 1; null = unreadable
export function gameReader(root = ROOT) {
  const infos = new Map();
  const surfs = new Map();
  let measure = null;
  return {
    info(path) {
      if (!infos.has(path)) {
        const i = prefabInfo(root, path);
        infos.set(path, i?.bounds ? { bounds: i.bounds, scale: i.scale ?? 1 } : null);
      }
      return infos.get(path);
    },
    async surfaces(path) {
      if (!surfs.has(path)) {
        let list = null;
        try {
          if (!measure) measure = (await import("../../commands/measure.mjs")).measureCache(root);
          const m = await measure(path);
          const k = 1 / (m.scale || 1);
          list = m.surfaces.map((s) => ({ y: s.y * k, x: s.x.map((v) => v * k), z: s.z.map((v) => v * k) }));
        } catch {
          list = null;
        }
        surfs.set(path, list);
      }
      return surfs.get(path);
    },
  };
}

// The entity's scale: its own line, else the prefab's.
export const scaleOf = (e, info) => (e.scale != null && e.scale !== 0 ? e.scale : info?.scale ?? 1);

// Surfaces with no collider data fall back to the bounds top.
async function surfacesOf(reader, e, info) {
  const s = await reader.surfaces(e.path);
  if (s) return s;
  const b = info.bounds;
  return [{ y: b.max[1], x: [b.min[0], b.max[0]], z: [b.min[2], b.max[2]] }];
}

// A model that reaches below its pivot (a vice's clamp, a lamp's mounting foot) rests by the pivot, not by that part.
export const BELOW_PIVOT = 0.03;

// A prop's bottom offset above its pivot, and its footprint centre (world x, z).
export function footOf(e, info) {
  const k = scaleOf(e, info);
  const b = info.bounds;
  const [dx, dz] = rot(e.yaw ?? 0, ((b.min[0] + b.max[0]) / 2) * k, ((b.min[2] + b.max[2]) / 2) * k);
  return { bottom: (b.min[1] < -BELOW_PIVOT ? 0 : b.min[1]) * k, cx: e.x + dx, cz: e.z + dz, height: (b.max[1] - b.min[1]) * k, plan: Math.max(b.max[0] - b.min[0], b.max[2] - b.min[2]) * k, k };
}

// Every top under world point (px, pz): [{ y, ent }], highest first. `skip` is the prop itself.
export async function levelsUnder(ents, reader, px, pz, skip = null) {
  const out = [];
  for (const e of ents) {
    if (e === skip || !e.path || e.tilted || e.socket) continue;
    const info = reader.info(e.path);
    if (!info) continue;
    const k = scaleOf(e, info);
    const [lx, lz] = unrot(e.yaw ?? 0, px - e.x, pz - e.z);
    const b = info.bounds;
    if (lx < b.min[0] * k - SLACK || lx > b.max[0] * k + SLACK || lz < b.min[2] * k - SLACK || lz > b.max[2] * k + SLACK) continue;
    const here = [];
    for (const s of await surfacesOf(reader, e, info)) {
      if (lx >= s.x[0] * k - EDGE && lx <= s.x[1] * k + EDGE && lz >= s.z[0] * k - EDGE && lz <= s.z[1] * k + EDGE) here.push({ y: e.y + s.y * k, ent: e });
    }
    // An open crate or box: its only up-facing collider is the inside floor, but things stack on its rim (bounds top).
    // A small prop with no collider top under the point (a loaf, a notebook) is stacked on by its bounds top. The collider
    // top stays a level too: a wall shelf's board sits under its bracket, and what stands on the board stands.
    const top = e.y + b.max[1] * k;
    const low = (b.max[1] - b.min[1]) * k <= RIM.maxHeight;
    if (low && (!here.length || top - here[0].y >= RIM.recess)) here.push({ y: top, ent: e, rim: true });
    out.push(...here);
  }
  return out.sort((a, b) => b.y - a.y);
}

const r3 = (n) => Math.round(n * 1000) / 1000;
const label = (e) => e.id || e.name || baseName(e.path);

// ---- parsing a written prefab or layer back into entities ----------------------------------------------------------

// Every entity with a prefab and coords under a furniture prefab or a layer: { id, name, path, x, y, z, yaw, scale, tilted }.
// A child's coords are relative to its parent entity, as in the engine.
export function prefabChildren(text) {
  const out = [];
  const visit = (block, par, grp) => {
    for (const k of block.kids) {
      if (k.header === "components") continue;
      if (k.header.startsWith("$grp")) {
        visit(k, par, prefabOf(k.header) || grp);
        continue;
      }
      if (!k.props.coords) {
        visit(k, par, grp);
        continue;
      }
      const c = k.props.coords.trim().split(/\s+/).map(Number);
      // A base-game furniture set writes `angles x y z`; ours write `angleY` (and angleX/angleZ for a tilt).
      const ang = k.props.angles ? k.props.angles.trim().split(/\s+/).map(Number) : null;
      const yaw = ang ? ang[1] || 0 : Number(k.props.angleY || 0);
      const [dx, dz] = par ? rot(par.yaw, c[0], c[2]) : [c[0], c[2]];
      const path = prefabOf(k.header) || grp || null;
      const head = k.header.replace(/^\$grp\s+/, "").replace(/:\s*".*$/, "").trim().split(/\s+/);
      const socket = k.kids.some((c) => c.header === "components" && c.kids.some((h) => h.header.startsWith("Hierarchy")));
      const ent = {
        socket,
        id: (k.props.ID || "").replace(/"/g, ""),
        name: head.length > 1 ? head[1] : "",
        path,
        x: (par ? par.x : 0) + dx,
        y: (par ? par.y : 0) + c[1],
        z: (par ? par.z : 0) + dz,
        yaw: (par ? par.yaw : 0) + yaw,
        scale: k.props.scale ? Number(k.props.scale) : null,
        tilted: Boolean(Number(k.props.angleX || 0) || Number(k.props.angleZ || 0) || (ang && (ang[0] || ang[2]))),
      };
      out.push(ent);
      visit(k, ent, null);
    }
  };
  visit(parseTree(text), null, null);
  return out;
}

// What a building's spec adds as supports that are not in its prefabs: the host building's own colliders (a counter or desk
// built into the shell) and spec.check_supports [{ prefab, at: [x, y, z], yaw?, scale?, id?, why }] (a counter the layer places).
export function specSupports(spec) {
  const out = [];
  const base = String(spec.vanilla?.base ?? "").replace(/^{[0-9A-Fa-f]{16}}/, "");
  if (base) out.push({ id: "host", path: base, x: 0, y: 0, z: 0, yaw: 0, scale: null });
  for (const s of spec.check_supports ?? []) out.push({ id: s.id || baseName(s.prefab), path: s.prefab.replace(/^{[0-9A-Fa-f]{16}}/, ""), x: s.at[0], y: s.at[1], z: s.at[2], yaw: s.yaw ?? 0, scale: s.scale ?? null });
  return out;
}

// ---- snapping at write time -------------------------------------------------------------------------------------------

// Snap each item's y onto the top under it. `items` rows are { path, x, y, z, yaw?, scale?, id?, on?, free? }; the scale
// is the row's value when it is not 1, else the prefab's own (a generator writes a `scale` line only when it is not 1).
// `fixed` is what the prefab already holds (prefabChildren) and may be a support. Returns { changes, unmeasured }.
export async function snapItems(items, { fixed = [], reader = gameReader() } = {}) {
  const ents = items.map((it) => ({ ...it, scale: it.scale != null && it.scale !== 1 ? it.scale : null, ref: it }));
  const all = [...fixed, ...ents];
  const byId = new Map();
  for (const e of all) if (e.id) byId.set(e.id, e);
  const supportOf = (e, i) => {
    if (e.on == null) return null;
    const s = typeof e.on === "number" ? ents[e.on] : byId.get(e.on);
    if (!s) throw new Error(`snapItems: ${baseName(e.path)} (row ${i}) stands on "${e.on}", which is no item id, entity ID or row index`);
    return s;
  };
  const changes = new Map();
  const unmeasured = new Set();
  for (let pass = 0; pass < 6; pass++) {
    let moved = false;
    for (let i = 0; i < ents.length; i++) {
      const e = ents[i];
      if (e.free) continue;
      const named = e.on != null;
      if (!named && isHung(e.path)) continue;
      const info = reader.info(e.path);
      if (!info) {
        unmeasured.add(baseName(e.path));
        continue;
      }
      const f = footOf(e, info);
      let level = null;
      if (named) {
        const s = supportOf(e, i);
        const under = (await levelsUnder([s], reader, f.cx, f.cz, e)).filter((l) => l.y <= e.y + f.bottom + 1.5);
        if (!under.length) throw new Error(`snapItems: ${baseName(e.path)} at ${r3(e.x)},${r3(e.z)} stands on "${e.on}" (${baseName(s.path)} at ${r3(s.x)},${r3(s.z)}), but no top of it is under the prop's centre`);
        level = under[0];
      } else {
        const bottom = e.y + f.bottom;
        const near = (await levelsUnder(all, reader, f.cx, f.cz, e)).filter((l) => l.y - bottom <= BAND.up && bottom - l.y <= BAND.down);
        if (near.length) level = near.sort((a, b) => Math.abs(a.y - bottom) - Math.abs(b.y - bottom))[0];
      }
      if (!level) continue;
      const y = r3(level.y - f.bottom);
      // An unnamed prop already within the check's tolerance stays as typed (a 5 mm lift on a shelf board is a choice, and
      // moving every floor prop by a millimetre only churns the prefab); a named `on` always lands exactly.
      if (!named && Math.abs(y - e.y) <= TOL) continue;
      if (Math.abs(y - e.y) > 0.0005) {
        if (!changes.has(i)) changes.set(i, { row: i, path: e.path, from: r3(e.y), to: y, on: label(level.ent) });
        else Object.assign(changes.get(i), { to: y, on: label(level.ent) });
        e.y = y;
        e.ref.y = y;
        moved = true;
      }
    }
    if (!moved) break;
  }
  const list = [...changes.values()].filter((c) => Math.abs(c.to - c.from) > 0.0005);
  return { changes: list, unmeasured: [...unmeasured], summary: summarise(list) };
}

function summarise(list) {
  if (!list.length) return "snap: every prop already stands on its surface";
  const worst = list.reduce((a, c) => (Math.abs(c.to - c.from) > Math.abs(a.to - a.from) ? c : a));
  return `snap: ${list.length} prop(s) moved onto their surface (largest ${r3(worst.to - worst.from)} m, ${baseName(worst.path)} on ${worst.on})`;
}

// ---- the check ------------------------------------------------------------------------------------------------------

// Judge entities already written. `floors` are the y of the building's floor slabs; `allow` entries are { match, why }
// (the ground, y 0, counts as a floor) where match is an entity ID or a regex on the prefab path. Returns [{ id, path, x, y, z, kind, gap, support, text }].
export async function floaters(ents, { reader = gameReader(), floors: slabs = [], allow = [], tol = TOL, extra = [] } = {}) {
  const supports = [...ents, ...extra];
  const out = [];
  const floors = slabs.includes(0) ? slabs : [0, ...slabs]; // outside, the pavement is the ground
  const allowed = (e) => allow.some((a) => a.match === e.id || (e.path && safeRx(a.match)?.test(e.path)));
  for (const e of ents) {
    if (!e.path || e.tilted || e.socket || isHung(e.path) || allowed(e)) continue;
    const info = reader.info(e.path);
    if (!info) continue;
    const f = footOf(e, info);
    if (f.height > SMALL.height || f.plan > SMALL.plan) continue;
    const bottom = e.y + f.bottom;
    if (floors.some((y) => Math.abs(bottom - y) <= tol)) continue;
    const under = await levelsUnder(supports, reader, f.cx, f.cz, e);
    if (under.some((l) => Math.abs(bottom - l.y) <= tol)) continue;
    const below = under.filter((l) => l.y < bottom).sort((a, b) => b.y - a.y)[0];
    const above = under.filter((l) => l.y > bottom).sort((a, b) => a.y - b.y)[0];
    const floorBelow = floors.filter((y) => y < bottom).sort((a, b) => b - a)[0];
    const row = { id: e.id, file: e.file, path: e.path, x: r3(e.x), y: r3(e.y), z: r3(e.z) };
    if (below && bottom - below.y <= 0.6) out.push({ ...row, to: r3(below.y - f.bottom), kind: "floating", gap: r3(bottom - below.y), support: label(below.ent), text: `floats ${r3(bottom - below.y)} m above the top of ${baseName(below.ent.path)} (${label(below.ent)}, top y ${r3(below.y)})` });
    else if (above && above.y - bottom <= 0.3) out.push({ ...row, to: r3(above.y - f.bottom), kind: "sunk", gap: r3(above.y - bottom), support: label(above.ent), text: `is ${r3(above.y - bottom)} m inside ${baseName(above.ent.path)} (${label(above.ent)}, top y ${r3(above.y)})` });
    else out.push({ ...row, kind: "unsupported", gap: floorBelow == null ? null : r3(bottom - floorBelow), support: null, text: `has nothing under it (bottom y ${r3(bottom)}${floorBelow == null ? "" : `, ${r3(bottom - floorBelow)} m over the floor at ${r3(floorBelow)}`})` });
  }
  return out;
}

function safeRx(s) {
  try {
    return new RegExp(s, "i");
  } catch {
    return null;
  }
}

// The floor slabs of a host building: the large up-facing colliders, grouped by height (to 1 cm).
export async function floorLevels(hostPath, { reader = gameReader(), minArea = 6 } = {}) {
  const surfaces = (await reader.surfaces(hostPath)) ?? [];
  const ys = new Set();
  for (const s of surfaces) if ((s.x[1] - s.x[0]) * (s.z[1] - s.z[0]) >= minArea) ys.add(Math.round(s.y * 100) / 100);
  return [...ys].sort((a, b) => a - b);
}

// Rewrite the y of entities in a prefab's text (by entity ID): fixes = [{ id, to }]. Returns { text, done }. Only the entity's own
// coords line changes, and only for an entity at the top level of the child list.
export function setHeights(text, fixes) {
  const lines = text.split("\n");
  const want = new Map(fixes.map((f) => [f.id, f.to]));
  let done = 0;
  for (let i = 0; i < lines.length; i++) {
    const id = /^ {3}ID "([0-9A-Fa-f]{16})"$/.exec(lines[i])?.[1];
    if (!id || !want.has(id)) continue;
    for (let j = i + 1; j < lines.length && lines[j] !== "  }"; j++) {
      const m = /^ {3}coords (\S+) (\S+) (\S+)$/.exec(lines[j]);
      if (m) {
        lines[j] = `   coords ${m[1]} ${want.get(id)} ${m[3]}`;
        done++;
        break;
      }
    }
  }
  return { text: lines.join("\n"), done };
}
