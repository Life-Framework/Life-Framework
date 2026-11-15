// opening-run: host=outside
// Beat B6 of the opening-day script (#1602): the desk half of the 2-hour day (docs/playtest/m4-opening-day.md "B6").
//
//   node tools/playtest/opening/b6-day.mjs --host <scratch dir> [--config <file>] [--proofs <dir>]
//
//   --host     the host folder whose perf/ the check reads (required; LF_HOST_DIR is how `tools\cli opening` passes it).
//              Read only: this script writes nothing.
//   --config   the config.json to judge (default server/flagship/config.json, the committed live config).
//   --proofs   where proof verbs live (default agent-box/proofs); a verb is found by its `name:`, as `opening` does.
//
// One line per check, `BEAT B6 <check-id> PASS|FAIL|BLOCKED <reason>`, then `SUMMARY b6-day ok|blocked|fail ...`.
// Exit 1 only on a FAIL; BLOCKED exits 0; exit 2 for a bad call (no BEAT line then).
//
//   day-config    the config's `dayCycle` is accelerated with a 120-minute day (12x: one real minute is 12 game minutes).
//   perf-samples  the newest <host>/perf/perf-*.csv (`tools\cli host perf`, #1406) holds at least one sample row, and
//                 every row has a CPU, a memory and an upload number. No csv at all is BLOCKED (nothing has sampled this
//                 host yet, #1693); an empty or broken csv is FAIL. Under `tools\cli opening` the host is the beat's own
//                 folder (it prints it), so run `host perf` with LF_HOST_DIR set to that folder.
//   clock-rate    five real minutes move the clock one in-game hour. Only a game session can read the clock, so the
//   late-join     desk check is whether the `day-rate` proof verb (#1303) is on main: BLOCKED by #1301 and #1303 while it
//                 is not, then PASS naming the verb to run on the session (and B's rejoin, read by eye, for late-join).
// Proof: tools/validation/opening-b6-selftest.mjs.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { CSV_HEADER } from "../../host/perf.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const DAY_LENGTH_MINUTES = 120;
export const DAY_VERB = "day-rate";
const NEEDED_COLUMNS = ["cpu_pct", "ws_mb", "up_mbit"];

// { ok, reason } for a parsed config.json. Pure.
export function judgeDayConfig(config) {
  const d = config?.dayCycle;
  if (!d || typeof d !== "object") return { ok: false, reason: "config has no dayCycle section" };
  if (d.mode !== "accelerated") return { ok: false, reason: `dayCycle.mode is ${JSON.stringify(d.mode)}, not "accelerated"` };
  if (d.dayLengthMinutes !== DAY_LENGTH_MINUTES) return { ok: false, reason: `dayCycle.dayLengthMinutes is ${JSON.stringify(d.dayLengthMinutes)}, not ${DAY_LENGTH_MINUTES} (a ${DAY_LENGTH_MINUTES / 60} hour day)` };
  return { ok: true, reason: `dayCycle is accelerated, ${DAY_LENGTH_MINUTES}-minute day (12x)` };
}

// { ok, reason, rows } for the text of one perf CSV. Pure.
export function judgePerfCsv(text) {
  const lines = String(text).split(/\r?\n/).filter((l) => l.trim());
  if (!lines.length) return { ok: false, reason: "the perf CSV is empty", rows: 0 };
  const header = lines[0].split(",").map((s) => s.trim());
  const missing = NEEDED_COLUMNS.filter((c) => !header.includes(c));
  if (missing.length) return { ok: false, reason: `the perf CSV has no ${missing.join(", ")} column (expected "${CSV_HEADER}")`, rows: 0 };
  const rows = lines.slice(1);
  if (!rows.length) return { ok: false, reason: "the perf CSV has a header and no samples (did `host perf` see the server?)", rows: 0 };
  for (let i = 0; i < rows.length; i++) {
    const cells = rows[i].split(",").map((s) => s.trim());
    for (const c of NEEDED_COLUMNS) {
      const raw = cells[header.indexOf(c)];
      const n = raw === undefined || raw === "" ? NaN : Number(raw);
      if (!Number.isFinite(n) || n < 0) return { ok: false, reason: `sample ${i + 1} has no usable ${c} (${JSON.stringify(raw ?? null)})`, rows: rows.length };
    }
  }
  return { ok: true, reason: `${rows.length} samples with CPU, memory and upload`, rows: rows.length };
}

