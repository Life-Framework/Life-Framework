// The full-perks line of `tools\cli economy` (WK-W11_2, #4409): each chain's steady hourly with the passive trade perks
// next to the novice line. Two income perks make a pick pair (level 10 and level 20): `double_yield` (a chance of a
// double gather, LF_PerkEffects.DOUBLE_YIELD_CHANCE) and `good_word` (a share more at the trader, LF_PerkPrice
// .GOOD_WORD_PERCENT). Swift hands and light touch change the gather clock and the tool bill, which this model leaves
// at the novice figures, so the line is the cautious one. The shares are read from the Enforce Script by the selftest.
//
// A loop is flagged when its full-perks hourly drops under the floor of the rung it sits on, or passes the crime band
// of that rung (docs/roadmap/next-release-0.3.0.md "The economy table": crime sits on top, never under the civilian
// rung at the same stage, and a perked civilian loop must not pass it).

/** The shares, as the game holds them (economy-perks-selftest reads the .c constants and compares). */
export const PERKS = {
  yieldShare: 0.10, // LF_PerkEffects.DOUBLE_YIELD_CHANCE
  pricePercent: 10, // LF_PerkPrice.GOOD_WORD_PERCENT
};

/** The civilian rungs by the novice hourly ($/h) and the top of the crime band that sits above each. */
export const RUNGS = [
  { id: "entry", floor: 400, upTo: 480, crimeCeiling: 650 }, // moonshine, tier 1: $500 to $650
  { id: "mid", floor: 480, upTo: 600, crimeCeiling: 900 }, // chop shop, tier 1: $600 to $900
  { id: "high", floor: 600, upTo: 800, crimeCeiling: 1500 }, // runs, tills and safes: up to $1,500
  { id: "master", floor: 800, upTo: Infinity, crimeCeiling: 1500 },
];

const factor = () => (1 + PERKS.yieldShare) * (1 + PERKS.pricePercent / 100);

/** The Cadet shift: a loop under it is flagged by the report itself and sits on no rung here. */
export const CADET_SHIFT = 350;

/** The rung a novice hourly sits on: a loop from the Cadet shift up counts as entry until it reaches the mid rung. */
export function rungOf(novice) {
  if (!(novice >= CADET_SHIFT)) return null;
  return RUNGS.find((r) => novice < r.upTo) ?? RUNGS[RUNGS.length - 1];
}

/**
 * A chain's steady $/h with both perks. A mining loop carries gross and cost: the perks lift the gross, the reagent and
 * fuel bill stays. Other loops lift the steady figure.
 */
export function fullPerksHourly(loop) {
  if (typeof loop.grossSteady === "number" && typeof loop.costPerHour === "number") {
    return Math.round(loop.grossSteady * factor()) - loop.costPerHour;
  }
  return Math.round(loop.steady * factor());
}

const money = (n) => `$${n}`;

/**
 * Rows, lines and flags for a built report ({ lumber, wheat, mining } each with `loops`). A loop may carry
 * `fullOverride` (tests plant the result of a bad tuning).
 */
export function perksReport(report) {
  const rows = [], flags = [];
  for (const kind of ["lumber", "wheat", "mining"]) {
    for (const l of report?.[kind]?.loops ?? []) {
      const novice = l.steady;
      const full = typeof l.fullOverride === "number" ? l.fullOverride : fullPerksHourly(l);
      const rung = rungOf(novice);
      const problems = [];
      if (full < novice) problems.push(`pays ${money(full)}/h with the perks, under its novice ${money(novice)}/h`);
      if (rung && full < rung.floor) problems.push(`drops to ${money(full)}/h, under the ${rung.id} rung floor ${money(rung.floor)}`);
      const crime = rung && full > rung.crimeCeiling;
      rows.push({ kind, id: l.id, label: l.label, novice, first: l.first, full, rung: rung?.id ?? "none", problems, crime: !!crime, alreadyFlagged: !!l.flagged });
      // A loop the report already flags for its novice pay (stone on foot) gets its line, not a second flag.
      if (l.flagged) continue;
      if (problems.length) flags.push({ kind: "perks-rung", id: `perks:${kind}:${l.id}`, text: `${kind} chain ${l.label}: ${problems.join("; ")}` });
      if (crime) flags.push({ kind: "perks-crime", id: `perks:${kind}:${l.id}`, text: `${kind} chain ${l.label} pays ${money(full)}/h with full perks, past the ${money(rung.crimeCeiling)} crime band of the ${rung.id} rung` });
    }
  }
  const lines = [];
  if (rows.length) {
    lines.push(`full perks (double yield +${Math.round(PERKS.yieldShare * 100)}% units and good word +${PERKS.pricePercent}% at the trader, steady $/h; swift hands and light touch left out; flags a loop under its rung or past crime):`);
    for (const r of rows) {
      const tag = r.problems.length ? `  UNDER ITS RUNG: ${r.problems.join("; ")}` : "";
      const crime = r.crime ? `  PASSES CRIME (${money(RUNGS.find((x) => x.id === r.rung).crimeCeiling)} band)` : "";
      lines.push(`  PERKS ${r.label}: novice ${money(r.novice)}/h, full perks ${money(r.full)}/h (${r.rung} rung)${tag}${crime}`);
    }
  }
  return { rows, lines, flags };
}
