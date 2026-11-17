// tools/map/render/render.mjs - draws the Everon map a given player would see, to a PNG, with no game and no
// Workbench. Base layer: the terrain colour and heights read once from the game paks (tools/map/lib/basemap.mjs),
// or, with no cache, the fallback layer (schematic roads, buildings, a grid) from docs/everon/catalog-buildings.json.
// Overlays: POIs and areas from the confs the game reads (tools/map/lib/overlay.mjs), places from the catalog.
// View arguments match the in-game `map-shots` proof: --place, --bbox, --zoom (view width = 12800 m / 2^zoom).
import { buildStamp } from "../../lib/install-api.mjs";
import { gameVfs } from "../../lib/game-prefab.mjs";
import { extract, HEIGHT_N, HEIGHT_SCALE, loadBase, TERRAIN_PX, WORLD_M } from "../lib/basemap.mjs";
import { loadCatalog, loadOverlay, personaVisibility } from "../lib/overlay.mjs";
import { encodePng } from "../lib/png.mjs";
import { Canvas } from "../lib/raster.mjs";
import { loadRules } from "../rules/index.mjs";
import { buildHtml } from "./html.mjs";

const SEA_RAW = 6500; // raw height of the waterline (terrain y = raw * HEIGHT_SCALE - 100, sea at y 0)
const PAPER = [232, 220, 190];
const INK = [58, 46, 36];
const PLACES = {
  island: { x: 6400, z: 6400, zoom: 0 },
  "saint-philippe": { x: 4503, z: 10772, zoom: 3 },
  square: { x: 4592, z: 10818, zoom: 4 },
};

export function resolveView({ place, bbox, zoom, size }, catalog) {
  let x0, z0, x1, z1, z;
  if (bbox) {
    [x0, z0, x1, z1] = bbox;
    z = Math.log2(WORLD_M / (x1 - x0));
  } else {
    let p = PLACES[place || "island"];
    if (!p && place) {
      const slug = String(place).toLowerCase().replace(/[^a-z0-9]/g, "");
      const c = catalog.places.find((q) => `${q.key}`.toLowerCase() === slug || q.name.toLowerCase().replace(/[^a-z0-9]/g, "") === slug);
      if (c) p = { x: c.x, z: c.z, zoom: 3 };
    }
    if (!p) throw new Error(`unknown --place ${place} (island, saint-philippe, square, or a catalog place name)`);
    z = zoom ?? p.zoom;
    const wm = WORLD_M / 2 ** z;
    const hm = size ? (wm * size[1]) / size[0] : wm;
    x0 = p.x - wm / 2; x1 = p.x + wm / 2; z0 = p.z - hm / 2; z1 = p.z + hm / 2;
  }
  if (zoom !== undefined && bbox) z = zoom;
  let w = size?.[0] ?? 1024, h = size?.[1] ?? Math.min(2048, Math.max(64, Math.round((1024 * (z1 - z0)) / (x1 - x0))));
  return { x0, z0, x1, z1, w, h, scale: w / (x1 - x0), zoom: z };
}

const baseCache = new Map();
// The cached terrain layer, extracting it from the game paks once when missing (unless extract is false).
export async function ensureBase({ cacheRoot, extract: doExtract = true, log = () => {} } = {}) {
  let build = "unknown";
  try { build = buildStamp().buildid; } catch {}
  const key = `${cacheRoot || ""}|${build}`;
  if (baseCache.has(key)) return baseCache.get(key);
  let base = loadBase(build, cacheRoot);
  if (!base && doExtract) {
    log(`map: extracting the base layer from the game paks (once, about 5 s) -> ${build}`);
    extract(await gameVfs(), build, { log, cacheRoot });
    base = loadBase(build, cacheRoot);
  }
  if (base) baseCache.set(key, base);
  return base;
}

function hSample(height, wx, wz) {
  const gx = Math.min(HEIGHT_N - 1.001, Math.max(0, wx / 8)), gz = Math.min(HEIGHT_N - 1.001, Math.max(0, wz / 8));
  const ix = gx | 0, iz = gz | 0, fx = gx - ix, fz = gz - iz, k = iz * HEIGHT_N + ix;
  return (height[k] * (1 - fx) + height[k + 1] * fx) * (1 - fz) + (height[k + HEIGHT_N] * (1 - fx) + height[k + HEIGHT_N + 1] * fx) * fz;
}

