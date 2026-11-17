// tools/lib/game-prefab.mjs - name a prefab any way an agent writes it and get the real thing: the
// resource path, its {GUID}path, the file text (addon or game pak), and the model it shows.
//
//   "{GUID}Prefabs/X.et"      as given
//   "Prefabs/X.et"            an addon file (any addons/<addon>/), else the game's: the GUID comes from a
//                             file that references it (see guidFor)
//   "ShelfShop_01"            the prefab of that name: addon first, then the game's Prefabs/ tree
//   "Assets/.../X.xob"        a model, no prefab
//
// The game's .et files carry no .meta in the paks, so a vanilla prefab's GUID is read from whatever
// references it: the game's editable twin (PrefabsEditable/Auto/.../E_<name>.et inherits from it),
// then our own layers, prefabs and configs, then an index of every {GUID}path.et in the
// game's .et and .conf files (about 10 s once per game build, kept in ~/.life-framework/cache/).
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
import { buildStamp, openVfs } from "./install-api.mjs";

const GUID_RE = /^\{([0-9A-Fa-f]{16})\}(.+)$/;
export const CACHE_DIR = () => process.env.LF_CACHE_DIR || join(homedir(), ".life-framework", "cache");

let vfsPromise = null;
export function gameVfs() {
  if (!vfsPromise) vfsPromise = openVfs().catch((e) => {
    vfsPromise = null;
    throw e;
  });
  return vfsPromise;
}

export function addonDirs(root) {
  const addons = join(root, "addons");
  if (!existsSync(addons)) return [];
  return readdirSync(addons, { withFileTypes: true }).filter((d) => d.isDirectory()).map((d) => join(addons, d.name));
}

// The addon file for a resource path, or null.
export function addonFile(root, rel) {
  for (const d of addonDirs(root)) {
    const f = join(d, ...rel.split("/"));
    if (existsSync(f) && statSync(f).isFile()) return f;
  }
  return null;
}

// Text of a resource: addon first, then the game. null when neither has it.
export function readText(root, vfs, rel) {
  const f = addonFile(root, rel);
  if (f) return readFileSync(f, "utf8");
  if (vfs?.exists(rel)) return vfs.readTextFile(rel);
  return null;
}

export function readBytes(root, vfs, rel) {
  const f = addonFile(root, rel);
  if (f) return readFileSync(f);
  if (vfs?.exists(rel)) return vfs.readFile(rel);
  return null;
}

// Pure: "{GUID}path" -> { guid, path }; anything else -> { guid: null, path }.
export function splitRef(arg) {
  const m = GUID_RE.exec(String(arg).trim());
  if (m) return { guid: m[1].toUpperCase(), path: m[2] };
  return { guid: null, path: String(arg).trim().replace(/\\/g, "/").replace(/^\/+/, "") };
}

// Pure: the {GUID} a text gives `path`, or null. Case-insensitive on the path, as the engine is.
export function guidIn(text, path) {
  const esc = path.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const m = new RegExp(`\\{([0-9A-Fa-f]{16})\\}${esc}(?=["\\s]|$)`, "i").exec(text);
  return m ? m[1].toUpperCase() : null;
}

// Pure: where the game keeps the editable twin of a prefab (it inherits from the prefab, GUID included).
export function editableTwin(path) {
  const m = /^Prefabs\/(.+\/)?([^/]+)\.et$/i.exec(path);
  if (!m) return null;
  return `PrefabsEditable/Auto/${m[1] ?? ""}E_${m[2]}.et`;
}

function guidFromRepo(root, path) {
  try {
    const re = `\\{[0-9A-Fa-f]{16}\\}${path.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}"`;
    const out = execFileSync("git", ["-C", root, "grep", "-h", "-o", "-i", "-m", "1", "-E", re, "--", "addons/*.et", "addons/*.layer", "addons/*.conf"], { encoding: "utf8", windowsHide: true, stdio: ["ignore", "pipe", "ignore"] });
    return guidIn(out, path);
  } catch {
    return null;
  }
}

const indexFile = () => join(CACHE_DIR(), `prefab-guids-${buildStamp().buildid}.json`);

