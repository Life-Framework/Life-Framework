// Proves the anchor math and the reach check on fixed bounds (no game files needed).
import { anchorPoint, suggestRadius, gapToBounds, reachDistance } from "../actions/action-anchor.mjs";

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
console.log("action-anchor-selftest: OK");
