// tools/scout/scout.mjs - `tools\cli scout <place>`: a location dossier for a named place on vanilla Everon.
//
//   scout "Saint Philippe"                 docs/everon/places/saint-philippe/{dossier.md,dossier.json,img/}
//   scout 4502,10771 --radius 90           a place by coordinates (named xz-4502-10771)
//   scout list [filter]                    the map's named places and where they are
//   scout types                            the building and prop type names a dossier uses
//   scout "Saint Philippe" --from-dump <dump.txt> --quick     analyse a dump already taken; no Workbench
//
// How it works: one `look` run on the CoreOnEveron world (vanilla Everon plus the core game mode) takes the overview
// pictures and, through the plugin's `dump` directive, asks the engine for every entity, road and ground height near the
// place (tools/scout/dump.mjs). Node then measures each building's model (tools\cli measure), finds footprints,
// entrances, roads, junctions, free plots and slope (analyse.mjs), picks the main street and the civic buildings, and
// a second `look` run takes a picture of each; `look --interior` goes inside the first few. The buildings themselves (type, footprint, floors,
// interior) are read from the Everon catalog, docs/everon/catalog-buildings.json, when it is in the checkout. The pool runs the
// Workbench, so many scouts proceed at once, and each writes only its own place folder (and tmp/scout/<slug>/).
import { spawn } from "node:child_process";
import { cpSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { buildStamp } from "../lib/install-api.mjs";
import { EVERON_WORLD } from "../lib/worlds.mjs";
import { measureOne } from "../commands/measure.mjs";
import { analyse, compass } from "./analyse.mjs";
import { parseDump } from "./dump.mjs";
import { catalogBuildings, loadCatalog, loadPlaceFile } from "./catalog.mjs";
import { gazetteer, matchPlace, parseCoords, placesNear } from "./gazetteer.mjs";
import { renderDossier } from "./render.mjs";
import { typeTable } from "./types.mjs";

export const SCHEMA = 1;
const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..");
const LOOK_TIMEOUT_MS = 40 * 60 * 1000;
// look's boot budget (seconds before its per-shot part). Vanilla Everon is a whole island: a pool machine that is also
// running tests took 96 s to load it and enter play (2026-11-16), over look's default of 90.
export const LOOK_BOOT_BUDGET_S = 240;

export const USAGE = `usage: tools\\cli scout <place name | x,z> [--radius 120] [--view 60] [--civic 6] [--interiors 2] [--quick] [--from-dump file] [--slug s] [--out dir] [--machine m] [--wait] [--budget s] [--catalog file]
       tools\\cli scout list [filter]      tools\\cli scout types`;

// Pure: "Saint-Philippe" -> "saint-philippe"; accents folded, anything else a dash.
export const slugOf = (name) =>
  String(name)
    .normalize("NFD")
    .replace(/[̀-ͯ]/g, "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");

// Pure: argv -> { pos, flags }. The valued flags take the next word.
export function parseScoutArgs(argv) {
  const valued = new Set(["radius", "view", "civic", "interiors", "slug", "out", "machine", "from-dump", "dumpstep", "budget", "catalog"]);
  const flags = {};
  const pos = [];
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (!a.startsWith("--")) pos.push(a);
    else if (valued.has(a.slice(2))) flags[a.slice(2)] = argv[++i];
    else flags[a.slice(2)] = true;
  }
  return { pos, flags };
}

const num = (v, d) => (v === undefined || Number.isNaN(Number(v)) ? d : Number(v));
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

// Pure: the place to scout: { query, name, key, kind, x, y, z, source, slug, others } or throws a one-line reason.
export function resolveTarget(pos, entries) {
  const query = pos.join(" ").trim();
  if (!query) throw new Error("name a place (tools\\cli scout list shows the map's names) or give x,z");
  const c = parseCoords(query);
  if (c) {
    const slug = `xz-${Math.round(c.x)}-${Math.round(c.z)}`;
    return { query, name: slug, key: null, kind: null, x: c.x, y: null, z: c.z, source: "coordinates", slug, others: [], radius: c.radius };
  }
  const hits = matchPlace(entries, query);
  if (!hits.length) {
    const near = entries.map((e) => e.name).filter((n, i, a) => a.indexOf(n) === i).slice(0, 12);
    throw new Error(`no named place matches "${query}" on Everon (tools\\cli scout list; for example ${near.join(", ")}) or give x,z`);
  }
  const h = hits[0];
  return { query, name: h.name, key: h.key, kind: h.kind, x: h.x, y: h.y, z: h.z, source: "gazetteer", slug: slugOf(h.name), others: hits.slice(1) };
}

