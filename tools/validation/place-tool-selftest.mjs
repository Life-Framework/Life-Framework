// Proves the placement tool's primitives (tools/place/place.mjs) on fixed bounds and synthetic probe
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
// rays: no game files, no server. Each block names the failure it guards (docs/placement-tool.md).
// red-proof: 2026-11-11, one line broken per primitive, each restored after its FAIL was seen:
//   restOn without the edge clamp       -> FAIL on: an east anchor keeps the till inside the edge
//   surfaceTop ignoring the probe cells -> FAIL top: the working top is the probed counter top, not the lower shelf
//   againstWall with `back` dropped     -> FAIL against: the back face stands 2 cm off the wall
//   onGround without the level check    -> FAIL ground: a kerb under the footprint is refused
//   arrangeOn without the cursor step   -> FAIL arrange: three crates do not touch
//   offlineProblems road test inverted  -> FAIL offline: a sign on the carriageway is refused
//   setTransform writing every depth    -> FAIL edit: a child's coords stay
//   judge without the gap test          -> FAIL judge: a base 0.2 m above its support fails
// red-proof: 2026-11-12, one line broken per move bug (#833 follow-up), each restored after its FAIL was seen:
//   setTransform ignoring `scale`       -> FAIL edit: --scale on a move writes the scale line
//   excludeOf returning item.name only  -> FAIL exclude: an unnamed moved entity is excluded by prefab and spot
//   onGround raising a tree by its roots (2026-11-14, #924) -> FAIL ground: a tree's pivot stands on the ground, its roots below
//   judge resting a tree on its bounds base (2026-11-14, #924) -> FAIL judge: a tree whose roots go 0.51 m below the ground
import { checkEntities } from "../actions/prop-on-surface.mjs";
import { obbPenetration } from "../../agent-box/place-audit.mjs";
import { PlaceError, TOL, againstWall, appendEntity, arrangeOn, boxOf, cacheFromRays, faceNormal, findEntity, footprintRect, judge, modelOf, offlineProblems, onGround, restOn, rot, rowCentres, excludeOf, setTransform, surfaceGrid, surfaceTop, unrot, yawForFacing } from "../place/place.mjs";

let failed = 0;
const ok = (cond, what) => {
  if (cond) return;
  failed++;
  console.log(`FAIL ${what}`);
};
const near = (a, b, tol = 1e-6) => Math.abs(a - b) <= tol;
const refuses = (fn, pattern, what) => {
  try {
    fn();
    ok(false, `${what} (no refusal)`);
  } catch (e) {
    ok(e instanceof PlaceError && pattern.test(e.message), `${what} (got: ${e.message})`);
  }
};

// ---- maths: the yaw convention is the engine's (yaw 90 turns local +z to world +x) -----------------
{
  const [x, z] = rot(90, 0, 1);
  ok(near(x, 1) && near(z, 0), "maths: yaw 90 points local +z at world +x");
  const [lx, lz] = unrot(37, ...rot(37, 0.3, -1.2));
  ok(near(lx, 0.3) && near(lz, -1.2), "maths: unrot undoes rot");
  ok(yawForFacing(180, "+z") === 180 && yawForFacing(180, "-z") === 0 && yawForFacing(0, "+x") === -90, "maths: yawForFacing turns the chosen front to the heading");
  const b = boxOf({ x: 0, y: 0, z: 0, yaw: 90, scale: 1, bounds: { min: [-1, 0, -1], max: [1, 1, 1] } });
  ok(near(b.a[2][0], 1) && near(b.a[2][2], 0), "maths: the oriented box's z axis follows the yaw");
}

