// tools/validation/town-fit-seat-selftest.mjs
// inputs: tools/town/**, tools/buildings/pipeline/surface.mjs, tools/validation/town-fit-seat-selftest.mjs
//
// Proves that `town fit` seats each kit row on the HOST building (tools/town/seat.mjs), on fixture hosts read through an
// injected reader (no game files, no server): a box room 10 x 8 m with inner wall faces and a floor top whose height the
// case picks. The island audits on #2369, #2378 and #2394 are the cases:
//   - floor offset: a kit row at its playtest floor (0.199) stands on a host floor at 0.5 (it sank) or 0 (it floated);
//   - wall offset: a wall-mounted row (`on: "wall"`, a hung kind such as ElectricityBox) moves along its facing until its
//     back touches the nearest host wall, keeping its height;
//   - clip: a counter 15 cm through a wall is pushed out; one half through a wall is a clip, which refuses the fit
//     (--allow-clip writes it with a warning);
//   - a host showing the kit's own building model takes the kit at its origin and yaw (never turned to an entrance), and
//     a kit's offsets are not lifted by the dossier floor a second time;
//   - a row standing in a prop the host carries itself (its vanilla furniture set, which a layer cannot remove) is
//     flagged, not moved;
//   - a row hung on a socket of the source building is left out of the layer;
//   - `town fit --check` reports per-row corrections against the layer on disk and writes nothing.
// red-proof: 2026-11-17, each guard broken once, FAIL seen, then restored:
//   seatRows floor branch skipped (no `l.y += top - bottom`)  -> FAIL floor offset: a row that sank stayed 0.301 m off the host floor
//   wall snap returning null                                     -> FAIL wall offset: the box stayed 1.250 m off the wall at z -4
//   cutWalls returning []                                        -> FAIL clip: a counter half through a wall was not refused (and the shallow one stayed in it)
//   the `lift` line in seatRows removed                          -> FAIL lift: a floor row with lift -0.045 stands at y 0.200, not 0.155 (2026-11-19)
//   (#3552, 2026-11-19, each broken once, FAIL seen, then restored)
//   terrain branch using the centre height only                  -> FAIL terrain row: the row rests on the centre ground, so its high corner sinks into the slope
//   the terrain branch removed (row left at the kit height)      -> FAIL terrain row: y ... is not the highest ground under its footprint
//   `relative` mount removed from mountOf                        -> FAIL fence: held as floor, layer y ... (want relative, -0.1 over the ground)
//   footprintPoints without the pole rule                        -> FAIL pole: 5 point(s), y ... is not the ground under its pivot
//   anchorFor ignoring sameShell                                -> FAIL same shell: the kit was not anchored at the host's origin and yaw (yaw -90)

import { readFileSync, readdirSync, rmSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "town", "fixtures");
let failed = 0;
const fail = (msg) => {
  console.log(`FAIL  ${msg}`);
  failed++;
};
const check = (cond, msg) => {
  if (!cond) fail(msg);
};
const near = (a, b, eps = 0.002) => Math.abs(a - b) <= eps;

let seat, fitMod;
try {
  seat = await import(pathToFileURL(join(ROOT, "tools", "town", "seat.mjs")).href);
  fitMod = await import(pathToFileURL(join(ROOT, "tools", "town", "fit.mjs")).href);
} catch (e) {
  fail(`cannot load tools/town/seat.mjs or fit.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY town-fit-seat-selftest fail ${failed}`);
  process.exit(1);
}