// The newest <host>/perf/perf-*.csv (the stamp sorts by time), or null.
export function newestPerf(host) {
  const dir = join(host, "perf");
  if (!existsSync(dir)) return null;
  const names = readdirSync(dir).filter((f) => /^perf-.*\.csv$/.test(f)).sort();
  return names.length ? join(dir, names[names.length - 1]) : null;
}

// Is a proof verb with this `name:` in the folder? Same rule as `tools\cli opening`.
export function hasVerb(dir, name) {
  if (!existsSync(dir)) return false;
  for (const f of readdirSync(dir)) {
    if (!f.endsWith(".mjs")) continue;
    const m = /\bname:\s*["']([^"']+)["']/.exec(readFileSync(join(dir, f), "utf8"));
    if (m && m[1] === name) return true;
  }
  return false;
}

export function runB6({ host, config, proofs }) {
  const results = [];
  const check = (id, status, detail = "") => {
    results.push({ id, status });
    console.log(`BEAT B6 ${id} ${status}${detail ? ` ${detail}` : ""}`);
  };

  // day-config
  let cfg = null;
  try {
    cfg = JSON.parse(readFileSync(config, "utf8"));
  } catch (e) {
    check("day-config", "FAIL", `cannot read ${config}: ${e.code ?? e.message}`);
  }
  if (cfg) {
    const d = judgeDayConfig(cfg);
    check("day-config", d.ok ? "PASS" : "FAIL", d.reason);
  }

  // perf-samples
  const file = newestPerf(host);
  if (!file) check("perf-samples", "BLOCKED", `no perf csv yet under ${join(host, "perf")}: run \`tools\\cli host perf\` for the length of the beat (a fresh host has sampled nothing)`);
  else {
    const p = judgePerfCsv(readFileSync(file, "utf8"));
    check("perf-samples", p.ok ? "PASS" : "FAIL", `${p.reason} (${file.split(/[\\/]/).pop()})`);
  }

  // clock-rate, late-join
  const verb = hasVerb(proofs, DAY_VERB);
  const line = `run \`node agent-box/harness.mjs proof ${DAY_VERB}\` on the game session (\`tools\\cli opening\` lists it)`;
  if (verb) {
    check("clock-rate", "PASS", `the ${DAY_VERB} proof verb is on main: ${line}`);
    check("late-join", "PASS", `the ${DAY_VERB} proof verb is on main: ${line}, then B leaves and rejoins and reads the same clock`);
  } else {
    check("clock-rate", "BLOCKED", `#1301 (the server applies the day) and #1303 (the ${DAY_VERB} proof verb) are not on main`);
    check("late-join", "BLOCKED", `#1301 and #1303 are not on main: the ${DAY_VERB} verb and a joiner's clock have nothing to read yet`);
  }

  const n = (s) => results.filter((r) => r.status === s).length;
  const verdict = n("FAIL") ? "fail" : n("BLOCKED") ? "blocked" : "ok";
  console.log(`SUMMARY b6-day ${verdict} pass=${n("PASS")} blocked=${n("BLOCKED")} fail=${n("FAIL")}`);
  return n("FAIL") ? 1 : 0;
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const args = process.argv.slice(2);
  const val = (name) => {
    const i = args.indexOf(name);
    return i === -1 || i + 1 >= args.length ? null : args[i + 1];
  };
  const host = val("--host") ?? process.env.LF_HOST_DIR;
  if (!host) {
    console.error("b6-day: --host <scratch dir> is required\nusage: node tools/playtest/opening/b6-day.mjs --host <scratch dir> [--config <file>] [--proofs <dir>]");
    process.exit(2);
  }
  process.exit(
    runB6({
      host: resolve(host),
      config: resolve(val("--config") ?? join(ROOT, "server", "flagship", "config.json")),
      proofs: resolve(val("--proofs") ?? join(ROOT, "agent-box", "proofs")),
    }),
  );
}
