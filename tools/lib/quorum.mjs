// The quorum rule for `promote --confirm` (docs/playtest/quorum.md, #575).
//
// A feature whose PR lists manual steps is `verified` when enough trusted testers confirmed it in a
// session. Confirmations come from a JSON file the session coordinator writes:
//   { "sha": "<promoted commit>", "results": [ { "issue": 12, "tester": "name", "result": "pass" | "fail" } ] }
// Only people on the roster (docs/playtest/testers.json) count. The rule is off until
// docs/playtest/quorum.json says "enabled": true, which is the maintainer's call.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const QUORUM_FILE = "docs/playtest/quorum.json";

const readJson = (file) => JSON.parse(readFileSync(file, "utf8"));

// The rule and the roster for a checkout. Missing quorum.json means the rule is off.
export function loadQuorum(root) {
  const file = join(root, QUORUM_FILE);
  if (!existsSync(file)) return { enabled: false, size: 0, roster: [] };
  const q = readJson(file);
  const size = Number(q.size);
  if (!Number.isInteger(size) || size < 1) throw new Error(`${QUORUM_FILE}: "size" must be a whole number of at least 1`);
  const rosterPath = join(root, q.roster || "docs/playtest/testers.json");
  const roster = existsSync(rosterPath) ? (readJson(rosterPath).testers ?? []) : [];
  return { enabled: q.enabled === true, size, roster };
}

const norm = (s) => String(s ?? "").trim().toLowerCase();

// Pure: which of `issues` have enough confirmations, which are failed, and which fall short.
// A roster tester counts once per issue; a later result from the same tester replaces an earlier one.
export function evaluateQuorum({ issues, results, roster, size }) {
  const trusted = new Set(roster.map((t) => norm(t.id)));
  const latest = new Map(); // "issue|tester" -> result
  for (const r of results ?? []) {
    const t = norm(r.tester);
    if (!trusted.has(t)) continue;
    latest.set(`${Number(r.issue)}|${t}`, norm(r.result));
  }
  const met = [], short = [], failed = [];
  for (const n of issues) {
    let pass = 0, fail = 0;
    for (const [key, result] of latest) {
      if (!key.startsWith(`${n}|`)) continue;
      if (result === "pass") pass++;
      else if (result === "fail") fail++;
    }
    if (fail > 0) failed.push({ issue: n, pass, fail });
    else if (pass >= size) met.push({ issue: n, pass });
    else short.push({ issue: n, pass, need: size });
  }
  return { met, short, failed, ok: short.length === 0 && failed.length === 0 };
}

export function readConfirmations(file) {
  if (!file || !existsSync(file)) throw new Error(`confirmations file not found: ${file ?? "(none given)"}`);
  const c = readJson(file);
  if (!Array.isArray(c.results)) throw new Error(`${file}: expected { "results": [ ... ] }`);
  return c;
}