// ---- on: the till that hung 0.34 m off the counter (#743) ------------------------------------------
const counterBounds = { min: [-0.75, 0, -0.53], max: [0.75, 1.04, 0.6] };
const counter = { name: "Counter", mesh: "A/CounterShop_01.xob", bounds: counterBounds, x: 179, y: 0, z: 244.3, yaw: 0, scale: 1 };
const tillBounds = { min: [-0.35, 0, -0.28], max: [0.24, 0.5, 0.23] };
const till = { bounds: tillBounds, scale: 1 };
const asEntity = (p, yaw = 0) => ({ file: "t", name: "Till", mesh: "A/Till_01.xob", bounds: tillBounds, x: p.x, y: p.y, z: p.z, yaw: p.yaw ?? yaw, scale: 1 });
{
  const c = restOn(counter, till);
  ok(near(c.y, 1.04), "on: the base rests on the top (mesh bounds top with no probe)");
  ok(checkEntities([counter, asEntity(c)]).length === 0, "on: centred, validate-prop-on-surface agrees");
  ok(checkEntities([counter, asEntity({ x: 179.85, y: 1.04, z: 244.33 })]).length === 1, "on: the #743 till position is the failure the validator flags");
  const east = restOn(counter, till, { at: "east" });
  const edge = east.x + tillBounds.max[0];
  ok(edge <= 179.75 - TOL.inset + 1e-6 && edge > 179.6, "on: an east anchor keeps the till inside the edge");
  ok(checkEntities([counter, asEntity(east)]).length === 0, "on: the east anchor passes the validator");
  const turned = { ...counter, yaw: 90 };
  const t2 = restOn(turned, till, { at: "north", yaw: 0 });
  ok(checkEntities([turned, asEntity(t2)]).length === 0 && t2.yaw === 90, "on: on a turned counter the till turns with it and stays on");
  const nearPt = restOn(counter, till, { near: [185, 244.3] });
  ok(near(nearPt.x, east.x, 1e-6), "on: --near clamps a far point to the nearest spot on the top");
  refuses(() => restOn(counter, { bounds: { min: [-1, 0, -1], max: [1, 0.3, 1] }, scale: 1 }), /does not fit/, "on: a prop wider than the top is refused");
}

// ---- surface tops: a counter with a lower shelf along its +z side; the probe finds the real top ----
{
  const { nx, nz, cells } = surfaceGrid(counterBounds);
  const hits = cells.map((c) => {
    if (c.j >= nz - 2) return { d: 0.66, len: 1.54, p: [0, 0.78, 0], hit: "Counter", root: "Counter" }; // the shelf
    if (c.i === 5 && c.j === 5) return { d: 0.2, len: 1.54, p: [0, 1.24, 0], hit: "Apple", root: "Apple" }; // something on the top
    return { d: 0.4, len: 1.54, p: [0, 1.04, 0], hit: "Counter", root: "Counter" };
  });
  const cache = cacheFromRays(counter, hits);
  ok(cache.h.filter((v) => v === "o").length === 1 && cache.h.filter((v) => v === 0.78).length === nx * 2, "top: rays become heights, an occupied cell and the shelf");
  const top = surfaceTop(counterBounds, cache);
  ok(near(top.y, 1.04, 1e-6), "top: the working top is the probed counter top, not the lower shelf");
  ok(top.rect.maxZ < counterBounds.max[2] - 0.1 && near(top.rect.minZ, counterBounds.min[2]), "top: the usable rectangle stops before the shelf");
  const north = restOn(counter, till, { top, at: "north" });
  ok(north.z + tillBounds.max[2] <= counter.z + top.rect.maxZ + 1e-6, "top: a north anchor stays on the working top");
  const lip = { ...cache, h: cache.h.map((v, k) => (cells[k].i === 8 && cells[k].j === 4 ? 1.2 : v)) };
  refuses(() => restOn(counter, till, { top: surfaceTop(counterBounds, lip) }), /not level/, "top: a raised part under the footprint is refused");
  ok(surfaceTop(counterBounds, null).source === "mesh bounds", "top: without a probe the source says mesh bounds");
}

