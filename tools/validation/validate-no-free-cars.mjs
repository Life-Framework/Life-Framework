// tools/validation/validate-no-free-cars.mjs
// inputs: addons/LF_Everon1989/Worlds/MainWorld/**/*.layer
//
// No free cars on the island (2026-11-22 playtest: "a bunch of unlocked cars that one could steal"). A vehicle
// placed in an island layer as a prop (the taxi rank's cab, the woodlot truck, the cash van, the depot tanker) is
// a real drivable car: a vanilla prefab has no lock that replicates, and an LF prefab starts unlocked. Each one must
// carry the lock and start locked: the inherited lock component overridden by its GUID with m_bDebugLocked 1,
//   LF_VehicleLockComponent "{54D6FBC0EA806FE5}" {
//    m_bDebugLocked 1
//   }
// A parked car the town should have goes on a parked-car marker instead (LF_ParkedCarMarker.et), which puts out a
// locked car and brings one back after it is taken.
// red-proof: tools/validation/no-free-cars-selftest.mjs plants an unlocked vanilla car and an LF car with the lock off.
import { readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

// A drivable vehicle prefab: the vanilla families and the LF children under Prefabs/Vehicles/<family>/.
const VEHICLE = /"\{[0-9A-Fa-f]{16}\}Prefabs\/Vehicles\/(Wheeled|Tracked|Helicopters|Air|Water)\//;
const HEAD = /^\S+(?:\s+(\S+))?\s*:\s*"(\{[0-9A-Fa-f]{16}\}[^"]+)"\s*\{\s*$/;
const LOCK = /LF_VehicleLockComponent\s+"\{54D6FBC0EA806FE5\}"\s*\{[^{}]*\bm_bDebugLocked\s+1\b/;

// [{ line, name, prefab }] for every top-level vehicle entity in a layer's text that does not start locked.
export function freeCars(layerText) {
  const lines = layerText.split("\n");
  const hits = [];
  for (let i = 0; i < lines.length; i++) {
    const head = HEAD.exec(lines[i]);
    if (!head || !VEHICLE.test(lines[i])) continue;
    let end = i + 1;
    while (end < lines.length && lines[end] !== "}") end++;
    const body = lines.slice(i + 1, end).join("\n");
    if (!LOCK.test(body)) hits.push({ line: i + 1, name: head[1] || "(unnamed)", prefab: head[2] });
    i = end;
  }
  return hits;
}

function layersUnder(dir) {
  const out = [];
  for (const name of readdirSync(dir)) {
    const path = join(dir, name);
    if (statSync(path).isDirectory()) out.push(...layersUnder(path));
    else if (name.endsWith(".layer")) out.push(path);
  }
  return out;
}

if (process.argv[1] && process.argv[1].split("\\").join("/").endsWith("validate-no-free-cars.mjs")) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const dir = join(root, "addons", "LF_Everon1989", "Worlds", "MainWorld");
  let bad = 0;
  for (const file of layersUnder(dir)) {
    for (const h of freeCars(readFileSync(file, "utf8"))) {
      bad++;
      const rel = relative(root, file).split("\\").join("/");
      console.log(`FAIL ${rel}:${h.line}: ${h.name} (${h.prefab}) is a free car: override LF_VehicleLockComponent "{54D6FBC0EA806FE5}" with m_bDebugLocked 1, or use a parked-car marker`);
    }
  }
  if (bad) process.exit(1);
  console.log("validate-no-free-cars: OK");
}