// Pure: the civic buildings to photograph, nearest first, at most `max`, no more than two of one vanilla prefab
// (a street of identical shops is one picture). -> the building records, each with a `view` radius.
export function pickCandidates(buildings, max = 6) {
  const out = [];
  const seen = new Map();
  for (const b of buildings) {
    if (!b.civic || b.ruin) continue;
    const n = seen.get(b.stem) ?? 0;
    if (n >= 2) continue;
    seen.set(b.stem, n + 1);
    out.push({ ...b, view: clamp(Math.round(Math.max(b.footprint.w, b.footprint.d) * 0.9 + 6), 14, 40) });
    if (out.length >= max) break;
  }
  return out;
}

// Pure: the buildings to go inside: civic candidates that have a door entity, church and municipal office first.
export function pickInteriors(candidates, max = 2) {
  const order = ["church", "government", "police", "fire", "pub", "hotel"];
  const rank = (b) => {
    const i = order.indexOf(b.type);
    return i < 0 ? order.length : i;
  };
  return candidates
    .filter((b) => b.interior.value === "yes")
    .sort((a, b) => rank(a) - rank(b) || a.distance - b.distance)
    .slice(0, max)
    .map((b) => ({ ...b, room: { x: b.footprint.cx, y: Math.round((b.y + (b.firstFloor ?? 0.3)) * 10) / 10, z: b.footprint.cz, radius: clamp(Math.round((Math.min(b.footprint.w, b.footprint.d) / 2) * 0.8), 3, 8) } }));
}

// Pure: the look area spec for a point and radius, and the name look gives it (tools/visual/look.mjs resolveArea).
export const areaSpec = (x, z, r) => `${Math.round(x * 10) / 10},${Math.round(z * 10) / 10},${r}`;
export const areaName = (x, z) => `xz-${Math.round(Math.round(x * 10) / 10)}-${Math.round(Math.round(z * 10) / 10)}`;

// Pure: the dossier object from its parts. `pictures` are { role, building?, file?, caption, missing? }.
export function buildDossier({ target, radius, analysis, pictures, build, world, at, command, nearby }) {
  return {
    schema: SCHEMA,
    place: { name: target.name, key: target.key, kind: target.kind, source: target.source, slug: target.slug, x: target.x, y: target.y, z: target.z },
    radius,
    generated: { at, tool: "tools\\cli scout", command, gameBuild: build, world },
    nearbyPlaces: nearby,
    summary: analysis.summary,
    terrain: analysis.terrain,
    mainStreet: analysis.mainStreet,
    roads: analysis.roads,
    junctions: analysis.junctions,
    buildings: analysis.buildings,
    props: analysis.props,
    propCounts: analysis.propCounts,
    freePlots: analysis.freePlots,
    pictures,
    notes: analysis.notes,
  };
}

// ------------------------------------------------------------------ the look runs

// One `tools\cli look` in the foreground; its text goes to `logFile`. -> { code, text }.
function runLook(root, argv, logFile) {
  return new Promise((resolveRun) => {
    const child = spawn(process.execPath, [join(root, "tools", "cli.mjs"), "look", ...argv], { cwd: root, windowsHide: true });
    let text = "";
    const timer = setTimeout(() => child.kill(), LOOK_TIMEOUT_MS);
    child.stdout.on("data", (d) => (text += d));
    child.stderr.on("data", (d) => (text += d));
    child.on("close", (code) => {
      clearTimeout(timer);
      try {
        writeFileSync(logFile, text);
      } catch {}
      resolveRun({ code: code ?? 1, text });
    });
  });
}

const lookLines = (text) => text.split(/\r?\n/).filter((l) => /^(look: (on|ran on|queued|not here)|FAIL|TIMEOUT|SUMMARY)/.test(l.trim())).map((l) => `  ${l.trim()}`);

// A look's output folder for one area: the PNGs by kind, dump.txt and index.json.
const areaDir = (outRoot, x, z, suffix = "") => join(outRoot, `${areaName(x, z)}${suffix}`);

