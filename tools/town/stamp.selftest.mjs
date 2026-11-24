// tools/town/stamp.selftest.mjs: `town stamp` on tools/town/fixtures/stamp/ (card #3881). No game, no server.
// The fixture place `stampville` has two buildings of one prefab (b1 at yaw 0, b2 at yaw 180) mapped to the kit's two slots, and
// a third of another prefab (b3). The kit is lifted by `town author extract` from the fixture grid layer, as a real author run does.
//   - stamping twice writes the same bytes (layers and claims);
//   - a moved kit row moves in both stamped layers, turned by each building's yaw;
//   - a stamp onto a building whose prefab differs from the kit is refused and writes nothing;
//   - a stamped row with a floating or blocks-door finding refuses the stamp and puts the previous bytes back; a contact
//     finding (not hard) does not;
//   - a wall-mounted row (a fascia sign) is seated on the island building's real wall (card #3931): where the wall under its
//     centre steps (a lintel), it slides along the wall to the nearest flat run in every copy, turned by each building's yaw, and the
//     floor rows stay where the grid proved them; with no flat run in reach it stays seated at its centre and warns;
//   - a row's component GUID travels with its config: the stamped layer writes it though the prefab does not carry the component.
//   - yard rows (kit mount terrain/ground) are seated on the ISLAND ground from the pins, within 2 cm, keeping their building-relative x, z and
//     yaw; with no pin the stamp is refused naming `--terrain`; `--terrain` probes once per slot, keeps the pins, and --dry restores the files (#3961).
// red-proof (#3961, 2026-11-21): with STAMP_SEAT back to ["wall"] (the grid height copied onto the island) the selftest failed: the unprobed yard
//   stamped instead of being refused and the probe step threw ("first2.probes[0]" undefined); restored, OK.
// red-proof (#3931, 2026-11-21): seatRows with the flatWalls branch off printed "FAIL the sign slid off the lintel ..."; planFit without the
//   kit config guid fallback printed "FAIL the config guid reaches the stamped layer"; restored, OK.
// red-proof (#4778, 2026-11-24): with the scale write off in fit.mjs entityText, "a kit row scale 0.5 is a scale line in the stamped layer" failed; restored, OK.
// red-proof: 2026-11-21, #3924: with the carrier push removed, "each stamped copy carries a Fit_<slot>_clear carrier" failed; restored, OK.
//   2026-11-21, planStamp with the prefab comparison removed printed "FAIL a stamp onto b3 (another prefab) is refused";
//   applyStamp with the restore loop removed printed "FAIL a floating finding restores the previous layer"; restored, OK.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { authorExtract } from "./author.mjs";
import { clearBoxCarriers, clearBoxesOf, clearListOf } from "./fit.mjs";
import { toLocal } from "./kit.mjs";
import { footprintPoints } from "./seat.mjs";
import { applyStamp, planStamp, probeAndReplan, stampTargets, stampVerdict } from "./stamp.mjs";
import { readPins, terrainFromPins } from "./terrain.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..");
const FIX = join(HERE, "fixtures", "stamp");
const fails = [];
const ok = (cond, msg) => {
  if (!cond) fails.push(msg);
};
const near = (a, b) => Math.abs(a - b) < 0.0015;
const now = () => new Date("2026-11-21T12:00:00Z");
const quiet = () => {};

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
const CLEAR = ["{4D2E0B7F8A9C1E36}Prefabs/Props/Civilian/StepladderWooden_01.et", "{1AB7A2C69A30391D}Prefabs/Props/Furniture/CoatRack_01.et"];
const kit = authorExtract(join(FIX, "Grid_stamp_fixture.layer"), "Shop_*", { kit: "fixture-shop", clear: CLEAR });
const opts = { placesDir: join(FIX, "places"), templatesDir: join(FIX, "templates") };
const targets = stampTargets("fixture-shop", opts);
ok(targets.length === 2 && targets.every((t) => t.building), `two mapped copies found (got ${targets.length})`);

const out = scratchDir("lf-stamp-");
const po = { root: ROOT, outRoot: out, layerDir: "layers", claimsDir: "claims", skipClaims: true, now };
const coordsOf = (text, id) => {
  const m = new RegExp(`_${id} : "[^"]+" \\{[\\s\\S]*?coords ([-\\d. ]+)`).exec(text);
  return m ? m[1].trim().split(" ").map(Number) : null;
};
const read = (f) => (existsSync(join(out, f)) ? readFileSync(join(out, f), "utf8") : null);

