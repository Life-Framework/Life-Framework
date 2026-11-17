import fs from "node:fs";
import { load } from "./parse.mjs";
import { typeOf } from "./classify.mjs";

const D = "docs/everon";
fs.mkdirSync(D, { recursive: true });
const B = JSON.parse(fs.readFileSync("tmp/everon/buildings.json", "utf8"));
const ms = JSON.parse(fs.readFileSync("tmp/everon/measure.json", "utf8"));
const mb = new Map(ms.map((m) => [m.ref, m.bounds]));
const world = load("tmp/everon/raw/world.txt");
const short = (p) => p.split("/").pop().replace(".et", "");

// furnished sample from the four town dumps
const towns = { "Saint Philippe": "saintphilippe", "Saint Pierre": "saintpierre", Morton: "morton", Montignac: "montignac" };
const furn = new Map();
for (const f of Object.values(towns)) {
  const all = load(`tmp/everon/raw/${f}.txt`);
  const props = all.filter((r) => /^Prefabs\/Props\/(Furniture|Decorations)/.test(r.path));
  for (const b of all) {
    if (!typeOf(b.path) || !mb.get(b.res) || /Furniture\/|BuildingAddons/.test(b.path)) continue;
    const bb = mb.get(b.res);
    let n = 0;
    const a = (b.yaw * Math.PI) / 180;
    for (const r of props) {
      const dx = r.x - b.x, dz = r.z - b.z;
      if (Math.abs(dx) > 40 || Math.abs(dz) > 40) continue;
      const lx = dx * Math.cos(a) - dz * Math.sin(a), lz = dx * Math.sin(a) + dz * Math.cos(a);
      if (lx >= bb.min[0] && lx <= bb.max[0] && lz >= bb.min[2] && lz <= bb.max[2] && r.y >= b.y - 3 && r.y <= b.y + bb.max[1]) n++;
    }
    const e = furn.get(b.res) || { n: 0, sum: 0, max: 0 };
    e.n++; e.sum += n; e.max = Math.max(e.max, n);
    furn.set(b.res, e);
  }
}
for (const p of B.prefabs) {
  const e = furn.get(p.res);
  p.furnishedSample = e ? { buildingsSampled: e.n, avgFurnitureProps: Math.round(e.sum / e.n), maxFurnitureProps: e.max } : null;
}

// instances
const pidx = new Map(B.prefabs.map((p, i) => [p.res, i]));
const PL = B.places;
const inst = [];
for (const r of world) {
  const i = pidx.get(r.res);
  if (i === undefined) continue;
  let best = -1, bs = 9;
  PL.forEach((p, k) => { const s = Math.hypot(r.x - p.x, r.z - p.z) / p.radiusM; if (s < 1 && s < bs) { bs = s; best = k; } });
  inst.push([i, +r.x.toFixed(1), +r.z.toFixed(1), Math.round(r.yaw), best]);
}

const types = {};
for (const p of B.prefabs) {
  const t = (types[p.type] ??= { prefabs: 0, instances: 0, enterablePrefabs: 0, enterableInstances: 0 });
  t.prefabs++; t.instances += p.instances;
  if (p.enterable) { t.enterablePrefabs++; t.enterableInstances += p.instances; }
}
const meta = {
  generated: "2026-11-16",
  source: "In-engine dump of the vanilla Everon world (Worlds/CoreOnEveron: a SubScene of worlds/Eden/Eden.ent) on a dedicated server, Arma Reforger build 26266485. Every entity with a prefab under Prefabs/Structures, de-duplicated across the 400 m query tiles. Footprint, floors and cellar from `tools\\cli measure --surfaces` (collider bounds and horizontal surfaces).",
  coordinateSystem: "Enfusion world metres: x east, z north, y up; Everon spans about x -107..12348, z -100..12654.",
  fields: {
    footprintM: "[x,z] extent of the model bounds (local axes)",
    heightM: "top of the model bounds above the entity origin",
    basementM: "depth of the model below the origin when more than 2 m (cellar)",
    floors: "count of walkable floor levels (large horizontal collider surfaces) above y -1; null when the prefab was not measured",
    doorChildren: "door entities spawned under the building (per instance)",
    enterable: "door entities present and at least one walkable floor level: the building has a real interior (a heuristic, not a play test)",
    furnishedSample: "in the four town dumps, how many Furniture/Decorations props sit inside the building's footprint",
    towns: "nearest named place and instance count (rural = outside every place radius)",
  },
  instanceColumns: ["prefabIndex", "x", "z", "yawDeg", "placeIndex"],
  totalInstances: inst.length,
};
fs.writeFileSync(`${D}/catalog-buildings.json`, JSON.stringify({ meta, types, prefabs: B.prefabs, places: B.places, naturalPlaces: B.naturalPlaces, instances: inst }));

