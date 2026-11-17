// The pub's pass 1 (#2294): the cafe radio, the chess table and its two chairs, and one shadowless light per room,
// written into the furniture prefab after prune.mjs has copied the vanilla props (prune swaps every chair for a seat).
// The bar sells over the counter through the `store` block in spec.json (`tools\cli building run pub --only store`).
//
//   node tools/buildings/pub/decor.mjs          rewrite the decor block in Furniture/Pub_furniture_01.et
//   node tools/buildings/pub/decor.mjs --check  exit 1 when the prefab is not what this script writes
//
// Building space is prefab space: x across, y up from the taproom floor, z along; the street front is -z. Heights
// are snapped onto the real top under each prop (surface.mjs: `on` names the support, a vanilla entity ID; an LF prefab is measured as the base-game prop in `as`). Entity
// IDs start with CAFE, so a re-run removes its own old block. Lights: ClipEV -10, NearPlane 0.3, CastShadow 0 (the
// writer puts all three on every light), short radius; the medium budget is 6 lights and 1 shadow caster.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { prefabChildren, snapItems } from "../pipeline/surface.mjs";
import { renderBlock, swapBlock } from "./decor-lib.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const FURNITURE = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "Pub", "Furniture", "Pub_furniture_01.et");
const PREFIX = "CAFE";

const P = {
  radio: "{C6AF6D2D6EA3CBF9}Prefabs/Atmosphere/LF_Radio_Pub.et", // Atmosphere: the pub's music, 10:00 to 02:00
  chess: "{33881F3E4E64549D}Prefabs/Props/Chess/LF_ChessTable.et", // Chess: a table two players sit down at
  seat: "{69D6D9BFDFEC3225}Prefabs/Props/Seats/LF_Seat_Chair_02_green.et", // Emote: Sit
};

const decor = {
  items: [
    // the cafe radio on the corner table by the toilets door (vanilla Table_01 58E48906EA714F94)
    { path: P.radio, as: "{B68D3829D4442F80}Prefabs/Props/Civilian/RadioSmall_01_Base.et", x: 3.95, y: 0.8, z: 0.75, yaw: -90, on: "58E48906EA714F94" },
    // the chess table in the cafe's street window, a chair either side
    { path: P.chess, as: "{00C9C0860D1D8A84}Prefabs/Props/Furniture/TableSmall_01/TableSmall_01_DarkWood.et", x: 2.2, y: 0, z: -7.4 },
    { path: P.seat, as: "{0DC4F830876C6671}Prefabs/Props/Furniture/Chair_02/Chair_02_green.et", x: 1.3, y: 0, z: -7.4, yaw: 90 },
    { path: P.seat, as: "{0DC4F830876C6671}Prefabs/Props/Furniture/Chair_02/Chair_02_green.et", x: 3.1, y: 0, z: -7.4, yaw: -90 },
  ],
  lights: [
    { x: -4.2, y: 3.0, z: -6.4, radius: 7 }, // taproom, over the bar front and the street tables
    { x: 2.2, y: 3.0, z: -4.0, radius: 6 }, // cafe
    { x: 6.0, y: 3.0, z: -6.0, radius: 4, lv: 5 }, // keg store
    { x: -6.2, y: 3.0, z: -0.8, radius: 4 }, // kitchen
    { x: 6.0, y: 3.0, z: 3.2, radius: 4, lv: 5 }, // toilets
    { x: -2.0, y: 6.4, z: -3.5, radius: 7, lv: 6 }, // upstairs landing and flat
  ],
};

const text = readFileSync(FURNITURE, "utf8");
const vanilla = prefabChildren(swapBlock(text, PREFIX, "")).filter((e) => e.path);
// An LF prefab is measured through the base-game prop it inherits (`as`), then written under its own path.
const probe = decor.items.map((o) => ({ ...o, path: (o.as ?? o.path).replace(/^\{[0-9A-F]+\}/, "") }));
const snap = await snapItems(probe, { fixed: vanilla });
probe.forEach((o, i) => (decor.items[i].y = o.y));
const next = swapBlock(text, PREFIX, await renderBlock(ROOT, PREFIX, decor));
if (process.argv.includes("--check")) {
  const same = next === text;
  console.log(same ? "decor: the furniture prefab is current" : "decor: the furniture prefab differs; run node tools/buildings/pub/decor.mjs");
  process.exit(same ? 0 : 1);
}
writeFileSync(FURNITURE, next);
console.log(`decor: ${decor.items.length} prop(s), ${decor.lights.length} light(s) written to Furniture/Pub_furniture_01.et; ${snap.summary}${snap.unmeasured.length ? ` (unmeasured: ${snap.unmeasured.join(", ")})` : ""}`);
