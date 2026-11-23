// The duty section of `tools\cli economy` (#4423, slice of #4170; plan police D6, docs/roadmap/next-release-0.3.0.md
// "The economy table"). Three things, all in work-minutes so a fine reads against the wage it competes with:
//   1. each outcome bonus (LF_DutyBonus.c, #4210) and the hourly cap its rank pays under (LADDER.bonusCaps);
//   2. the duty-with-bonus line per police and medic rank: shift + full bonus cap, flagged when it passes the rung's
//      ceiling ($520/h for police, $600/h for medic; LADDER);
//   3. every pack law-book fine (LF_CrimeCatalog_Everon1989.conf, #4208) as minutes of entry civilian work at
//      FINE_RATE_PER_HOUR, the $8 a minute #4208 priced the book at.
// Police stay the lowest paid; this section is where that shows up beside the fines.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { parseConf } from "../economy/profit-loops.mjs";
import { LADDER } from "./economy-lumber.mjs";

/** Entry civilian work in $ an hour: the top of the entry rung, $8 a minute (the scale #4208 scaled the law book to). */
export const FINE_RATE_PER_HOUR = LADDER.entry.max;

/** Which duty role each outcome bonus belongs to (the enum in LF_DutyBonus.c). Unknown kinds print as "?". */
const BONUS_ROLE = { CALL_RESOLVED: "police", BOOKING: "police", STOLEN_CAR: "police", REVIVE: "medic", PATIENT_DELIVERED: "medic" };
const BONUS_LABEL = {
  CALL_RESOLVED: "call resolved", BOOKING: "suspect booked", STOLEN_CAR: "stolen car recovered",
  REVIVE: "player revived", PATIENT_DELIVERED: "patient delivered",
};

const money = (n) => `$${n}`;

/** Work-minutes that `amount` takes at `perHour`, to one decimal (0 when there is no rate). */
export function minutesOf(amount, perHour) {
  if (!(perHour > 0)) return 0;
  return Math.round((amount * 60 / perHour) * 10) / 10;
}

/** The outcome bonus amounts from LF_DutyBonus.c: [{ kind, role, amount }], [] without the script. */
export function readBonusTable(root) {
  const file = join(root, "addons", "LifeFramework", "Scripts", "Game", "Feature", "Police", "LF_DutyBonus.c");
  if (!existsSync(file)) return [];
  return [...readFileSync(file, "utf8").matchAll(/static const int (\w+)_AMOUNT = (\d+);/g)]
    .map((m) => ({ kind: m[1], role: BONUS_ROLE[m[1]] ?? "?", amount: Number(m[2]) }));
}

/** The pack law book from LF_CrimeCatalog_Everon1989.conf: [{ id, fine }] for every row with a fine, [] without it. */
export function readLawBook(root) {
  const file = join(root, "addons", "LF_Everon1989", "Configs", "Crime", "LF_CrimeCatalog_Everon1989.conf");
  if (!existsSync(file)) return [];
  const tree = parseConf(readFileSync(file, "utf8"));
  return (tree.props.m_aCrimes?.list ?? [])
    .map((e) => ({ id: String(e.props.m_sCrimeId ?? ""), fine: Number(e.props.m_iFine ?? 0) }))
    .filter((f) => f.fine > 0);
}

/**
 * The section from resolved inputs (pure: the selftest feeds it a fixture). `duty` is the report's
 * { police: { ranks: [{ id, perHour }] }, medic: ... }; `bonuses` and `lawBook` as readBonusTable/readLawBook return them.
 * Returns { bonuses, ranks, fines, flags, lines }; one "duty-bonus" flag per rank whose shift plus full cap is over its ceiling.
 */
export function dutySection({ duty, bonuses = [], lawBook = [], caps = LADDER.bonusCaps }) {
  const flags = [];
  const ranks = [];
  for (const job of ["police", "medic"]) {
    const ceiling = job === "police" ? LADDER.policeCeiling : LADDER.medicCeiling;
    for (const r of duty?.[job]?.ranks ?? []) {
      const cap = caps[job]?.[r.id] ?? 0;
      const most = r.perHour + cap;
      const over = most > ceiling;
      ranks.push({ job, id: r.id, shift: r.perHour, cap, most, over, ceiling });
      if (over) flags.push({ kind: "duty-bonus", id: `duty-bonus:${job}:${r.id}`, text: `${job} ${r.id}: ${money(most)}/h with its full bonus is over the ${money(ceiling)} rung` });
    }
  }
  const fines = lawBook
    .filter((f) => f.fine > 0)
    .map((f) => ({ id: f.id, label: f.id.replace(/^#LF-(Ticket_Offence|Offence_)/, ""), fine: f.fine, minutes: minutesOf(f.fine, FINE_RATE_PER_HOUR) }));

  const lines = [];
  lines.push("duty bonuses (paid with the shift at clock-out, capped per rank each hour):");
  for (const b of bonuses) lines.push(`  ${b.role.toUpperCase().padEnd(6)} ${BONUS_LABEL[b.kind] ?? b.kind} ${money(b.amount)}`);
  for (const x of ranks) {
    lines.push(`  ${x.job} ${x.id} ${money(x.shift)}/h shift, bonus cap ${money(x.cap)}, most ${money(x.most)}/h${x.over ? `  OVER THE ${money(x.ceiling)} MID-TIER FLOOR` : ""}`);
  }
  if (fines.length) {
    lines.push(`law-book fines as minutes of entry civilian work (at ${money(FINE_RATE_PER_HOUR)}/h, $8 a minute):`);
    for (const f of fines) lines.push(`  ${f.label} ${money(f.fine)} = ${f.minutes} min`);
  }
  return { bonuses, ranks, fines, flags, lines };
}

/** The duty section of a built report, read from the root's configs (what `tools\cli economy` prints). */
export function dutyReport(report, root) {
  return dutySection({ duty: report.duty, bonuses: readBonusTable(root), lawBook: readLawBook(root) });
}