// A PNG as a small JPEG in the place folder (the tools' image helper; a plain copy when it is not there).
async function keepPicture(png, jpg) {
  mkdirSync(dirname(jpg), { recursive: true });
  try {
    const { image } = await import("../../agent-box/shots.mjs");
    image({ op: "sheet", tiles: [{ file: png, label: "", marks: [] }], out: jpg, cols: 1, maxWidth: 1280, quality: 78 });
    return true;
  } catch {
    cpSync(png, jpg.replace(/\.jpg$/, ".png"));
    return false;
  }
}

// ------------------------------------------------------------------ measure

// The models' bounds for the buildings of a dump: { prefab path: { min, max } | null }, the prefab's own root scale undone
// (the engine's per-entity scale already includes it).
export async function measureBuildings(root, dump, classify) {
  const refs = new Map();
  for (const e of dump.entities) {
    const c = classify(e.prefab);
    if (c.group !== "building") continue;
    refs.set(e.prefab.replace(/^\{[0-9A-Fa-f]{16}\}/, ""), e.prefab);
  }
  const out = {};
  const failed = [];
  for (const [path, ref] of refs) {
    try {
      const m = await measureOne(root, ref);
      out[path] = m.bounds ? { min: m.bounds.min.map((v) => v / (m.scale || 1)), max: m.bounds.max.map((v) => v / (m.scale || 1)) } : null;
    } catch (e) {
      out[path] = null;
      failed.push(`${path}: ${e.message}`);
    }
  }
  return { measured: out, failed };
}

// ------------------------------------------------------------------ the command

async function listPlaces(filter) {
  const entries = await gazetteer();
  const hits = filter ? matchPlace(entries, filter) : entries;
  for (const e of hits) console.log(`${e.kind.padEnd(8)} ${e.name.padEnd(28)} x ${String(Math.round(e.x)).padStart(6)}  z ${String(Math.round(e.z)).padStart(6)}   ${e.prefab.replace(/^Prefabs\/World\/Locations\/Eden\//, "")}`);
  console.log(`SUMMARY scout list ok ${hits.length} place(s)`);
  return 0;
}

