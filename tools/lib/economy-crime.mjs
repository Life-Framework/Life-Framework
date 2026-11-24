// The crime lines of `tools\cli economy` (X-2, #4715; docs/roadmap/next-release-0.3.0.md "The economy table", crime).
// Each line's expected clean $/h is its gross after the pack's wash cut, less the share of runs lost and the inputs:
//   clean = gross x (1 - failShare) x (1 - cutPct / 100) - cost
// A line must sit inside its plan band, and at or above the floor of the civilian rung it sits above (RUNGS in
// economy-perks.mjs). A line under that floor is flagged `crime-rung:<id>`. A line with no input prints as missing.
//
// Pack-read: the wash cut (LF_Launder_Everon1989.conf, m_iCutPct) and the moonshine buyer price (LF_BuyerPrices_Everon1989.conf).
// Assumed, not yet in the pack: every other line's gross, cost and fail share (PLANNED_INPUTS). They print tagged "(assumed)"
// until a pack config carries them.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { RUNGS } from "./economy-perks.mjs";

export const LAUNDER_CONF = "addons/LF_Everon1989/Configs/Crime/LF_Launder_Everon1989.conf";
export const BUYER_CONF = "addons/LF_Everon1989/Configs/Crime/LF_BuyerPrices_Everon1989.conf";
export const MOONSHINE_PREFAB = "{5280F09CEB005042}";
/** Moonshine units a crew hour makes (assumed until the still's output is a config value). */
export const MOONSHINE_UNITS_PER_HOUR = 12;

/** The plan's crime table, one row each: its band ($/h, clean) and the civilian rung it sits above. */
export const CRIME_LINES = [
  { id: "moonshine", label: "Moonshine", tier: 1, band: [500, 650], rung: "entry" },
  { id: "chop-shop", label: "Chop shop (pays clean)", tier: 1, band: [600, 900], rung: "mid" },
  { id: "run", label: "Cigarette and vodka run", tier: 2, band: [750, 950], rung: "mid" },
  { id: "till", label: "Till and store safe", tier: "rungs 1, 2", band: [700, 1000], rung: "mid" },
  { id: "crew", label: "Cash van, pharmacy, bank, as a crew evening", tier: "rungs 3 to 5", band: [1000, 1500], rung: "master" },
  { id: "gang", label: "Gang member across a night", tier: "all", band: [900, 1500], rung: "high" },
];

/** Planning inputs for the lines the pack does not carry yet: gross before the wash, $/h; cost, $/h; failShare, 0 to 1. */
export const PLANNED_INPUTS = {
  "chop-shop": { gross: 1300, failShare: 0.1, cost: 150 },
  run: { gross: 1900, failShare: 0.15, cost: 250 },
  till: { gross: 1900, failShare: 0.2, cost: 200 },
  crew: { gross: 3000, failShare: 0.25, cost: 400 },
  gang: { gross: 2800, failShare: 0.3, cost: 300 },
};

const money = (n) => `$${n}`;

/** The expected clean $/h of one line after the wash, the lost runs and the inputs, rounded to the dollar. */
export function cleanPerHour(input, cutPct) {
  return Math.round(input.gross * (1 - input.failShare) * (1 - cutPct / 100) - input.cost);
}

/** The pack's wash cut and moonshine buyer price, read from the configs; null where a file or row is missing. */
export function readCrimeReadings(root) {
  const readText = (rel) => {
    const path = join(root, rel);
    return existsSync(path) ? readFileSync(path, "utf8") : "";
  };
  const cut = readText(LAUNDER_CONF).match(/m_iCutPct\s+(\d+)/);
  const escaped = MOONSHINE_PREFAB.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const price = readText(BUYER_CONF).match(new RegExp(`${escaped}[\\s\\S]*?m_iPrice\\s+(\\d+)`));
  return {
    cutPct: cut ? Number(cut[1]) : null,
    moonshinePrice: price ? Number(price[1]) : null,
  };
}

/** The inputs of the planned run: the moonshine line from the pack's buyer price, the others from PLANNED_INPUTS. */
export function plannedInputs(readings) {
  const moonshine = readings.moonshinePrice == null
    ? undefined
    : { gross: readings.moonshinePrice * MOONSHINE_UNITS_PER_HOUR, failShare: 0.05, cost: 60 };
  return { ...PLANNED_INPUTS, ...(moonshine ? { moonshine } : {}) };
}

/**
 * Rows, lines and flags for the crime table. `inputs` maps a line id to { gross, failShare, cost }; a line with no input
 * has clean null and no flag. `assumed` names the lines whose input is a planning figure (printed as "(assumed)").
 */
export function crimeSection({ cutPct, inputs = {}, assumed = new Set(Object.keys(PLANNED_INPUTS)) }) {
  const rows = [], flags = [];
  for (const line of CRIME_LINES) {
    const input = inputs[line.id];
    const rung = RUNGS.find((r) => r.id === line.rung);
    if (!input) {
      rows.push({ id: line.id, label: line.label, clean: null, band: line.band, rung: line.rung, inBand: false, underRung: false });
      continue;
    }
    const clean = cleanPerHour(input, cutPct);
    const inBand = clean >= line.band[0] && clean <= line.band[1];
    const underRung = clean < rung.floor;
    rows.push({ id: line.id, label: line.label, clean, band: line.band, rung: line.rung, inBand, underRung, assumed: assumed.has(line.id) });
    if (underRung) flags.push({ kind: "crime-rung", id: `crime-rung:${line.id}`, text: `${line.label} clears ${money(clean)}/h, under the ${line.rung} rung floor ${money(rung.floor)}` });
  }
  const lines = [];
  if (rows.length) {
    lines.push(`crime (expected clean $/h after the ${cutPct}% wash, lost runs and inputs; flags a line under its civilian rung):`);
    for (const r of rows) {
      const tag = r.clean == null ? "  NO INPUT" : `  ${money(r.clean)}/h`;
      const band = `band ${money(r.band[0])} to ${money(r.band[1])}`;
      const where = r.clean == null ? "" : r.inBand ? `, in band` : `, OUT OF BAND`;
      const under = r.underRung ? `  UNDER ITS RUNG (${r.rung})` : "";
      const assumed = r.assumed ? " (assumed)" : "";
      lines.push(`  CRIME ${r.label}:${tag}${assumed}, ${band}${where}, above ${r.rung} rung${under}`);
    }
  }
  return { rows, lines, flags };
}