const TYPES = [
  ["townhouse", "Town houses (2-3 floor row and apartment-style houses)"], ["house", "Village, wooden and prefab houses"], ["villa", "Villas"],
  ["farmhouse", "Farm houses"], ["shop", "Shops and shop-houses"], ["pub", "Pubs"], ["kiosk", "Kiosks and booths"], ["fuel", "Fuel stations"],
  ["church", "Churches"], ["chapel", "Chapels"], ["government", "Government, municipal office, school"], ["police", "Police (the police-variant villa)"],
  ["fire", "Fire stations"], ["hotel", "Hotels"], ["garage", "Garages and workshops"], ["shed", "Sheds"],
  ["farm", "Barns, cow sheds, hay lofts, weigh stations"], ["farm-small", "Hen houses and greenhouses"],
  ["industrial", "Industrial: warehouses, factory, offices, sawmill, cabins, metal sheds"], ["industrial-tank", "Fuel tanks and silos"],
  ["industrial-tower", "Smokestacks and light towers"], ["military", "Military: barracks, bunkers, garages, guard boxes, fortifications"],
  ["airport", "Airport buildings"], ["recreation", "Cottages and recreation"], ["port", "Port: piers, quays, breakwater, bollards"],
  ["castle", "Montfort Castle pieces"], ["ruin", "House ruins"], ["waterworks", "Waterworks (wells, reservoirs, treatment plant)"],
];
const tick = "`";
let md = `# Everon building catalog

Generated ${meta.generated}. Machine-readable twin: [catalog-buildings.json](catalog-buildings.json) (all ${B.prefabs.length} prefabs, ${inst.length} placed instances with coordinates and place index, ${B.places.length} named places with buildings plus ${B.naturalPlaces.length} natural places). Props: [catalog-props.md](catalog-props.md). Saint Philippe in full: [saint-philippe.md](saint-philippe.md).

## How this was made

${meta.source}

- **Coordinates**: ${meta.coordinateSystem}
- **Instances** are every placed copy of a prefab; variants (colour, ${tick}_V2${tick}) are separate rows.
- **Floors** are counted from the collider's horizontal surfaces (areas of at least 15 % of the footprint, levels merged within 1 m). A cellar shows in the ${tick}cellar${tick} column. It is a measurement of the model, not a walk through it.
- **Interior = yes** means the prefab spawns door entities and has at least one walkable floor. Vanilla Everon interiors are furnished: the ${tick}furn${tick} column is the average count of Furniture and Decorations props inside the footprint in the four town dumps.
- Method files: [method/](method/) (the in-engine dump script and the node build scripts).

## Summary

| type | prefabs | instances | with interior (instances) |
|---|---:|---:|---:|
`;
for (const [t, l] of TYPES) { const s = types[t]; if (!s) continue; md += `| ${l} | ${s.prefabs} | ${s.instances} | ${s.enterableInstances} |\n`; }
md += "\n";
for (const [t, l] of TYPES) {
  const ps = B.prefabs.filter((p) => p.type === t);
  if (!ps.length) continue;
  md += `## ${l}\n\n| prefab | ${tick}{GUID}path${tick} | footprint m | height | floors | cellar | doors | interior | furn | count | used in |\n|---|---|---|---:|---:|---:|---:|---|---:|---:|---|\n`;
  for (const p of ps.slice(0, t === "port" ? 30 : 200)) {
    const tw = Object.entries(p.towns).slice(0, 4).map(([k, v]) => `${k} ${v}`).join(", ");
    md += `| ${short(p.path)} | ${tick}${p.res}${tick} | ${p.footprintM[0]}×${p.footprintM[1]} | ${p.heightM} | ${p.floors ?? "?"} | ${p.basementM || ""} | ${p.doorChildren} | ${p.enterable ? "yes" : "no"} | ${p.furnishedSample ? p.furnishedSample.avgFurnitureProps : ""} | ${p.instances} | ${tw} |\n`;
  }
  if (t === "port" && ps.length > 30) md += `\n(${ps.length - 30} more port pieces in the JSON)\n`;
  md += "\n";
}
md += `## Named places

Counts include every building type inside the place's radius (city 800 m, town 550 m, village 380 m, hamlet 230 m, site 300 m; each building goes to the place it is nearest to in radius units). Names are the map's own descriptors (${tick}#AR-MapLocation_<key>${tick}). Saint Philippe's map key is ${tick}StPhillipe${tick}.

`;
const cols = ["townhouse", "house", "villa", "farmhouse", "shop", "pub", "church", "chapel", "kiosk", "fuel", "government", "police", "fire", "hotel", "garage", "shed", "farm", "industrial", "military", "port", "ruin"];
const hdr = ["townhouse", "house", "villa", "farmh.", "shop", "pub", "church", "chapel", "kiosk", "fuel", "govt", "police", "fire", "hotel", "garage", "shed", "farm", "indust.", "military", "port", "ruin"];
for (const kind of ["city", "town", "village", "hamlet", "site", "castle"]) {
  const ps = B.places.filter((p) => p.kind === kind).sort((a, b) => b.total - a.total);
  if (!ps.length) continue;
  md += `### ${kind}s\n\n| place | key | x | z | ${hdr.join(" | ")} | total |\n|---|---|---:|---:|${hdr.map(() => "---:").join("|")}|---:|\n`;
  for (const p of ps) md += `| ${p.name} | ${p.key} | ${p.x} | ${p.z} | ${cols.map((c) => p.buildings[c] || "").join(" | ")} | ${p.total} |\n`;
  md += "\n";
}
md += `### Natural places (named, no buildings counted)

Map type: 63 hill, 64 headland or location, 65 island, 66 pond or stream, 67 lake, 68 bay, 69 strait or gulf, 70 forest or ridge, 71 valley.

| place | map type | x | z |
|---|---:|---:|---:|
`;
for (const n of B.naturalPlaces) md += `| ${n.name} | ${n.mapType} | ${n.x} | ${n.z} |\n`;
fs.writeFileSync(`${D}/catalog-buildings.md`, md);
console.log("md bytes", md.length, "json bytes", fs.statSync(`${D}/catalog-buildings.json`).size);
