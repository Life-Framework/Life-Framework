// Proves the 0.3.0 pay ladder in `tools\cli economy` (#4178, X-8 of #4170; docs/roadmap/next-release-0.3.0.md
// "The economy table"): police duty is the lowest steady pay ($350 to $450 a shift), medics sit on the mid-tier rung
// ($480 to $560, at most $600 with the bonus cap), and the report flags a police rank above the entry rung or over the
// $520 mid-tier floor with its bonus, a medic rank off the mid-tier rung or over $600 with its bonus, and a civilian loop
// below the Cadet shift. On the shipped configs only stone on foot is flagged ("you'll want a truck").
// inputs: tools/lib/economy-lumber.mjs, tools/lib/economy-mining.mjs, tools/lib/economy-wheat.mjs, tools/lib/economy.mjs, tools/commands/economy.mjs, addons/LF_Everon1989/Configs/Police/LF_Police_Everon1989.conf, addons/LF_Everon1989/Configs/Medic/LF_Medic_Everon1989.conf, tools/validation/economy-ladder-selftest.mjs
// red-proof: written first against the 1.5 to 2.5x band configs (police 0.25 to 0.35/s, medic 0.22 to 0.34/s); it
// printed "FAIL police shifts" ($900 to $1260), "FAIL medic shifts" and "FAIL the ladder exports" (no LADDER,
// dutyLadder, renderDutyLadder); with the retuned rows and the rung checks it prints "economy-ladder-selftest: OK".
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as lumber from "../lib/economy-lumber.mjs";
import { buildReport } from "../lib/economy.mjs";
import * as cmd from "../commands/economy.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

// 1. The live rates, exactly as the plan's table writes them.
const rates = (rel) => [...readFileSync(join(root, rel), "utf8").matchAll(/m_fPayPerSecond\s+([\d.]+)/g)].map((m) => m[1]);
const pr = rates("addons/LF_Everon1989/Configs/Police/LF_Police_Everon1989.conf");
const mr = rates("addons/LF_Everon1989/Configs/Medic/LF_Medic_Everon1989.conf");
expect(same(pr, ["0.0972", "0.1042", "0.1111", "0.1181", "0.125"]), `police m_fPayPerSecond rows (got ${pr.join(", ")})`);
expect(same(mr, ["0.1333", "0.1389", "0.1472", "0.1556"]), `medic m_fPayPerSecond rows (got ${mr.join(", ")})`);

// 2. The merged report: the shift per rank, in rank order.
const rep = buildReport(root, {});
expect(!rep.error, `the report builds (${rep.error})`);
const shifts = (k) => (rep.duty?.[k]?.ranks ?? []).map((r) => r.perHour);
expect(same(shifts("police"), [350, 375, 400, 425, 450]), `police shifts $350/$375/$400/$425/$450 (got ${shifts("police").join("/")})`);
expect(same(shifts("medic"), [480, 500, 530, 560]), `medic shifts $480/$500/$530/$560 (got ${shifts("medic").join("/")})`);

// 3. The rung checks on fixtures.
const { LADDER, dutyLadder, rungCheck, renderDutyLadder } = lumber;
expect(typeof cmd.ladderReport === "function" && LADDER && typeof dutyLadder === "function" && typeof rungCheck === "function" && typeof renderDutyLadder === "function", "the ladder exports (LADDER, dutyLadder, rungCheck, renderDutyLadder, ladderReport)");
if (cmd.ladderReport && LADDER && dutyLadder && rungCheck && renderDutyLadder) {
  expect(same(LADDER.bonusCaps.police, { patrol: 100, senior: 100, senior_patrol: 100, sergeant: 95, lieutenant: 70 }), "police bonus caps $100/$100/$100/$95/$70");
  expect(same(LADDER.bonusCaps.medic, { orderly: 40, emt: 50, paramedic: 50, doctor: 40 }), "medic bonus caps $40/$50/$50/$40");
  const fx = dutyLadder({
    police: { ranks: [{ id: "patrol", perHour: 350 }, { id: "senior", perHour: 500 }, { id: "sergeant", perHour: 450 }] },
    medic: { ranks: [{ id: "orderly", perHour: 450 }, { id: "emt", perHour: 500 }, { id: "doctor", perHour: 580 }, { id: "paramedic", perHour: 620 }] },
  });
  const flagged = (job, id) => fx.rows.find((r) => r.job === job && r.id === id)?.problems ?? ["missing"];
  expect(flagged("police", "patrol").length === 0, `police $350 + $100 is on the duty rung (got ${flagged("police", "patrol")})`);
  expect(flagged("police", "senior").some((p) => /above the entry rung/.test(p)), "a police rank at $500 is above the entry rung");
  expect(flagged("police", "sergeant").some((p) => /mid-tier floor/.test(p)), "a police sergeant at $450 + $95 is over the $520 mid-tier floor");
  expect(flagged("medic", "orderly").some((p) => /off the mid-tier rung/.test(p)), "a medic at $450 is off the mid-tier rung");
  expect(flagged("medic", "emt").length === 0, `a medic EMT at $500 + $50 is on the rung (got ${flagged("medic", "emt")})`);
  expect(flagged("medic", "doctor").some((p) => /over \$600/.test(p)), "a medic doctor at $580 + $40 is over $600 with the bonus");
  expect(flagged("medic", "paramedic").some((p) => /off the mid-tier rung/.test(p)), "a medic at $620 is off the mid-tier rung");
  expect(fx.flags.length === 5 && fx.flags.every((f) => f.kind === "duty-rung"), `one duty-rung flag per rank off its rung (got ${fx.flags.length})`);
  // A civilian loop: at or above the lowest duty shift (the Cadet's) is on the ladder; below it is flagged.
  expect(!rungCheck(350, { police: 350, medic: 480 }).flagged && rungCheck(349, { police: 350, medic: 480 }).flagged, "a loop at the Cadet shift passes, a dollar under is flagged");
  expect(rungCheck(0, { police: 350 }).flagged, "a loop paying nothing is flagged");

  // 4. The shipped report: every duty rank on its rung, and only stone on foot below the Cadet shift.
  const lad = cmd.ladderReport(rep);
  expect(lad.duty.flags.length === 0, `no duty rank off its rung (got ${lad.duty.flags.map((f) => f.text).join("; ")})`);
  const civ = [...(rep.lumber?.loops ?? []), ...(rep.wheat?.loops ?? []), ...(rep.mining?.loops ?? [])];
  const low = civ.filter((l) => l.flagged).map((l) => l.id);
  expect(civ.length > 5 && same(low, ["stone-foot"]), `only stone-foot is below its rung (got ${low.join(", ") || "none"} of ${civ.length})`);
  const lines = lad.lines.join("\n");
  for (const want of ["patrol $350/h shift, bonus cap $100, most $450/h, typical $400/h", "lieutenant $450/h shift, bonus cap $70, most $520/h, typical $485/h",
    "orderly $480/h shift, bonus cap $40, most $520/h, typical $500/h", "doctor $560/h shift, bonus cap $40, most $600/h, typical $580/h"])
    expect(lines.includes(want), `economy prints the duty row "${want}"`);
  expect(!/1\.5 to 2\.5|OUT OF BAND|in band/.test(lines), "no line states the old 1.5 to 2.5x band");
}

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-ladder-selftest: OK");