// ---- against: the locker half inside the police wall, the vault door 1 m off its wall (#740, #742) -
const locker = { bounds: { min: [-0.37, 0, -0.25], max: [0.37, 1.98, 0.29] }, scale: 1 };
const wallRays = (dAt) => {
  const out = [];
  for (const lat of [-0.34, 0, 0.34]) for (let h = -0.5; h <= 3; h += 0.15) out.push({ lat, h, d: h < 0 ? Infinity : dAt(lat, h), label: "Wall" });
  return out;
};
const flatFloor = Array.from({ length: 31 }, (_, k) => ({ dist: k * 0.2, y: 0 }));
{
  const n = [0, 1]; // the face looks north; the prop's front (+z) faces north at yaw 0
  const res = againstWall({ rays: wallRays(() => 2), floors: flatFloor, prop: locker, yaw: 0, n });
  // along is metres from the start toward the wall; the back face (local z -0.25) is 0.25 m further.
  ok(near(res.along + 0.25, 2 - TOL.wallGap), "against: the back face stands 2 cm off the wall");
  ok(near(res.y, 0) && near(res.lat, 0), "against: it stands on the floor, centred on the start line");
  refuses(() => againstWall({ rays: wallRays((lat, h) => (h < 0.4 ? 1.88 : 2)), floors: flatFloor, prop: locker, yaw: 0, n }), /not flat/, "against: a plinth under the wall is refused");
  const off = againstWall({ rays: wallRays((lat, h) => (h < 0.4 ? 1.88 : 2)), floors: flatFloor, prop: locker, yaw: 0, n, allowUneven: true });
  ok(near(off.along + 0.25, 1.88 - TOL.wallGap), "against: --allow-uneven stands it off the plinth, never inside it");
  refuses(() => againstWall({ rays: wallRays((lat) => (lat > 0 ? Infinity : 2)), floors: flatFloor, prop: locker, yaw: 0, n }), /not flat/, "against: a doorway behind half the prop is refused");
  const passing = againstWall({ rays: wallRays((lat, h) => (h < 0.2 && lat === 0 ? 0.1 : 2)), floors: flatFloor, prop: locker, yaw: 0, n });
  ok(near(passing.along + 0.25, 2 - TOL.wallGap) && passing.notes.some((x) => /in front of it/.test(x)), "against: a low thing near the start does not move the face");
  refuses(() => againstWall({ rays: wallRays((lat, h) => (h < 1.5 ? 0.3 : 2)), floors: flatFloor, prop: locker, yaw: 0, n }), /stands between/, "against: an obstacle across most rays is refused");
  const board = { bounds: { min: [-0.6, -0.4, -0.02], max: [0.6, 0.4, 0.02] }, scale: 1 };
  const hung = againstWall({ rays: wallRays(() => 2), floors: flatFloor, prop: board, yaw: 0, n, hang: 1.4 });
  ok(near(hung.y, 1.8) && near(hung.along + 0.02, 2 - TOL.wallGap), "against: --hang puts the bottom at that height, flush");
  refuses(() => againstWall({ rays: wallRays(() => 2), floors: flatFloor.map((f) => (f.dist > 1.5 ? { ...f, y: 0.3 } : f)), prop: locker, yaw: 0, n }), /floor under the prop/, "against: a step under the prop is refused");
  const house = { x: 10, y: 0, z: 10, yaw: 30, scale: 1, bounds: { min: [-5, 0, -4], max: [5, 6, 4] } };
  const f = faceNormal(house, { near: [10 + rot(30, 0, 7)[0], 10 + rot(30, 0, 7)[1]] });
  ok(near(f.n[0], Math.sin(Math.PI / 6)) && near(f.n[1], Math.cos(Math.PI / 6)), "against: a turned building's face normal is its own axis");
  ok(near(faceNormal(house, { face: "south" }).n[1], -Math.cos(Math.PI / 6)), "against: --face south picks the side nearest south");
}

