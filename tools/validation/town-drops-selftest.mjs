// Proves the outposts slot (`town drops`, #2959): a slot whose drops stand in OTHER places' buildings is checked against each
// drop's own footprint, a mapping needs real drops, and the one-row kit puts the mailbox outside the entrance wall.
// inputs: tools/town/drops.mjs, tools/town/check.mjs, tools/town/map.mjs, tools/town/templates/town.json, docs/everon/places/saint-philippe/mapping.json
// red-proof: 2026-11-18, before check.mjs knew `drops` (slotGround read only entry.building) the outposts cases below printed FAIL:
//   a layer entity standing at a drop building was reported 300 m outside "building undefined"; with drops it passes.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkClaimsAndFootprints, distToPolygon, slotGround } from "../town/check.mjs";
import { DROP_PREFAB, dropKit, exitDistance } from "../town/drops.mjs";
import { checkMapping, loadTemplate } from "../town/map.mjs";
import { checkOutposts, dropAsEntry, dropKeys } from "../town/outposts.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

// A 10 x 6 m building at the origin, facing +z, with a door on its +z wall at x = 2.
const box = (cx, cz, w, d) => [[cx - w / 2, cz - d / 2], [cx + w / 2, cz - d / 2], [cx + w / 2, cz + d / 2], [cx - w / 2, cz + d / 2]];
const building = (id, cx, cz, entrances = []) => ({ id, prefab: "{0000000000000001}Prefabs/Test/House.et", x: cx, y: 5, z: cz, yaw: 0, firstFloor: 0, footprint: { cx, cz, w: 10, d: 6, corners: box(cx, cz, 10, 6) }, entrances, interior: { value: "yes" } });
const dossierOf = (slug, buildings) => ({ schema: 1, place: { slug, name: slug, x: 0, y: 0, z: 0 }, buildings, freePlots: { plots: [] } });

// exitDistance: from the centre along +z the 6 m deep box is left after 3 m.
check(Math.abs(exitDistance(box(0, 0, 10, 6), 0, 0, 0, 1) - 3) < 1e-6, "a ray along +z leaves a 6 m deep box after 3 m");
check(exitDistance(box(0, 0, 10, 6), 0, 0, 0, 0) === 0, "a zero ray travels nowhere");

// dropKit: the mailbox stands outside the footprint (z beyond its 3 m reach), beside the door when one is known.
const noDoor = dropKit(building("b1", 0, 0), "parcel-drops");
const doorOffset = noDoor.kit.rows[0].offset;
check(doorOffset[2] > 3 && doorOffset[2] < 6, `the mailbox must start just outside the 3 m reach, got z ${doorOffset[2]}`);
check(noDoor.kit.rows[0].prefab === DROP_PREFAB && noDoor.kit.rows[0].tags.includes("wall"), "the row is the drop prefab, wall-seated");
const withDoor = dropKit(building("b2", 0, 6, [{ side: "front", x: 2, z: 9 }]), "parcel-drops");
check(withDoor.kit.rows[0].offset[0] > 2 && withDoor.kit.rows[0].offset[0] <= 4.4, `with a door at x 2 the mailbox stands beside it, got x ${withDoor.kit.rows[0].offset[0]}`);
const shifted = dropKit(building("b1", 0, 0), "parcel-drops", 0.8);
check(Math.abs(shifted.kit.rows[0].offset[0] - 0.8 * 5) < 0.01 || shifted.kit.rows[0].offset[0] !== noDoor.kit.rows[0].offset[0], "a sideways share moves the mailbox along the wall");

// turn: a quarter turn puts the mailbox on another side (here +x: the 10 m wide side is 5 m out, the row faces east).
const turned = dropKit(building("b1", 0, 0), "parcel-drops", null, 0, 1).kit.rows[0];
check(turned.offset[0] > 5 && turned.offset[0] < 8 && Math.abs(turned.yaw - 90) < 0.01, `turn 1 stands east of the box facing east, got ${JSON.stringify([turned.offset, turned.yaw])}`);

// checkOutposts: an empty list, the own place, a missing dossier, a missing building, a repeat.
const own = "home";
check(checkOutposts("parcel-drops", { drops: [] }, null, own).length === 1, "no drops is a problem");
check(checkOutposts("parcel-drops", { drops: [{ place: own, building: "b1" }] }, null, own).length === 1, "a drop in the own place is a problem");
const others = { dock: dossierOf("dock", [building("b1", 0, 0)]) };
check(checkOutposts("parcel-drops", { drops: [{ place: "dock", building: "b1" }] }, others, own).length === 0, "a real drop passes");
check(checkOutposts("parcel-drops", { drops: [{ place: "dock", building: "b9" }] }, others, own).length === 1, "a missing building is a problem");
check(checkOutposts("parcel-drops", { drops: [{ place: "gone", building: "b1" }] }, others, own).length === 1, "a missing dossier is a problem");
check(checkOutposts("parcel-drops", { drops: [{ place: "dock", building: "b1" }, { place: "dock", building: "b1" }] }, others, own).length === 1, "a repeated drop is a problem");