function drawRaster(cv, view, base, night) {
  const { terrain, height } = base;
  const { w, h, scale, x0, z1 } = view;
  const interval = scale >= 0.3 ? 20 : scale >= 0.1 ? 50 : 100;
  const hm = new Float32Array(w * h);
  const sea = new Uint8Array(w * h);
  for (let py = 0; py < h; py++) {
    const wz = z1 - (py + 0.5) / scale;
    const trow = Math.min(TERRAIN_PX - 1, Math.max(0, Math.floor((WORLD_M - wz) / 4)));
    for (let px = 0; px < w; px++) {
      const wx = x0 + (px + 0.5) / scale;
      const raw = hSample(height, wx, wz);
      const i = py * w + px;
      const metres = (raw - SEA_RAW) * HEIGHT_SCALE;
      hm[i] = metres;
      const k = (py * w + px) * 3;
      if (raw < SEA_RAW) {
        sea[i] = 1;
        const depth = Math.min(0.4, (SEA_RAW - raw) / 5000);
        cv.data[k] = 176 - 56 * depth; cv.data[k + 1] = 196 - 46 * depth; cv.data[k + 2] = 200 - 36 * depth;
        continue;
      }
      const tcol = Math.min(TERRAIN_PX - 1, Math.max(0, Math.floor(wx / 4)));
      const tk = (trow * TERRAIN_PX + tcol) * 3;
      const shade = (hSample(height, wx + 8, wz - 8) - hSample(height, wx - 8, wz + 8)) * HEIGHT_SCALE * 0.018;
      const f = 1 + Math.max(-0.35, Math.min(0.35, shade));
      for (let c = 0; c < 3; c++) {
        const v = (terrain.data[tk + c] * 0.62 + PAPER[c] * 0.38) * 1.12 * f;
        cv.data[k + c] = v > 255 ? 255 : v;
      }
    }
  }
  // Coast and contour lines from the height field: a pixel whose level differs from its right or lower neighbour.
  for (let py = 0; py < h - 1; py++) for (let px = 0; px < w - 1; px++) {
    const i = py * w + px;
    for (const j of [i + 1, i + w]) {
      if (sea[i] !== sea[j]) { cv.px(px, py, [96, 116, 128], 0.9); break; }
      if (sea[i]) continue;
      const a = Math.floor(hm[i] / interval), b = Math.floor(hm[j] / interval);
      if (a !== b) {
        const index = Math.max(a, b) % (interval === 20 ? 5 : interval === 50 ? 2 : 1) === 0;
        cv.px(px, py, [128, 88, 52], index ? 0.85 : 0.45);
        break;
      }
    }
  }
  if (night) night(cv);
}

// MST over the named places: a schematic road net, only drawn when no raster shows the real ones.
function schematicRoads(places) {
  const pts = places.filter((p) => p.kind === "town" || p.kind === "site");
  const inTree = [0], edges = [];
  const rest = new Set(pts.keys());
  rest.delete(0);
  while (rest.size) {
    let best = null;
    for (const a of inTree) for (const b of rest) {
      const d = Math.hypot(pts[a].x - pts[b].x, pts[a].z - pts[b].z);
      if (!best || d < best.d) best = { a, b, d };
    }
    edges.push([pts[best.a], pts[best.b]]);
    inTree.push(best.b);
    rest.delete(best.b);
  }
  return edges;
}

function drawFallback(cv, view, catalog, night) {
  const { scale, x0, z0, x1, z1 } = view;
  const sx = (x) => (x - x0) * scale, sy = (z) => (z1 - z) * scale;
  const gridM = scale >= 0.3 ? 100 : scale >= 0.06 ? 500 : 1000;
  for (let x = Math.ceil(x0 / gridM) * gridM; x <= x1; x += gridM) cv.line(sx(x), 0, sx(x), cv.h, [205, 190, 160], 1);
  for (let z = Math.ceil(z0 / gridM) * gridM; z <= z1; z += gridM) cv.line(0, sy(z), cv.w, sy(z), [205, 190, 160], 1);
  for (const [a, b] of schematicRoads(catalog.places)) cv.line(sx(a.x), sy(a.z), sx(b.x), sy(b.z), [196, 160, 112], 1, Math.max(1, Math.round(scale * 6)));
  cv.text("FALLBACK LAYER: NO RASTER CACHE", 8, cv.h - 18, [150, 60, 40], 1, PAPER);
  if (night) night(cv);
}