// ---- ground: the tree on asphalt, the sign in the lane (#742) --------------------------------------
{
  const tree = { bounds: { min: [-0.3, -0.1, -0.3], max: [0.3, 5, 0.3] }, scale: 1 };
  const g = onGround({ samples: [{ y: 2.5 }, { y: 2.5 }, { y: 2.52 }, { y: 2.49 }, { y: 2.5 }], prop: tree });
  ok(near(g.y, 2.6), "ground: the mesh base, not the pivot, stands on the ground");
  const rowan = { name: "Market_Tree_1", prefab: "{AA557A5ECB3DF0B1}Prefabs/Vegetation/Tree/t_sorbus_aucuparia/t_sorbus_aucuparia_2s.et", bounds: { min: [-2, -0.394, -2.4], max: [2.3, 6, 2] }, scale: 1.3 };
  ok(near(onGround({ samples: [{ y: 0 }, { y: 0 }, { y: 0 }, { y: 0 }, { y: 0 }], prop: rowan }).y, 0), "ground: a tree's pivot stands on the ground, its roots below (#924)");
  refuses(() => onGround({ samples: [{ y: 0 }, { y: 0 }, { y: 0.4 }, { y: 0 }, { y: 0.4 }], prop: tree }), /not level/, "ground: a kerb under the footprint is refused");
}

// ---- arrange: four cabbages wider than their crate, a cabinet through a till (#742) ---------------
{
  const crate = { bounds: { min: [-0.2, 0, -0.15], max: [0.2, 0.3, 0.15] }, scale: 1 };
  const table = { name: "Table", x: 50, y: 0, z: 50, yaw: 40, scale: 1, bounds: { min: [-0.9, 0, -0.4], max: [0.9, 0.8, 0.4] } };
  const placed = arrangeOn(table, [crate, crate, crate]);
  const boxes = placed.map((p) => boxOf({ ...p, scale: 1, bounds: crate.bounds }));
  ok(obbPenetration(boxes[0], boxes[1]) < -0.04 && obbPenetration(boxes[1], boxes[2]) < -0.04, "arrange: three crates do not touch");
  ok(placed.every((p) => near(p.y, 0.8) && p.yaw === 40), "arrange: all rest on the top, turned with the table");
  const mid = unrot(40, placed[1].x - 50, placed[1].z - 50);
  ok(near(mid[0], 0) && near(mid[1], 0), "arrange: the row is centred");
  const cabbage = { bounds: { min: [-0.41, -0.1, -0.47], max: [0.42, 0.27, 0.44] }, scale: 1 };
  refuses(() => arrangeOn({ ...table, bounds: { min: [-0.3, 0, -0.2], max: [0.3, 0.3, 0.2] } }, [cabbage, cabbage, cabbage, cabbage]), /do not fit/, "arrange: four cabbages on a 0.6 m crate are refused");
  const deep = { bounds: { min: [-0.1, 0, -0.35], max: [0.1, 0.2, 0.35] }, scale: 1 };
  const shelf = { ...table, yaw: 0, bounds: { min: [-0.9, 0, -0.2], max: [0.9, 0.8, 0.2] } };
  ok(arrangeOn(shelf, [deep, deep])[0].yaw === 90, "arrange: a prop too deep for the top is turned to fit");
  ok(rowCentres([0, 0], [4, 0], [1, 1, 1]).map((c) => c[0]).join() === "0,2,4", "arrange: a row spaces its centres evenly");
  refuses(() => rowCentres([0, 0], [1, 0], [1, 1, 1]), /too short/, "arrange: a row too short for its props is refused");
}

