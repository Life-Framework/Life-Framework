// tools\cli perf building <slug|prefab.et> [--size small|medium|large] [--json]
//
// Counts what a fitted building puts in the world, from its prefab files, and checks the counts against
// the building budget (docs/architecture/performance.md "The budget"). A slug is a folder under
// addons/*/Prefabs/Buildings/ (GunStore, Supermarket, ...): every .et in it that no other prefab of the
// folder references is a root. The walk follows child entities and the prefabs they name: repo prefabs
// from addons/, vanilla prefabs from the game paks when the enfusion-mcp build is present (otherwise
// they are listed as unresolved and their insides are not counted).
//
// Counted per root and in total:
//   entities   every entity block (a building, a child, a prefab's own children), an upper bound: an
//              override of an inherited child counts again
//   lights     LightEntity blocks
//   shadow     lights with CastShadow 1, plus lights that leave CastShadow unset (vanilla lamps that leave
//              it unset report IsCastShadow() true in game, so unset is counted as a caster)
//   unset      lights with no CastShadow line: write it, either way
//   point      point lights casting shadows (six shadow maps each; the budget allows none)
//   frame      script classes in the tree with EOnFrame / SetEventMask(...FRAME): none here today, the
//              runtime count (`perfstress count`) is the check for scripts
// Dynamic physics and replication are runtime facts: `perfstress count <LayerName> <r>` in a session.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { basename, dirname, join, relative, resolve as resolvePath } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

// The building budget (docs/architecture/performance.md "The budget"). Keep the two in step.
export const BUDGET = {
  small: { entities: 150, lights: 3, shadow: 0, point: 0 },
  medium: { entities: 300, lights: 6, shadow: 1, point: 0 },
  large: { entities: 500, lights: 10, shadow: 2, point: 0 },
};

const HEADER = /^\s*(?:\$grp\s+)?(\w*Entity)\b(?:\s+(\w+))?\s*(?::\s*"(\{[0-9A-F]{16}\}[^"]+)")?\s*\{\s*$/;

// Blocks of one .et text: [{ cls, ref, lines: [own property lines], children: [...] }] for entity blocks.
// Component and property blocks are kept as plain nesting (their lines are not an entity's own lines).
export function parseEntities(text) {
  const root = { cls: "(file)", ref: null, lines: [], children: [], entity: true };
  const stack = [root];
  for (const raw of text.split("\n")) {
    const line = raw.replace(/"[^"]*"/g, (m) => (HEADER.test(raw) ? m : '""'));
    const h = HEADER.exec(raw);
    const opens = (line.match(/\{/g) ?? []).length;
    const closes = (line.match(/\}/g) ?? []).length;
    const top = stack[stack.length - 1];
    if (h) {
      const node = { cls: h[1], name: h[2] ?? "", ref: h[3] ?? null, lines: [], children: [], entity: true };
      nearestEntity(stack).children.push(node);
      stack.push(node);
      continue;
    }
    if (top.entity) top.lines.push(raw.trim());
    for (let i = 0; i < opens; i++) stack.push({ entity: false, children: [] });
    for (let i = 0; i < closes && stack.length > 1; i++) stack.pop();
  }
  return root.children;
}

function nearestEntity(stack) {
  for (let i = stack.length - 1; i >= 0; i--) if (stack[i].entity) return stack[i];
  return stack[0];
}