const RESIDENTIAL = new Set(["house", "townhouse", "villa", "farm", "farm-small", "farmhouse", "hotel"]);
const CIVIC = new Set(["shop", "pub", "kiosk", "government", "police", "fire", "church", "chapel", "fuel", "recreation"]);
function drawBuildings(cv, view, catalog) {
  const { scale, x0, z0, x1, z1 } = view;
  const pad = 40;
  let n = 0;
  for (const b of catalog.buildings) {
    if (b.x < x0 - pad || b.x > x1 + pad || b.z < z0 - pad || b.z > z1 + pad) continue;
    const cx = (b.x - x0) * scale, cy = (z1 - b.z) * scale;
    const hw = Math.max(1.2, (b.fp[0] * scale) / 2), hd = Math.max(1.2, (b.fp[1] * scale) / 2);
    const th = (-b.yaw * Math.PI) / 180, c = Math.cos(th), s = Math.sin(th);
    const pts = [[-hw, -hd], [hw, -hd], [hw, hd], [-hw, hd]].map(([u, v]) => [cx + u * c - v * s, cy - (u * s + v * c)]);
    const col = RESIDENTIAL.has(b.type) ? [176, 134, 100] : CIVIC.has(b.type) ? [138, 74, 62] : [132, 128, 120];
    cv.polygon(pts, col, 0.95);
    if (hw > 2.5) cv.outline(pts, [84, 62, 48], 0.8);
    n++;
  }
  return n;
}

const circle = (x, z, r, n = 40) => Array.from({ length: n }, (_, i) => [x + r * Math.cos((i / n) * Math.PI * 2), z + r * Math.sin((i / n) * Math.PI * 2)]);
const KIND_GLYPH = { CLINIC: "+", POLICE: "P", GOVERNMENT: "H", FUEL: "F", FIRE: "F", CHURCH: "C", PUB: "B", SHOP: "S", FENCE: "X", DEPOT: "D" };
const ACCESS_COLOR = { CIVIC: [40, 70, 120], PUBLIC: [70, 56, 44], DISCOVER: [170, 118, 38], POLICE: [30, 60, 170], UNDERWORLD: [150, 40, 40] };

function drawAreas(cv, view, overlay, hits) {
  const { scale, x0, z1 } = view;
  let n = 0;
  for (const a of overlay.areas) {
    const world = a.shape === "POLYGON" ? a.points : circle(a.x, a.z, a.radius);
    const pts = world.map(([x, z]) => [(x - x0) * scale, (z1 - z) * scale]);
    if (pts.every((p) => p[0] < 0) || pts.every((p) => p[0] > cv.w) || pts.every((p) => p[1] < 0) || pts.every((p) => p[1] > cv.h)) continue;
    if (a.style === "LAWLESS") { cv.polygon(pts, [176, 52, 40], 0.5, 6); cv.outline(pts, [160, 40, 32], 0.9, 6, 2); }
    else if (a.style === "SAFE") { cv.polygon(pts, [70, 130, 80], 0.2); cv.outline(pts, [50, 110, 62], 0.9, 0, 2); }
    else cv.outline(pts, [90, 80, 70], 0.7, 4);
    const cx = pts.reduce((s, p) => s + p[0], 0) / pts.length, cy = pts.reduce((s, p) => s + p[1], 0) / pts.length;
    if (scale >= 0.3) cv.text(a.name, cx - cv.textWidth(a.name) / 2, cy - 3, a.style === "LAWLESS" ? [150, 30, 24] : [40, 100, 56], 1, PAPER);
    hits.push({ kind: "area", label: a.name, detail: `${a.style} area`, pts });
    n++;
  }
  return n;
}

function drawPlaces(cv, view, catalog, taken) {
  const { scale, x0, z0, x1, z1, zoom } = view;
  let n = 0;
  const list = [];
  for (const p of catalog.places) if (p.kind === "town" || zoom >= 1.5) list.push({ ...p, big: p.kind === "town" });
  if (zoom >= 3) for (const p of catalog.natural) list.push({ ...p, big: false, nat: true });
  list.sort((a, b) => (b.big - a.big) || (b.total || 0) - (a.total || 0));
  for (const p of list) {
    if (p.x < x0 || p.x > x1 || p.z < z0 || p.z > z1) continue;
    const sc = p.big ? (zoom >= 2 ? 2 : zoom >= 0.5 ? 1 : 1) : 1;
    const tw = cv.textWidth(p.name, sc), th = 7 * sc;
    const lx = (p.x - x0) * scale - tw / 2, ly = (z1 - p.z) * scale - th / 2;
    if (taken.some((r) => lx < r[0] + r[2] && lx + tw > r[0] && ly < r[1] + r[3] && ly + th > r[1])) continue;
    taken.push([lx, ly, tw, th]);
    cv.text(p.name, lx, ly, p.nat ? [86, 100, 70] : INK, sc, PAPER);
    n++;
  }
  return n;
}

