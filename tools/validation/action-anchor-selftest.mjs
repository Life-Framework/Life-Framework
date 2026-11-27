// Proves the anchor math and the reach check on fixed bounds (no game files needed).
// inputs: tools/**, agent-box/**, .githooks/**
import { customerDistance } from "../actions/counter-npc.mjs";
import { anchorPoint, suggestRadius, gapToBounds, reachDistance, isCharacterMesh } from "../actions/action-anchor.mjs";

const min = [-1, 0, -0.5];
const max = [1, 0.8, 0.5];
const eq = (a, b, what) => {
  if (JSON.stringify(a) !== JSON.stringify(b)) { console.log(`FAIL ${what}: ${JSON.stringify(a)} != ${JSON.stringify(b)}`); process.exit(1); }
};
eq(anchorPoint(min, max, "center"), [0, 0.4, 0], "center");
eq(anchorPoint(min, max, "top"), [0, 0.8, 0], "top");
eq(anchorPoint(min, max, "front", { height: 1.0 }), [0, 0.8, 0.5], "front clamps to the top");
eq(anchorPoint(min, max, "front", { height: 0.3, front: "-z" }), [0, 0.3, -0.5], "front -z");
eq(suggestRadius(min, max, "front"), 1.5, "front radius");
eq(gapToBounds([0, 2.0, 0], min, max) > 1.1, true, "floating point is outside");
eq(reachDistance([0, 1, 0], min, max) <= 1.5, true, "centre point reachable at r1.5 from a 1 m wide desk");
eq(reachDistance([0, 1, 0], min, max), 1, "nearest standing spot is 1 m from the centre");
// A counter NPC: a clerk at the origin facing +Z with a 1.2 x 0.9 m counter 1 m in front; the customer
// stands 0.35 m past its far face (1.45 m + 0.35 m), so a 0.6 m sphere cannot reach it and 2.4 m can.
const box = (cx, cz, hx, hz) => [[cx - hx, cz - hz], [cx - hx, cz + hz], [cx + hx, cz - hz], [cx + hx, cz + hz]];
const across = customerDistance({ x: 0, z: 0, yaw: 0 }, box(0, 1, 0.6, 0.45), [0, 1]);
eq(Math.round(across.distance * 100), 180, "customer stands 1.8 m from a clerk across a 0.9 m counter");
eq(across.distance > 0.6 && across.distance <= 2.4, true, "0.6 m fails and 2.4 m reaches across the counter");
eq(customerDistance({ x: 0, z: 0, yaw: 0 }, box(0, -1, 0.6, 0.45), [0, -1]), null, "a counter behind the clerk is not between clerk and customer");
eq(customerDistance({ x: 0, z: 0, yaw: 90 }, box(1, 0, 0.45, 0.6), [1, 0]) !== null, true, "yaw 90 faces +X");
eq(isCharacterMesh("Assets/Characters/Basebody/Basebody_Male_01.xob"), true, "a character body is skipped by the radius check");
eq(isCharacterMesh("Basebody_Male_01.xob"), true, "a bare body name too");
eq(isCharacterMesh("Assets/Props/Furniture/Chair_02.xob"), false, "a chair keeps the radius check");
eq(isCharacterMesh(undefined), false, "no mesh is not a character");
console.log("action-anchor-selftest: OK");
