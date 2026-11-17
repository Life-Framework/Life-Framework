// tools/town/kit.mjs: `town kit extract|check|apply` (card #2203, docs/architecture/everon-world.md "Fit-out: kits").
//
// A kit is a finished playtest-town shop lifted whole: prefab rows with a role and a place-tool verb, in the
// building's own frame (offset, yaw), no world coordinates. Config is REFERENCED, not copied: a row keeps the
// scalar overrides it sets (an outlet id, an alarm reach) and names the blocks it carries (stock lines, shop
// items, action contexts); the blocks themselves stay in the source file, which the kit names, so a kit applied
// on Everon behaves like the playtest shop. `anchoring` records how the kit sits in its building (floor height,
// footprint, which wall each counter hugs, the street side) so `town fit` can adapt it to another building.
//
//   node tools/town/kit.mjs extract <source> <group> --kit <name> [--system s] [--origin <spec>] [--exclude g]
//                                   [--role id=role] [--verb id=verb] [--out file]
//   node tools/town/kit.mjs check [kit.json ...]        parse, resolve prefabs, leak-check
//   node tools/town/kit.mjs apply <kit.json> <x> <z> <yawDeg> [floorY]   absolute rows (what fit will call)
//
// <source>  a .layer (entities in world coordinates) or a .et store prefab (child entities are building-local).
// <group>   comma list of entity-name globs (`Roadmap_Police_*`); `*` = every entity (every child for a .et).
// <origin>  layers only: `Name` (entity name) or `prefab:<substring>` of the entity the kit is relative to,
//           optionally `file.layer::` first when it lives in another layer (the building), or `x,z,yaw`.
// Reads layers, never writes them (world-zones.md).
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const KITS_DIR = join(ROOT, "tools", "town", "kits");
const r3 = (n) => Math.round(n * 1000) / 1000;
const REF_RE = /^\{([0-9A-Fa-f]{16})\}(.+)$/;

// ---------- layer / prefab text -> block tree ----------
// A block is `{head, props:[line], kids:[block]}`; `head` is the text before `{`, "" for a bare `{`.
export function parseBlocks(text) {
  const root = { head: "", props: [], kids: [] };
  const stack = [root];
  for (const raw of String(text).split("\n")) {
    const line = raw.trim();
    if (!line || line.startsWith("//")) continue;
    if (line === "}") { if (stack.length > 1) stack.pop(); continue; }
    if (line.endsWith("{")) {
      const b = { head: line.slice(0, -1).trim(), props: [], kids: [] };
      stack[stack.length - 1].kids.push(b);
      stack.push(b);
    } else stack[stack.length - 1].props.push(line);
  }
  return root;
}

const HEAD_RE = /^(\$grp\s+)?([\w]+)?(?:\s+([\w.\-]+))?(?:\s*:\s*"([^"]+)")?$/;
const isEntityHead = (h) => h === "" || /^\$grp\b/.test(h) || / : "/.test(h) || /^[A-Za-z_]\w*\s+[\w.\-]+$/.test(h) && !/^(components|additionalActions|ActionContexts)\b/.test(h);

function propsOf(b) {
  const o = { coords: [0, 0, 0], yaw: 0, pitch: 0, roll: 0, scale: 1 };
  for (const p of b.props) {
    const [k, ...v] = p.split(/\s+/);
    const n = v.map(Number);
    if (k === "coords") o.coords = n;
    else if (k === "angles") { o.pitch = n[0] || 0; o.yaw = n[1] || 0; o.roll = n[2] || 0; }
    else if (k === "angleY") o.yaw = n[0] || 0;
    else if (k === "angleX") o.pitch = n[0] || 0;
    else if (k === "angleZ") o.roll = n[0] || 0;
    else if (k === "scale") o.scale = n[0];
  }
  return o;
}