// ---- fixture hosts: a room x -5..5, z -4..4, walls 0..4 m high, inner faces pointing into the room
const wall = (n, a, b) => [
  { n, p: [[a[0], 0, a[1]], [b[0], 0, b[1]], [b[0], 4, b[1]]] },
  { n, p: [[a[0], 0, a[1]], [b[0], 4, b[1]], [a[0], 4, a[1]]] },
];
const room = (floorY, mesh = "Fixture/Room.xob") => ({
  mesh,
  surfaces: [{ y: floorY, x: [-5, 5], z: [-4, 4], area: 80 }],
  walls: [...wall([0, 0, 1], [-5, -4], [5, -4]), ...wall([0, 0, -1], [-5, 4], [5, 4]), ...wall([1, 0, 0], [-5, -4], [-5, 4]), ...wall([-1, 0, 0], [5, -4], [5, 4])],
});
const BOX = { bounds: { min: [-0.5, 0, -0.25], max: [0.5, 1, 0.25] }, scale: 1 };
const reader = (shells) => ({
  info: (path) => (/Furniture_Set/.test(path) ? null : BOX),
  shell: async (ref) => shells[String(ref).replace(/^\{[0-9A-Fa-f]{16}\}/, "")] ?? null,
});
const STALL = "{4855AC571C89CA6D}Prefabs/Props/Commercial/LF_FenceStall.et";
const HOST = { x: 100, y: 10, z: 200, yaw: 90, prefab: "Fixture/Host.et" };
// a kit row at host-local (lx, ly, lz) with local yaw, as applyKit would hand it to the seat (world coords)
const { toWorld } = await import(pathToFileURL(join(ROOT, "tools", "town", "kit.mjs")).href);
const at = (id, lx, ly, lz, yaw = 0, prefab = STALL) => {
  const [wx, wz] = toWorld(lx, lz, HOST.yaw);
  return { id, prefab, coords: [HOST.x + wx, HOST.y + ly, HOST.z + wz], yaw: HOST.yaw + yaw };
};
const local = (c) => {
  const [lx, lz] = toWorld(c[0] - HOST.x, c[2] - HOST.z, -HOST.yaw);
  return [lx, c[1] - HOST.y, lz];
};

// ---- floor offset
for (const [floorY, what] of [[0.5, "sank"], [0, "floated"]]) {
  const rows = [at("display", 1.5, 0.199, 1)];
  const res = await seat.seatRows(rows, [{ id: "display", verb: "on floor", role: "dressing", prefab: STALL }], HOST, { reader: reader({ "Fixture/Host.et": room(floorY) }) });
  const [, ly] = local(rows[0].coords);
  check(near(ly, floorY), `floor offset: a row that ${what} stayed ${(floorY - ly).toFixed(3)} m off the host floor (y ${ly.toFixed(3)}, floor ${floorY})`);
  check(res.rows[0].mount === "floor", `floor offset: the row was held as ${res.rows[0].mount}`);
}

// ---- wall offset: on:"wall", and a hung kind by prefab name
{
  const rows = [at("box", 0, 1.5, -2.5, 0), at("ebox", 3, 2.6, 1, 180, "{CDE4035B661F9555}Prefabs/Structures/BuildingParts/Eletrical/ElectricityBoxes/ElectricityBox_small_01.et")];
  const kitRows = [{ id: "box", verb: "against wall", role: "alarm", on: "wall", prefab: STALL }, { id: "ebox", verb: "against wall", role: "alarm", prefab: rows[1].prefab }];
  const res = await seat.seatRows(rows, kitRows, HOST, { reader: reader({ "Fixture/Host.et": room(0.2) }) });
  const box = local(rows[0].coords);
  check(near(box[2], -4 + 0.25 + seat.WALL_GAP), `wall offset: the box stayed ${(box[2] - (-4 + 0.25)).toFixed(3)} m off the wall at z -4 (z ${box[2].toFixed(3)})`);
  check(near(box[1], 1.5) && near(box[0], 0), `wall offset: the box left its height or slid along the wall (${box.map((v) => v.toFixed(3)).join(" ")})`);
  const eb = local(rows[1].coords);
  check(res.rows[1].mount === "wall", `a hung kind (ElectricityBox) was held as ${res.rows[1].mount}`);
  check(near(eb[2], 4 - 0.25 - seat.WALL_GAP), `wall offset: the electricity box (facing -z) did not back onto the wall at z 4 (z ${eb[2].toFixed(3)})`);
}

