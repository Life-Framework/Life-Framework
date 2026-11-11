// The perf budget config must match its class (#335): every [Attribute] field of LF_PerfBudget
// has a value in Configs/Perf/LF_PerfBudget.conf and the conf has no unknown field, the player
// count is at least 128, and every limit is a positive number. A budget nobody can read is not a
// gate. LF_Test_PerfBudget does the measuring; this keeps the thresholds honest without a boot.
//
// red-proof: 2026-11-11, deleting `m_iMaxCrimeEventUs` from the conf printed
// `FAIL conf has no value for m_iMaxCrimeEventUs` and exited 1; restored, it prints OK.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const cls = readFileSync(join(root, "addons/LifeFramework/Scripts/Game/Core/LF_PerfBudget.c"), "utf8");
const conf = readFileSync(join(root, "addons/LifeFramework/Configs/Perf/LF_PerfBudget.conf"), "utf8");

const fields = [...cls.matchAll(/\[Attribute\("(-?\d+)"[^\n]*\]\s*\n\s*int (m_i\w+);/g)].map((m) => ({ name: m[2], def: Number(m[1]) }));
const values = new Map([...conf.matchAll(/^\s*(m_i\w+)\s+(-?\d+)\s*$/gm)].map((m) => [m[1], Number(m[2])]));
const bad = [];
if (fields.length < 8) bad.push(`class lists only ${fields.length} budget fields; the parser or the class is broken`);
for (const f of fields) {
  if (!values.has(f.name)) bad.push(`conf has no value for ${f.name}`);
  else if (values.get(f.name) <= 0) bad.push(`${f.name} must be positive (got ${values.get(f.name)})`);
}
for (const name of values.keys()) {
  if (!fields.some((f) => f.name === name)) bad.push(`conf sets ${name}, which the class does not declare`);
}
if ((values.get("m_iPlayers") ?? 0) < 128) bad.push("m_iPlayers must be at least 128 (decision 2026-11-08)");

if (bad.length) {
  for (const b of bad) console.log(`FAIL ${b}`);
  process.exit(1);
}
console.log(`validate-perf-budget: OK (${fields.length} budget fields, players ${values.get("m_iPlayers")})`);
