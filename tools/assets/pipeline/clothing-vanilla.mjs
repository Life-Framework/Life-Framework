// What a vanilla garment is made of, read from the game paks: the prefab chain, the worn and item
// models, their materials and textures, the slot, the body parts it hides, the inventory item and
// storage. The clothing stages build on this record (tmp/assets/<slug>/vanilla.json) and the verify
// stage compares our prefab against it. docs/asset-pipeline/clothing.md "What a garment is".
import { existsSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { pathToFileURL, fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { parse as parseXob } from "./xob.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..", "..");

// ------------------------------------------------------------------ Enfusion text

// Parse .et / .emat / .conf text into nodes: { head: [tokens], props: { key: [values] }, children: [node] }.
// A header like `SCR_Foo "{GUID}" : "{GUID}parent.conf" {` may be split after the colon; that is joined.
export function parseText(text) {
  const lines = String(text).replace(/^\uFEFF/, "").split(/\r?\n/);
  const joined = [];
  for (const raw of lines) {
    const l = raw.trim();
    if (!l) continue;
    if (joined.length && /:\s*$/.test(joined[joined.length - 1])) joined[joined.length - 1] += " " + l;
    else joined.push(l);
  }
  const root = { head: ["<root>"], props: {}, children: [] };
  const stack = [root];
  for (const l of joined) {
    const top = stack[stack.length - 1];
    if (l === "}") {
      if (stack.length > 1) stack.pop();
      continue;
    }
    const opens = /\{\s*$/.test(l) && !/^"\{[0-9A-F]{16}\}/.test(l);
    const toks = tokens(opens ? l.replace(/\{\s*$/, "") : l);
    if (opens) {
      const node = { head: toks, props: {}, children: [] };
      top.children.push(node);
      stack.push(node);
      continue;
    }
    // A list body (`"Torso" "Arm_L"` inside `OverrideMeshesToHide {`) or a `key value...` line.
    if (/^"/.test(l)) (top.values ||= []).push(...toks);
    else top.props[toks[0]] = toks.slice(1);
  }
  return root.children[0] || root;
}

function tokens(s) {
  const out = [];
  const re = /"((?:[^"\\]|\\.)*)"|(\S+)/g;
  let m;
  while ((m = re.exec(s))) out.push(m[1] !== undefined ? m[1] : m[2]);
  return out.filter((t) => t !== ":");
}

// The parent resource of a top-level node: `GameEntity : "{GUID}path"` -> "{GUID}path".
export function parentOf(node) {
  const t = node.head.find((x) => /^\{[0-9A-F]{16}\}/.test(x) && node.head.indexOf(x) > 0);
  return t || null;
}

export const guidOf = (node) => node.head.find((x) => /^\{[0-9A-F]{16}\}$/.test(x))?.slice(1, -1) || null;
export const pathOf = (ref) => String(ref || "").replace(/^\{[0-9A-F]{16}\}/, "");
export const child = (node, name) => node?.children.find((c) => c.head[0] === name) || null;
export const prop = (node, key) => node?.props[key]?.[0];

// The components of one prefab file: [{ cls, guid, node }].
export function componentsOf(root) {
  return (child(root, "components")?.children || []).map((n) => ({ cls: n.head[0], guid: guidOf(n), node: n }));
}

// ------------------------------------------------------------------ the game paks

let vfsInstance = null;
function mcpVfsPath(root = ROOT) {
  const rel = join("tools", "mcp", "enfusion-mcp-bk", "dist", "pak", "vfs.js");
  const local = join(root, rel);
  if (existsSync(local)) return local;
  const res = spawnSync("git", ["-C", root, "rev-parse", "--path-format=absolute", "--git-common-dir"], { windowsHide: true, encoding: "utf8" });
  const main = res.status === 0 ? join(dirname(res.stdout.trim()), rel) : "";
  if (main && existsSync(main)) return main;
  throw new Error(`enfusion-mcp is not built (${local}); run tools\\cli mcp install in the main checkout`);
}

// The game data as { read(path) -> Buffer|null, text(path) -> string|null, paths() }. The addon's own
// files win over the paks, so a prefab the pipeline wrote resolves through the same chain.
export async function gameData({ root = ROOT, addonDir = join(root, "addons", "LifeFramework") } = {}) {
  if (!vfsInstance) {
    const { PakVirtualFS } = await import(pathToFileURL(mcpVfsPath(root)).href);
    vfsInstance = PakVirtualFS.get(process.env.ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger");
    if (!vfsInstance) throw new Error("no game paks found (set ENFUSION_GAME_PATH)");
  }
  const vfs = vfsInstance;
  const local = (p) => join(addonDir, p);
  const read = (p) => {
    p = pathOf(p);
    if (existsSync(local(p))) return readFileSync(local(p));
    return vfs.exists(p) ? vfs.readFile(p) : null;
  };
  return { read, text: (p) => read(p)?.toString("latin1") ?? null, paths: () => vfs.allFilePaths(), vfs };
}

// ------------------------------------------------------------------ EDDS textures

const DXGI = { 71: "BC1", 72: "BC1_SRGB", 77: "BC3", 78: "BC3_SRGB", 80: "BC4", 83: "BC5", 98: "BC7", 99: "BC7_SRGB", 28: "RGBA8", 29: "RGBA8_SRGB" };

// An .edds is a DDS header (with a DX10 extension) followed by a table of mip blocks, smallest
// first, each "COPY" (raw) or "LZ4 " (compressed). Reads the header only.
export function eddsInfo(buf) {
  if (!buf || buf.toString("latin1", 0, 4) !== "DDS ") throw new Error("not an EDDS texture");
  const height = buf.readUInt32LE(12), width = buf.readUInt32LE(16), mips = Math.max(1, buf.readUInt32LE(28));
  const fourcc = buf.toString("latin1", 84, 88);
  const dx10 = fourcc === "DX10";
  const format = dx10 ? DXGI[buf.readUInt32LE(128)] || `dxgi${buf.readUInt32LE(128)}` : fourcc;
  return { width, height, mips, format, headerBytes: dx10 ? 148 : 128 };
}

// One LZ4 mip block of an .edds: a u32 uncompressed size, then chunks of `u32 size` (the high bit marks
// the last chunk) holding linked LZ4 blocks, each up to 64 KB of output and free to copy from the
// output of the chunks before it (layout read from vanilla textures on 2026-11-14).
export function lz4Block(buf) {
  const total = buf.readUInt32LE(0);
  const out = Buffer.alloc(total);
  let o = 4, w = 0;
  for (let last = false; !last; ) {
    if (o + 4 > buf.length) throw new Error("LZ4 block ends inside a chunk header");
    const head = buf.readUInt32LE(o);
    last = (head & 0x80000000) !== 0;
    const end = o + 4 + (head & 0x7fffffff);
    if (end > buf.length) throw new Error("LZ4 chunk runs past its block");
    w = lz4Chunk(buf, o + 4, end, out, w);
    o = end;
  }
  if (w !== total) throw new Error(`LZ4 block gave ${w} bytes, its header says ${total}`);
  return out;
}

// The LZ4 block format (lz4.org): sequences of a token, literals, a 2-byte offset and a match.
function lz4Chunk(src, i, end, out, w) {
  while (i < end) {
    const token = src[i++];
    let lit = token >> 4;
    if (lit === 15) for (let b = 255; b === 255 && i < end; ) { b = src[i++]; lit += b; }
    src.copy(out, w, i, i + lit);
    i += lit;
    w += lit;
    if (i >= end) break; // the last sequence has literals only
    const off = src[i] | (src[i + 1] << 8);
    i += 2;
    if (!off || off > w) throw new Error(`LZ4 match offset ${off} at output ${w}`);
    let len = (token & 15) + 4;
    if ((token & 15) === 15) for (let b = 255; b === 255; ) { b = src[i++]; len += b; }
    for (let k = 0; k < len; k++, w++) out[w] = out[w - off]; // byte by byte: a match may overlap itself
  }
  return w;
}

// A plain .dds Blender can read, from an .edds: the header, then the mips largest first.
export function eddsToDds(buf) {
  const info = eddsInfo(buf);
  let o = info.headerBytes;
  const table = [];
  for (let i = 0; i < info.mips; i++, o += 8) table.push({ kind: buf.toString("latin1", o, o + 4), size: buf.readUInt32LE(o + 4) });
  const blocks = [];
  for (const t of table) {
    const raw = buf.subarray(o, o + t.size);
    if (t.kind === "COPY") blocks.push(raw);
    else if (t.kind === "LZ4 ") blocks.push(lz4Block(raw));
    else throw new Error(`EDDS mip block "${t.kind}" is not COPY or LZ4`);
    o += t.size;
  }
  if (o !== buf.length) throw new Error(`EDDS ends at ${buf.length}, the mip table says ${o}`);
  const head = Buffer.from(buf.subarray(0, info.headerBytes));
  head.fill(0, 32, 36); // Enfusion's "ENF1" tag in the reserved words
  return Buffer.concat([head, ...blocks.reverse()]);
}

// ------------------------------------------------------------------ the garment

// Slots of Character_Base (BaseLoadoutManagerComponent), the loadout area class each takes, and
// the body parts vanilla hides there (MeshesToHide), read 2026-11-14 (game 1.9).
export const SLOTS = {
  Hat: { area: "LoadoutHeadCoverArea", hides: [] },
  Jacket: { area: "LoadoutJacketArea", hides: ["Torso", "Arm_L", "Arm_R", "ArmUpper_L", "ArmUpper_R"] },
  ArmoredVest: { area: "LoadoutArmoredVestSlotArea", hides: [] },
  Pants: { area: "LoadoutPantsArea", hides: ["Leg_L", "Leg_R", "Groin", "Calf_L", "Calf_R"] },
  Boots: { area: "LoadoutBootsArea", hides: [] },
  Back: { area: "LoadoutBackpackArea", hides: [] },
  Vest: { area: "LoadoutVestArea", hides: [] },
  HandWear: { area: "LoadoutHandwearSlotArea", hides: [] },
};
// The base body's mesh parts (Character_Template.fbx in the Arma Reforger Samples), the names
// OverrideMeshesToHide takes.
export const BODY_PARTS = ["Torso", "ArmUpper_L", "ArmUpper_R", "Arm_L", "Arm_R", "Hand_L", "Hand_R", "Groin", "Leg_L", "Leg_R", "Calf_L", "Calf_R", "CalfLower_L", "CalfLower_R", "Foot_L", "Foot_R"];

// The prefab chain, child first: [{ ref, text, root }]. `data` is gameData() (or a stub in the selftest).
export function prefabChain(data, ref) {
  const chain = [];
  const seen = new Set();
  for (let r = ref; r && !seen.has(pathOf(r)); ) {
    seen.add(pathOf(r));
    const text = data.text(r);
    if (text === null) throw new Error(`cannot read ${pathOf(r)} (neither in the addon nor in the game paks)`);
    const root = parseText(text);
    chain.push({ ref: r, root });
    r = parentOf(root);
  }
  return chain;
}

// One component across the chain: the child-most file that declares it by GUID (or by class when
// `guid` is null) wins per property; children nodes are kept per file.
export function mergedComponents(chain) {
  const byKey = new Map();
  for (let i = chain.length - 1; i >= 0; i--) {
    for (const c of componentsOf(chain[i].root)) {
      const key = c.guid || c.cls;
      const prev = byKey.get(key);
      byKey.set(key, { cls: c.cls, guid: c.guid, layers: [...(prev?.layers || []), c.node] });
    }
  }
  return [...byKey.values()];
}

// A property of a merged component, child-most layer first. `path` walks child nodes by head[0].
export function lookup(comp, path, key) {
  for (let i = comp.layers.length - 1; i >= 0; i--) {
    let n = comp.layers[i];
    for (const step of path) n = child(n, step);
    if (!n) continue;
    if (key === null) return n; // the node itself
    if (key === "[]") { if (n.values) return n.values; continue; }
    if (n.props[key]) return n.props[key][0];
  }
  return null;
}

function emat(data, ref) {
  const text = data.text(ref);
  if (text === null) return { ref, missing: true };
  const root = parseText(text);
  const maps = {};
  for (const [k, v] of Object.entries(root.props)) if (/\.edds$/.test(v[0] || "")) maps[k] = v[0];
  return { ref, cls: root.head[0], parent: parentOf(root), maps, props: Object.fromEntries(Object.entries(root.props).map(([k, v]) => [k, v.join(" ")])) };
}

// Every map an emat ends up with, its own first, then its parents'.
export function ematMaps(data, ref, depth = 0) {
  const e = emat(data, ref);
  if (e.missing || depth > 6) return {};
  return { ...(e.parent ? ematMaps(data, e.parent, depth + 1) : {}), ...e.maps };
}

function model(data, ref) {
  if (!ref) return null;
  const buf = data.read(ref);
  if (!buf) return { ref, missing: true };
  const m = parseXob(buf);
  const usages = m.strings.filter((s) => /^(FireGeo|ItemFireView|Interaction|Character|Projectile)$/.test(s));
  return { ref, bytes: buf.length, bounds: m.bounds, nodes: m.nodes.length, nodeNames: m.nodes.map((n) => n.name), materials: m.materials, colliders: m.colliders, usages, gamemats: m.strings.filter((s) => /\.gamemat$/.test(s)) };
}

// The record of one garment prefab: everything the stages and the verify step need.
export function describeGarment(data, ref) {
  const chain = prefabChain(data, ref);
  const comps = mergedComponents(chain);
  const find = (cls) => comps.find((c) => c.cls === cls) || null;
  const cloth = find("BaseLoadoutClothComponent");
  if (!cloth) throw new Error(`${pathOf(ref)} has no BaseLoadoutClothComponent: not a wearable`);
  const areaNode = cloth.layers.map((l) => l.children.find((c) => c.head[0] === "AreaType")).filter(Boolean).pop();
  const listOf = (key) => lookup(cloth, [key], "[]");
  const mesh = find("MeshObject");
  const item = comps.find((c) => c.layers.some((l) => child(l, "Attributes")));
  const attr = (path, key) => (item ? lookup(item, ["Attributes", ...path], key) : null);
  const storage = find("SCR_UniversalInventoryStorageComponent");
  const faction = find("SCR_ItemOutfitFactionComponent");
  const worn = model(data, lookup(cloth, [], "WornModel"));
  const itemModel = model(data, lookup(cloth, [], "ItemModel"));
  const wornMats = listOf("WornMaterialsOverride") || (worn?.materials || []).map((m) => m.path);
  const itemMats = listOf("ItemMaterialsOverride") || (itemModel?.materials || []).map((m) => m.path);
  const matRecord = (ref) => {
    const maps = ematMaps(data, ref);
    const textures = {};
    for (const [k, t] of Object.entries(maps)) {
      try { textures[k] = { ref: t, ...eddsInfo(data.read(t)) }; } catch (e) { textures[k] = { ref: t, error: e.message }; }
    }
    return { ref, ...emat(data, ref), textures };
  };
  return {
    ref,
    chain: chain.map((c) => c.ref),
    entityClass: chain[0].root.head[0],
    id: prop(chain[0].root, "ID") || chain.map((c) => prop(c.root, "ID")).find(Boolean) || null,
    components: comps.map((c) => ({ cls: c.cls, guid: c.guid })),
    cloth: {
      guid: cloth.guid,
      area: areaNode ? areaNode.head[1] : null,
      areaGuid: areaNode ? guidOf(areaNode) : null,
      hides: listOf("OverrideMeshesToHide"),
      wornModel: worn?.ref || null,
      itemModel: itemModel?.ref || null,
      deflatedModel: lookup(cloth, [], "DeflatedModel"),
      wornMaterialsOverride: listOf("WornMaterialsOverride"),
      itemMaterialsOverride: listOf("ItemMaterialsOverride"),
    },
    mesh: mesh ? { guid: mesh.guid, object: lookup(mesh, [], "Object") } : null,
    item: item ? {
      cls: item.cls, guid: item.guid,
      attributesGuid: guidOf(item.layers.map((l) => child(l, "Attributes")).filter(Boolean).pop()),
      displayGuid: guidOf(item.layers.map((l) => child(child(l, "Attributes"), "ItemDisplayName")).filter(Boolean).pop() || { head: [] }),
      name: attr(["ItemDisplayName"], "Name"), description: attr(["ItemDisplayName"], "Description"),
      weight: attr(["ItemPhysAttributes"], "Weight"), volume: attr(["ItemPhysAttributes"], "ItemVolume"),
      preview: !!attr(["CustomAttributes", "PreviewRenderAttributes"], null),
      previewWorn: attr(["CustomAttributes", "PreviewRenderAttributes"], "PreviewWornModel") === "1",
    } : null,
    storage: storage ? { guid: storage.guid, maxVolume: lookup(storage, [], "MaxCumulativeVolume"), maxWeight: lookup(storage, [], "m_fMaxWeight") } : null,
    faction: faction ? { guid: faction.guid } : null,
    worn, itemModel,
    materials: { worn: wornMats.map(matRecord), item: itemMats.map(matRecord) },
  };
}