// ---- offline checks: roads, doors, neighbours, simulated bodies, scale ----------------------------
{
  const sign = { name: "Sign_Test", prefab: "Prefabs/Props/Signs/LF_Sign_PolePlate.et", x: 10, y: 0, z: 0, yaw: 0, scale: 1, bounds: { min: [-0.4, 0, -0.05], max: [0.4, 2, 0.05] } };
  const roads = [{ name: "Village_MainStreet", width: 7, pts: [[0, 0], [100, 0]] }];
  ok(offlineProblems(sign, { roads }).some((p) => /carriageway/.test(p)), "offline: a sign on the carriageway is refused");
  ok(!offlineProblems({ ...sign, z: 4.2 }, { roads }).length, "offline: the same sign on the verge is fine");
  const door = { name: "Door", root: "House_01", rp: [30, 0, -5], box: { c: [30, 1, 0], h: [0.5, 1, 0.03], a: [[1, 0, 0], [0, 1, 0], [0, 0, 1]] } };
  const pot = { name: "Pot", prefab: "Prefabs/Props/Flowerpot.et", x: 30, y: 0, z: 0.6, yaw: 0, scale: 1, bounds: { min: [-0.3, 0, -0.3], max: [0.3, 0.8, 0.3] } };
  ok(offlineProblems(pot, { doors: [door] }).some((p) => /swing of a door/.test(p)), "offline: a pot in a door's swing is refused");
  ok(offlineProblems({ ...pot, z: 2.4 }, { doors: [door] }).some((p) => /walk-up/.test(p)), "offline: a pot in the walk-up to a door is refused");
  ok(!offlineProblems({ ...pot, x: 33 }, { doors: [door] }).length, "offline: beside the door is fine");
  const booth = { file: "T.layer", name: "Payphone_1", prefab: "Prefabs/Comms/LF_Payphone.et", x: 30, y: 0, z: 30, yaw: 0, scale: 1, bounds: { min: [-0.5, 0, -0.5], max: [0.5, 2.2, 0.5] }, parent: null };
  const bin = { ...pot, name: "Bin", prefab: "Prefabs/Props/TrashBin.et", x: 30.1, z: 30 };
  ok(offlineProblems(bin, { ents: [booth] }).some((p) => /overlaps Payphone_1/.test(p)), "offline: a bin inside a payphone booth is refused");
  ok(!offlineProblems({ ...bin, self: booth }, { ents: [booth] }).length, "offline: an entity does not collide with its own old spot");
  ok(!offlineProblems({ ...bin, y: 2.2 }, { ents: [booth] }).length, "offline: resting on top of something is not an overlap");
  const log = { name: "Log", prefab: "Prefabs/Resources/WoodLog.et", x: 0, y: 0.8, z: 50, yaw: 0, scale: 1, dynamic: true, support: booth, bounds: { min: [-0.09, 0, -0.09], max: [0.09, 1.9, 0.09] } };
  ok(offlineProblems(log, {}).some((p) => /simulated rigid body/.test(p)), "offline: a tall simulated log stood on a table is refused");
  ok(!offlineProblems({ ...log, bounds: { min: [-0.09, 0, -0.95], max: [0.09, 0.18, 0.95] } }, {}).length, "offline: the same log lying flat is allowed");
  ok(offlineProblems({ ...pot, scale: 3 }, {}).some((p) => /scale 3/.test(p)), "offline: a prop at three times its size is refused");
}

