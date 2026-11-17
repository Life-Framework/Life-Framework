// tools/validation/town-ready-selftest.mjs
// inputs: tools/town/ready.mjs, agent-box/proofs/town-reach.mjs, docs/everon/places/saint-philippe/dossier.json
//
// Selftest for tools/town/ready.mjs and agent-box/proofs/town-reach.mjs pure parts (#2207).
// red-proof: 2026-11-17 with verdict() returning [] this printed "FAIL not run counts as not ready: expected 2, got 0" and "a failed check is one problem"; restored, PASS.
import { readFileSync } from "node:fs";
import { switchOffLines, parseReach, verdict } from "../town/ready.mjs";
import { acceptanceMarkers, slotTargets } from "../../agent-box/proofs/town-reach.mjs";

const fails = [];
const eq = (a, b, what) => { if (JSON.stringify(a) !== JSON.stringify(b)) fails.push(`${what}: expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`); };

eq(verdict([{ place: "a", check: true, reach: true, switchOff: true }]), [], "a fully green place is ready");
eq(verdict([{ place: "a", check: true, reach: null, switchOff: null }]).length, 2, "not run counts as not ready");
eq(verdict([{ place: "a", check: false, reach: true, switchOff: true }]).length, 1, "a failed check is one problem");
eq(switchOffLines("x\n[LFDebug:Systems] jail switches off: no JailCell placed\ny").length, 1, "a switches-off line is found");
eq(switchOffLines("[LFDebug:Bank] vault switched off by an admin").length, 0, "only the Systems feature counts");
eq(parseReach("noise\nTOWNREACH {\"place\":\"p\",\"pass\":true,\"results\":[]}\nPASS").pass, true, "the proof's json line parses");
eq(parseReach("no line"), null, "no line is null");

const d = JSON.parse(readFileSync(new URL("../../docs/everon/places/saint-philippe/dossier.json", import.meta.url), "utf8"));
const m = acceptanceMarkers(d);
const area = Math.max(...d.buildings.map((b) => b.footprint?.area ?? 0));
eq(d.buildings.find((b) => b.footprint?.cx === m.closed.x)?.footprint.area, area, "the closed marker is the biggest building's centre");
const dd = Math.hypot(m.from.x - m.reach.x, m.from.z - m.reach.z);
if (dd < 50 || dd > 150) fails.push(`the reachable marker is ${dd} m away, expected 50 to 150`);
const t = slotTargets({ buildings: [{ id: "b1", x: 1, y: 0, z: 2, entrances: [{ side: "front", x: 5, y: 0, z: 6 }] }, { id: "b2", x: 9, y: 0, z: 9 }], freePlots: [] }, { slots: { arrival: { building: "b1" }, shop: { building: "b2" } } });
eq(t.length, 1, "one target besides arrival");
eq(t[0].from, { x: 5, y: 0, z: 6 }, "arrival uses its front entrance");
eq(t[0].to, { x: 9, y: 0, z: 9 }, "an entrance-less building uses its centre");

for (const f of fails) console.log(`FAIL  ${f}`);
console.log(fails.length ? `SUMMARY town-ready-selftest fail ${fails.length}` : "SUMMARY town-ready-selftest ok");
process.exit(fails.length ? 1 : 0);
