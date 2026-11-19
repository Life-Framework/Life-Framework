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
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { applyKit } from "./kit.mjs";
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

export function entityBlocks(kit, x, z, yaw, skip = []) {
  const rows = applyKit(kit, x, z, yaw);
  return kit.rows.map((r, i) => {
    if (skip.includes(r.id)) return null;
    const a = rows[i];
    const lines = [`${classOf(r)} Kit_${ident(kit.kit)}_${ident(r.id)} : "${r.prefab}" {`, ` coords ${a.coords.join(" ")}`];
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
    console.error("usage: kit-grid.mjs put <kit> <slug> <x> <z> [yawDeg] [--dry] | clear <kit> <slug>");
    return 2;
  }
  const kitFile = join(ROOT, "tools", "town", "kits", `${kitName}.json`);
  if (!existsSync(kitFile)) { console.error(`kit-grid: no kit ${kitName}`); return 1; }
  const kit = JSON.parse(readFileSync(kitFile, "utf8"));
  const layer = layerFileOf(slug);
  if (!existsSync(layer)) { console.error(`kit-grid: ${layer} is missing: claim the cell first (tools\\cli grid place ${slug})`); return 1; }
  const prefix = `Kit_${ident(kit.kit)}_`;
  let text = stripEntities(readFileSync(layer, "utf8"), prefix);
  if (cmd === "put") {
    const [x, z, yaw = 0] = rest.filter((a) => !a.startsWith("--")).map(Number);
    if (![x, z, yaw].every(Number.isFinite)) { console.error("kit-grid: put needs numeric <x> <z> [yaw]"); return 2; }
    const skip = (rest.find((a) => a.startsWith("--skip=")) || "").slice(7).split(",").filter(Boolean);
    const blocks = entityBlocks(kit, x, z, yaw, skip);
    text = text.replace(/\n*$/, "\n") + blocks.join("\n") + "\n";
    console.log(`kit-grid: ${blocks.length} ${kit.kit} rows at ${x},${z} yaw ${yaw}`);
  }
  if (rest.includes("--dry")) { console.log("kit-grid: dry run, nothing written"); return 0; }
  writeFileSync(layer, text);
  console.log(`SUMMARY kit-grid ${cmd} ok kit=${kit.kit} layer=${layer.slice(ROOT.length + 1)}`);
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(main(process.argv.slice(2)));