function drawPois(cv, view, overlay, persona, rules, taken, hits) {
  const { scale, x0, z1, zoom } = view;
  let n = 0;
  const vis = rules.visibility?.visible || personaVisibility;
  for (const p of overlay.pois) {
    if (p.minZoom > zoom + 1e-6) continue;
    const state = vis(p, persona);
    if (state === "hidden") continue;
    const cx = (p.x - x0) * scale, cy = (z1 - p.z) * scale;
    if (cx < -10 || cy < -10 || cx > cv.w + 10 || cy > cv.h + 10) continue;
    const base = ACCESS_COLOR[p.access] || [110, 60, 130];
    const r = zoom >= 4 ? 8 : 6;
    const alpha = state === "faded" ? 0.45 : 1;
    if (state === "pencil") {
      cv.line(cx - r, cy - r, cx + r, cy + r, [110, 40, 40], 0.85, 2);
      cv.line(cx - r, cy + r, cx + r, cy - r, [110, 40, 40], 0.85, 2);
    } else {
      cv.disc(cx, cy, r + 1, PAPER, 0.9 * alpha);
      cv.disc(cx, cy, r, base, alpha);
      const g = state === "faded" ? "?" : KIND_GLYPH[p.kind] || p.kind[0];
      cv.text(g, cx - 2, cy - 3, [250, 244, 226], 1, null, alpha);
    }
    const label = state === "faded" ? `${p.name} (HEARD OF)` : p.name;
    const sc = zoom >= 4 ? 2 : 1;
    const tw = cv.textWidth(label, sc), lx = cx + r + 3, ly = cy - (7 * sc) / 2;
    if (!taken.some((q) => lx < q[0] + q[2] && lx + tw > q[0] && ly < q[1] + q[3] && ly + 7 * sc > q[1])) {
      taken.push([lx, ly, tw, 7 * sc]);
      cv.text(label, lx, ly, state === "pencil" ? [110, 40, 40] : INK, sc, PAPER, alpha);
    }
    hits.push({ kind: "poi", x: cx, y: cy, r: r + 2, label: p.name, detail: `${p.kind} / ${p.access} / ${state}` });
    n++;
  }
  return n;
}

function drawFrame(cv, view, persona, layer) {
  const { w, h, scale } = view;
  cv.outline([[0, 0], [w - 1, 0], [w - 1, h - 1], [0, h - 1]], INK, 1);
  const nice = [10, 20, 50, 100, 200, 500, 1000, 2000, 5000];
  const target = w * 0.2;
  const len = nice.reduce((b, m) => (Math.abs(m * scale - target) < Math.abs(b * scale - target) ? m : b), nice[0]);
  const bx = 12, by = h - 34, bw = len * scale;
  cv.rect(bx, by, bw, 4, INK);
  cv.text(len >= 1000 ? `${len / 1000} KM` : `${len} M`, bx, by - 10, INK, 1, PAPER);
  cv.text(`EVERON  ${persona.toUpperCase()}  Z${view.zoom.toFixed(1)}  ${layer.toUpperCase()}`, 8, 8, INK, 1, PAPER);
  cv.line(w - 20, 36, w - 20, 14, INK, 1, 2);
  cv.line(w - 20, 14, w - 25, 22, INK, 1, 2);
  cv.line(w - 20, 14, w - 15, 22, INK, 1, 2);
  cv.text("N", w - 23, 40, INK, 1, PAPER);
}

// opts: place, bbox [x0,z0,x1,z1], zoom, as, night, size [w,h], html, root, cacheRoot, extract (default true).
export async function renderMap(opts = {}) {
  const t0 = Date.now();
  const root = opts.root;
  const catalog = loadCatalog(root);
  const overlay = loadOverlay(root);
  const rules = await loadRules();
  const persona = opts.as || "new";
  const view = resolveView(opts, catalog);
  const base = await ensureBase({ cacheRoot: opts.cacheRoot, extract: opts.extract !== false, log: opts.log });
  const cv = new Canvas(view.w, view.h, PAPER);
  const night = opts.night ? (c) => { for (let i = 0; i < c.data.length; i += 3) { c.data[i] *= 0.34; c.data[i + 1] *= 0.42; c.data[i + 2] *= 0.62; } } : null;
  const layer = base ? "raster" : "fallback";
  if (base) drawRaster(cv, view, base, night);
  else drawFallback(cv, view, catalog, night);
  const hits = [];
  const stats = { buildings: 0, places: 0, pois: 0, areas: 0, overlay: overlay.source };
  if (view.scale >= (base ? 0.5 : 0.04)) stats.buildings = drawBuildings(cv, view, catalog);
  stats.areas = drawAreas(cv, view, overlay, hits);
  const taken = [];
  stats.pois = drawPois(cv, view, overlay, persona, rules, taken, hits);
  stats.places = drawPlaces(cv, view, catalog, taken);
  drawFrame(cv, view, persona, layer);
  const png = encodePng(cv.image(), 3);
  stats.ms = Date.now() - t0;
  const out = { png, layer, stats, view, hits, persona };
  if (opts.html) out.html = buildHtml({ png, view, hits, persona, layer });
  return out;
}