// Every {GUID}path.et the game's own .et and .conf files reference: { lowercased path: GUID }.
export function guidIndex(vfs, { build = false, log = () => {} } = {}) {
  const file = indexFile();
  if (existsSync(file)) {
    try {
      return JSON.parse(readFileSync(file, "utf8"));
    } catch {}
  }
  if (!build) return null;
  log("indexing the game's prefab GUIDs (once per game build, about 10 s)");
  const re = /\{([0-9A-Fa-f]{16})\}([^"\s]+\.et)\b/g;
  const map = {};
  for (const f of vfs.allFilePaths()) {
    if (!/\.(et|conf)$/i.test(f)) continue;
    let t;
    try {
      t = vfs.readTextFile(f);
    } catch {
      continue;
    }
    for (const x of t.matchAll(re)) map[x[2].toLowerCase()] = x[1].toUpperCase();
  }
  mkdirSync(CACHE_DIR(), { recursive: true });
  writeFileSync(file, JSON.stringify(map));
  return map;
}

// The GUID of a game or addon prefab path, or null. full: build the game index when nothing else knows it.
export function guidFor(root, vfs, path, { full = false, log } = {}) {
  const local = addonFile(root, path);
  if (local) {
    const meta = `${local}.meta`;
    const g = existsSync(meta) ? /Name "\{([0-9A-Fa-f]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1] : null;
    return g ? g.toUpperCase() : null;
  }
  const twin = editableTwin(path);
  if (twin && vfs?.exists(twin)) {
    const g = guidIn(vfs.readTextFile(twin).split("\n")[0], path);
    if (g) return g;
  }
  const fromRepo = guidFromRepo(root, path);
  if (fromRepo) return fromRepo;
  const idx = vfs ? guidIndex(vfs, { build: full, log }) : null;
  return idx?.[path.toLowerCase()] ?? null;
}

// "ShelfShop_01" -> the resource path of the prefab with that file name: addon first, then the game's
// Prefabs/ tree (not PrefabsEditable/). Several in one tree: the shortest path, the rest are listed.
export function findByName(root, vfs, name) {
  const leaf = `/${name.replace(/\.et$/i, "")}.et`.toLowerCase();
  const hits = [];
  for (const d of addonDirs(root)) {
    try {
      const out = execFileSync("git", ["-C", root, "ls-files", "--", `${d.slice(root.length + 1).replace(/\\/g, "/")}/Prefabs`], { encoding: "utf8", windowsHide: true, stdio: ["ignore", "pipe", "ignore"] });
      for (const l of out.split("\n")) if (l.toLowerCase().endsWith(leaf)) hits.push(l.split("/").slice(2).join("/"));
    } catch {}
  }
  if (!hits.length && vfs) for (const p of vfs.allFilePaths()) if (/^Prefabs\//i.test(p) && p.toLowerCase().endsWith(leaf)) hits.push(p);
  hits.sort((a, b) => a.length - b.length);
  return hits;
}

// The prefab's own path for any way of naming it: { path, guid, ref, local (addon file or null), others }.
// Throws a one-line reason an agent can act on.
export async function resolvePrefab(root, arg, { full = false, log } = {}) {
  const { guid, path: given } = splitRef(arg);
  let path = given;
  let others = [];
  const needGame = !addonFile(root, path);
  const vfs = needGame ? await gameVfs().catch(() => null) : null;
  if (!/\.(et|xob)$/i.test(path)) {
    const hits = findByName(root, vfs, path);
    if (!hits.length) throw new Error(`no prefab named "${arg}" in addons/ or the game's Prefabs/ (give {GUID}path or a path ending in .et)`);
    path = hits[0];
    others = hits.slice(1);
  }
  const local = addonFile(root, path);
  if (!local && !vfs?.exists(path)) {
    throw new Error(vfs ? `no ${path} in addons/ or the game's paks` : `no ${path} in addons/, and the game's paks cannot be read (tools\\cli mcp install)`);
  }
  if (/\.xob$/i.test(path)) return { path, guid, ref: guid ? `{${guid}}${path}` : path, local, others, vfs };
  const g = guid ?? guidFor(root, local ? await gameVfs().catch(() => null) : vfs, path, { full, log });
  return { path, guid: g, ref: g ? `{${g}}${path}` : null, local, others, vfs };
}

const headerParent = (text) => /^[^\n]*?:\s*"\{[0-9A-Fa-f]{16}\}([^"]+\.et)"/.exec(text)?.[1] ?? null;

// The entity's own components block (depth 1), so a child entity's mesh is never taken for the root's.
export function ownComponents(text) {
  const re = /(?:^|\n)[ \t]*components\s*\{/g;
  let m;
  while ((m = re.exec(text))) {
    let depth = 0;
    for (let i = 0; i < m.index; i++) {
      if (text[i] === "{") depth++;
      else if (text[i] === "}") depth--;
    }
    if (depth !== 1) continue;
    let d = 0;
    const open = text.indexOf("{", m.index + m[0].lastIndexOf("components"));
    for (let i = open; i < text.length; i++) {
      if (text[i] === "{") d++;
      else if (text[i] === "}" && --d === 0) return text.slice(open + 1, i);
    }
    return text.slice(open + 1);
  }
  return "";
}

// The model a prefab shows and its root scale, through its parents: { mesh, meshRef, scale, chain }.
// meshRef is the {GUID}path the prefab names the model by (a MeshObject of its own can show it).
export function meshOf(root, vfs, path) {
  let mesh = null;
  let meshRef = null;
  let scale = null;
  const chain = [];
  const seen = new Set();
  for (let p = path; p && !seen.has(p.toLowerCase()); ) {
    seen.add(p.toLowerCase());
    const text = readText(root, vfs, p);
    if (!text) break;
    chain.push(p);
    if (!mesh) {
      const m = /MeshObject\s+"\{[0-9A-Fa-f]{16}\}"\s*\{[^{}]*?\bObject\s+"(\{[0-9A-Fa-f]{16}\})([^"]+\.xob)"/.exec(ownComponents(text));
      if (m) [mesh, meshRef] = [m[2], m[1].toUpperCase() + m[2]];
    }
    if (scale == null) {
      const s = /^ scale\s+(-?\d+(?:\.\d+)?)/m.exec(text)?.[1];
      if (s != null) scale = Number(s);
    }
    p = headerParent(text);
  }
  return { mesh, meshRef, scale: scale ?? 1, chain };
}