// ---- clip: pushed when shallow, refused when deep
{
  const rows = [at("shallow", 0, 0.2, -3.9), at("deep", 5, 0.2, 0)];
  const kitRows = [{ id: "shallow", verb: "on floor", role: "counter", prefab: STALL }, { id: "deep", verb: "on floor", role: "counter", prefab: STALL }];
  const res = await seat.seatRows(rows, kitRows, HOST, { reader: reader({ "Fixture/Host.et": room(0.2) }) });
  const sh = res.rows.find((r) => r.id === "shallow");
  const dp = res.rows.find((r) => r.id === "deep");
  check(!sh.clip, `clip: a counter 15 cm through a wall was not pushed out (${sh.clip?.text})`);
  check(local(rows[0].coords)[2] >= -4 + 0.25 - seat.CLIP_TOL - 0.003, `clip: the shallow counter is still in the wall (z ${local(rows[0].coords)[2].toFixed(3)})`);
  check(Boolean(dp.clip), "clip: a counter half through a wall was not refused");
  check(!res.ok && res.clips.length === 1, `clip: seatRows ok=${res.ok} with ${res.clips.length} clip(s), expected a refusal with 1`);
  check(seat.triCutsBox([[0, 0, 0], [1, 0, 0], [0, 1, 0]], { c: [5, 5, 5], h: [1, 1, 1], axes: [[1, 0, 0], [0, 1, 0], [0, 0, 1]] }) === false, "triCutsBox: a far triangle cuts a box");
}

// ---- the host's own furniture (a vanilla set the island building keeps) is flagged, not moved
{
  const furnished = { ...room(0.2), kids: [{ path: "Fixture/Table.et", x: 2, y: 0.2, z: 1, yaw: 30 }] };
  const rows = [at("on-table", 2.2, 0.2, 1.1), at("clear", -3, 0.2, 2)];
  const kitRows = rows.map((r) => ({ id: r.id, verb: "on floor", role: "counter", prefab: STALL }));
  const before = local(rows[0].coords);
  const res = await seat.seatRows(rows, kitRows, HOST, { reader: reader({ "Fixture/Host.et": furnished }) });
  check(Boolean(res.rows[0].furniture) && /Table/.test(res.rows[0].furniture), "a row standing in the host's own furniture was not flagged");
  check(!res.rows[1].furniture && res.furniture.length === 1, "a row clear of the host's furniture was flagged");
  check(near(local(rows[0].coords)[0], before[0]) && near(local(rows[0].coords)[2], before[2]), "a row in the host's furniture was moved instead of flagged");
}

// ---- a kit row's `lift` sinks a floor row below the model's top surface (the hall tiles of House_Town_E_2I02, #3236)
{
  const rows = [at("lifted", -3, 0.2, 2)];
  const kitRows = [{ id: "lifted", verb: "on floor", role: "counter", prefab: STALL, lift: -0.045 }];
  const res = await seat.seatRows(rows, kitRows, HOST, { reader: reader({ "Fixture/Host.et": room(0.2) }) });
  check(near(local(rows[0].coords)[1], 0.2 - 0.045), `lift: a floor row with lift -0.045 stands at y ${local(rows[0].coords)[1].toFixed(3)}, not 0.155`);
  check(res.rows[0].notes.some((n) => /lifted/.test(n)), "lift: the seat report does not say the row was lifted");
}

// ---- a socket row is left out; a furniture set with no bounds stays where the kit puts it
{
  const rows = [at("fascia", 0, 1.4, 0, 180), at("set", 0, 0, 0, 0, "{0000000000000000}Prefabs/Fixture/Furniture_Set.et")];
  const kitRows = [{ id: "fascia", verb: "against wall", role: "sign", prefab: STALL, config: [{ component: "Hierarchy", set: { PivotID: '"socket_win_01"' } }] }, { id: "set", verb: "on floor", role: "furniture", prefab: rows[1].prefab }];
  const before = [...rows[1].coords];
  const res = await seat.seatRows(rows, kitRows, HOST, { reader: reader({ "Fixture/Host.et": room(0.2) }) });
  check(res.skipped.length === 1 && res.skipped[0].id === "fascia", "a row hung on a socket of the source building was not left out");
  check(rows[1].coords.every((v, i) => near(v, before[i])), "a furniture set with no bounds was moved");
}

