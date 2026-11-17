// tools\cli map: the offline Everon map emulator (docs/architecture/everon-map.md, card M0 #2227).
//   map render [--place saint-philippe|square|island|fit|<catalog place>|--bbox x0,z0,x1,z1] [--zoom N]
//              [--as new|known|police|org:<id>] [--night] [--size WxH] [--html] [--out file] [--fallback]
//              [--world main|town|debug|grid|<layers dir|.layer>] [--live-states 1,2] [--base everon|plain] [--fixture]
//   POIs are the live places of --world (default main, the pack island) joined to the POI kind table; --fixture
//   draws the stand-in rows instead. A world other than main draws on a plain grid and fits the view to its places.
//   map extract [--force]    read the base layer (terrain colour + heights) from the game paks into the cache
//   map status               where the cache is and whether it exists
// No game boot and no Workbench: a render is about 1 to 2 s with the base layer cached.
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { buildStamp } from "../lib/install-api.mjs";
import { gameVfs } from "../lib/game-prefab.mjs";
import { cacheDir, extract, loadBase } from "../map/lib/basemap.mjs";
import { renderMap } from "../map/render/render.mjs";

export const help = "map render [--place saint-philippe|square|island|fit|--bbox x0,z0,x1,z1] [--zoom N] [--as new|known|police|org:<id>] [--world main|town|debug|grid] [--fixture] [--night] [--html] [--out f]   the Everon map to a PNG in ~1-2 s, no game, no Workbench; map extract|status manage the base-layer cache";

function opt(args, name, takes = true) {
  const i = args.indexOf(`--${name}`);
  if (i < 0) return takes ? undefined : false;
  return takes ? args[i + 1] : true;
}

export async function main(args, { root }) {
  const sub = args[0] && !args[0].startsWith("--") ? args[0] : "render";
  const rest = sub === args[0] ? args.slice(1) : args;
  let build = "unknown";
  try { build = buildStamp().buildid; } catch {}
  if (sub === "status") {
    const base = loadBase(build);
    console.log(`map cache: ${cacheDir(build)} (${base ? "extracted" : "missing: run map extract"})`);
    return 0;
  }
  if (sub === "extract") {
    const t0 = Date.now();
    if (!opt(rest, "force", false) && loadBase(build)) { console.log(`SUMMARY map extract ok cached dir=${cacheDir(build)}`); return 0; }
    const dir = extract(await gameVfs(), build, { log: console.log });
    console.log(`SUMMARY map extract ok ms=${Date.now() - t0} dir=${dir}`);
    return 0;
  }
  if (sub !== "render") { console.error(`map: unknown subcommand ${sub}. ${help}`); return 2; }

  const bboxArg = opt(rest, "bbox");
  const bbox = bboxArg ? bboxArg.split(",").map(Number) : undefined;
  if (bbox && (bbox.length !== 4 || bbox.some((n) => !Number.isFinite(n)) || bbox[2] <= bbox[0] || bbox[3] <= bbox[1])) { console.error("map: --bbox x0,z0,x1,z1 with x1 > x0 and z1 > z0"); return 2; }
  const sizeArg = opt(rest, "size");
  const size = sizeArg ? sizeArg.split("x").map(Number) : undefined;
  const zoomArg = opt(rest, "zoom");
  const as = opt(rest, "as") || "new";
  if (!/^(new|known|police|org:[\w-]+)$/.test(as)) { console.error("map: --as new|known|police|org:<id>"); return 2; }
  const html = opt(rest, "html", false);
  const world = opt(rest, "world") || "main";
  const baseArg = opt(rest, "base");
  if (baseArg && !/^(everon|plain)$/.test(baseArg)) { console.error("map: --base everon|plain"); return 2; }
  const statesArg = opt(rest, "live-states");
  const liveStates = statesArg === undefined ? undefined : statesArg.split(",").filter(Boolean).map(Number);
  if (liveStates?.some((n) => !Number.isFinite(n))) { console.error("map: --live-states 1,2 (numbers)"); return 2; }
  const fixture = opt(rest, "fixture", false);
  let r;
  try {
    r = await renderMap({ root, place: opt(rest, "place"), bbox, zoom: zoomArg === undefined ? undefined : Number(zoomArg), as, world, fixture, base: baseArg, liveStates, night: opt(rest, "night", false), size, html, extract: !opt(rest, "fallback", false), cacheRoot: opt(rest, "fallback", false) ? join(root, "tmp", "map", "no-cache") : undefined, log: console.log });
  } catch (e) {
    console.error(`SUMMARY map render fail ${e.message}`);
    return 1;
  }
  const placeName = opt(rest, "place") || (bbox ? "bbox" : fixture || world === "main" ? "island" : "fit");
  const slug = `${fixture ? "fixture-" : world === "main" ? "" : `${String(world).replace(/[^\w-]+/g, "_")}-`}${placeName}-z${r.view.zoom.toFixed(1)}-${as.replace(":", "-")}${opt(rest, "night", false) ? "-night" : ""}`;
  const out = opt(rest, "out") || join(root, "tmp", "map", `${slug}.png`);
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, r.png);
  let htmlOut = "";
  if (html) {
    htmlOut = out.replace(/\.png$/i, ".html");
    writeFileSync(htmlOut, r.html);
  }
  const s = r.stats;
  const i = s.info;
  if (s.overlay === "none") {
    console.error(`map: no places with a POI kind in world ${i.world} (${i.files} layer files, ${i.places} places, ${i.kinds} POI kinds${Object.keys(i.unjoined).length ? `; kinds with no POI row: ${Object.entries(i.unjoined).map(([k, n]) => `${k} x${n}`).join(", ")}` : ""}). No POIs drawn; pass --fixture for the stand-in rows.`);
  }
  const world_ = s.overlay === "fixture" ? "" : ` world=${i.world} places-in-world=${i.places} poi-places=${i.pois} dark=${i.dark}`;
  console.log(`SUMMARY map render ok layer=${r.layer} ${r.view.w}x${r.view.h} zoom=${r.view.zoom.toFixed(1)} as=${as} pois=${s.pois}${world_} places=${s.places} areas=${s.areas} buildings=${s.buildings} overlay=${s.overlay} ms=${s.ms} png=${out}${htmlOut ? ` html=${htmlOut}` : ""}`);
  return 0;
}