export async function main(argv, { root = ROOT } = {}) {
  const { pos, flags } = parseScoutArgs(argv);
  if (flags.help || !pos.length) {
    console.log(USAGE);
    return flags.help ? 0 : 2;
  }
  if (pos[0] === "types") {
    for (const t of typeTable()) console.log(`${t.kind.padEnd(8)} ${t.type.padEnd(22)} ${t.civic ? "civic" : t.home ? "home " : "     "}  ${t.path}`);
    return 0;
  }
  if (pos[0] === "list") return listPlaces(pos.slice(1).join(" "));

  const t0 = Date.now();
  const wt = await import("../wt.mjs");
  if (!flags["from-dump"] && wt.isMainCheckout(root)) {
    console.log("REFUSE  'scout' runs in a worktree (it starts a Workbench through `look`); the main checkout is the world-editor copy.\n        create one first:  tools\\cli wt new <slug>");
    return 2;
  }

  let target;
  try {
    target = resolveTarget(pos, await gazetteer());
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 2;
  }
  if (flags.slug) target.slug = slugOf(flags.slug);
  const radius = num(flags.radius, target.radius ?? 120);
  const view = num(flags.view, 60);
  const placeDir = resolve(root, flags.out || join("docs", "everon", "places", target.slug));
  const scratch = join(root, "tmp", "scout", target.slug);
  const lookOut = join(scratch, "look");
  mkdirSync(scratch, { recursive: true });
  const build = buildStamp().text;
  const log = (m) => console.log(`scout ${target.slug}: ${m}`);
  log(`${target.name} at x ${target.x}, z ${target.z}, radius ${radius} m, ${target.source}`);

  // Phase A: the overview pictures and the engine's entity dump.
  let dumpText;
  const overviewDir = areaDir(lookOut, target.x, target.z);
  if (flags["from-dump"]) {
    dumpText = readFileSync(resolve(flags["from-dump"]), "utf8");
    log(`dump read from ${flags["from-dump"]}`);
  } else {
    rmSync(lookOut, { recursive: true, force: true });
    log("phase A: look + entity dump on vanilla Everon (the pool picks a machine)");
    const a = ["look", areaSpec(target.x, target.z, view), "--world", EVERON_WORLD, "--close", "0", "--dump", String(radius), "--dumpstep", String(num(flags.dumpstep, 5)), "--out", lookOut];
    a.push("--budget", String(num(flags.budget, LOOK_BOOT_BUDGET_S)));
    if (flags.machine) a.push("--machine", flags.machine);
    if (flags.wait) a.push("--wait");
    const r = await runLook(root, a.slice(1), join(scratch, "look-a.log"));
    for (const l of lookLines(r.text)) console.log(l);
    const f = join(overviewDir, "dump.txt");
    if (!existsSync(f)) {
      console.log(`SUMMARY scout fail ${target.slug}: look exit ${r.code} and no entity dump came back; log=${relative(root, join(scratch, "look-a.log"))}`);
      return 1;
    }
    dumpText = readFileSync(f, "utf8");
    writeFileSync(join(scratch, "dump.txt"), dumpText);
  }
  const dump = parseDump(dumpText);
  if (!dump.entities.length) {
    console.log(`SUMMARY scout fail ${target.slug}: the dump holds no entities (${JSON.stringify(dump.classes).slice(0, 160)}); log=${relative(root, scratch)}`);
    return 1;
  }
  const centre = { x: dump.request?.x ?? target.x, z: dump.request?.z ?? target.z };
  const ground = dump.grid ? dump.grid.h[Math.round(dump.grid.nz / 2)]?.[Math.round(dump.grid.nx / 2)] : null;
  if (target.y === null && ground !== null && ground !== undefined) target.y = Math.round(ground * 10) / 10;

  // Measure and analyse.
  const { classify } = await import("./types.mjs");
  // The buildings come from the Everon catalog when it is here (docs/everon/catalog-buildings.json): its types, footprints,
  // floors and interiors, built from a dump of the whole island. Without it the dump's own entities are typed and measured.
  const cat = loadCatalog(root, flags.catalog ? resolve(flags.catalog) : undefined);
  const analysisRadius = dump.request?.radius ?? radius;
  let measured = {};
  let failed = [];
  if (!cat) ({ measured, failed } = await measureBuildings(root, dump, classify));
  const catalog = cat ? { buildings: catalogBuildings(cat, centre.x, centre.z, analysisRadius), placeFile: loadPlaceFile(root, target.slug) } : null;
  if (!cat) log("no docs/everon/catalog-buildings.json: buildings are typed and measured from the dump instead");
  const analysis = analyse(dump, centre, analysisRadius, measured, { catalog });
  for (const f of failed) analysis.notes.push(`model not measured (${f}): its footprint is the engine's world bounds`);
  log(`${analysis.summary.buildings} buildings, ${analysis.summary.roads} roads, ${analysis.summary.junctions} junctions, ${analysis.summary.freePlots} free plots`);

  // Pictures.
  const pictures = [];
  rmSync(join(placeDir, "img"), { recursive: true, force: true });
  const take = async (srcDir, kind, role, caption, extra = {}) => {
    const png = join(srcDir, `${kind}.png`);
    if (!existsSync(png)) {
      pictures.push({ role, ...extra, file: null, caption, missing: `no ${kind} shot came back` });
      return;
    }
    const name = `${role}${extra.building ? `-${extra.building}` : ""}-${kind}`;
    const jpg = join(placeDir, "img", `${name}.jpg`);
    const jpeg = await keepPicture(png, jpg);
    pictures.push({ role, ...extra, file: `img/${name}.${jpeg ? "jpg" : "png"}`, caption });
  };
  if (existsSync(overviewDir)) {
    await take(overviewDir, "top", "overview", "from above, north up");
    await take(overviewDir, "wide", "overview", "wide, from the south");
    for (const [k, c] of [["ob270", "from the west"], ["ob000", "from the north"], ["ob090", "from the east"]]) await take(overviewDir, k, "overview", c);
  }

  const candidates = pickCandidates(analysis.buildings, num(flags.civic, 6));
  const street = analysis.mainStreet;
  if (!flags.quick && !flags["from-dump"]) {
    const areas = [];
    if (street) areas.push({ x: street.at[0], z: street.at[1], r: 22, role: "main-street" });
    for (const b of candidates) areas.push({ x: b.x, z: b.z, r: b.view, role: "civic", building: b.id, type: b.type });
    if (areas.length) {
      log(`phase B: ${areas.length} more view(s): ${street ? "the main street, " : ""}${candidates.length} civic building(s)`);
      const b = ["look", ...areas.map((a) => areaSpec(a.x, a.z, a.r)), "--world", EVERON_WORLD, "--close", "0", "--out", join(scratch, "look-b")];
      b.push("--budget", String(num(flags.budget, LOOK_BOOT_BUDGET_S)));
      if (flags.machine) b.push("--machine", flags.machine);
      if (flags.wait) b.push("--wait");
      const r = await runLook(root, b.slice(1), join(scratch, "look-b.log"));
      for (const l of lookLines(r.text)) console.log(l);
      for (const a of areas) {
        const dir = areaDir(join(scratch, "look-b"), a.x, a.z);
        if (a.role === "main-street") {
          await take(dir, "wide", "main-street", `${street.road} (${street.runs}), looking along it`);
          await take(dir, "ob090", "main-street", "from the east");
        } else {
          await take(dir, "wide", "civic", `${a.type}, from the front`, { building: a.building });
          await take(dir, "ob090", "civic", `${a.type}, from the east`, { building: a.building });
        }
      }
    }
    for (const b of pickInteriors(candidates, num(flags.interiors, 2))) {
      log(`phase C: inside ${b.id} ${b.type} ${b.stem}`);
      const out = join(scratch, `look-in-${b.id}`);
      const c = ["look", areaSpec(b.footprint.cx, b.footprint.cz, b.room.radius), "--interior", `${b.room.x},${b.room.y},${b.room.z}`, "--radius", String(b.room.radius), "--world", EVERON_WORLD, "--close", "0", "--out", out];
      c.push("--budget", String(num(flags.budget, LOOK_BOOT_BUDGET_S)));
      if (flags.machine) c.push("--machine", flags.machine);
      if (flags.wait) c.push("--wait");
      const r = await runLook(root, c.slice(1), join(scratch, `look-in-${b.id}.log`));
      for (const l of lookLines(r.text)) console.log(l);
      const dir = join(out, `${areaName(b.footprint.cx, b.footprint.cz)}-interior`);
      const kinds = existsSync(dir) ? readdirSync(dir).filter((f) => /^(wide|in\d+)\.png$/.test(f)).map((f) => f.replace(/\.png$/, "")) : [];
      if (!kinds.length) pictures.push({ role: "interior", building: b.id, file: null, caption: `${b.type} interior`, missing: `look --interior returned no pictures (exit ${r.code})` });
      for (const k of kinds) await take(dir, k, "interior", `${b.type}, inside (${k === "wide" ? "wide" : `looking ${k.slice(2)} deg`})`, { building: b.id });
    }
  }

  // The nearby labels, then the files.
  const entries = await gazetteer();
  const nearby = placesNear(entries, centre.x, centre.z, radius + 600)
    .filter((p) => !(Math.abs(p.x - target.x) < 1 && Math.abs(p.z - target.z) < 1))
    .slice(0, 8)
    .map((p) => ({ name: p.name, kind: p.kind, distance: Math.round(p.dist), direction: compass(p.x - centre.x, p.z - centre.z) }));
  const command = `tools\\cli scout ${target.source === "coordinates" ? `${target.x},${target.z}` : `"${target.name}"`}${radius !== 120 ? ` --radius ${radius}` : ""}`;
  const dossier = buildDossier({ target, radius, analysis, pictures, build, world: EVERON_WORLD, at: new Date().toISOString().slice(0, 10), command, nearby });
  mkdirSync(placeDir, { recursive: true });
  writeFileSync(join(placeDir, "dossier.json"), JSON.stringify(dossier, null, 1) + "\n");
  writeFileSync(join(placeDir, "dossier.md"), renderDossier(dossier));
  const shown = pictures.filter((p) => p.file).length;
  const lost = pictures.length - shown;
  console.log(`SUMMARY scout ok ${target.slug}: ${analysis.summary.buildings} buildings, ${analysis.summary.roads} roads, ${analysis.summary.freePlots} plots, ${shown} picture(s)${lost ? `, ${lost} missing` : ""} in ${Math.round((Date.now() - t0) / 1000)} s; ${relative(root, join(placeDir, "dossier.md"))}`);
  return 0;
}
