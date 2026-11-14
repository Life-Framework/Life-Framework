// Proves the overhang math on fixed bounds (no game files needed).
import { checkEntities, overhang, footprint, prefabScale } from "../actions/prop-on-surface.mjs";
import { ROOT } from "../actions/place-action-contexts.mjs";
// red-proof: 2026-11-14 (#924), with flatten() reading only the layer scale line the market's cabbages
// (Cabbage.et carries scale 0.5) failed validate-prop-on-surface: "Cabbage_01 ... hangs 0.19 m off Market_Stall_Veg".

const counter = { file: "t", name: "Counter", mesh: "A/CounterShop_01.xob", bounds: { min: [-0.75, 0, -0.53], max: [0.75, 1.04, 0.6] }, x: 179, y: 0, z: 244.3, yaw: 0, scale: 1 };
const till = (x, yaw = 0, z = 244.3) => ({ file: "t", name: "Till", mesh: "A/Till_01.xob", bounds: { min: [-0.35, 0, -0.28], max: [0.24, 0.5, 0.23] }, x, y: 1.04, z, yaw, scale: 1 });
const eq = (got, want, what) => {
  if (got !== want) { console.log(`FAIL ${what}: ${got} != ${want}`); process.exit(1); }
};

eq(checkEntities([counter, till(179.05)]).length, 0, "till centred is fine");
eq(checkEntities([counter, till(179.85)]).length, 1, "till half off the east edge is flagged");
eq(checkEntities([counter, till(179.85)])[0].over > 0.3, true, "overhang is about 0.34 m");
eq(checkEntities([counter, till(179.2)]).length, 0, "till near an edge but inside is fine");
eq(checkEntities([counter, { ...till(179.85), y: 0 }]).length, 0, "a thing standing on the floor beside the counter is not 'on' it");
eq(checkEntities([counter, { ...till(179.05), yaw: 90 }]).length, 0, "rotated 90 still fits a 1.1 m deep counter");
eq(checkEntities([counter, till(179, 0, 244.9)]).length, 1, "till at the long end of an unrotated counter overhangs in z");
eq(checkEntities([{ ...counter, yaw: 90 }, till(179, 0, 244.9)]).length, 0, "the same till fits when the counter is rotated 90");
eq(footprint(till(0, 90)).length, 4, "footprint has four corners");
eq(overhang(counter, till(179)).over, 0, "no overhang when inside");
eq(prefabScale(ROOT, "Prefabs/Items/Food/Cabbage.et"), 0.5, "a prefab's own root scale is read when the layer has none (#924)");
eq(prefabScale(ROOT, "Prefabs/Props/Commercial/LF_Sign_Nope.et"), 1, "a prefab that cannot be read counts as scale 1");
console.log("prop-on-surface-selftest: OK");
