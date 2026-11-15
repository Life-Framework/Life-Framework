// tools\cli measure: a prefab's size and the heights things stand at on it, read from the model file in a
// second or two, no game and no Workbench (tools/lib/xob.mjs). Cached per game build in
// ~/.life-framework/cache/measure-<build>.json; an addon prefab or model is re-read when its file changes.
import { existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { buildStamp } from "../lib/install-api.mjs";
import { CACHE_DIR, addonFile, gameVfs, meshOf, readBytes, resolvePrefab } from "../lib/game-prefab.mjs";
import { measureXob } from "../lib/xob.mjs";

export const help = "measure <prefab|{GUID}path|name|.xob> [more] [--surfaces] [--all] [--json] [--fresh]   model bounds and the horizontal surfaces (shelf boards, counter tops) from the .xob colliders, no game, cached";

const VERSION = 2;
const r3 = (n) => (Math.round(n * 1000) / 1000).toFixed(3);
const range = (a) => `${r3(a[0])}..${r3(a[1])}`;
const boxText = (b) => `x ${range([b.min[0], b.max[0]])}  y ${range([b.min[1], b.max[1]])}  z ${range([b.min[2], b.max[2]])}  (${[0, 1, 2].map((k) => (b.max[k] - b.min[k]).toFixed(2)).join(" x ")} m)`;

const cacheFile = (build) => join(CACHE_DIR(), `measure-${build}.json`);
function readCache(build) {
  try {
    return JSON.parse(readFileSync(cacheFile(build), "utf8"));
  } catch {
    return {};
  }
}
function writeCache(build, cache) {
  try {
    mkdirSync(CACHE_DIR(), { recursive: true });
    writeFileSync(cacheFile(build), JSON.stringify(cache));
  } catch {}
}
const stamp = (f) => {
  try {
    const s = statSync(f);
    return `${s.size}:${Math.round(s.mtimeMs)}`;
  } catch {
    return null;
  }
};
// A cached entry holds while every addon file it read is unchanged (game files change only with the build).
const fresh = (e) => e && e.v === VERSION && (e.files ?? []).every((f) => stamp(f.file) === f.stamp);

// Measures one prefab or model: { arg, path, ref, mesh, scale, ...measureXob }.
export async function measureOne(root, arg, { minSize = 0.05 } = {}) {
  const r = await resolvePrefab(root, arg);
  const vfs = r.vfs ?? (await gameVfs().catch(() => null));
  let mesh = r.path;
  let meshRef = r.ref;
  let scale = 1;
  let chain = [];
  if (!/\.xob$/i.test(r.path)) {
    ({ mesh, meshRef, scale, chain } = meshOf(root, vfs, r.path));
    if (!mesh) throw new Error(`${r.path} has no MeshObject model in its chain (${chain.join(" <- ") || "unreadable"})`);
  }
  const buf = readBytes(root, vfs, mesh);
  if (!buf) throw new Error(`${mesh} (the model of ${r.path}) is in neither addons/ nor the game's paks`);
  const m = measureXob(buf, { scale, minSize });
  const files = [...chain, mesh].map((p) => addonFile(root, p)).filter(Boolean).map((file) => ({ file, stamp: stamp(file) }));
  return { arg, path: r.path, ref: r.ref, others: r.others, mesh, meshRef, scale, ...m, files };
}

function print(m, { surfaces }) {
  console.log(`measure ${m.path.split("/").pop().replace(/\.(et|xob)$/, "")}  ${m.ref ?? `${m.path} (GUID unknown)`}${m.cached ? "  (cached)" : ""}`);
  if (m.others?.length) console.log(`  also named so: ${m.others.slice(0, 4).join(", ")}${m.others.length > 4 ? ", ..." : ""}`);
  console.log(`  model     ${m.mesh}${m.scale !== 1 ? `  (prefab scale ${m.scale})` : ""}`);
  if (m.bounds) console.log(`  bounds    ${boxText(m.bounds)}`);
  const kinds = Object.entries(m.kinds).map(([k, n]) => `${n} ${k}`).join(", ");
  console.log(`  colliders ${m.colliders ? `${m.colliders} (${kinds})  ${boxText(m.colliderBounds)}` : "none (nothing to stand on: a trace passes through)"}`);
  if (m.truncated) console.log("  note      a collider record this reader does not know ends the list; surfaces below may be incomplete");
  if (!surfaces) {
    const tops = [...new Set(m.surfaces.map((s) => r3(s.y)))];
    if (tops.length) console.log(`  surfaces  y ${tops.slice(0, 8).join(", ")}${tops.length > 8 ? ", ..." : ""}   (--surfaces for extents)`);
    return;
  }
  if (!m.surfaces.length) {
    console.log("  surfaces  none facing up");
    return;
  }
  console.log("  surfaces (faces up, prefab space: +z is the front, y from the origin; y is where a thing stands):");
  for (const s of m.surfaces) {
    const w = s.x[1] - s.x[0];
    const d = s.z[1] - s.z[0];
    console.log(`    y ${r3(s.y)}  x ${range(s.x)}  z ${range(s.z)}  (${w.toFixed(2)} x ${d.toFixed(2)} m, mid z ${r3((s.z[0] + s.z[1]) / 2)})  ${s.kind}${s.name ? ` ${s.name}` : ""}`);
  }
}

export async function main(args, { root }) {
  const flag = (n) => args.includes(n);
  const names = args.filter((a) => !a.startsWith("--"));
  if (!names.length || flag("--help")) {
    console.log(`usage: tools\\cli ${help}`);
    return names.length ? 0 : 2;
  }
  const t0 = Date.now();
  const build = buildStamp().buildid;
  const minSize = flag("--all") ? 0 : 0.05;
  const cache = readCache(build);
  const out = [];
  let fail = 0;
  let dirty = false;
  for (const arg of names) {
    const key = `${arg}|${minSize}`;
    const hit = flag("--fresh") ? null : cache[key];
    if (fresh(hit)) {
      out.push({ ...hit.m, cached: true });
      continue;
    }
    try {
      const m = await measureOne(root, arg, { minSize });
      cache[key] = { v: VERSION, files: m.files, m };
      dirty = true;
      out.push(m);
    } catch (e) {
      console.log(`FAIL  measure ${arg}: ${e.message}`);
      fail = 1;
    }
  }
  if (dirty) writeCache(build, cache);
  if (flag("--json")) console.log(JSON.stringify(out.map(({ files, ...m }) => m), null, 1));
  else for (const m of out) print(m, { surfaces: flag("--surfaces") });
  if (!flag("--json")) console.log(`SUMMARY measure ${fail ? "fail" : "ok"} ${out.length} model(s) in ${((Date.now() - t0) / 1000).toFixed(1)} s (colliders: what a trace and SnapToSurface hit)`);
  return fail;
}
