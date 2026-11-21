// tools/town/kit-grid.mjs: lay a kit's rows into a grid claim's layer so `look grid:<slug>` can prove the kit (#3857).
//
//   node tools/town/kit-grid.mjs put <kit> <slug> <x> <z> [yawDeg] [--dry]   write the rows as Kit_<kit>_<id> entities
//   node tools/town/kit-grid.mjs clear <kit> <slug>                         remove them again
//
// `place arrange` takes three rows of props; a kit is 10 to 27 rows with fences and stacked parts, so its offsets
// (already rebased to y 0, `kit apply`) are written as they are, on the flat grid ground, and the audit
// (`tools\cli place verify`, `audit placement --grid`) and `look contact` judge the result. The layer is the claim's
// own: the pack grid world for a claim with "pack": true, GridWorld's otherwise. Idempotent: `put` replaces the
// entities a previous `put` of the same kit wrote.
import { createHash } from "node:crypto";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { applyKit, bayClearName, bayRowName } from "./kit.mjs";
import { prefabInfo } from "../place/place.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r3 = (n) => Math.round(n * 1000) / 1000;
const ident = (s) => String(s).replace(/[^A-Za-z0-9]+/g, "_").replace(/^_|_$/g, "");

export function layerFileOf(slug) {
  const claim = JSON.parse(readFileSync(join(ROOT, "tools", "grid", "cells", `${slug}.json`), "utf8"));
  const dir = claim.pack
    ? "addons/LF_Everon1989/Worlds/Everon1989Grid/Everon1989Grid_Layers"
    : "addons/LifeFramework/Worlds/GridWorld/GridWorld_Layers";
  return join(ROOT, dir, claim.layer || `Grid_${slug.replace(/-/g, "_")}.layer`);
}

// Drop every top-level entity whose name starts with `prefix` (brace depth counted line by line).
export function stripEntities(text, prefix) {
  const out = [];
  let skip = false;
  let depth = 0;
  for (const line of text.split("\n")) {
    if (!skip && depth === 0 && new RegExp(`^\\w+ ${prefix}`).test(line)) skip = true;
    if (skip) {
      if (line.trim().endsWith("{")) depth++;
      if (line.trim() === "}") depth--;
      if (depth === 0 && line.trim() === "}") skip = false;
      continue;
    }
    out.push(line);
  }
  return out.join("\n");
}

// The entity class the prefab's own file starts with (a StaticModelEntity prefab loaded as a GenericEntity does not render).
const classOf = (r) => prefabInfo(ROOT, String(r.prefab).replace(/^{[0-9A-Fa-f]{16}}/, "")).cls || "GenericEntity";

// The top-level entity blocks of a layer text as { name -> body lines }, without the lines a put writes itself (position, angles,
// scale). A body keeps its component blocks and nested entities as the layer wrote them (#3987: a bay row that came from a kit
// must carry its till, sign and action blocks, or an extract writes bare props over the kit's source).
export function entityBodies(text) {
  const out = new Map();
  let name = null;
  let body = [];
  for (const line of text.split("\n")) {
    const head = name === null ? /^\w+ ([\w.-]+)(?: : "[^"]*")? \{$/.exec(line) : null;
    if (head) {
      name = head[1];
      body = [];
    } else if (name !== null && line === "}") {
      out.set(name, body);
      name = null;
    } else if (name !== null && !/^ (coords|angles|angleX|angleY|angleZ|scale)\s/.test(line)) body.push(line);
  }
  return out;
}

// The kit's source layers (the archive beside the kit, then its `also` files) as one name -> body map; the first file to name an entity wins.
export function sourceBodies(kit, root = ROOT) {
  const out = new Map();
  for (const f of [kit.source?.file, ...(kit.source?.also ?? [])]) {
    if (!f || !f.endsWith(".layer") || !existsSync(join(root, f))) continue;
    for (const [n, b] of entityBodies(readFileSync(join(root, f), "utf8"))) if (!out.has(n)) out.set(n, b);
  }
  return out;
}

// The row's own body: under its recorded layer name, else under the name a previous put gave it.
const bodyOf = (kit, r, bodies) => bodies.get(bayRowName(kit.kit, r.id)) ?? bodies.get(r.name) ?? null;