// A `{GUID}path` reference to text: repo addons first, then the game paks through the enfusion-mcp VFS.
export async function makeResolver(root = ROOT) {
  const addons = join(root, "addons");
  const roots = existsSync(addons) ? readdirSync(addons).map((a) => join(addons, a)).filter((p) => statSync(p).isDirectory()) : [];
  let vfs = null;
  // The MCP build is gitignored: a worktree finds it in the main checkout (git's common dir's parent).
  const candidates = [join(root, "tools", "mcp", "enfusion-mcp-bk", "dist")];
  const common = spawnSync("git", ["rev-parse", "--git-common-dir"], { cwd: root, encoding: "utf8", windowsHide: true }).stdout?.trim();
  if (common) candidates.push(join(resolvePath(root, common), "..", "tools", "mcp", "enfusion-mcp-bk", "dist"));
  const dist = candidates.find((d) => existsSync(join(d, "pak", "vfs.js")));
  if (dist) {
    try {
      const { PakVirtualFS } = await import(pathToFileURL(join(dist, "pak", "vfs.js")).href);
      const { loadConfig } = await import(pathToFileURL(join(dist, "config.js")).href);
      const quiet = console.error;
      console.error = () => {};
      try {
        vfs = PakVirtualFS.get(loadConfig().gamePath);
      } finally {
        console.error = quiet;
      }
    } catch {
      vfs = null;
    }
  }
  return (ref) => {
    const path = ref.replace(/^\{[0-9A-F]{16}\}/, "");
    for (const r of roots) {
      const f = join(r, path);
      if (existsSync(f)) return { text: readFileSync(f, "utf8"), where: "repo" };
    }
    if (vfs && vfs.exists(path)) {
      try {
        return { text: vfs.readTextFile(path), where: "game" };
      } catch {
        return null;
      }
    }
    return null;
  };
}

const prop = (lines, key) => {
  const m = lines.map((l) => new RegExp(`^${key}\\s+(.+)$`).exec(l)).find(Boolean);
  return m ? m[1].trim() : null;
};

// Totals for a list of entity blocks, following prefab references (a reference's own root block is the
// referencing entity, so only its insides are added). depth guards a reference loop.
export function countTree(nodes, resolve, seen = new Map(), depth = 0) {
  const t = { entities: 0, lights: 0, shadow: 0, unset: 0, point: 0, unresolved: new Set() };
  const add = (o) => {
    for (const k of ["entities", "lights", "shadow", "unset", "point"]) t[k] += o[k];
    for (const u of o.unresolved) t.unresolved.add(u);
  };
  for (const n of nodes) {
    t.entities++;
    if (n.cls === "LightEntity") {
      t.lights++;
      const cast = prop(n.lines, "CastShadow");
      const type = prop(n.lines, "Type") ?? "Point";
      if (cast === null) t.unset++;
      if (cast !== "0") {
        t.shadow++;
        if (/^point$/i.test(type)) t.point++;
      }
    }
    if (n.ref && depth < 12) add(refInsides(n.ref, resolve, seen, depth));
    add(countTree(n.children, resolve, seen, depth));
  }
  return t;
}

function refInsides(ref, resolve, seen, depth) {
  if (seen.has(ref)) return seen.get(ref);
  const empty = { entities: 0, lights: 0, shadow: 0, unset: 0, point: 0, unresolved: new Set() };
  seen.set(ref, empty);
  const got = resolve(ref);
  if (!got) {
    empty.unresolved.add(ref);
    return empty;
  }
  const blocks = parseEntities(got.text);
  // The file's own root block is the referencing entity: count its parent chain's insides and children,
  // and a root that is itself a light (a vanilla `_on` lamp is a mesh entity with LightEntity children).
  const out = { entities: 0, lights: 0, shadow: 0, unset: 0, point: 0, unresolved: new Set() };
  for (const b of blocks) {
    if (b.ref) {
      const up = refInsides(b.ref, resolve, seen, depth + 1);
      for (const k of ["entities", "lights", "shadow", "unset", "point"]) out[k] += up[k];
      for (const u of up.unresolved) out.unresolved.add(u);
    }
    const inner = countTree(b.children, resolve, seen, depth + 1);
    for (const k of ["entities", "lights", "shadow", "unset", "point"]) out[k] += inner[k];
    for (const u of inner.unresolved) out.unresolved.add(u);
  }
  seen.set(ref, out);
  return out;
}

// The building's root prefabs: .et files in the folder that no other .et in the folder references.
export function buildingRoots(dir) {
  const files = [];
  const walk = (d) => {
    for (const n of readdirSync(d)) {
      const p = join(d, n);
      if (statSync(p).isDirectory()) walk(p);
      else if (n.endsWith(".et")) files.push(p);
    }
  };
  walk(dir);
  const texts = new Map(files.map((f) => [f, readFileSync(f, "utf8")]));
  const referenced = (f) => {
    const tail = relative(join(dir, "..", "..", ".."), f).replace(/\\/g, "/");
    return [...texts].some(([g, t]) => g !== f && t.includes(`}${tail}"`));
  };
  return files.filter((f) => !referenced(f));
}