// stamp, then stamp again: same bytes
const p1 = await planStamp(kit, targets, po);
ok(p1.ok, `the fixture stamps (${p1.errors.join("; ")})`);
ok(applyStamp(p1, { root: out, log: quiet }) === 0, "first stamp writes");
const files = p1.copies.flatMap((c) => [c.layerFile, c.claimFile]);
const first = files.map(read);
const p2 = await planStamp(kit, targets, { ...po, now: () => new Date("2026-12-01T00:00:00Z") });
ok(applyStamp(p2, { root: out, log: quiet }) === 0, "second stamp runs");
ok(files.every((f, i) => read(f) === first[i] && first[i] !== null), "stamping twice writes the same bytes (layers and claims, a later date kept out)");

// the composed transform: counter at building-local (2, 0.2, 3); b1 yaw 0 at 5000,20,6000; b2 yaw 180 at 5100,25.5,6050
const counterId = "counter";
const a1 = coordsOf(first[0], counterId);
const b1 = coordsOf(first[2], counterId);
ok(a1 && near(a1[0], 5002) && near(a1[1], 20.2) && near(a1[2], 6003), `counter in b1 at 5002 20.2 6003 (got ${a1})`);
ok(b1 && near(b1[0], 5098) && near(b1[1], 25.7) && near(b1[2], 6047), `counter in b2 at 5098 25.7 6047 (got ${b1})`);
ok(!/coords 1003 /.test(first[0]), "no grid coordinate leaks into a stamped layer");

