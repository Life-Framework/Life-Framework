// Selftest for the shared-mark rules (#2237): the shared case table passes through the node twin, a
// deliberately broken table fails it, and the Enforce copy of the table is current. The Enforce twin runs
// the same table in LF_Test_MapShares.
// inputs: tools/map/**, addons/LifeFramework/Configs/Map/**, addons/LifeFramework/Scripts/Game/Feature/Map/**
import { AREA_STYLE_IDS, areaValid, diff, evaluate, expired, generatedIsCurrent, inAudience, loadCases, runCases } from "../map/rules/shares.mjs";
import { STYLES } from "../map/rules/areas.mjs";

const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const table = loadCases();
const live = runCases(table);
expect(live.length === 0, `the case table must pass: ${live.join("; ")}`);
expect(generatedIsCurrent(), "the Enforce copy of the table is current: run node tools/map/rules/shares.mjs --gen");
const inputs = table.cases.map((c) => c.input);
for (const a of ["PLAYER", "ROLE", "ORG", "EVERYONE"]) {
  expect(inputs.some((i) => i.op === "audience" && i.audience === a), `the table must have a ${a} case`);
}
for (const op of ["expired", "diff", "area"]) expect(inputs.some((i) => i.op === op), `the table must have a ${op} case`);

// Red: a table that lets a medic see a police row must fail the rule.
const broken = structuredClone(table);
for (const c of broken.cases) {
  if (c.input.op === "audience" && c.input.id === "police" && c.input.roles.includes("medic")) c.expect = "yes";
}
expect(runCases(broken).length > 0, "a table whose medic case receives a police row must fail the rule");
const brokenArea = structuredClone(table);
for (const c of brokenArea.cases) {
  if (c.input.op === "area" && c.input.radius > 5000) c.expect = "yes";
}
expect(runCases(brokenArea).length > 0, "a table that lets an oversize area through must fail the rule");

expect(!inAudience("ROLE", "police", { roles: [] }), "an off-duty player gets no police row");
expect(inAudience("EVERYONE", "", {}), "everyone means everyone");
expect(!expired(0, 1e9) && expired(5, 5), "0 never expires; a row expires at its time");
expect(diff(["a"], ["a", "b"]).add.join() === "b", "a diff adds only what is new");
expect(!areaValid(0, 5) && areaValid(150, 5) && !areaValid(5001, 5) && !areaValid(150, 6), "an area needs 1 to 5000 m and a known style id");
expect(AREA_STYLE_IDS.length === Object.keys(STYLES).length && AREA_STYLE_IDS.every((k) => k in STYLES), "the share twin's style ids are the area styles' (LF_MapAreaStyle)");
expect(evaluate({ op: "nope" }).startsWith("unknown"), "an unknown op is reported, not passed");

if (fails.length) {
  for (const f of fails) console.error(`FAIL ${f}`);
  process.exit(1);
}
console.log("map-shares-selftest: ok");