// ---- layer edits: only the transform lines change ---------------------------------------------------
const LAYER = `$grp GenericEntity {
 Shop_Counter {
  components {
   MeshObject "{5A4B0B84C86E2841}" {
    Object "{3200A3508CA2E09B}Assets/Props/Commercial/CounterShop_01/CounterShop_01.xob"
   }
  }
  coords 179 0 244.3
 }
}
GenericEntity Shop_Till : "{4E4E1620C6B02E1F}Prefabs/Props/Commercial/EL_FuelTill.et" {
 components {
  ActionsManagerComponent "{5A4B0B84C86E4845}" {
  }
 }
 coords 179.85 1.04 244.33
 {
  GenericEntity Shop_Till_Light : "{AAAA1620C6B02E1F}Prefabs/Props/Light.et" {
   coords 0 0.5 0
  }
 }
}
GenericEntity : "{0E3533EE196F01EC}Prefabs/Lamps/LampStreet.et" {
 coords 196 0 255.4
}
GenericEntity : "{0E3533EE196F01EC}Prefabs/Lamps/LampStreet.et" {
 coords 204.5 0 255.4
}
`;
{
  const moved = setTransform(LAYER, "Shop_Till", { coords: [179.0561, 1.039, 244.2764], yaw: 90 });
  ok(moved.includes(" coords 179.056 1.039 244.276\n angleY 90\n"), "edit: coords rounded to the millimetre, angleY added after it");
  ok(moved.includes("   coords 0 0.5 0\n") && moved.includes("  coords 179 0 244.3\n"), "edit: a child's coords stay");
  ok(moved.replace(" coords 179.056 1.039 244.276\n angleY 90\n", " coords 179.85 1.04 244.33\n") === LAYER, "edit: no other byte changes");
  const again = setTransform(moved, "Shop_Till", { coords: [179, 1.039, 244], yaw: 0 });
  ok(again.includes(" coords 179 1.039 244\n angleY 0\n"), "edit: an existing angleY line is rewritten, not doubled");
  // `place ground --move X --scale S` kept the old scale: the move wrote coords and angleY only.
  const scaled = setTransform(LAYER, "Shop_Till", { coords: [179.0561, 1.039, 244.2764], yaw: 90, scale: 0.8 });
  ok(scaled.includes(" coords 179.056 1.039 244.276\n angleY 90\n scale 0.8\n"), "edit: --scale on a move writes the scale line after angleY");
  const rescaled = setTransform(scaled, "Shop_Till", { coords: [179, 1.039, 244], yaw: 90, scale: 0.5 });
  ok(rescaled.includes(" angleY 90\n scale 0.5\n") && !rescaled.includes("scale 0.8"), "edit: an existing scale line is rewritten, not doubled");
  ok(setTransform(LAYER, "Shop_Till", { coords: [1, 2, 3], yaw: 0 }) === setTransform(LAYER, "Shop_Till", { coords: [1, 2, 3], yaw: 0, scale: null }) && !setTransform(LAYER, "Shop_Till", { coords: [1, 2, 3], yaw: 0 }).includes("scale"), "edit: no --scale leaves the scale alone");
  // The ground probe stood on the collision of the pot being moved: an unnamed one had no exclude.
  const pot = { mode: "move", name: "", prefab: "Prefabs/Props/FlowerPot.et", ent: { x: 241.4, y: 0.1234, z: 241.3 } };
  ok(excludeOf(pot) === "@FlowerPot:241.4,0.123,241.3", `exclude: an unnamed moved entity is excluded by prefab and spot (got ${excludeOf(pot)})`);
  ok(excludeOf({ ...pot, name: "Pot_1" }) === "Pot_1" && excludeOf({ ...pot, mode: "new" }) === "", "exclude: a named entity by name, a new one not at all");
  const lamp = setTransform(LAYER, { prefab: "Prefabs/Lamps/LampStreet.et", coordsText: "196 0 255.4" }, { coords: [199, 0, 255.4], yaw: 0 });
  ok(lamp.includes(" coords 199 0 255.4\n") && lamp.includes(" coords 204.5 0 255.4\n") && !lamp.includes("coords 196 "), "edit: an unnamed entity is found by its prefab and coords");
  refuses(() => setTransform(LAYER, "Shop_Nope", { coords: [0, 0, 0], yaw: 0 }), /0 entity blocks/, "edit: an unknown entity is refused");
  const added = appendEntity(LAYER, { cls: "SCR_DestructibleEntity", name: "Shop_Crate", guid: "2B1D0128ED660BE9", path: "Prefabs/Props/Crates/BoxWooden_01.et", coords: [1.23456, 0, -2], yaw: -90, scale: null });
  ok(added.startsWith(LAYER) && added.endsWith('SCR_DestructibleEntity Shop_Crate : "{2B1D0128ED660BE9}Prefabs/Props/Crates/BoxWooden_01.et" {\n coords 1.235 0 -2\n angleY -90\n}\n'), "edit: a new entity is appended in Workbench's form, the rest untouched");
  const ents = modelOf("/nowhere", LAYER, "T.layer");
  ok(ents.length === 5 && findEntity(ents, "Shop_Counter").x === 179 && findEntity(ents, "Shop_Till_Light").y === 1.54, "model: names, group children and a child's world position");
  ok(findEntity(ents, "LampStreet@204,255").x === 204.5, "model: an unnamed entity is addressed by prefab and the nearest point");
  refuses(() => findEntity(ents, "LampStreet"), /say which/, "model: two unnamed lamps need a point");
}

