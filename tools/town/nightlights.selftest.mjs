// tools/town/nightlights.selftest.mjs - acceptance for slice #3534 (`node tools/town/nightlights.mjs`: the Saint-Philippe night-light layer).
//
// Run: node tools/town/nightlights.selftest.mjs
//   - a fitted business, a resident's home and the open church get a glow; a for-sale house, a locked planned shop, a shed and the
//     side building of a fitted business do not;
//   - a glow stands outside the street-facing wall of its building, at window height above the dossier's ground;
//   - a big three-floor building gets three glows, a small one a single glow; hours go off between 22 and 02, never in one blink;
//   - the committed layer matches the script, holds 25 to 40 glows and 10 to 15 lamp pieces, and overrides the prefab component by GUID.
// red-proof: with `occupied` letting a `partOf` side building through, three cases printed FAIL (the side building, 54 glows over the
// 40 cap, the committed layer no longer matching the script) (2026-11-19).
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { blockedLamps, buildAll, frameRows, ringRays, buildGlows, glowCount, GLOW, GLOW_COMPONENT, hourOff, layerPath, loadPlace, nearestRoad, occupied, renderLayer, seatsPath, streetWall, WALL_OFFSET } from "./nightlights.mjs";
import { gameVfsSync } from "../lib/height-frame.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${msg}`);
  }
};

const row = (id, purpose, status, extra = {}) => ({ id, purpose, status, use: "x", ...extra });
check(occupied(row("sp:b01", "b", "exists")), "a fitted business is occupied");
check(occupied(row("sp:b02", "b", "flight")), "a flight row is occupied");
check(occupied(row("sp:b03", "c", "planned")), "a resident's home is occupied");
check(occupied(row("sp:b04", "d", "planned", { card: "D1" })), "the open church is occupied");
check(!occupied(row("sp:b05", "a", "planned")), "a house for sale is dark");
check(!occupied(row("sp:b06", "b", "planned")), "a locked planned shop is dark");
check(!occupied(row("sp:b07", "e", "planned")), "an outbuilding is dark");
check(!occupied(row("sp:b08", "b", "exists", { partOf: "sp:b01" })), "a side building of a fitted business is dark");

// One house at the origin, its south wall (+z) facing a road at z = 20.
const square = (x, z, h) => [[x - h, z - h], [x + h, z - h], [x + h, z + h], [x - h, z + h]];
const house = (stem, floors, h) => ({ id: "x", stem, y: 10, floors, footprint: { w: 2 * h, d: 2 * h, cx: 0, cz: 0, corners: square(0, 0, h) } });
const roads = [[[-30, 20], [30, 20]]];
const near = nearestRoad(roads, [0, 0]);
check(Math.abs(near.at[1] - 20) < 1e-6 && Math.abs(near.d - 20) < 1e-6, "nearestRoad finds the road");
const wall = streetWall(square(0, 0, 8), [0, 0], near.at);
check(Math.abs(wall.mid[1] - 8) < 1e-6 && Math.abs(wall.normal[1] - 1) < 1e-6, "the wall that looks at the road is chosen");
const buildings = new Map([["sp:b01", house("ShopHouse_E_2I01t_x", 3, 8.5)], ["sp:b02", house("Shed_02_line", 1, 3)], ["sp:b03", house("House_Village_E_1I02", 1, 4)]]);
const made = buildGlows({
  purpose: { buildings: [row("sp:b01", "b", "exists"), row("sp:b02", "b", "exists"), row("sp:b03", "c", "planned"), row("sp:b04", "a", "planned")] },
  buildings,
  roads,
});
const byId = (id) => made.glows.filter((g) => g.id === id);
check(byId("sp:b01").length === 3, "a big three-floor building gets three glows");
check(byId("sp:b02").length === 0, "a shed gets none");
check(byId("sp:b03").length === 1, "a small home gets one");
check(byId("sp:b04").length === 0, "a for-sale house gets no glow");
check(made.errors.length === 0, `a dark row with no dossier building is no error (${made.errors.join("; ")})`);
for (const g of byId("sp:b01")) {
  check(g.z > 8.5 && g.z < 8.5 + WALL_OFFSET + 0.01, `a glow stands just outside the wall (z ${g.z})`);
  check(g.y > 10.5 && g.y < 17, `a glow stands at window height (y ${g.y})`);
}
check(glowCount(house("x", 1, 4)) === 1 && glowCount(house("x", 2, 6)) === 2, "glow count follows floors and size");
for (const id of ["sp:b01", "sp:b02", "sp:b03", "sp:b04", "sp:b05"]) {
  const off = hourOff(row(id, "c", "planned"), id);
  check(off >= 0 && off <= 2 || off === 23, `a home's window goes dark between 23 and 02 (${id}: ${off})`);
}
check(hourOff(row("sp:b09", "b", "exists", { use: "general store" }), "sp:b09") === 23, "a shop goes dark at 23");