// the clear list: one carrier per copy, at the building's x and z, naming the building prefab and the list (card #3924)
const carrierOf = (text) => /GenericEntity Fit_stampville_shop_[ab]_clear \{[\s\S]*?\n\}/.exec(text)?.[0] ?? null;
const c1 = carrierOf(first[0]);
const c2 = carrierOf(first[2]);
ok(c1 && c2, "each stamped copy carries a Fit_<slot>_clear carrier");
ok(c1 && /LF_BaseWorldClearComponent/.test(c1) && /m_sBuilding "\{[0-9A-F]{16}\}/.test(c1), "the carrier names the building prefab");
ok(c1 && CLEAR.every((p) => c1.includes(p)) && !/until/.test(c1), "the carrier lists every clear prefab");
const cc1 = c1 && /coords ([-\d. ]+)/.exec(c1)?.[1].trim().split(" ").map(Number);
const cc2 = c2 && /coords ([-\d. ]+)/.exec(c2)?.[1].trim().split(" ").map(Number);
ok(cc1 && near(cc1[0], 5000) && near(cc1[2], 6000), `the carrier of b1 stands at the building's x and z (got ${cc1})`);
ok(cc2 && near(cc2[0], 5100) && near(cc2[2], 6050), `the carrier of b2 stands at its building's x and z (got ${cc2})`);
const noClear = await planStamp({ ...kit, stamp: { ...kit.stamp, clear: [] } }, targets, po);
ok(noClear.ok && noClear.copies.every((c) => !/_clear /.test(c.layerText)), "a kit with an empty clear list writes no carrier");

// a boxed clear entry (one door of a prefab the building uses many times): a box carrier of its own at the kit offset turned by the
// anchor's yaw, and the prefab is not on the building carrier's list
const DOOR = "{66A908B66DC3277A}Prefabs/Structures/BuildingParts/Doors/DoorSet_Village_E_01_L_INT_COV_B.et";
const LEAF = "{E2C75A974A146D9F}Prefabs/Structures/BuildingParts/Doors/Door_Village_E_A_RIGHT_INT_COV_B.et";
const BOX = { offset: [3, -1], half: [0.7, 0.8], height: 4 };
const boxed = { clear: [{ prefab: CLEAR[0] }, { prefab: DOOR, box: BOX }, { prefab: LEAF, box: { ...BOX } }] };
ok(clearListOf(boxed).join() === CLEAR[0], "a boxed entry is not on the building-wide list");
const boxes = clearBoxesOf(boxed);
ok(boxes.length === 1 && boxes[0].prefabs.length === 2 && boxes[0].half.join() === "0.7,0.8", "two entries with one box share one carrier, read back with the half size");
const boxCarrier = clearBoxCarriers({ place: "stampville", slot: "shop-a", anchor: { x: 5000, y: 25, z: 6000, yaw: 90 }, boxes })[0];
ok(boxCarrier.name === "Fit_stampville_shop_a_clear_box1" && boxCarrier.yaw === 90, "the box carrier is named per box and turned by the anchor");
ok(near(boxCarrier.coords[0], 5000 - 1) && near(boxCarrier.coords[2], 6000 - 3), `a yaw-90 anchor turns the kit offset 3, -1 to world (got ${boxCarrier.coords})`);
const boxSet = boxCarrier.comps[0].set;
ok(boxSet.m_vBoxHalfM === "0.7 0 0.8" && boxSet.m_fBoxHeightM === "4" && boxSet.m_aPrefabs.includes(DOOR) && boxSet.m_aPrefabs.includes(LEAF) && !("m_sBuilding" in boxSet), "the box carrier sets the half size, the height and the door, and no building");

// a moved kit row moves in both copies
const moved = JSON.parse(JSON.stringify(kit));
moved.rows.find((r) => r.id === counterId).offset[0] += 0.5;
const p3 = await planStamp(moved, targets, po);
const a3 = coordsOf(p3.copies[0].layerText, counterId);
const b3 = coordsOf(p3.copies[1].layerText, counterId);
ok(a3 && near(a3[0], 5002.5) && near(a3[2], 6003), `the moved row moves in b1 (got ${a3})`);
ok(b3 && near(b3[0], 5097.5) && near(b3[2], 6047), `the moved row moves in b2, turned by its yaw (got ${b3})`);

// a building of another prefab is refused
const wrong = targets.map((t, i) => (i === 1 ? { ...t, building: t.dossier.buildings.find((b) => b.id === "b3") } : t));
const p4 = await planStamp(kit, wrong, po);
ok(!p4.ok && p4.errors.some((e) => /b3 .*same prefab/.test(e)), `a stamp onto b3 (another prefab) is refused (${p4.errors.join("; ")})`);
ok(applyStamp(p4, { root: out, log: quiet }) === 1 && files.every((f, i) => read(f) === first[i]), "a refused plan writes nothing");

// the island audit is the verdict: floating and blocks-door refuse and restore; contact does not
const layerName = p3.copies[0].layerFile.split("/").pop();
const report = (kind) => ({ ranked: [{ name: `Fit_stampville_shop_a_${counterId}`, layer: layerName, problems: [{ kind, text: kind }] }] });
for (const kind of ["floating", "blocks-door"]) {
  ok(stampVerdict(report(kind), [layerName]).length === 1, `${kind} is a hard finding`);
  const rc = applyStamp(p3, { root: out, log: quiet, audit: () => ({ report: report(kind) }) });
  ok(rc === 1, `a ${kind} finding refuses the stamp`);
  ok(files.every((f, i) => read(f) === first[i]), `a ${kind} finding restores the previous layer`);
}
ok(stampVerdict(report("floating"), ["Place_other_slot.layer"]).length === 0, "a finding on another layer is not the stamp's");
ok(applyStamp(p3, { root: out, log: quiet, audit: () => ({ report: report("contact") }) }) === 0, "a contact finding does not refuse");
ok(read(p3.copies[0].layerFile) === p3.copies[0].layerText, "the accepted re-stamp is on disk");
ok(applyStamp(p3, { root: out, log: quiet, audit: () => ({ error: "no server" }) }) === 1, "an audit that did not run refuses");

// ---- wall rows (#3931): the fixture host is a 10 x 8 m room whose +z wall carries a 0.2 m wide lintel standing 0.3 m proud of it
const wallQuad = (n, a, b) => [
  { n, p: [[a[0], 0, a[1]], [b[0], 0, b[1]], [b[0], 4, b[1]]] },
  { n, p: [[a[0], 0, a[1]], [b[0], 4, b[1]], [a[0], 4, a[1]]] },
];
const lintel = (reach) => wallQuad([0, 0, -1], [-reach, 3.7], [reach, 3.7]);
const shellOf = (extra) => ({
  mesh: "Fixture/House.xob",
  surfaces: [{ y: 0, x: [-5, 5], z: [-4, 4], area: 80 }],
  walls: [...wallQuad([0, 0, 1], [-5, -4], [5, -4]), ...wallQuad([0, 0, -1], [-5, 4], [5, 4]), ...wallQuad([1, 0, 0], [-5, -4], [-5, 4]), ...wallQuad([-1, 0, 0], [5, -4], [5, 4]), ...extra],
});
const PLATE = { bounds: { min: [-0.5, 0, -0.25], max: [0.5, 1, 0.25] }, scale: 1 };
const readerOf = (extra) => ({ info: () => PLATE, shell: async () => shellOf(extra) });
const hostPath = "Prefabs/Structures/Houses/Town/House_Town_E_2I01/House_Town_E_2I01.et";
const withSign = (slide) => {
  const k = JSON.parse(JSON.stringify(kit));
  const proto = k.rows.find((r) => r.id === counterId);
  k.rows.push({ id: "fascia", prefab: proto.prefab, role: "sign", verb: "against wall", wall: "free", floor: "f0", offset: [0, 2, 3.6], yaw: 180, ...(slide ? { slide } : {}) });
  return k;
};
const hostOf = (t) => t.building;
const localOf = (text, id, t) => {
  const c = coordsOf(text, id);
  const [lx, lz] = toLocal(c[0] - hostOf(t).x, c[2] - hostOf(t).z, hostOf(t).yaw);
  return [lx, c[1] - hostOf(t).y, lz];
};
const stampOf = async (k, extra) => planStamp(k, targets, { ...po, shell: readerOf(extra) });
const flatKit = withSign();
const slid = await stampOf(flatKit, lintel(0.1));
ok(slid.ok, `the wall fixture stamps (${slid.errors.join("; ")})`);
const flatRun = await stampOf(flatKit, []);
for (const [i, t] of targets.entries()) {
  const onFlat = localOf(flatRun.copies[i].layerText, "fascia", t);
  ok(near(onFlat[0], 0) && near(onFlat[2], 3.745), `on a flat wall the sign stays at its x, flush (got ${onFlat})`);
  const s = localOf(slid.copies[i].layerText, "fascia", t);
  ok(Math.abs(Math.abs(s[0]) - 0.6) < 0.0015 && near(s[2], 3.745), `the sign slid off the lintel to the nearest flat run, flush with the wall (copy ${i}: got ${s})`);
  const cnt = coordsOf(slid.copies[i].layerText, counterId);
  const was = coordsOf(first[i * 2], counterId);
  ok(cnt && was && near(cnt[0], was[0]) && near(cnt[2], was[2]), "a floor row stays where the grid proved it");
}
const wide = await stampOf(withSign(0.3), lintel(0.1));
const stuck = localOf(wide.copies[0].layerText, "fascia", targets[0]);
ok(wide.ok && near(stuck[0], 0) && near(stuck[2], 3.445), `with no flat run in reach the sign stays seated at its centre (got ${stuck})`);
ok(wide.copies[0].warnings.some((w) => /fascia.*no flat wall run within 0.3 m/.test(w)), `and the stamp warns (${wide.copies[0].warnings.join("; ")})`);

// ---- a row's component GUID travels with its config
const guided = JSON.parse(JSON.stringify(kit));
guided.rows.find((r) => r.id === counterId).config = [{ component: "LF_StoreComponent", guid: "ABCDEF0123456789", set: { m_sOutletId: '"fixture"' }, blocks: [] }];
const withGuid = await planStamp(guided, targets, po);
ok(withGuid.ok && /LF_StoreComponent "{ABCDEF0123456789}"/.test(withGuid.copies[0].layerText), "the config guid reaches the stamped layer");
void hostPath;

// ---- a row's scale travels into the layer (#4778): the speed-trap gravel piles are 0.5 and came out 8 m wide without it
const scaledKit = JSON.parse(JSON.stringify(kit));
scaledKit.rows.find((r) => r.id === counterId).scale = 0.5;
const scaledPlan = await planStamp(scaledKit, targets, po);
const entityOf = (text, id) => new RegExp(`_${id} : "[^"]+" \\{[\\s\\S]*?\\n\\}`).exec(text)?.[0] ?? "";
ok(scaledPlan.ok && /\n scale 0\.5\n/.test(entityOf(scaledPlan.copies[0].layerText, counterId)), "a kit row's scale 0.5 is a scale line in the stamped layer");
ok(!/\n scale /.test(entityOf(withGuid.copies[0].layerText, counterId)), "a row at scale 1 writes no scale line");
// the seat's footprint counts the scale: a 2 m wide bound at 0.5 spans one metre
const wideInfo = { bounds: { min: [-1, 0, -1], max: [1, 1, 1] }, scale: 1 };
const spanOf = (pts) => Math.max(...pts.map((p) => p[0])) - Math.min(...pts.map((p) => p[0]));
ok(near(spanOf(footprintPoints([0, 0, 0], 0, wideInfo, 0.5)), 1) && near(spanOf(footprintPoints([0, 0, 0], 0, wideInfo, null)), 2), "the seat's footprint is scaled by the row's scale");

// ---- yard rows (#3961): open-ground rows are seated on the island ground, not on the flat grid's height
// The island slopes 5 cm per metre east and 2 cm per metre north from 19 m at the origin of the fixture buildings; the grid is flat at 0.
const slope = (x, z) => Math.round((19 + 0.05 * (x - 5000) + 0.02 * (z - 6000)) * 1000) / 1000;
const plane = { at: (x, z) => slope(x, z) };
const CRATE = "{57873EAF69D5450E}Prefabs/Structures/Walls/Net/NetFence_01/NetFence_01_4m.et";
const yardKit = JSON.parse(JSON.stringify(kit));
yardKit.rows.push(
  { id: "yard-crate-1", prefab: CRATE, role: "dressing", verb: "on floor", mount: "terrain", offset: [8, 0, -6], yaw: 30, wall: "free", floor: "f0" },
  { id: "yard-crate-2", prefab: CRATE, role: "dressing", verb: "on floor", mount: "terrain", offset: [-7, 0, 9], yaw: 200, wall: "free", floor: "f0" },
  { id: "yard-post", prefab: CRATE, role: "dressing", verb: "on floor", mount: "ground", offset: [4, 0, -9], yaw: 0, wall: "free", floor: "f0" },
);
// A crate has no bounds (its own point is the ground that counts); the building is the 10 x 8 m room of the wall fixture.
const yardReader = { info: (p) => (String(p).includes("NetFence") ? null : PLATE), shell: async () => shellOf([]) };
const worldFrame = () => "world";
const yo = { ...po, shell: yardReader, frameOf: worldFrame };
const stub = (rel) => ({ ...yo, terrainOf: rel });
const crateWorld = (text, id) => coordsOf(text, id);

// no probed ground: the stamp refuses, names the probe and writes nothing; it never copies the grid height
const bare = await planStamp(yardKit, targets, { ...yo, terrainOf: () => null });
ok(!bare.ok && bare.errors.some((e) => /yard-crate-1.*town stamp fixture-shop --terrain/.test(e)), `an unprobed yard is refused with the probe command (${bare.errors.join("; ")})`);
ok(bare.copies.length === 0 && bare.probes.length === 2 && bare.probes.every((p) => p.points.length >= 3), "the refusal lists the points to probe, per slot");
const bareFiles = (await planStamp(yardKit, targets, { ...yo, terrainOf: () => null, allowUnprobed: true })).copies;
ok(bareFiles.length === 2, "the probe boot gets interim copies to boot around");
// a stamp with no host model cannot seat a yard
const noHost = await planStamp(yardKit, targets, { ...po, shell: null, terrainOf: () => plane, frameOf: worldFrame });
ok(!noHost.ok && noHost.errors.some((e) => /open-ground row.s. cannot be seated/.test(e)), `a yard with no host model is refused (${noHost.errors.join("; ")})`);

// with the ground: every yard row stands within 2 cm of it, in both copies, whatever the building's own height
const yard = await planStamp(yardKit, targets, stub(() => plane));
ok(yard.ok, `the yard kit stamps on a probed ground (${yard.errors.join("; ")})`);
for (const [i, t] of targets.entries()) {
  const text = yard.copies[i].layerText;
  for (const id of ["yard-crate-1", "yard-crate-2", "yard-post"]) {
    const w = crateWorld(text, id.replaceAll("-", "_"));
    ok(w && Math.abs(w[1] - slope(w[0], w[2])) <= 0.02, `${id} in copy ${i} is within 2 cm of the island ground (got y ${w?.[1]}, ground ${w ? slope(w[0], w[2]) : "?"})`);
  }
  // building-relative x and z, and the yaw, stay as the grid proved them
  const [lx, , lz] = localOf(text, "yard_crate_1", t);
  ok(near(lx, 8) && near(lz, -6), `yard-crate-1 keeps its building-relative x and z in copy ${i} (got ${lx}, ${lz})`);
  const yawOf = /_yard_crate_1 : "[^"]+" \{[\s\S]*?angleY ([-\d.]+)/.exec(text)?.[1];
  ok(near(((Number(yawOf) % 360) + 360) % 360, i === 0 ? 30 : 210), `yard-crate-1 keeps its yaw in copy ${i} (got ${yawOf})`);
  const was = coordsOf(first[i * 2], counterId);
  const now2 = coordsOf(text, counterId);
  ok(was && now2 && near(was[1], now2[1]), "a floor row inside the building keeps the grid's floor height");
}
const yardAgain = await planStamp(yardKit, targets, stub(() => plane));
ok(yardAgain.copies.every((c, i) => c.layerText === yard.copies[i].layerText), "a yard stamp twice writes the same bytes");
// a prefab that reads y over the terrain (RELATIVE_Y) writes the height over it: 0 for a post on the ground
const terrainFrame = await planStamp(yardKit, targets, { ...stub(() => plane), frameOf: () => "terrain" });
const rel = terrainFrame.ok && coordsOf(terrainFrame.copies[0].layerText, "yard_post");
ok(rel && Math.abs(rel[1]) <= 0.02, `a terrain-frame post on the ground is written at 0 over the terrain (got ${rel?.[1]})`);