// The clear carrier of an authoring bay: the same LF_BaseWorldClearComponent `town stamp` writes per copy, so the bay's audit and its
// look see the building without the clutter the kit hides (#3987). prefabs: resource names. Null when there is nothing to clear.
export function clearCarrierBlock(kitName, buildingPrefab, prefabs, x, z, y = 0) {
  if (!prefabs.length || !buildingPrefab) return null;
  const guid = createHash("sha1").update(`bay-clear|${kitName}`).digest("hex").slice(0, 16).toUpperCase();
  return [
    `GenericEntity ${bayClearName(kitName)} {`,
    " components {",
    `  LF_BaseWorldClearComponent "{${guid}}" {`,
    `   m_sBuilding "${buildingPrefab}"`,
    "   m_aPrefabs {",
    ...prefabs.map((p) => `    "${p}"`),
    "   }",
    "  }",
    " }",
    ` coords ${x} ${y} ${z}`,
    "}",
  ].join("\n");
}

export const clearPrefabsOf = (kit) => (kit.stamp?.clear ?? []).map((c) => (typeof c === "string" ? c : c?.prefab)).filter(Boolean);

// y: the height the kit's offsets count from (an authoring bay's building origin; flat ground, 0, for any other use).
export function entityBlocks(kit, x, z, yaw, skip = [], bodies = new Map(), y = 0) {
  const rows = applyKit(kit, x, z, yaw, y);
  return kit.rows.map((r, i) => {
    if (skip.includes(r.id)) return null;
    const a = rows[i];
    const lines = [`${classOf(r)} ${bayRowName(kit.kit, r.id)} : "${r.prefab}" {`, ...(bodyOf(kit, r, bodies) ?? []), ` coords ${a.coords.join(" ")}`];
    if (r.pitch || r.roll) lines.push(` angles ${r3(r.pitch || 0)} ${a.yaw} ${r3(r.roll || 0)}`);
    else if (a.yaw) lines.push(` angleY ${a.yaw}`);
    if (r.scale && r.scale !== 1) lines.push(` scale ${r.scale}`);
    lines.push("}");
    return lines.join("\n");
  }).filter(Boolean);
}

function main(argv) {
  const [cmd, kitName, slug, ...rest] = argv;
  if (!["put", "clear"].includes(cmd) || !kitName || !slug) {
    console.error("usage: kit-grid.mjs put <kit> <slug> <x> <z> [yawDeg] [--y=<h>] [--dry] | clear <kit> <slug>");
    return 2;
  }
  const kitFile = join(ROOT, "tools", "town", "kits", `${kitName}.json`);
  if (!existsSync(kitFile)) { console.error(`kit-grid: no kit ${kitName}`); return 1; }
  const kit = JSON.parse(readFileSync(kitFile, "utf8"));
  const layer = layerFileOf(slug);
  if (!existsSync(layer)) { console.error(`kit-grid: ${layer} is missing: claim the cell first (tools\\cli grid place ${slug})`); return 1; }
  const prefix = `Kit_${ident(kit.kit)}_`;
  let text = stripEntities(stripEntities(readFileSync(layer, "utf8"), prefix), bayClearName(kit.kit));
  if (cmd === "put") {
    const [x, z, yaw = 0] = rest.filter((a) => !a.startsWith("--")).map(Number);
    const y = Number((rest.find((a) => a.startsWith("--y=")) || "--y=0").slice(4));
    if (![x, z, yaw, y].every(Number.isFinite)) { console.error("kit-grid: put needs numeric <x> <z> [yaw] [--y=<building height>]"); return 2; }
    const skip = (rest.find((a) => a.startsWith("--skip=")) || "").slice(7).split(",").filter(Boolean);
    const blocks = entityBlocks(kit, x, z, yaw, skip, sourceBodies(kit), y);
    const clear = clearPrefabsOf(kit);
    const carrier = clearCarrierBlock(kit.kit, kit.stamp?.prefab ?? kit.building?.vanilla, clear, x, z, y);
    const rowCount = blocks.length;
    if (carrier) blocks.push(carrier);
    text = text.replace(/\n*$/, "\n") + blocks.join("\n") + "\n";
    console.log(`kit-grid: ${rowCount} ${kit.kit} rows at ${x},${z} yaw ${yaw}${carrier ? `, clear carrier for ${clear.length} props` : ""}`);
  }
  if (rest.includes("--dry")) { console.log("kit-grid: dry run, nothing written"); return 0; }
  writeFileSync(layer, text);
  console.log(`SUMMARY kit-grid ${cmd} ok kit=${kit.kit} layer=${layer.slice(ROOT.length + 1)}`);
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(main(process.argv.slice(2)));