// The committed place.
const real = loadPlace(ROOT, "saint-philippe");
// The committed layer is built on the island probe's seats (tools/town/probes), as `main` builds it.
const seatsFile = seatsPath(ROOT, "saint-philippe");
const all = frameRows(buildAll(real, existsSync(seatsFile) ? JSON.parse(readFileSync(seatsFile, "utf8")) : null)); // each row in its prefab's frame, as main() writes it
check(all.errors.length === 0, `Saint-Philippe lights build: ${all.errors.join("; ")}`);
check(all.glows.length >= 25 && all.glows.length <= 40, `25 to 40 glows (${all.glows.length})`);
check(all.lamps.length + all.heads.length >= 10 && all.lamps.length + all.heads.length <= 15, `10 to 15 lamp pieces (${all.lamps.length + all.heads.length})`);
const text = renderLayer("saint-philippe", all);
check(text.includes(`: "${GLOW}" {`) && text.includes(`LF_NightLightComponent "${GLOW_COMPONENT}"`), "the layer overrides the glow component by GUID");
check(!text.includes("CastShadow"), "a layer entity sets no shadow");
const file = layerPath(ROOT, "saint-philippe");
// The vanilla lamp's frame comes from the game paks (tools/lib/height-frame.mjs); without a game install (the GitHub runner)
// the rows cannot be put in their frame, so the committed layer is compared where the paks are (every dev box, the main guard).
if (gameVfsSync()) check(existsSync(file) && readFileSync(file, "utf8") === text, "Lights_saint-philippe.layer matches the script (rerun node tools/town/nightlights.mjs saint-philippe)");
else console.log("note: no game install here: the committed Lights layer is not compared (its lamps' frame needs the paks)");

// A lamp whose ring meets a rock, stands on a wall, or sits on a steep bank is blocked; a lamp on level ground beside a kerb is not;
// a rejected spot is skipped by the lamp builder (#3938: lamp 10 stood beside a granite rock).
{
  const lamp = (name) => ({ name, x: 100, y: 10, z: 200 });
  const ray = (id, root, py, sy, hit = true) => `ray ${id} d=${hit ? 3 : 7} len=7 hit=${hit ? root : "none"} root=${hit ? root : "none"} p=100,${py},200 sy=${sy}`;
  const ring = (roots) => roots.map((r, i) => ray(`L_r${i}`, r, 10, 10)).join("\n");
  const ok = ["Eden", "ConcreteKerb_01_2m_v2", "Eden", "Eden", "Eden", "Eden", "Eden", "Eden"];
  check(Object.keys(blockedLamps(`${ray("L", "Eden", 10, 10)}\n${ring(ok)}`, [lamp("L")])).length === 0, "a lamp on level ground beside a kerb is not blocked");
  const rock = [...ok];
  rock[3] = "GraniteCliff_01";
  check(blockedLamps(`${ray("L", "Eden", 10, 10)}\n${ring(rock)}`, [lamp("L")]).L?.includes("GraniteCliff_01"), "a granite rock within the ring blocks the lamp");
  check(blockedLamps(`${ray("L", "BrickWall_02_8m", 11.9, 10)}\n${ring(ok)}`, [lamp("L")], new Set(["L"])).L?.includes("stands on"), "a moved lamp on top of a wall is blocked");
  check(!blockedLamps(`${ray("L", "BrickWall_02_8m", 11.9, 10)}\n${ring(ok)}`, [lamp("L")]).L, "a committed lamp is not judged for what it stands on");
  const steep = ok.map((r, i) => ray(`L_r${i}`, r, 10, i === 2 ? 10.7 : 10)).join("\n");
  check(blockedLamps(`${ray("L", "Eden", 10, 10)}\n${steep}`, [lamp("L")]).L?.includes("ground varies"), "a 0.7 m step in the ground blocks the lamp");
  check(ringRays([lamp("L")]).length === 8, "eight ring rays per lamp");
}

if (failed) {
  console.log(`SUMMARY nightlights-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY nightlights-selftest ok");