export function findBuilding(slug, root = ROOT) {
  if (slug.endsWith(".et")) {
    const f = existsSync(slug) ? slug : join(root, slug);
    return existsSync(f) ? { files: [f] } : null;
  }
  const addons = join(root, "addons");
  for (const a of readdirSync(addons)) {
    const b = join(addons, a, "Prefabs", "Buildings");
    if (!existsSync(b)) continue;
    const hit = readdirSync(b).find((n) => n.toLowerCase() === slug.toLowerCase());
    if (hit) return { files: buildingRoots(join(b, hit)) };
  }
  return null;
}

export function verdict(t, size) {
  const b = BUDGET[size];
  const over = [];
  if (t.entities > b.entities) over.push(`entities ${t.entities} > ${b.entities}`);
  if (t.lights > b.lights) over.push(`lights ${t.lights} > ${b.lights}`);
  if (t.shadow > b.shadow) over.push(`shadow casters ${t.shadow} > ${b.shadow}`);
  if (t.point > b.point) over.push(`point lights casting shadows ${t.point} > ${b.point}`);
  if (t.unset > 0) over.push(`${t.unset} light(s) leave CastShadow unset (write CastShadow 0 or 1)`);
  return over;
}

export async function runPerfBuilding(args, root = ROOT) {
  const slug = args.find((a) => !a.startsWith("--") && args[args.indexOf(a) - 1] !== "--size");
  const si = args.indexOf("--size");
  const size = si === -1 ? "medium" : args[si + 1];
  if (!slug || !BUDGET[size]) {
    console.log("usage: tools\\cli perf building <slug|prefab.et> [--size small|medium|large] [--json]");
    return 2;
  }
  const found = findBuilding(slug, root);
  if (!found || !found.files.length) {
    console.log(`perf building: no building '${slug}' under addons/*/Prefabs/Buildings/ (or no such .et)`);
    return 2;
  }
  const resolve = await makeResolver(root);
  const total = { entities: 0, lights: 0, shadow: 0, unset: 0, point: 0, unresolved: new Set() };
  const rows = [];
  for (const f of found.files) {
    const t = countTree(parseEntities(readFileSync(f, "utf8")), resolve);
    rows.push({ file: relative(root, f).replace(/\\/g, "/"), ...t, unresolved: [...t.unresolved] });
    for (const k of ["entities", "lights", "shadow", "unset", "point"]) total[k] += t[k];
    for (const u of t.unresolved) total.unresolved.add(u);
  }
  const over = verdict(total, size);
  const result = { slug, size, budget: BUDGET[size], total: { ...total, unresolved: [...total.unresolved] }, roots: rows, over, ok: over.length === 0 };
  if (args.includes("--json")) {
    console.log(JSON.stringify(result, null, 2));
  } else {
    for (const r of rows) console.log(`${r.file}: entities ${r.entities}, lights ${r.lights} (shadow ${r.shadow}, unset ${r.unset}, point-shadow ${r.point})`);
    if (total.unresolved.size) console.log(`unresolved (not counted inside): ${[...total.unresolved].slice(0, 5).join(", ")}${total.unresolved.size > 5 ? ` +${total.unresolved.size - 5} more` : ""}`);
    for (const o of over) console.log(`OVER  ${o}`);
    console.log(`SUMMARY perf-building ${over.length ? "fail" : "ok"} ${slug} size=${size} entities=${total.entities}/${BUDGET[size].entities} lights=${total.lights}/${BUDGET[size].lights} shadow=${total.shadow}/${BUDGET[size].shadow}`);
  }
  return over.length ? 1 : 0;
}

if (process.argv[1] && basename(process.argv[1]) === "building-count.mjs") process.exit(await runPerfBuilding(process.argv.slice(2)));
