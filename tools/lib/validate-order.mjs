// tools/lib/validate-order.mjs - which checks `tools\cli validate` starts first.
//
// The runner starts the longest checks first so one slow selftest never starts last. It knew how long a check took
// only from this worktree's last green run (tmp/validate-state.json), so a fresh worktree and every GitHub run
// (a fresh checkout; only the green fingerprints are cached) started in file-name order: world-sync-selftest and
// merge-drivers-selftest, 'w' and 'm', began last and ran alone while eleven workers sat idle (a full run took 486 s
// with 308 s of work per worker). tools/lib/validate-order-seed.json is the fallback: the measured seconds of the
// slow checks, refreshed with
//     node tools/lib/validate-order.mjs --write        (after a `validate --all` in this worktree)
// It only orders; a stale or missing seed changes nothing but the start order.
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
export const SEED_FILE = join(HERE, "validate-order-seed.json");
const MIN_SECS = 2; // faster checks do not move the order

export function loadSeed(file = SEED_FILE) {
  try {
    const seed = JSON.parse(readFileSync(file, "utf8"));
    return seed && typeof seed.secs === "object" && seed.secs ? seed.secs : {};
  } catch {
    return {};
  }
}

// Jobs sorted longest first: the time recorded in this worktree, else the seed, else 0 (stable for ties).
export function orderJobs(jobs, state, seed) {
  const secs = (j) => state?.checks?.[j.s]?.secs ?? seed?.[j.s] ?? 0;
  return [...jobs].sort((a, b) => secs(b) - secs(a));
}

// The seed text for a state file's check times: the slow checks, slowest first.
export function seedFrom(state) {
  const rows = Object.entries(state?.checks ?? {})
    .map(([name, e]) => [name, Number(e?.secs)])
    .filter(([, s]) => Number.isFinite(s) && s >= MIN_SECS)
    .sort((a, b) => b[1] - a[1]);
  const secs = {};
  for (const [name, s] of rows) secs[name] = Math.round(s * 10) / 10;
  return JSON.stringify({ note: "seconds of the slow validate checks; start order only (tools/lib/validate-order.mjs)", secs }, null, 1) + "\n";
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1] && process.argv.includes("--write")) {
  const stateFile = join(HERE, "..", "..", "tmp", "validate-state.json");
  if (!existsSync(stateFile)) {
    console.error(`no ${stateFile}: run 'tools\\cli validate --all' first`);
    process.exit(1);
  }
  writeFileSync(SEED_FILE, seedFrom(JSON.parse(readFileSync(stateFile, "utf8"))));
  console.log(`wrote ${SEED_FILE}`);
}
