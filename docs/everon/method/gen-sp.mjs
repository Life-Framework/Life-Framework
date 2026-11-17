import fs from "node:fs";
import { load } from "./parse.mjs";
import { typeOf } from "./classify.mjs";

const D = "docs/everon";
const S = JSON.parse(fs.readFileSync("tmp/everon/saint-philippe.json", "utf8"));
const B = JSON.parse(fs.readFileSync(`${D}/catalog-buildings.json`, "utf8"));
const t = "`";
const LABEL = {
  townhouse: "town house", house: "house", villa: "villa", farmhouse: "farm house", shop: "shop", pub: "pub", kiosk: "kiosk", fuel: "fuel station",
  church: "church", chapel: "chapel", government: "government", police: "police", fire: "fire station", hotel: "hotel", garage: "garage",
  shed: "shed", farm: "barn / farm", "farm-small": "hen house / greenhouse", industrial: "industrial", "industrial-tank": "tank / silo",
  military: "military", ruin: "ruin", recreation: "cottage", airport: "airport",
};
const order = ["police", "government", "fire", "church", "chapel", "pub", "shop", "kiosk", "fuel", "hotel", "townhouse", "villa", "house", "farmhouse", "garage", "industrial", "farm", "industrial-tank", "military", "recreation", "airport", "farm-small", "shed", "ruin"];
const rows = S.buildings;
const counts = S.counts;

// the full data file: every building row within 450 m, plus port pieces and top props
fs.writeFileSync(`${D}/saint-philippe.json`, JSON.stringify({
  generated: "2026-11-16",
  centre: S.centre, radiusM: S.radiusM,
  note: "Every Prefabs/Structures building within 450 m of the map descriptor StPhillipe (4503, 10772). interior: door entities plus a walkable floor. propsInside: Furniture and Decorations props whose position falls inside the footprint (approximate, rotation convention unverified).",
  counts, buildings: rows.map((r) => ({ type: r.type, prefab: r.prefab, res: r.res, x: r.x, z: r.z, yawDeg: r.yaw, distM: r.distM, footprintM: r.footprintM, floors: r.floors, interior: r.enterable, doorChildren: r.doorChildren, propsInside: r.propsInside, topProps: r.topProps })),
  pierQuayPieces: S.pierQuayPieces, topProps: S.topProps, asciiMap: S.asciiMap,
}));

let md = `# Saint Philippe: building survey

Generated 2026-11-16 from the in-engine dump of vanilla Everon ([catalog-buildings.md](catalog-buildings.md) says how). Data: [saint-philippe.json](saint-philippe.json) (one row per building: prefab, x, z, yaw, interior flag, props inside).

**Centre**: the map descriptor ${t}#AR-MapLocation_StPhillipe${t} at x 4503, z 10772 (the town centre; arrival spawn marker of the CoreOnEveron walk proof is at 4471, 10535). Everything below is within 450 m of it. Other map places nearby: Harbour site, Meaux (village) 1.3 km south at 4520, 9468.

## What is here

| type | count |
|---|---:|
`;
for (const k of order) if (counts[k]) md += `| ${LABEL[k] || k} | ${counts[k]} |\n`;
md += `\nSheds and hen houses smaller than 6 m² footprint were not measured and are left out of these counts (the 400 m counts in the planning notes include them: about 100 sheds). Pier and quay pieces (Pier_01, PierConcrete, breakwater and wooden piers): ${S.pierQuayPieces.length} placed pieces, mostly west of the centre.\n\n`;

// landmarks (non-house civic) table
const civic = ["police", "government", "fire", "church", "chapel", "pub", "shop", "kiosk", "fuel", "hotel"];
md += `## Civic and commercial buildings (all of them)\n\n| type | prefab | ${t}{GUID}path${t} | x | z | yaw | dist m | footprint m | floors | furniture inside |\n|---|---|---|---:|---:|---:|---:|---|---:|---:|\n`;
for (const r of rows.filter((r) => civic.includes(r.type)).sort((a, b) => order.indexOf(a.type) - order.indexOf(b.type) || a.distM - b.distM))
  md += `| ${LABEL[r.type]} | ${r.prefab} | ${t}${r.res}${t} | ${r.x} | ${r.z} | ${r.yaw} | ${r.distM} | ${r.footprintM[0]}×${r.footprintM[1]} | ${r.floors ?? "?"} | ${r.propsInside} |\n`;

md += `\n## Housing (every house with an interior, nearest first)\n\n| type | prefab | x | z | yaw | dist m | footprint m | floors | doors | furniture inside | most common props |\n|---|---|---:|---:|---:|---:|---|---:|---:|---:|---|\n`;
for (const r of rows.filter((r) => ["townhouse", "villa", "house", "farmhouse"].includes(r.type)).sort((a, b) => a.distM - b.distM))
  md += `| ${LABEL[r.type]} | ${r.prefab} | ${r.x} | ${r.z} | ${r.yaw} | ${r.distM} | ${r.footprintM[0]}×${r.footprintM[1]} | ${r.floors ?? "?"} | ${r.doorChildren} | ${r.propsInside} | ${r.topProps} |\n`;

md += `\n## Garages, industrial, farm and military buildings\n\n| type | prefab | x | z | yaw | dist m | footprint m | interior |\n|---|---|---:|---:|---:|---:|---|---|\n`;
for (const r of rows.filter((r) => ["garage", "industrial", "farm", "industrial-tank", "military", "recreation", "airport"].includes(r.type)).sort((a, b) => order.indexOf(a.type) - order.indexOf(b.type) || a.distM - b.distM))
  md += `| ${LABEL[r.type]} | ${r.prefab} | ${r.x} | ${r.z} | ${r.yaw} | ${r.distM} | ${r.footprintM[0]}×${r.footprintM[1]} | ${r.enterable ? "yes" : "no"} |\n`;

md += `\nSheds (${counts.shed || 0}), hen houses and greenhouses (${counts["farm-small"] || 0}) and house ruins (${counts.ruin || 0}) are in [saint-philippe.json](saint-philippe.json); they are yard clutter around the houses.\n\n`;

md += `## Layout map\n\nEach character is a 30 m cell, north up, 900 m square centred on the town centre (cell column 15, row 15 is the centre). Where several buildings share a cell the more civic one is shown.\n\n`;
const lg = Object.entries(S.asciiMap.legend).filter(([k]) => k !== "farmhouse").map(([k, v]) => `${v} ${LABEL[k] || k}`);
md += `Legend: ${lg.join(", ")}, ~ pier/quay piece, . nothing.\n\n${t}${t}${t}\n${S.asciiMap.rows.join("\n")}\n${t}${t}${t}\n\n`;
md += `Cell (col, row) to world: x = ${S.asciiMap.originX} + col × 30, z = ${S.asciiMap.topZ} − row × 30.\n\n`;

md += `## Props placed around the town (non-vegetation, non-structure)\n\nThe 40 most common prop prefabs inside the 450 m dump, with counts. Interiors are furnished with these in the vanilla world; the full prop catalog is [catalog-props.md](catalog-props.md).\n\n| ${t}{GUID}path${t} | count |\n|---|---:|\n`;
for (const p of S.topProps) md += `| ${t}${p.res}${t} | ${p.count} |\n`;
fs.writeFileSync(`${D}/saint-philippe.md`, md);
console.log("sp md", md.length);