// ---- planFitSeated: same shell anchors at the origin and yaw; a clip refuses; --allow-clip writes; the socket row is not written
const dossier = JSON.parse(readFileSync(join(FIX, "map-dossier.json"), "utf8"));
const mapping = JSON.parse(readFileSync(join(FIX, "fit-mapping.json"), "utf8"));
const template = JSON.parse(readFileSync(join(FIX, "fit-template.json"), "utf8"));
const base = JSON.parse(readFileSync(join(FIX, "fit-kit.json"), "utf8"));
const b03 = dossier.buildings.find((b) => b.id === "b03");
const hostPath = b03.prefab.replace(/^\{[0-9A-Fa-f]{16}\}/, "");
const VANILLA = "{0000000000000000}Prefabs/Fixture/Sample_Base.et";
const kit = {
  ...base,
  building: { mode: "override", vanilla: VANILLA },
  anchoring: { ...base.anchoring, floorY: 0.199, floors: [{ id: "f0", y: 0.199 }] },
  rows: [
    { ...base.rows[1], id: "fit-display", offset: [1.5, 0.199, 1], yaw: 0 },
    { ...base.rows[0], id: "fit-box", on: "wall", offset: [0, 1.5, -2.5], yaw: 0 },
    { ...base.rows[2], id: "fit-sign", config: [...base.rows[2].config, { component: "Hierarchy", set: { PivotID: '"socket_win_01"' }, blocks: [] }] },
  ],
};
const noGit = () => ({ status: 0, stdout: "", stderr: "" });
const opts = { place: "sample-square", slot: "shop", dossier, mapping, template, kits: { "fit-kit": kit }, fetch: false, skipClaims: true };
const shells = { [hostPath]: room(0.5, "Fixture/Shop.xob"), "Prefabs/Fixture/Sample_Base.et": room(0.199, "Fixture/Shop.xob") };
const ctx = { root: ROOT, run: noGit, shell: reader(shells) };
{
  const plan = await fitMod.planFitSeated(opts, ctx);
  check(plan.ok, `planFitSeated refused a sound slot: ${(plan.errors ?? []).join("; ")}`);
  check(plan.anchor?.sameShell === true && plan.anchor.yaw === (b03.yaw ?? 0) && plan.anchor.x === b03.x && plan.anchor.z === b03.z, `same shell: the kit was not anchored at the host's origin and yaw (${JSON.stringify(plan.anchor)}; old rule turned it to the entrance, yaw -90)`);
  const disp = plan.rows?.find((r) => r.id === "fit-display");
  check(disp && near(disp.coords[1], b03.y + 0.5), `planFitSeated: the display is at y ${disp?.coords[1]}, not on the host floor ${b03.y + 0.5}`);
  check(!/Fit_sample_square_shop_fit_sign/.test(plan.layerText ?? ""), "planFitSeated: the socket row was written to the layer");
  check(/Fit_sample_square_shop_fit_box/.test(plan.layerText ?? "") && plan.claim?.rows === 2, `planFitSeated: the layer lost a row or the claim counts ${plan.claim?.rows}`);
  check((plan.warnings ?? []).some((w) => /fit-sign .*socket/.test(w)), "planFitSeated: no warning names the left-out socket row");

  // a different model: the old rule (footprint centre, turned to the entrance) and no double floor
  const other = { ...shells, "Prefabs/Fixture/Sample_Base.et": room(0.199, "Fixture/Other.xob") };
  const p2 = await fitMod.planFitSeated(opts, { ...ctx, shell: reader(other) });
  check(p2.anchor && p2.anchor.sameShell === false && p2.anchor.yaw === -90, `other shell: anchor ${JSON.stringify(p2.anchor)}`);
  const a = fitMod.anchorFor({ ...b03, firstFloor: 0.3 }, kit);
  check(near(a.base, b03.y + 0.3 - 0.199), `the kit's own floor is added on top of the dossier floor (base ${a.base})`);

  const clipKit = { ...kit, rows: [...kit.rows, { ...base.rows[1], id: "fit-clip", offset: [5, 0.199, 0], yaw: 0 }] };
  const p3 = await fitMod.planFitSeated({ ...opts, kits: { "fit-kit": clipKit } }, ctx);
  check(!p3.ok && (p3.errors ?? []).some((e) => /fit-clip cuts/.test(e)), `a clipping row did not refuse the fit (${(p3.errors ?? []).join("; ") || "no error"})`);
  const p4 = await fitMod.planFitSeated({ ...opts, kits: { "fit-kit": clipKit }, allowClip: true }, ctx);
  check(p4.ok && (p4.warnings ?? []).some((w) => /fit-clip .*allow-clip/.test(w)), "--allow-clip did not write a clipping row with a warning");
}