// town stamp --terrain: probe, keep the pins, plan again; --dry puts the interim files back
const root2 = scratchDir("lf-stamp-probe-");
const probeOf = (n) => async (points) => ({ points: Object.fromEntries(points.map((p) => [`${Math.round(p[0] * 1000) / 1000},${Math.round(p[1] * 1000) / 1000}`, slope(p[0], p[1])])), missing: [], n });
const po2 = { ...yo, root: ROOT, outRoot: root2, layerDir: "layers", claimsDir: "claims", skipClaims: true, terrainOf: (pl, sl) => terrainFromPins(readPins(root2, pl, sl)) };
const first2 = await planStamp(yardKit, targets, po2);
ok(!first2.ok && first2.probes.length === 2, "nothing probed yet: the plan asks for a probe");
let probed = 0;
const spy = async (points, o) => {
  probed++;
  ok(existsSync(join(root2, "layers", o.layerName)), "the layer the probe boots around is on disk");
  return probeOf(0)(points);
};
const dry = await probeAndReplan(yardKit, targets, po2, first2, { root: root2, dry: true, probe: spy, log: quiet });
ok(dry.ok && probed === 2, `the probe runs once per slot and the plan then passes (${dry.errors.join("; ")})`);
ok(!existsSync(join(root2, "layers", first2.probes[0].layerName)), "a dry probe puts the interim layer back (it did not exist)");
ok(existsSync(join(root2, "tools/town/terrain", `stampville-shop-a.json`)) && readPins(root2, "stampville", "shop-a")?.place === "stampville", "the pins are kept for the slot");
const real = await probeAndReplan(yardKit, targets, po2, first2, { root: root2, dry: false, probe: spy, log: quiet });
ok(real.ok && real.copies.every((c) => !existsSync(join(root2, c.layerFile))), "the interim layers are gone before the real stamp writes");
ok(real.ok && applyStamp(real, { root: root2, log: quiet }) === 0, "the stamp after a probe writes");
ok(real.copies.every((c) => existsSync(join(root2, c.layerFile))), "the probed stamp is on disk");
const missingProbe = async (points) => ({ points: {}, missing: points.map((p) => String(p)) });
const none = await probeAndReplan(yardKit, targets, { ...po2, terrainOf: () => null }, first2, { root: root2, dry: true, probe: missingProbe, log: quiet });
ok(!none.ok && none.errors.some((e) => /need the island ground height/.test(e)), "a probe that returns no height leaves the stamp refused");


if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("town stamp selftest: OK");
}
