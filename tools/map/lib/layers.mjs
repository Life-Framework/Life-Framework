// tools/map/lib/layers.mjs - a reader for the world `.layer` files and the prefabs they instance, enough to
// answer "which entities carry which component, with which property values, and where do they stand".
// No game, no Workbench. Used by tools/map/lib/places.mjs to find the live places of a world (card follow-up
// to M0/M2: the POI table is per place kind and holds no coordinates, so positions come from the layers).
//
// Layer grammar read here (Enfusion text serialisation):
//   GenericEntity Name : "{GUID}Prefabs/x.et" {      an entity (prefab optional, name optional)
//   $grp GenericEntity : "{GUID}Prefabs/x.et" {      a group: every `Name {` inside shares the class and prefab
//    components { LF_Cls "{guid}" { m_sKind "x" } }  component properties (scalar lines only)
//    coords x y z    angles p y r    angleY y        placement; a nested entity is relative to its parent
//    { GenericEntity Child { ... } }                 a bare `{` opens the child entities
// A prefab path is resolved under the addon folders; its ancestry is followed (`: "{GUID}parent.et"`) and the
// components are merged by class name, the most derived value winning. Vanilla prefabs do not resolve (they
// live in the game paks) and contribute nothing: no LF place component lives on one.
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";

const ADDON_DIRS = ["addons/LifeFramework", "addons/LF_Everon1989"];
const HEADER = /^(\$grp\s+)?([A-Za-z_][\w]*)(?:\s+([A-Za-z_][\w$.-]*))?(?:\s*:\s*"([^"]*)")?$/;

const unquote = (v) => {
  const s = v.trim();
  return s.startsWith('"') && s.lastIndexOf('"') > 0 ? s.slice(1, s.lastIndexOf('"')) : s;
};

// "{GUID}Prefabs/x.et" -> "Prefabs/x.et"
export const prefabPath = (ref) => (ref || "").replace(/^\{[0-9A-Fa-f]+\}/, "");

/** Parses layer or prefab text into a flat list of entities: { name, cls, prefab, local, yaw, parent, comps }. */
export function parseLayer(text) {
  const out = [];
  const stack = [{ t: "root" }];
  for (const raw of text.split("\n")) {
    const line = raw.replace(/\/\/.*$/, "").trim();
    if (!line) continue;
    const top = stack[stack.length - 1];
    if (line === "}") {
      if (stack.length > 1) stack.pop();
      continue;
    }
    if (line.endsWith("{")) {
      const header = line.slice(0, -1).trim();
      if (top.t === "entity" && header === "") stack.push({ t: "children", parent: top.e });
      else if (top.t === "entity" && header === "components") stack.push({ t: "components", e: top.e });
      else if (top.t === "components") {
        const m = header.match(/^([A-Za-z_]\w*)(?:\s+"[^"]*")?$/);
        const props = {};
        if (m) top.e.comps[m[1]] = { __props: props };
        stack.push({ t: m ? "component" : "other", props, e: top.e });
      } else if (top.t === "group") {
        const e = newEntity(header, top.cls, top.prefab, top.parent);
        out.push(e);
        stack.push({ t: "entity", e, parent: top.parent });
      } else if (top.t === "root" || top.t === "children") {
        const m = header.match(HEADER);
        if (!m) {
          stack.push({ t: "other" });
        } else if (m[1]) {
          stack.push({ t: "group", cls: m[2], prefab: m[4] || "", parent: top.parent || null });
        } else {
          const e = newEntity(m[3] || "", m[2], m[4] || "", top.parent || null);
          out.push(e);
          stack.push({ t: "entity", e, parent: top.parent || null });
        }
      } else {
        stack.push({ t: "other" });
      }
      continue;
    }
    const kv = line.match(/^([A-Za-z_]\w*)\s+(.*)$/);
    if (!kv) continue;
    if (top.t === "component") top.props[kv[1]] = unquote(kv[2]);
    else if (top.t === "entity") {
      const nums = () => kv[2].trim().split(/\s+/).map(Number);
      if (kv[1] === "coords") top.e.local = nums();
      else if (kv[1] === "angles") top.e.yaw = nums()[1] || 0;
      else if (kv[1] === "angleY") top.e.yaw = nums()[0] || 0;
    }
  }
  // Flatten component props: comps[cls] = props.
  for (const e of out) for (const [c, v] of Object.entries(e.comps)) e.comps[c] = v.__props;
  return out;
}

function newEntity(name, cls, prefab, parent) {
  return { name, cls, prefab, local: [0, 0, 0], yaw: 0, parent, comps: {} };
}

/** World position (x, z) of a parsed entity: its own coords plus every parent's, turned by the parent's yaw. */
export function worldXZ(e) {
  let x = e.local[0], z = e.local[2];
  for (let p = e.parent; p; p = p.parent) {
    const a = (p.yaw * Math.PI) / 180, c = Math.cos(a), s = Math.sin(a);
    const rx = x * c + z * s, rz = -x * s + z * c;
    x = rx + p.local[0];
    z = rz + p.local[2];
  }
  return [x, z];
}

/** Resolves prefab components, caching by path. A prefab is its own components over its ancestor's. */
export function prefabResolver(root) {
  const cache = new Map();
  const find = (rel) => {
    for (const d of ADDON_DIRS) {
      const f = join(root, d, rel);
      if (existsSync(f)) return f;
    }
    return "";
  };
  const resolve = (ref, depth = 0) => {
    const rel = prefabPath(ref);
    if (!rel || depth > 10) return {};
    if (cache.has(rel)) return cache.get(rel);
    cache.set(rel, {});
    const f = find(rel);
    let merged = {};
    if (f) {
      const top = parseLayer(readFileSync(f, "utf8")).find((e) => e.parent === null);
      if (top) {
        merged = {};
        const base = resolve(top.prefab, depth + 1);
        for (const [c, p] of Object.entries(base)) merged[c] = { ...p };
        for (const [c, p] of Object.entries(top.comps)) merged[c] = { ...(merged[c] || {}), ...p };
      }
    }
    cache.set(rel, merged);
    return merged;
  };
  return resolve;
}

/** The `.layer` files of a folder (not recursive; a world keeps its layers flat). */
export function layerFiles(dir) {
  return existsSync(dir) ? readdirSync(dir).filter((n) => n.endsWith(".layer")).sort().map((n) => join(dir, n)) : [];
}

/** Entities of every layer file given, each with `comps` = prefab components under the layer's own, and `xz`. */
export function readLayers(root, files) {
  const resolve = prefabResolver(root);
  const all = [];
  for (const f of files) {
    for (const e of parseLayer(readFileSync(f, "utf8"))) {
      const comps = {};
      for (const [c, p] of Object.entries(resolve(e.prefab))) comps[c] = { ...p };
      for (const [c, p] of Object.entries(e.comps)) comps[c] = { ...(comps[c] || {}), ...p };
      all.push({ ...e, comps, xz: worldXZ(e), file: f });
    }
  }
  return all;
}
