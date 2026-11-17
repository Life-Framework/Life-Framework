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
//   - a row hung on a socket of the source building is left out of the layer;
//   - `town fit --check` reports per-row corrections against the layer on disk and writes nothing.
// red-proof: 2026-11-17, each guard broken once, FAIL seen, then restored:
//   seatRows floor branch skipped (no `l.y += top - bottom`)  -> FAIL floor offset: a row that sank stayed 0.301 m off the host floor
//   wall snap returning null                                     -> FAIL wall offset: the box stayed 1.250 m off the wall at z -4
//   cutWalls returning []                                        -> FAIL clip: a counter half through a wall was not refused (and the shallow one stayed in it)
//   anchorFor ignoring sameShell                                 -> FAIL same shell: the kit was not anchored at the host's origin and yaw (yaw -90)

import { mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync, mkdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

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
  const out = mkdtempSync(join(tmpdir(), "lf-fit-seat-"));
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

console.log(failed ? `SUMMARY town-fit-seat-selftest fail ${failed}` : "SUMMARY town-fit-seat-selftest ok");
process.exit(failed ? 1 : 0);