// ---- judge: the audit's verdict on what was written ------------------------------------------------
{
  const row = (o) => `bmin=0,0,0 bmax=1,1,1 gap=${o.gap ?? 0} tilt=0 walk=-1 pgap=0 clip=${o.clip ?? "none"} scale=1 on=${o.on ?? "Counter"} over=${o.over ?? 0} drop=0 backed=${o.backed ?? 0} c=5,0.3,5 h=0.3,0.3,0.3 a0=${o.a0 ?? "1,0,0"} a1=0,1,0 a2=${o.a2 ?? "0,0,1"} flags=none found=x`;
  const recs = [{ name: "Till", layer: "T.layer", x: 5, y: 1, z: 5, prefab: "Prefabs/Props/Till.et" }];
  const t = { name: "Till", prefab: "Prefabs/Props/Till.et", x: 5, y: 1, z: 5, yaw: 0, hung: false };
  ok(!judge([t], recs, `0 ${row({})}`)[0].problems.length, "judge: a clean row passes");
  ok(judge([t], recs, `0 ${row({ gap: 0.2 })}`)[0].problems.some((p) => /floating|above/.test(p)), "judge: a base 0.2 m above its support fails");
  ok(judge([t], recs, `0 ${row({ gap: 0.08 })}`)[0].problems.some((p) => /0.08 m above/.test(p)), "judge: 8 cm of air fails the tool though the audit's own limit is 15 cm");
  ok(judge([t], recs, `0 ${row({ over: 2 })}`)[0].problems.some((p) => /hang off/.test(p)), "judge: corners off the edge fail");
  const tree = { name: "Market_Tree_1", prefab: "Prefabs/Vegetation/Tree/t_sorbus_aucuparia/t_sorbus_aucuparia_2s.et", x: 5, y: 1, z: 5, yaw: 0, hung: false };
  const treeRecs = [{ ...recs[0], name: "Market_Tree_1", prefab: tree.prefab }];
  ok(!judge([tree], treeRecs, `0 ${row({ gap: -0.51, on: "terrain" }).replace("h=0.3,0.3,0.3", "h=2.2,3.2,2.2")}`)[0].problems.length, "judge: a tree whose roots go 0.51 m below the ground, its pivot on it, passes (#924)");
  ok(judge([t], recs, `0 ${row({ a0: "0,0,-1", a2: "1,0,0" })}`)[0].problems.some((p) => /facing/.test(p)), "judge: a yaw the engine read differently fails");
  ok(judge([t], recs, "0 missing=1")[0].problems.some((p) => /not in the world/.test(p)), "judge: a prefab that did not load fails");
  ok(judge([{ ...t, hung: true }], recs, `0 ${row({ gap: 1.4, backed: 0 })}`)[0].problems.some((p) => /hangs in the air|floating/.test(p)), "judge: a hung thing with nothing behind it fails");
}

if (failed) {
  console.log(`place-tool-selftest: FAILED (${failed})`);
  process.exit(1);
}
console.log("place-tool-selftest: OK");
