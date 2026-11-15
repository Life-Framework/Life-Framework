// Proves the .layer semantic diff (tools/lib/world-diff.mjs, `tools\cli world diff`) on the maintainer's real
// Workbench save (tools/test/fixtures/world-diff/), and the pieces `world ship` builds on it.
// inputs: tools/lib/world-diff.mjs, tools/commands/world.mjs, tools/test/fixtures/world-diff/
// red-proof: with the angles normalisation off (angleY read as a property) this printed "real case: expected
//   1 MOVED + noise, got 1 MOVED + 1 PROP CHANGED + noise"; with re-order detection off, "re-order not marked as noise".
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { changeLine, diffFiles, diffLayer, handEditProblem, parseLayer, summaryLine, testTokens, touchedNames } from "../lib/world-diff.mjs";
import { allowEntries, prBody, workbenchState } from "../commands/world.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const FX = join(here, "..", "test", "fixtures", "world-diff");
const rd = (f) => readFileSync(join(FX, f), "utf8");
const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

const t0 = Date.now();
const sjB = rd("Street_Jobs.before.layer");
const sjA = rd("Street_Jobs.after.layer");
const dfB = rd("default.before.layer");
const dfA = rd("default.after.layer");

// The real case: 1 MOVED (S105 at TaxiPad, ~4.2 m) + noise.
const d = diffFiles([{ path: "Street_Jobs.layer", before: sjB, after: sjA }, { path: "default.layer", before: dfB, after: dfA }]);
check(summaryLine(d) === "1 MOVED + noise", `real case: expected 1 MOVED + noise, got ${summaryLine(d)}`);
const mv = d.files[0].changes[0];
check(mv && mv.kind === "MOVED" && /S105_beige\.et at Roadmap_Job_TaxiPad/.test(mv.label), `real case: the move is not S105 at TaxiPad: ${mv && changeLine(mv)}`);
check(mv && Math.abs(mv.metres - 4.19) < 0.02, `real case: distance ${mv && mv.metres} is not ~4.19 m`);
check(mv && changeLine(mv).includes("0 0 4 -> -0.773 -0.055 -0.119"), `real case: line lacks old -> new coords: ${mv && changeLine(mv)}`);
const sjNoise = d.files[0].noise.join(" | ");
check(/re-ordered/.test(sjNoise), "re-order not marked as noise");
check(/re-serialized/.test(sjNoise), "angleY 180 -> angles 0 180 0 not marked as noise");
check(/class prefix/.test(sjNoise), "dropped GenericEntity prefix not marked as noise");
check(!d.files[0].noiseOnly, "Street_Jobs.layer marked noise-only");
check(d.files[1].noiseOnly && /BSP/.test(d.files[1].noise.join()), `default.layer: expected BSP noise only, got ${JSON.stringify(d.files[1])}`);

// Each noise kind alone is noise only.
const ws = diffLayer("x.layer", sjB, sjB.replace(/\n/g, "\n\n").replace(/ coords 151 0 240.5/, "  coords   151 0 240.5"));
check(ws.changes.length === 0 && ws.noise.length, `whitespace: ${JSON.stringify(ws)}`);
const depotEnd = sjB.indexOf("\n}\n") + 3;
const reord = diffLayer("x.layer", sjB, sjB.slice(depotEnd) + sjB.slice(0, depotEnd));
check(reord.changes.length === 0 && /re-ordered/.test(reord.noise.join()), `re-order alone: ${JSON.stringify(reord)}`);
const ang = diffLayer("x.layer", sjB, sjB.replace("  angleY 180", "  angles 0 180 0"));
check(ang.changes.length === 0 && /re-serialized/.test(ang.noise.join()), `angles alone: ${JSON.stringify(ang)}`);
const ang360 = diffLayer("x.layer", sjB, sjB.replace("  angleY 180", "  angleY -180"));
check(ang360.changes.length === 0, `angleY 180 vs -180 is a move: ${JSON.stringify(ang360.changes)}`);