// ---- town fit --check: per-row corrections against the layer on disk, nothing written
{
  const out = scratchDir("lf-fit-seat-");
  const kits = join(out, "kits");
  mkdirSync(kits);
  writeFileSync(join(kits, "fit-kit.json"), JSON.stringify(kit));
  const layer = join(out, "Place_sample-square_shop.layer");
  writeFileSync(layer, `GenericEntity Fit_sample_square_shop_fit_display : "${STALL}" {\n coords ${b03.x + 1.5} ${b03.y + 0.399} ${b03.z + 1}\n}\n`);
  const lines = [];
  const log = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  try {
    code = await fitMod.main(["sample-square", "shop", "--check", "--dossier", join(FIX, "map-dossier.json"), "--mapping", join(FIX, "fit-mapping.json"), "--template-file", join(FIX, "fit-template.json"), "--kits-dir", kits, "--out-dir", out], { root: ROOT, run: noGit, shell: reader(shells) });
  } finally {
    console.log = log;
  }
  const text = lines.join("\n");
  check(code === 0, `fit --check exited ${code}: ${text.slice(-400)}`);
  // the layer has the display at the playtest floor 0.399 above the origin; the host floor is 0.5: up 0.101
  check(/^fix +fit-display .*dy 0\.101 /m.test(text) && /SUMMARY town-fit check ok/.test(text), `fit --check did not report the display's correction against the layer:\n${text}`);
  check(/^SKIP +fit-sign/m.test(text), "fit --check did not mark the socket row SKIP");
  check(readdirSync(out).sort().join(",") === "Place_sample-square_shop.layer,kits" && readFileSync(layer, "utf8").includes(`${b03.y + 0.399}`), "fit --check wrote or changed files");
  rmSync(out, { recursive: true, force: true });
}

