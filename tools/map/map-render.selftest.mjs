// tools/map/map-render.selftest.mjs - acceptance for `tools\cli map render` (slice #2227, card M0).
// Renders the island, Saint-Philippe and the square at zoom 4; checks the PNG size and a few pixels; a render
// is under 3 s with the base layer cached; with the raster cache removed the fallback layer still draws.
// The POI legs use the stand-in rows (fixture: true); the real overlay is tools/validation/map-places-selftest.mjs.
// Also runs the rule loader and the case-table format. Run: node tools/map/map-render.selftest.mjs
import { mkdtempSync, readFileSync, writeFileSync, mkdirSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { decodePng } from "./lib/png.mjs";
import { renderMap, ensureBase } from "./render/render.mjs";
import { loadRules, loadCases, runCases } from "./rules/index.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const ok = (cond, msg) => { if (!cond) { failed++; console.error(`FAIL ${msg}`); } else console.log(`ok   ${msg}`); };
const px = (img, x, y) => [...img.data.subarray((y * img.w + x) * img.ch, (y * img.w + x) * img.ch + 3)];
const near = (a, b, tol) => Math.abs(a[0] - b[0]) + Math.abs(a[1] - b[1]) + Math.abs(a[2] - b[2]) <= tol;
const FIXTURES = [
  { name: "island", opts: { zoom: 0 } },
  { name: "saint-philippe", opts: { place: "saint-philippe" } },
  { name: "square z4", opts: { place: "saint-philippe", zoom: 4 } },
];

// 1. Fallback layer: an empty cache dir, no extraction. Roads, buildings and places still draw.
{
  const empty = mkdtempSync(join(tmpdir(), "lf-map-empty-"));
  for (const f of FIXTURES) {
    const t0 = Date.now();
    const r = await renderMap({ root: ROOT, cacheRoot: empty, extract: false, size: [512, 512], ...f.opts });
    const img = decodePng(r.png);
    ok(img.w === 512 && img.h === 512, `fallback ${f.name}: PNG is 512x512`);
    ok(r.layer === "fallback", `fallback ${f.name}: layer is fallback (got ${r.layer})`);
    ok(r.stats.buildings > 0 || f.opts.zoom === 0, `fallback ${f.name}: buildings drawn (${r.stats.buildings})`);
    ok(r.stats.places > 0, `fallback ${f.name}: places drawn (${r.stats.places})`);
    const distinct = new Set();
    for (let i = 0; i < img.data.length; i += 3 * 211) distinct.add(`${img.data[i]},${img.data[i + 1]},${img.data[i + 2]}`);
    ok(distinct.size > 6, `fallback ${f.name}: not a blank sheet (${distinct.size} colours)`);
    ok(Date.now() - t0 < 3000, `fallback ${f.name}: under 3 s (${Date.now() - t0} ms)`);
  }
}

// 2. Raster layer from the game paks (skipped when this machine has no game install).
const base = await ensureBase({ root: ROOT, log: () => {} }).catch(() => null);
if (!base) console.log("SKIP raster legs: no game install or cache on this machine");
else {
  for (const f of FIXTURES) {
    const t0 = Date.now();
    const r = await renderMap({ root: ROOT, size: [1024, 1024], fixture: true, ...f.opts });
    const ms = Date.now() - t0;
    const img = decodePng(r.png);
    ok(img.w === 1024 && img.h === 1024, `raster ${f.name}: PNG is 1024x1024`);
    ok(r.layer === "raster", `raster ${f.name}: layer is raster (got ${r.layer})`);
    ok(ms < 3000, `raster ${f.name}: under 3 s with the cache warm (${ms} ms)`);
    if (f.name === "island") {
      const sea = px(img, 60, 150), land = px(img, 400, 450);
      ok(near(sea, px(img, 960, 600), 90), `raster island: two sea points agree ${sea} ${px(img, 960, 600)}`);
      ok(!near(sea, land, 40), `raster island: land differs from sea ${land} vs ${sea}`);
    }
    if (f.name === "square z4") ok(r.stats.pois > 0, `raster square: POIs drawn (${r.stats.pois})`);
  }
  const a = await renderMap({ root: ROOT, place: "saint-philippe", zoom: 5, as: "new", fixture: true });
  const b = await renderMap({ root: ROOT, place: "saint-philippe", zoom: 5, as: "police", fixture: true });
  ok(b.stats.pois > a.stats.pois, `persona: police sees more places than a new player (${b.stats.pois} > ${a.stats.pois})`);
  const html = await renderMap({ root: ROOT, place: "saint-philippe", zoom: 4, html: true });
  ok(typeof html.html === "string" && html.html.includes("data:image/png;base64,") && html.html.includes("mousemove"), "html variant: embeds the PNG and has hover");
}

// 3. Rule loader and case table.
{
  const rules = await loadRules();
  ok(rules && typeof rules === "object", "rules: loader returns an object (loaded: " + Object.keys(rules).join(",") + ")");
  const dir = mkdtempSync(join(tmpdir(), "lf-map-cases-"));
  writeFileSync(join(dir, "demo.cases.json"), JSON.stringify({ rule: "demo", version: 1, cases: [
    { name: "adds", input: { a: 1, b: 2 }, expect: 3 },
    { name: "wrong", input: { a: 1, b: 1 }, expect: 5 },
  ] }));
  const cases = loadCases("demo", { dir });
  ok(cases.cases.length === 2, "rules: case table loads");
  const res = runCases("demo", ({ a, b }) => a + b, { dir });
  ok(res.pass === 1 && res.fail === 1 && res.failures[0].name === "wrong", "rules: runCases names the mismatching case");
}

if (failed) { console.error(`SUMMARY map-render selftest fail ${failed}`); process.exit(1); }
console.log("SUMMARY map-render selftest ok");
