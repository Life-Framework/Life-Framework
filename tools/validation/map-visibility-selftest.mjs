// Selftest for the map visibility rule (#2230): the shared case table passes through the node twin,
// a deliberately broken table fails it, and the emulator's visible(poi, persona) agrees with the rule.
// The Enforce twin runs the same table in LF_Test_MapPois.
// inputs: tools/map/**, addons/LifeFramework/Configs/Map/**
import { evaluate, generatedIsCurrent, loadCases, runCases, visibleList, visible } from "../map/rules/visibility.mjs";

const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const table = loadCases();
const live = runCases(table);
expect(live.length === 0, `the case table must pass: ${live.join("; ")}`);
expect(generatedIsCurrent(), "the Enforce copy of the table is current: run node tools/map/rules/visibility.mjs --gen");
const inputs = table.cases.map((c) => c.input);
expect(inputs.length >= 12, "the table must cover every visibility and known state");
for (const v of ["PUBLIC", "DISCOVER", "ROLE", "HIDDEN"]) {
  expect(inputs.some((i) => i.visibility === v), `the table must have a ${v} case`);
}

// Red: a table that expects a HIDDEN place to show full must fail the rule.
const broken = structuredClone(table);
for (const c of broken.cases) {
  if (c.input.visibility === "HIDDEN" && c.input.known === "none") c.expect = "full";
}
expect(runCases(broken).length > 0, "a table whose HIDDEN case expects full must fail the rule");

// The rule.
expect(evaluate({ visibility: "DISCOVER", live: true, known: "heard" }) === "faded", "heard-of is faded");
expect(evaluate({ visibility: "BOGUS", live: true }) === "hidden", "an unknown visibility is never drawn");
expect(visibleList({ A: { visibility: "PUBLIC" } }, { b: { kind: "B", live: true } }, {}).length === 0, "a kind with no row never shows");

// The emulator adapter: stand-in `access` rows and game `visibility` rows, per persona.
const civic = { access: "CIVIC" };
const shop = { access: "DISCOVER", heardOf: false };
const clerkShop = { access: "DISCOVER", heardOf: true };
const armoury = { access: "POLICE" };
const fence = { access: "UNDERWORLD" };
expect(visible(civic, "new") === "full", "a new player sees a civic place");
expect(visible(shop, "new") === "hidden", "a new player does not see a shop nobody told them of");
expect(visible(clerkShop, "new") === "faded", "the clerk's shop is faded for a new player");
expect(visible(shop, "known") === "full", "a known player sees every shop in full");
expect(visible(armoury, "new") === "hidden" && visible(armoury, "police") === "full", "the armoury is for police");
expect(visible(fence, "new") === "hidden" && visible(fence, "known") === "pencil", "the fence is a pencil mark once met, never public");
expect(visible({ access: "ORG:crew" }, "org:crew") === "full" && visible({ access: "ORG:crew" }, "org:other") === "hidden", "an org place shows to its org only");
expect(visible({ visibility: "ROLE", role: "police" }, "police") === "full", "a game row with a role reads as one");

if (fails.length) {
  for (const f of fails) console.error(`FAIL ${f}`);
  process.exit(1);
}
console.log("map-visibility-selftest: ok");