// Flatten a block's children into entities: {name, cls, prefab, ...props, comps, kids:[entity]}.
export function entitiesOf(block, inherit = {}) {
  const out = [];
  let idx = 0;
  for (const k of block.kids) {
    if (!isEntityHead(k.head)) continue;
    const m = HEAD_RE.exec(k.head);
    if (!m) continue;
    const [, grp, a, b, ref] = m;
    if (grp) {
      const cls = a, prefab = ref || null;
      for (const c of k.kids) {
        // `$grp Class "ref" { {instance} Name {instance} }`
        const cm = HEAD_RE.exec(c.head);
        const name = c.head === "" ? "" : (cm && cm[2]) || "";
        out.push(mk(c, name, cls, prefab, ++idx));
      }
      continue;
    }
    // `Class Name : "ref"`, `Class : "ref"`, `Class Name`, bare `{` (child of an entity), or `Name` (in a $grp)
    let name = "", cls = inherit.cls || "GenericEntity";
    if (k.head === "") name = "";
    else if (b) { cls = a; name = b; }
    else if (ref) cls = a || cls;
    else if (a && inherit.grp) name = a;
    else if (a) { cls = a; }
    out.push(mk(k, name, cls, ref || null, ++idx));
  }
  return out;
}

function mk(b, name, cls, prefab, idx) {
  const comps = b.kids.find((x) => x.head === "components");
  const nested = b.kids.filter((x) => x.head === "" || / : "/.test(x.head) || /^\$grp/.test(x.head));
  return {
    name, cls, prefab, idx, ...propsOf(b),
    comps: comps ? comps.kids : [],
    kids: entitiesOf({ kids: nested }).flatMap((c) => (c.name === "" && !c.prefab && c.kids.length ? c.kids : [c])),
    flags: (b.props.find((p) => p.startsWith("Flags ")) || "").slice(6),
  };
}

// ---------- config: referenced, not copied ----------
// scalar single-line properties of a component are the instance's own overrides (kept); every nested block
// (stock lines, shop items, action contexts) is named and stays in the source.
export function configOf(comps) {
  const out = [];
  for (const c of comps) {
    const cls = c.head.split(/\s+/)[0];
    const set = {};
    for (const p of c.props) {
      const i = p.search(/\s/);
      if (i < 0) continue;
      set[p.slice(0, i)] = p.slice(i + 1).trim();
    }
    const blocks = c.kids.map((x) => x.head.split(/\s+/)[0]);
    if (Object.keys(set).length || blocks.length) out.push({ component: cls, set, blocks });
  }
  return out;
}

// ---------- geometry ----------
const rad = (d) => (d * Math.PI) / 180;
const norm = (d) => { let x = ((d % 360) + 360) % 360; if (x > 180) x -= 360; return x; };
// Enfusion yaw: local forward (0,0,1) turned by yaw lands on (sin, cos).
export const toWorld = (lx, lz, yaw) => [lx * Math.cos(rad(yaw)) + lz * Math.sin(rad(yaw)), -lx * Math.sin(rad(yaw)) + lz * Math.cos(rad(yaw))];
export const toLocal = (wx, wz, yaw) => toWorld(wx, wz, -yaw);

function glob(pat) {
  return new RegExp("^" + pat.split("*").map((s) => s.replace(/[.+?^${}()|[\]\\]/g, "\\$&")).join(".*") + "$");
}
const globs = (s) => String(s || "").split(",").filter(Boolean).map(glob);

// ---------- role, verb ----------
export function roleOf(e) {
  const p = e.prefab || "";
  if (/NpcBaseEntity|ChimeraCharacter/.test(e.cls) || /\/Characters\/|Medic\/LF_Downed/i.test(p)) return "npc";
  if (/Sign/i.test(p)) return "sign";
  if (/Till|Cashier|Register|(^|[/_])ATM([_./]|$)/i.test(p)) return "till";
  if (/Counter|Desk/i.test(p)) return "counter";
  if (/Board/i.test(p)) return "board";
  if (/Light|Lamp/i.test(p)) return "light";
  if (/ElectricityBox/i.test(p) || e.comps.some((c) => /AlarmMount/.test(c.head))) return "alarm";
  if (/furniture/i.test(basename(p))) return "furniture"; // a building's whole furniture prefab, not a bed or a shelf
  if (e.comps.some((c) => /Action|Store|Shop|Place/.test(c.head)) || /Locker|Vault|Store|Safe|Armory|Payphone/i.test(p)) return "interactable";
  return "dressing";
}

const slug = (s) => String(s).replace(/([a-z0-9])([A-Z])/g, "$1-$2").replace(/[^A-Za-z0-9]+/g, "-").replace(/^-|-$/g, "").toLowerCase();

// ---------- extract ----------
function readText(p) { return readFileSync(p, "utf8"); }
function findOrigin(spec, defaultEntities, srcPath) {
  const m = /^(?:(.+\.layer)::)?(.+)$/.exec(spec);
  if (/^-?[\d.]+,-?[\d.]+,-?[\d.]+$/.test(spec)) { const [x, z, yaw] = spec.split(",").map(Number); return { coords: [x, 0, z], yaw, label: spec }; }
  let ents = defaultEntities;
  if (m[1]) ents = entitiesOf(parseBlocks(readText(locate(m[1], srcPath))));
  const want = m[2];
  const hit = ents.find((e) => (want.startsWith("prefab:") ? (e.prefab || "").includes(want.slice(7)) : e.name === want));
  if (!hit) throw new Error(`origin ${spec}: no such entity`);
  return { coords: hit.coords, yaw: hit.yaw, label: spec };
}

export function extractKit(srcPath, group, opts = {}) {
  opts = { with: [], ...opts };
  const srcList = srcPath.split(",");
  srcPath = srcList[0];
  const text = readText(srcPath);
  const isPrefab = srcPath.endsWith(".et");
  const tree = parseBlocks(text);
  let ents, origin, storeCfg = [];
  const also = new Set();
  let building = null;
  if (isPrefab) {
    const root = entitiesOf(tree)[0];
    if (!root) throw new Error(`${srcPath}: no root entity`);
    storeCfg = configOf(root.comps);
    const flat = (r) => r.kids.flatMap((e) => (e.name === "" && !e.prefab && e.kids.length ? e.kids : [e]));
    ents = flat(root);
    // the building prefabs this store inherits carry the shelves and the street sign: lift them too
    let up = root.prefab, hops = 0;
    while (up && hops++ < 4) {
      const m = REF_RE.exec(up);
      const f = m && ["LifeFramework", "LF_Everon1989"].map((a) => join(ROOT, "addons", a, m[2])).find((q) => existsSync(q));
      if (!f) break;
      const pr = entitiesOf(parseBlocks(readText(f)))[0];
      if (!pr) break;
      ents = [...flat(pr).map((e) => ({ ...e, from: basename(f) })), ...ents];
      also.add(relFromRoot(f));
      up = pr.prefab;
    }
    building = { mode: "override", via: root.prefab, vanilla: up || null };
    // the building frame is the prefab itself: offsets are already relative
    origin = { coords: [0, 0, 0], yaw: 0, label: "prefab" };
    // a store prefab also nests its children one block deeper (`{ { Generic... } }`)
    ents = ents.length ? ents : root.kids;
  } else {
    ents = [...entitiesOf(tree), ...srcList.slice(1).flatMap((f) => entitiesOf(parseBlocks(readText(f))))];
    for (const f of srcList.slice(1)) also.add(relFromRoot(f));
  }
  const extra = [];
  if (isPrefab && opts.with.length) {
    // clerk and other layer rows beside the store, taken relative to the store's building entity (offsets keep the layer's y)
    const [bf, bn] = String(opts.building || "").split("::");
    if (!bn) throw new Error("--with needs --building <layer>::<EntityName>");
    const bEnts = entitiesOf(parseBlocks(readText(locate(bf, srcPath))));
    const b = bEnts.find((e) => e.name === bn);
    if (!b) throw new Error("building " + bn + " not found in " + bf);
    for (const w of opts.with) {
      const [wf, wg] = w.split("::");
      const wents = entitiesOf(parseBlocks(readText(locate(wf, srcPath)))).filter((e) => globs(wg).some((g) => g.test(e.name)));
      for (const e of wents) {
        const [lx, lz] = toLocal(e.coords[0] - b.coords[0], e.coords[2] - b.coords[2], b.yaw);
        also.add(relFromRoot(locate(wf, srcPath)));
        extra.push({ ...e, coords: [lx, e.coords[1] - b.coords[1], lz], yaw: norm(e.yaw - b.yaw), from: wf });
      }
    }
    ents = [...ents, ...extra];
  }
  const inc = globs(group), exc = globs(opts.exclude);
  const named = (e) => e.name || (e.prefab ? slug(basename(e.prefab, ".et")) : "entity");
  let picked = ents.filter((e) => (group === "*" || e.from || inc.some((g) => g.test(e.name))) && !exc.some((g) => g.test(e.name)) && !(opts.drop || []).includes(e.prefab ? slug(basename(e.prefab, ".et")) : e.name));
  if (isPrefab) picked = picked.filter((e) => (e.prefab || e.from) && e.prefab && !/BuildingParts\/(Windows|Doors|Entry)\//.test(e.prefab));
  if (!picked.length) throw new Error(`${srcPath}: group "${group}" matches no entity`);
  if (!isPrefab) origin = findOrigin(opts.origin || "centroid", ents, srcPath);
  if (!isPrefab && opts.origin === undefined) {
    const cx = picked.reduce((s, e) => s + e.coords[0], 0) / picked.length, cz = picked.reduce((s, e) => s + e.coords[2], 0) / picked.length;
    origin = { coords: [cx, 0, cz], yaw: 0, label: "centroid" };
  }
  const roleOv = Object.fromEntries((opts.roles || []).map((s) => s.split("=")));
  const verbOv = Object.fromEntries((opts.verbs || []).map((s) => s.split("=")));
  // a layer kit is rebased to its lowest row; a prefab kit keeps the building origin as y 0
  const floorY = isPrefab ? 0 : Math.min(...picked.map((e) => e.coords[1]));
  const interior = picked.filter((e) => ["npc","till","counter"].includes(roleOf(e)));
  const floorH = isPrefab && interior.length ? Math.min(...interior.map((e) => e.coords[1])) : 0;
  const ids = new Set();
  const rows = picked.map((e) => {
    const [lx, lz] = toLocal(e.coords[0] - origin.coords[0], e.coords[2] - origin.coords[2], origin.yaw);
    let id = slug(named(e).replace(/^(Roadmap|Shop|Grid|Annex)_/, "")) || "row";
    if (ids.has(id)) { let i = 2; while (ids.has(`${id}-${i}`)) i++; id = `${id}-${i}`; }
    ids.add(id);
    const prefab = e.prefab;
    const row = { id, name: e.name || undefined, prefab, role: roleOv[id] || roleOv[e.name] || roleOf(e), verb: "on floor",
      offset: [r3(lx), r3(e.coords[1] - floorY), r3(lz)], yaw: r3(norm(e.yaw - origin.yaw)) };
    if (e.pitch) row.pitch = r3(e.pitch);
    if (e.roll) row.roll = r3(e.roll);
    if (e.scale !== 1) row.scale = e.scale;
    if (e.cls && e.cls !== "GenericEntity") row.class = e.cls;
    if (e.from) row.from = basename(e.from);
    const config = configOf(e.comps);
    if (config.length) row.config = config;
    if (e.kids.length) row.children = e.kids.map((c) => ({ prefab: c.prefab, offset: c.coords.map(r3), yaw: r3(c.yaw), ...(c.comps.length ? { config: configOf(c.comps) } : {}) }));
    row._v = verbOv[id] || verbOv[e.name];
    return row;
  });
  // footprint: the store's own boundary when it names one, else the rows' box on the ground floor
  const sc = storeCfg.find((c) => c.set.m_vBoundaryCenter);
  let fp;
  if (sc) {
    const [cx, , cz] = sc.set.m_vBoundaryCenter.split(/\s+/).map(Number), [hx, , hz] = sc.set.m_vBoundaryHalf.split(/\s+/).map(Number);
    fp = { source: "store-boundary", min: [r3(cx - hx), r3(cz - hz)], max: [r3(cx + hx), r3(cz + hz)] };
  } else {
    const g = rows.filter((r) => r.offset[1] < 2.5);
    fp = { source: "rows", min: [r3(Math.min(...g.map((r) => r.offset[0]))), r3(Math.min(...g.map((r) => r.offset[2])))], max: [r3(Math.max(...g.map((r) => r.offset[0]))), r3(Math.max(...g.map((r) => r.offset[2])))] };
  }
  const sides = (x, z) => [["-x", x - fp.min[0]], ["+x", fp.max[0] - x], ["-z", z - fp.min[1]], ["+z", fp.max[1] - z]].sort((a, b) => a[1] - b[1]);
  for (const r of rows) {
    const [side, gap] = sides(r.offset[0], r.offset[2])[0];
    const supports = rows.filter((o) => o !== r && ["counter", "furniture", "dressing"].includes(o.role) && o.role !== "furniture" && Math.hypot(o.offset[0] - r.offset[0], o.offset[2] - r.offset[2]) < 1.2 && r.offset[1] > o.offset[1] + 0.3);
    if (r._v) r.verb = r._v;
    else if (r.role === "sign") r.verb = "against wall";
    else if (supports.length) r.verb = `on ${supports[0].id}`;
    else if (r.offset[1] - floorH > 1.5) r.verb = "against wall";
    else if (r.role === "furniture") r.verb = "on floor";
    else if (gap <= 1.2 && r.role !== "npc") r.verb = "against wall";
    if (r.verb === "against wall" || r.verb === "on floor") r.wall = gap <= 1.2 ? side : "free";
    delete r._v;
  }
  // storeys: a whole-building kit has rows on more than one floor; `all` is a furniture prefab that spans them
  for (const r of rows) r.floor = r.role === "furniture" ? "all" : "f" + Math.max(0, Math.floor((r.offset[1] - floorH + 0.4) / 3));
  const floors = [...new Set(rows.map((r) => r.floor).filter((f) => f !== "all"))].sort().map((id) => ({ id, y: r3(floorH + 3 * Number(id.slice(1))) }));
  if (!floors.length) floors.push({ id: "f0", y: r3(floorH) });
  const counters = rows.filter((r) => r.role === "counter" || r.role === "till").map((r) => { const [s, g] = sides(r.offset[0], r.offset[2])[0]; return { id: r.id, wall: g <= 1.5 ? s : "free", edgeGap: r3(g) }; });
  const sign = rows.find((r) => r.role === "sign");
  let front = null;
  if (sign) { const [s] = sides(sign.offset[0], sign.offset[2])[0]; front = s; }
  const kit = {
    schema: 1, kit: opts.kit || slug(basename(srcPath, extOf(srcPath))), system: opts.system || "",
    source: { file: relFromRoot(srcPath), group, origin: origin.label, frame: isPrefab ? "building-local" : "origin-entity", ...(also.size ? { also: [...also] } : {}) },
    scope: opts.scope || (isPrefab ? "building" : "room"),
    ...(isPrefab ? { building } : opts.inside ? { building: { mode: "inside", vanilla: opts.inside } } : {}),
    ...(isPrefab ? { store: { prefab: opts.storeRef || null, config: storeCfg.map((c) => ({ component: c.component, set: c.set, blocks: c.blocks })) } } : {}),
    anchoring: {
      floorY: r3(floorH), floors, footprint: fp, front, counters,
      adapt: "Counter rows keep `wall`: put that wall of the kit against the building wall named by the dossier (counter wall = longest inner wall). `front` is the street side: turn the kit so it faces the entrance. Rows are rigid: shift, never stretch.",
    },
    rows,
  };
  return kit;
}

const locate = (p, base) => (existsSync(resolve(p)) ? resolve(p) : resolve(dirname(base), p));
const extOf = (p) => (p.endsWith(".et") ? ".et" : ".layer");
function relFromRoot(p) { const a = resolve(p).replaceAll("\\", "/"), b = ROOT.replaceAll("\\", "/") + "/"; return a.startsWith(b) ? a.slice(b.length) : a; }

// ---------- apply / check ----------
// Absolute rows for a building anchor: what `town fit` feeds the place tool. floorY is the dossier's floor.
export function applyKit(kit, x, z, yaw, floorY = 0) {
  return kit.rows.map((r) => {
    const [wx, wz] = toWorld(r.offset[0], r.offset[2], yaw);
    return { id: r.id, prefab: r.prefab, role: r.role, verb: r.verb, coords: [r3(x + wx), r3(floorY + r.offset[1]), r3(z + wz)], yaw: r3(norm(r.yaw + yaw)) };
  });
}

const ROLES = new Set(["counter", "till", "npc", "board", "dressing", "sign", "light", "alarm", "interactable", "furniture"]);
export function checkKit(kit, { root = ROOT } = {}) {
  const errs = [];
  const bad = (m) => errs.push(`${kit.kit || "?"}: ${m}`);
  if (kit.schema !== 1) bad("schema must be 1");
  if (!["counter", "room", "building"].includes(kit.scope)) bad("scope must be counter, room or building");
  if (kit.building && !["override", "inside"].includes(kit.building.mode)) bad("building.mode must be override or inside");
  const floorIds = new Set((kit.anchoring?.floors || []).map((f) => f.id));
  if (!floorIds.size) bad("anchoring.floors is empty");
  for (const r of kit.rows || []) if (r.floor !== "all" && !floorIds.has(r.floor)) bad(`${r.id}: floor ${r.floor} is not in anchoring.floors`);
  if (!kit.kit || !/^[a-z0-9-]+$/.test(kit.kit)) bad("kit name must be a slug");
  if (!kit.source?.file || !existsSync(join(root, kit.source.file))) bad(`source file ${kit.source?.file} is missing`);
  const srcText = [kit.source?.file, ...(kit.source?.also || [])].filter((f) => f && existsSync(join(root, f))).map((f) => readText(join(root, f))).join("\n");
  if (!Array.isArray(kit.rows) || !kit.rows.length) bad("no rows");
  const ids = new Set();
  const refsAll = [];
  const walk = (rows) => { for (const r of rows) { refsAll.push(r.prefab); if (r.children) walk(r.children); } };
  walk(kit.rows || []);
  for (const r of kit.rows || []) {
    if (ids.has(r.id)) bad(`duplicate row id ${r.id}`);
    ids.add(r.id);
    if (!ROLES.has(r.role)) bad(`${r.id}: unknown role ${r.role}`);
    if (!Array.isArray(r.offset) || r.offset.length !== 3 || r.offset.some((n) => !Number.isFinite(n))) bad(`${r.id}: offset must be 3 numbers`);
    if (r.coords) bad(`${r.id}: carries absolute coords`);
    if (!/^(on floor|against wall|on [a-z0-9-]+)$/.test(r.verb || "")) bad(`${r.id}: bad verb ${r.verb}`);
    else if (r.verb.startsWith("on ") && r.verb !== "on floor" && !(kit.rows.some((o) => o.id === r.verb.slice(3)))) bad(`${r.id}: verb names row ${r.verb.slice(3)} which is not in the kit`);
  }
  for (const ref of new Set(refsAll.filter(Boolean))) {
    const m = REF_RE.exec(ref);
    if (!m) { bad(`prefab ${ref} is not {GUID}path`); continue; }
    const [, guid, path] = m;
    const f = ["LifeFramework", "LF_Everon1989"].map((a) => join(root, "addons", a, path)).find((p) => existsSync(p + ".meta"));
    if (f) { const mm = /guid\s*"?\{?([0-9A-Fa-f]{16})/i.exec(readText(f + ".meta")) || /\{([0-9A-Fa-f]{16})\}/.exec(readText(f + ".meta")); if (!mm || mm[1].toUpperCase() !== guid.toUpperCase()) bad(`prefab ${path}: GUID ${guid} is not the .meta GUID`); }
    else if (!srcText.includes(`{${guid}}${path}`) && !srcText.includes(`{${guid.toUpperCase()}}${path}`)) bad(`prefab ${path}: vanilla, and the source does not use {${guid}}`);
  }
  if (kit.source?.frame === "origin-entity" || kit.source?.file?.endsWith(".layer")) {
    // no absolute coordinate left: no row sits farther from the origin than a building can be, and config scalars hold no world vector
    const farthest = Math.max(0, ...(kit.rows || []).map((r) => Math.hypot(r.offset?.[0] || 0, r.offset?.[2] || 0)));
    if (farthest > 80) bad(`a row is ${r3(farthest)} m from the origin: an absolute coordinate was left in`);
  }
  return errs;
}

export function leakedWorldVector(kit, absCoords) {
  // a config scalar holding a 3-vector within 5 cm of a source world position is a leaked absolute coordinate
  const out = [];
  for (const r of kit.rows) for (const c of r.config || []) for (const [k, v] of Object.entries(c.set)) {
    const n = v.split(/\s+/).map(Number);
    if (n.length === 3 && n.every(Number.isFinite) && absCoords.some((a) => Math.abs(a[0] - n[0]) < 0.05 && Math.abs(a[2] - n[2]) < 0.05)) out.push(`${r.id}.${c.component}.${k}`);
  }
  return out;
}

export function loadKits(dir = KITS_DIR) {
  if (!existsSync(dir)) return [];
  return readdirSync(dir).filter((f) => f.endsWith(".json")).sort().map((f) => ({ file: join(dir, f), kit: JSON.parse(readText(join(dir, f))) }));
}

// ---------- CLI ----------
function parseArgs(argv) {
  const o = { _: [], roles: [], verbs: [], with: [] };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--role") o.roles.push(argv[++i]);
    else if (a === "--verb") o.verbs.push(argv[++i]);
    else if (a === "--with") o.with.push(argv[++i]);
    else if (a.startsWith("--")) o[a.slice(2)] = argv[++i];
    else o._.push(a);
  }
  return o;
}

export function main(argv) {
  const [cmd, ...rest] = argv;
  const a = parseArgs(rest);
  if (cmd === "extract") {
    const [src, group] = a._;
    if (!src || !group || !a.kit) { console.error("usage: kit extract <source> <group> --kit <name> [--origin spec] [--system s] [--exclude g] [--role id=role] [--verb id=verb] [--store {GUID}path]"); return 2; }
    const srcAbs = src.split(",").map((f) => resolve(f)).join(",");
    if (!a.store && srcAbs.endsWith(".et") && existsSync(srcAbs + ".meta")) { const mm = /Name "(\{[0-9A-F]{16}\}[^"]+)"/.exec(readText(srcAbs + ".meta")); if (mm) a.store = mm[1]; }
    const kit = extractKit(srcAbs, group, { kit: a.kit, system: a.system, origin: a.origin, exclude: a.exclude, roles: a.roles, verbs: a.verbs, drop: (a.drop || "").split(",").filter(Boolean), storeRef: a.store, scope: a.scope, inside: a.inside, with: a.with, building: a.building });
    const errs = checkKit(kit);
    if (errs.length) { console.error(errs.join("\n")); return 1; }
    const out = resolve(a.out || join(KITS_DIR, `${kit.kit}.json`));
    mkdirSync(dirname(out), { recursive: true });
    writeFileSync(out, JSON.stringify(kit, null, 2) + "\n");
    console.log(`kit ${kit.kit}: ${kit.rows.length} rows -> ${relFromRoot(out)}`);
    return 0;
  }
  if (cmd === "check") {
    const files = a._.length ? a._.map((f) => ({ file: f, kit: JSON.parse(readText(f)) })) : loadKits();
    const errs = files.flatMap(({ kit }) => checkKit(kit));
    for (const e of errs) console.error(e);
    console.log(`SUMMARY town-kit check ${errs.length ? "fail" : "ok"} kits=${files.length}`);
    return errs.length ? 1 : 0;
  }
  if (cmd === "apply") {
    const [file, x, z, yaw, fy] = a._;
    console.log(JSON.stringify(applyKit(JSON.parse(readText(file)), +x, +z, +yaw, +(fy || 0)), null, 1));
    return 0;
  }
  console.error("usage: kit extract|check|apply (see the header of tools/town/kit.mjs)");
  return 2;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(main(process.argv.slice(2)));