// ---- open ground (#3552): a terrain row rests on the probed ground, a terrain-relative class counts from the ground
{
  const terr = await import(pathToFileURL(join(ROOT, "tools", "town", "terrain.mjs")).href);
  const FENCE = "{57873EAF69D5450E}Prefabs/Structures/Walls/Net/NetFence_01/NetFence_01_4m.et";
  const POLE = "{DDE764E40C17C5DC}Prefabs/Sites/LF_SiteFlag.et";
  const infos = {
    [FENCE]: { bounds: { min: [-2, 0.1, -0.05], max: [2, 1.9, 0.05] }, scale: 1 },
    [POLE]: { bounds: { min: [-0.5, -0.8, -0.1], max: [0.5, 3, 0.1] }, scale: 1 },
  };
  const rd = { info: (p) => infos[p] ?? BOX, shell: async () => room(0) };
  const slope = { at: (x, z) => 20 + 0.1 * (x - 100) + 0.05 * (z - 200) }; // a plane rising to +x and +z
  const slopeKit = (id, extra = {}) => ({ id, verb: "on floor", role: "dressing", prefab: STALL, mount: "terrain", ...extra });

  // a box 1 x 0.5 m on the slope: its bottom rests on the highest of the centre and the four corners
  const rows = [at("t", 1.5, 0, 1)];
  const res = await seat.seatRows(rows, [slopeKit("t")], HOST, { reader: rd, terrain: slope });
  const heights = (res.rows[0].samples ?? []).map((p) => slope.at(p[0], p[1]));
  check(res.rows[0].mount === "terrain" && heights.length === 5, `terrain row: held as ${res.rows[0].mount} with ${heights.length} footprint points (want terrain, 5)`);
  check(near(rows[0].coords[1], Math.max(...heights)), `terrain row: y ${rows[0].coords[1]} is not the highest ground under its footprint ${Math.max(...heights)}`);
  check(rows[0].coords[1] > heights[0] + 0.01, "terrain row: the row rests on the centre ground, so its high corner sinks into the slope");
  check(res.terrainPoints.length === 5, `terrain row: seatRows named ${res.terrainPoints.length} probe point(s), not 5`);

  // no probed height: kept at the kit height and reported as unseated
  const kept = [at("t", 1.5, 3, 1)];
  const r2 = await seat.seatRows(kept, [slopeKit("t")], HOST, { reader: rd });
  check(near(kept[0].coords[1], HOST.y + 3) && r2.unseated.length === 1, `terrain row without a probe moved to y ${kept[0].coords[1]} or was not reported (unseated ${r2.unseated.length})`);
  const half = [at("t", 1.5, 3, 1)];
  let n = 0;
  const r3b = await seat.seatRows(half, [slopeKit("t")], HOST, { reader: rd, terrain: { at: (x, z) => (n++ === 2 ? null : slope.at(x, z)) } });
  check(near(half[0].coords[1], HOST.y + 3) && r3b.unseated.length === 1, "terrain row with one unprobed corner was moved instead of left");

  // a pole (mesh below the pivot) stands by its pivot: one point, the pivot's ground
  const pole = [at("p", 2, 0, 2, 0, POLE)];
  const r4 = await seat.seatRows(pole, [slopeKit("p", { prefab: POLE })], HOST, { reader: rd, terrain: slope });
  check(r4.rows[0].samples.length === 1 && near(pole[0].coords[1], slope.at(pole[0].coords[0], pole[0].coords[2])), `pole: ${r4.rows[0].samples.length} point(s), y ${pole[0].coords[1]} is not the ground under its pivot`);

  // a terrain-relative class: the layer's y is the height over the ground, the bottom of its bounds (here 0.1 up: -0.1)
  const fence = [at("f", -3, 3, 4, 0, FENCE)];
  const r5 = await seat.seatRows(fence, [{ id: "f", verb: "on floor", role: "dressing", prefab: FENCE, class: "SCR_DestructibleEntity" }], HOST, { reader: rd });
  check(r5.rows[0].mount === "relative" && near(fence[0].coords[1], -0.1), `fence: held as ${r5.rows[0].mount}, layer y ${fence[0].coords[1]} (want relative, -0.1 over the ground)`);

  // terrain.mjs: the answers are keyed by point; a missing ray is named, never guessed
  const pts = [[1, 2], [3.0004, 4], [1, 2]];
  const lines = terr.probeLines(pts, 100);
  check(lines.length === 2 && lines[0] === "ray t0 1 160 2 1 40 2", `probeLines: ${JSON.stringify(lines)}`);
  const pinned = terr.pinsFromRays(pts, new Map([["t0", { sy: 100.1234 }]]));
  check(pinned.points["1,2"] === 100.123 && pinned.missing.length === 1 && pinned.missing[0] === "3,4", `pinsFromRays: ${JSON.stringify(pinned)}`);
  const lookup = terr.terrainFromPins(pinned);
  check(lookup.at(1, 2) === 100.123 && lookup.at(3, 4) === null && terr.terrainFromPins(null) === null, "terrainFromPins: a probed point answers, an unprobed one is null");
  let asked = [];
  const probed = await terr.probeTerrain([[6638.419, 5970.232]], { layerName: "Place_farmcompound_compound.layer", aroundY: 115, log: () => {}, measure: (records, o) => ((asked = o.extra), { outText: "ray t0 d=1 len=2 hit=none root=none p=1,1,1 sy=116.25" }) });
  check(asked.length === 1 && /^ray t0 6638\.419 175 5970\.232 6638\.419 55 5970\.232$/.test(asked[0]) && probed.points["6638.419,5970.232"] === 116.25, `probeTerrain: asked ${JSON.stringify(asked)}, got ${JSON.stringify(probed)}`);
}

console.log(failed ? `SUMMARY town-fit-seat-selftest fail ${failed}` : "SUMMARY town-fit-seat-selftest ok");
process.exit(failed ? 1 : 0);