// Real changes of each kind.
const prop = diffLayer("x.layer", sjB, sjB.replace("m_eSite DEPOT", "m_eSite DROP"));
check(prop.changes.length === 1 && prop.changes[0].kind === "PROP CHANGED" && prop.changes[0].props.includes("components"), `prop change: ${JSON.stringify(prop.changes)}`);
const rot = diffLayer("x.layer", sjB, sjB.replace("  angleY 180", "  angleY 90"));
check(rot.changes.length === 1 && rot.changes[0].kind === "MOVED" && /angles 0 180 0 -> 0 90 0/.test(changeLine(rot.changes[0])), `rotation: ${rot.changes.map(changeLine)}`);
const KNOWN = '{12D5AD21E383B768}Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et';
const addTxt = `${sjB}$grp GenericEntity : "${KNOWN}" {\n Maintainer_Can_2 {\n  coords 10 0 20\n }\n}\n`;
const add = diffFiles([{ path: "a/Street_Jobs.layer", before: sjB, after: addTxt }]);
check(add.totals.added === 1 && add.files[0].changes[0].label === "Maintainer_Can_2", `add: ${summaryLine(add)}`);
const rem = diffFiles([{ path: "a/Street_Jobs.layer", before: addTxt, after: sjB }]);
check(rem.totals.removed === 1, `remove: ${summaryLine(rem)}`);
const nested = parseLayer(sjB).find((e) => e.name === "Roadmap_Job_FuelCan");
check(nested && nested.children.length === 1 && nested.children[0].name === "Roadmap_Job_FuelItem", "nested group child not parsed");

// The fast-tier rule: moves and known-prefab adds pass, a property change or an unknown prefab does not.
const known = (ref) => ref === KNOWN;
check(handEditProblem(d, known) === null, `real case refused: ${handEditProblem(d, known)}`);
check(handEditProblem(add, known) === null, `known-prefab add refused: ${handEditProblem(add, known)}`);
check(/no layer used before/.test(handEditProblem(add, () => false) || ""), "unknown-prefab add allowed");
check(/PROP CHANGED/.test(handEditProblem(diffFiles([{ path: "x.layer", before: sjB, after: sjB.replace("m_eSite DEPOT", "m_eSite DROP") }]), known) || ""), "prop change allowed");
check(/added/.test(handEditProblem(diffFiles([{ path: "n.layer", before: null, after: sjB }]), known) || ""), "new layer allowed");

// Tokens, allowlist and PR body.
const touched = touchedNames(d);
check(touched.length === 1 && touched[0].prefab && !touched[0].named, `touched: ${JSON.stringify(touched)}`);
const toks = testTokens(["Street_Jobs.layer"], touched);
check(toks.includes("Street_Jobs") && !toks.some((t) => /\.et$/.test(t)), `tokens: ${toks}`);
const report = { ranked: [{ name: "S105_beige", problems: [{ kind: "floating", text: "floating 0.2 m above terrain" }] }, { name: "Other", problems: [{ kind: "sunk", text: "x" }] }] };
const al = allowEntries(report, touched, []);
check(al.length === 1 && al[0].kind === "floating" && /maintainer hand placement \(world ship\)/.test(al[0].why), `allow entries: ${JSON.stringify(al)}`);
check(new RegExp(al[0]?.match ?? "^$").test(` ${touched[0].prefab}`), "allow entry does not match the audit's name+prefab string");
check(allowEntries(report, touched, al).length === 0, "allow entry added twice");
const body = prBody(d, { slug: "town-maintainer-edits", allowed: al, checks: ["validate: pass"] });
check(/## Manual testing/.test(body) && /MOVED S105_beige/.test(body) && /Placements allowlisted/.test(body) && /No screenshots: .{15,}/.test(body), "PR body lacks a section");

// Workbench unsaved state from the tasklist CSV.
const tl = (title) => `"Image Name","PID","Session Name","Session#","Mem Usage","Status","User Name","CPU Time","Window Title"\n"ArmaReforgerWorkbench.exe","1","Console","1","1 K","Running","x","0:00:01","${title}"`;
check(/unsaved/.test(workbenchState(tl("World Editor - DebugWorld.ent*")).refuse || ""), "dirty Workbench title not refused");
check(!workbenchState(tl("World Editor - DebugWorld.ent")).refuse && workbenchState(tl("World Editor")).warn, "clean Workbench not a warning");
check(!workbenchState("").refuse && !workbenchState("").warn, "no Workbench: warned");

const ms = Date.now() - t0;
check(ms < 3000, `world diff took ${ms} ms (budget 3 s)`);

if (fails.length) {
  for (const f of fails) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log(`world-diff-selftest: OK (${ms} ms)`);
