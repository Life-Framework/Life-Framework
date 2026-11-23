// Proves the duty section of `tools\cli economy` (#4423, slice of #4170; docs/roadmap/next-release-0.3.0.md "The economy
// table", police D6): each outcome bonus with its hourly cap, the duty-with-bonus line per police and medic rank, and every
// pack law-book fine as minutes of entry civilian work ($8 a minute). A police rank whose shift plus full bonus passes the
// $520/h mid-tier floor is flagged; a rank at or under it is not. A fixture law book and bonus table give exact minutes.
// inputs: tools/lib/economy-duty.mjs, tools/lib/economy-lumber.mjs, addons/LF_Everon1989/Configs/Crime/LF_CrimeCatalog_Everon1989.conf,
//         addons/LifeFramework/Scripts/Game/Feature/Police/LF_DutyBonus.c, tools/commands/economy.mjs
// red-proof: written first against an empty section (a stub that returns no lines, fines or flags); it printed
// 11 FAIL lines, among them "FAIL $75 at $480/h is 9.4 minutes (got 0)", "FAIL only the lieutenant flag (got none)" and
// "FAIL the shipped bonus table reads $15 ... (got [])"; the real section prints "economy-duty-selftest: OK".
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { dutySection, minutesOf, readBonusTable, readLawBook, FINE_RATE_PER_HOUR } from "../lib/economy-duty.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };

// 1. The fixture: $75 at $480/h is 9.4 minutes; $600 is 75 minutes; a $0 row is not a fine.
expect(FINE_RATE_PER_HOUR === 480, "entry civilian work is $480/h ($8 a minute, the scale of #4208)");
expect(minutesOf(75, 480) === 9.4, `$75 at $480/h is 9.4 minutes (got ${minutesOf(75, 480)})`);

const bonuses = [
  { kind: "CALL_RESOLVED", role: "police", amount: 15 },
  { kind: "BOOKING", role: "police", amount: 10 },
  { kind: "STOLEN_CAR", role: "police", amount: 20 },
  { kind: "REVIVE", role: "medic", amount: 10 },
  { kind: "PATIENT_DELIVERED", role: "medic", amount: 15 },
];
const lawBook = [
  { id: "#LF-Ticket_OffenceSpeedingMinor", fine: 75 },
  { id: "#LF-Ticket_OffenceHitAndRun", fine: 600 },
  { id: "#LF-Ticket_OffenceNoFine", fine: 0 },
];
const duty = {
  police: { ranks: [{ id: "patrol", perHour: 350 }, { id: "lieutenant", perHour: 460 }] },
  medic: { ranks: [{ id: "orderly", perHour: 480 }] },
};
const s = dutySection({ duty, bonuses, lawBook });
const lines = (s.lines ?? []).join("\n");

expect(s.fines?.length === 2, `a $0 row is not a fine: two fines (got ${s.fines?.length})`);
expect(s.fines?.[0]?.minutes === 9.4 && s.fines?.[1]?.minutes === 75, `fine minutes are 9.4 and 75 (got ${s.fines?.map((f) => f.minutes).join(", ")})`);
expect(lines.includes("SpeedingMinor $75 = 9.4 min") && lines.includes("HitAndRun $600 = 75 min"), "the duty section prints each fine as minutes");

// 2. The bonus rows and the flag: lieutenant $460 + $70 = $530 passes $520; patrol $350 + $100 = $450 and orderly $480 + $40 = $520 do not.
expect(s.bonuses?.length === 5 && s.bonuses.find((b) => b.kind === "CALL_RESOLVED")?.amount === 15, "five outcome bonuses, call resolved $15");
expect(lines.includes("call resolved $15"), "the duty section prints the call-resolved bonus");
const flagged = (s.flags ?? []).map((f) => f.id);
expect(flagged.length === 1 && flagged[0] === "duty-bonus:police:lieutenant", `only the lieutenant flag (got ${flagged.join(", ") || "none"})`);
expect(lines.includes("police lieutenant $460/h shift, bonus cap $70, most $530/h"), "the lieutenant duty line prints with its full bonus");
expect(lines.includes("OVER THE $520 MID-TIER FLOOR"), "the lieutenant line says it is over the mid-tier floor");
expect(!lines.includes("police patrol $350/h shift, bonus cap $100, most $450/h  OVER"), "the patrol line is not flagged");

// 3. The real configs read: the shipped bonus amounts and the pack law book (every fine a positive minute count).
const real = readBonusTable(root);
expect(real.some((b) => b.kind === "CALL_RESOLVED" && b.amount === 15) && real.some((b) => b.kind === "PATIENT_DELIVERED" && b.amount === 15), `the shipped bonus table reads $15 for call resolved and patient delivered (got ${JSON.stringify(real)})`);
const law = readLawBook(root);
expect(law.length > 20 && law.every((f) => f.fine > 0 && minutesOf(f.fine, FINE_RATE_PER_HOUR) > 0), `the pack law book reads every fine as minutes (got ${law.length} rows)`);

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-duty-selftest: OK");
