// Proves the interaction-point check (tools/actions/interaction-points.mjs): a point under or flush with the top it
// is over is flagged, one above it is not, and the fix moves only that Offset's y. Real tops come from the committed
// table, so this needs no game install.
// inputs: tools/actions/**, tools/validation/interaction-points-selftest.mjs
// red-proof: with buriedBy returning null at once, "booking desk's old point (0 0.9 0, top 0.906) is flagged" printed FAIL
//   and the run exited 1 (2026-11-24); restored, it passes.
import { allowedBy, buriedBy, clearY, fixOne, loadTable, CLEARANCE } from "../actions/interaction-points.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const table = loadTable().meshes;
const surfaces = (mesh) => table[mesh]?.surfaces ?? [];

const DESK = "Assets/Props/Furniture/Desk_01/Desk_01.xob";
check(surfaces(DESK).length > 0, "the table holds the booking desk's model (tools/actions/interaction-surfaces.json)");

// The real case: #4803's booking desk. The point was 0 0.9 0 on a desk whose top is 0.906 m.
const desk = buriedBy([0, 0.9, 0], surfaces(DESK));
check(desk && Math.abs(desk.y - 0.906) < 0.005, "booking desk's old point (0 0.9 0, top 0.906) is flagged");
check(buriedBy([0, 1.05, 0], surfaces(DESK)) === null, "the fixed point (0 1.05 0) is clear");
check(buriedBy([0, 0.93, 0], surfaces(DESK)) === null, "a point 2.5 cm above the top is clear");
check(buriedBy([0, 0.915, 0], surfaces(DESK)) !== null, "a point 9 mm above the top is flush with it");

// Synthetic tops [y, x0, x1, z0, z1].
const slab = [[1.0, -1, 1, -1, 1]];
check(buriedBy([0, 0.97, 0], slab) !== null, "3 cm under a top is flagged");
check(buriedBy([0, 0.81, 0], slab) !== null, "19 cm under a top is still flagged");
check(buriedBy([0, 0.79, 0], slab) === null, "21 cm under a top is a body point, not flagged");
check(buriedBy([2, 0.97, 0], slab) === null, "a point beside the top (x 2 m) is not over it");
check(buriedBy([1.01, 0.97, 0], slab) !== null, "a point 1 cm past the edge still counts as over it");
check(buriedBy([0, 0.97, 0], [[1.0, -0.05, 0.05, -1, 1]]) === null, "a 10 cm wide rim is not a top a ray lands on");
// A bookcase: a shelf board low inside, the roof above it. The roof is the top over that point, 0.8 m up: not flagged.
const bookcase = [[1.85, -1, 1, 0, 0.5], [0.228, -1, 1, 0, 0.5]];
check(buriedBy([0, 0.212, 0.2], bookcase) === null, "a point just above a low shelf board is not flagged: the roof is the top over it");
// A counter with a back bar: the top at the point's x and z is the counter's, not the back bar's.
const counterBar = [[1.484, -0.75, 0.75, 0.3, 0.6], [1.039, -0.75, 0.75, -0.5, 0.2]];
check(buriedBy([0, 0.9, 0], counterBar)?.y === 1.039, "a counter with a back bar: the counter top over the point is the one flagged");
check(buriedBy([0, 0.9, 0.45], counterBar) === null, "under the back bar (0.58 m) the top is out of reach: not flagged");

check(clearY({ y: 1.039 }) === Math.round((1.039 + CLEARANCE) * 1000) / 1000, "clearY is the top plus the clearance");
check(buriedBy([0, clearY({ y: 1.0 }), 0], slab) === null, "a fixed point is never flagged again");

// allow list
check(allowedBy("addons/LifeFramework/Prefabs/Placeables/Furniture/Furniture_Bed_Placed.et", [["addons/LifeFramework/Prefabs/Placeables/Furniture/", "why"]]) === "why", "an allowed prefix is skipped");
check(allowedBy("addons/LifeFramework/Prefabs/Props/LF_X.et", [["addons/LifeFramework/Prefabs/Placeables/", "why"]]) === null, "another path is not");

// fix: only that Offset's y changes, in the context named by its GUID
const prefab = [
  'GenericEntity : "{6B7B7CCC74AACA67}Prefabs/Props/Furniture/Desk_01_short.et" {',
  " components {",
  '  ActionsManagerComponent "{809919DD29FAB60F}" {',
  "   ActionContexts {",
  '    UserActionContext "{C5D84A388E018A21}" {',
  '     ContextName "Desk"',
  '     Position PointInfo "{74573C75AEC63F85}" {',
  "      Offset 0 0.9 0",
  "     }",
  "     Radius 1.8",
  "    }",
  '    UserActionContext "{AAAA4A388E018A22}" {',
  '     ContextName "Other"',
  '     Position PointInfo "{BBBB3C75AEC63F86}" {',
  "      Offset 0 0.9 0",
  "     }",
  "    }",
  "   }",
  "  }",
  " }",
  "}",
  "",
].join("\n");
const fixed = fixOne(prefab, { name: "(root)", ctx: "C5D84A388E018A21", to: 0.956 });
check(typeof fixed === "string" && fixed.includes("Offset 0 0.956 0"), "fixOne sets the flagged Offset's y");
check(fixed && fixed.split("Offset 0 0.9 0").length === 2, "fixOne leaves the other context's Offset alone");
check(fixed && fixed.replace("Offset 0 0.956 0", "Offset 0 0.9 0") === prefab, "fixOne changes nothing else");
check(fixOne(prefab, { name: "(root)", ctx: "DEADBEEFDEADBEEF", to: 1 }) === null, "an unknown context is not rewritten");
// A layer entity is addressed by its name (and Name2 is not Name).
const layer = ['GenericEntity Desk2 : "{6B7B7CCC74AACA67}Prefabs/x.et" {', " coords 0 0 0", "}", prefab.replace("GenericEntity :", "GenericEntity Desk :"), ""].join("\n");
const layerFixed = fixOne(layer, { name: "Desk", ctx: "C5D84A388E018A21", to: 1.1 });
check(layerFixed && layerFixed.includes("Offset 0 1.1 0"), "a layer entity is found by its name, not by a longer name that starts with it");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  console.log(`interaction-points-selftest: FAILED (${fails.length})`);
  process.exit(1);
}
console.log("interaction-points-selftest: OK");