// slotGround reads each drop as an entry of another place's survey (`dossier`, the `others` map, #3058); the layer entity must stand at one
// of them (within the 2 m margin), not near the place.
const outposts = { dock: dossierOf("dock", [building("b1", 500, 0), building("b2", 800, 0)]) };
const entry = { drops: [{ place: "dock", building: "b1" }, { place: "dock", building: "b2" }] };
const gs = entry.drops.map((dr) => slotGround(dropAsEntry(dr), dossierOf(own, []), outposts));
check(gs.every((x) => x && x.poly), "slotGround reads one polygon per drop");
check(distToPolygon(500, 3.5, gs[0].poly) < 1 && distToPolygon(800, 3.8, gs[1].poly) < 1, "an entity at a drop is within its polygon");
check(slotGround(dropAsEntry({ place: "dock", building: "nope" }), dossierOf(own, []), outposts) === null, "an unknown drop building has no ground");
check(dropKeys(entry.drops).join() === "dock/b1,dock/b2", "dropKeys sorts place/building");

// checkClaimsAndFootprints on a small place with the outposts slot: a layer at the drops passes, one at the place centre fails.
const template = { schema: 1, kind: "town", slots: [{ id: "parcel-drops", required: false, kit: "parcel-drops", host: "outposts", poi: "none", needs: { interior: "optional" }, places: [] }] };
const mapping = { schema: 1, template: "town", place: own, slots: { "parcel-drops": entry } };
const claim = { schema: 1, place: own, slot: "parcel-drops", drops: entry.drops, layer: "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_home_parcel-drops.layer" };
const layerAt = (x, z) => `GenericEntity Fit_a : "{CF8596F431D9EE52}Prefabs/Props/Roleplay/LF_ParcelDrop.et" {\n coords ${x} 5 ${z}\n}\n`;
const run = (layer, c = claim) => checkClaimsAndFootprints({ place: own, dossier: dossierOf(own, []), mapping, template, others: outposts, claims: { [`${own}-parcel-drops.json`]: c }, layers: { "Place_home_parcel-drops.layer": layer } });
check(run(layerAt(500, 3.6)).errors.length === 0, `a mailbox at the first drop passes: ${run(layerAt(500, 3.6)).errors.join("; ")}`);
check(run(layerAt(0, 0)).errors.some((e) => /outside the 2 drop buildings/.test(e)), "a mailbox far from every drop fails the footprint check");
check(run(layerAt(500, 3.6), { ...claim, drops: [entry.drops[0]] }).errors.some((e) => /differ from the mapping/.test(e)), "a claim that names other drops than the mapping fails");

// With the harbour slot (#3058, a building of the dock survey under `dossier`): the same dock building may carry the harbour master AND
// a drop mailbox (drops take no exclusive hold), and both read their footprint through the one `others` map.
const tpl2 = { schema: 1, kind: "town", slots: [{ id: "harbour", required: false, kit: "harbour-master", host: "building", poi: "none", needs: { interior: "required" }, places: [] }, template.slots[0]] };
const map2 = { schema: 1, template: "town", place: own, slots: { harbour: { building: "b1", dossier: "dock" }, "parcel-drops": entry } };
const claimH = { schema: 1, place: own, slot: "harbour", building: "dock/b1", layer: "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_home_harbour.layer" };
const both = checkClaimsAndFootprints({ place: own, dossier: dossierOf(own, []), mapping: map2, template: tpl2, others: outposts, claims: { [`${own}-harbour.json`]: claimH, [`${own}-parcel-drops.json`]: claim }, layers: { "Place_home_harbour.layer": layerAt(500, 0), "Place_home_parcel-drops.layer": layerAt(500, 3.6) } });
check(both.errors.length === 0, `a harbour master and a drop mailbox share a dock building: ${both.errors.join("; ")}`);

// The real mapping: the template carries the outposts slot, and every drop is in its dossier.
const town = loadTemplate("town", ROOT);
check(town.slots.some((s) => s.id === "parcel-drops" && s.host === "outposts"), "the town template has the parcel-drops outposts slot");
const real = JSON.parse(readFileSync(join(ROOT, "docs/everon/places/saint-philippe/mapping.json"), "utf8"));
const ds = {};
for (const d of real.slots["parcel-drops"]?.drops ?? []) ds[d.place] ??= JSON.parse(readFileSync(join(ROOT, "docs/everon/places", d.place, "dossier.json"), "utf8"));
const base = JSON.parse(readFileSync(join(ROOT, "docs/everon/places/saint-philippe/dossier.json"), "utf8"));
const problems = checkMapping(real, base, town, ds).filter((p) => p.startsWith("slot parcel-drops:"));
check(problems.length === 0, `the saint-philippe parcel-drops entry is sound: ${problems.join("; ")}`);
check((real.slots["parcel-drops"]?.drops ?? []).length >= 3, "at least 3 parcel drops are mapped (5 wanted)");

// 300 m rule: every mapped drop building stands 300 m or more from the depot kiosk (b21), runMinDistance never lowered.
const depot = base.buildings.find((b) => b.id === real.slots.parcel.building);
for (const d of real.slots["parcel-drops"]?.drops ?? []) {
  const b = ds[d.place].buildings.find((x) => x.id === d.building);
  const dist = Math.hypot(b.x - depot.x, b.z - depot.z);
  check(dist >= 305, `${d.place}/${d.building} is ${dist.toFixed(0)} m from the depot, under the 300 m run minimum (+5 m for the mailbox spot)`);
}

if (fails.length) {
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(`SUMMARY town-drops-selftest fail ${fails.length}`);
  process.exit(1);
}
console.log("SUMMARY town-drops-selftest ok");
