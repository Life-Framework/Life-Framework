// tools\cli opening [--beats B1,B9] [--host <scratch dir>]   run the opening-day beat scripts and write the report (#1495)
//
// The agent-runnable half of the M4 integration run (docs/playtest/m4-opening-day.md, chapter #1006). Every
// tools/playtest/opening/*.mjs is one beat script: found by glob, so a new beat needs no list, run standalone with
// `node <script> [--host <dir>]`, and it prints one line per check
//   BEAT <Bn> <check-id> PASS|FAIL|BLOCKED <reason>
// (BLOCKED when the command or slice it needs is not on main; a script exits 1 only on a FAIL). Lines that are not BEAT
// lines are ignored. Files starting with `_` and `*-setup.mjs` are helpers (b7-restart-setup.mjs needs --profile), not beats,
// and are not run. A script that dies, times out, or exits non-zero without a FAIL line counts as a FAIL for its beat.
//
// The report is tmp/playtest/opening-<date>.md in the shape the sheet's "After the run" asks for: one line per beat.
//   PASS                  every check passed
//   BLOCKED by #N, #M     no failure, but a check needs a slice that is not on main (not a failure; exit stays 0)
//   FAIL <check>: reason  the run goes on; the maintainer files a card (`slice new 1006 --attach <epic>`), nothing is fixed here
//   NOT RUN               a beat in the sheet with no script: a game beat names its proof verb and gets the
//                         `node agent-box/harness.mjs proof <verb>` line to run on the two-client session; the others are walked by hand
// Exit 1 only when a beat has a FAIL. Output is one SUMMARY line plus the failing beats; the report has the rest.
//
//   --beats B1,B9    only these beats (scripts and sheet lines)
//   --host <dir>     scratch host folder, passed to every script as `--host <dir>` and as LF_HOST_DIR
//   --dir <dir>      beat scripts folder (default tools/playtest/opening)
//   --sheet <file>   the beat sheet (default docs/playtest/m4-opening-day.md)
//   --proofs <dir>   where proof verbs live (default agent-box/proofs); a verb is found by its `name:`
//   --out <file>     report file (default tmp/playtest/opening-<date>.md)
//   --date YYYY-MM-DD, --timeout <seconds per script, default 600>
// The proof is tools/validation/opening-run-selftest.mjs.
import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";

export const help = "opening [--beats B1,B9] [--host <scratch>]   run every tools/playtest/opening/*.mjs beat script, write tmp/playtest/opening-<date>.md (one line per beat: pass, fail, blocked by #N) and list the proof verbs to run for the game beats";

const val = (args, name) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
};

// text -> [{ beat, id, status, reason }]; anything that is not `BEAT <Bn> <id> PASS|FAIL|BLOCKED [reason]` is skipped
export function parseBeatLines(text) {
  const out = [];
  for (const raw of String(text).split(/\r?\n/)) {
    const m = /^\s*BEAT\s+(B\d+)\s+(\S+)\s+(PASS|FAIL|BLOCKED)\b\s*(.*?)\s*$/i.exec(raw);
    if (m) out.push({ beat: m[1].toUpperCase(), id: m[2], status: m[3].toUpperCase(), reason: m[4] });
  }
  return out;
}

// the #N slice numbers the BLOCKED checks name, in order of first mention
export function blockedBy(lines) {
  const seen = [];
  for (const l of lines) {
    if (l.status !== "BLOCKED") continue;
    for (const m of l.reason.matchAll(/#(\d+)/g)) {
      const n = Number(m[1]);
      if (!seen.includes(n)) seen.push(n);
    }
  }
  return seen;
}

// one beat's check lines -> { status, pass, fail, blocked }; a FAIL wins, then BLOCKED
export function aggregate(lines) {
  const count = (s) => lines.filter((l) => l.status === s).length;
  const [pass, fail, blocked] = [count("PASS"), count("FAIL"), count("BLOCKED")];
  const status = fail ? "FAIL" : blocked ? "BLOCKED" : "PASS";
  return { status, pass, fail, blocked, total: lines.length };
}

const beatNum = (b) => Number(b.slice(1));

// the beat sheet -> [{ beat, title, verbs }] in sheet order; verbs are the proof verbs the beat names
export function sheetBeats(text) {
  const out = [];
  let cur = null;
  for (const raw of String(text).split(/\r?\n/)) {
    if (/^## /.test(raw)) {
      const m = /^## (B\d+)\s+(.*)$/.exec(raw);
      cur = m ? { beat: m[1], title: cleanTitle(m[2]), verbs: [], body: [] } : null;
      if (cur) out.push(cur);
    } else if (cur) cur.body.push(raw);
  }
  for (const b of out) {
    const body = b.body.join("\n");
    for (const m of body.matchAll(/`([a-z][a-z0-9]*(?:-[a-z0-9]+)+)`\s+proof/g)) if (!b.verbs.includes(m[1])) b.verbs.push(m[1]);
    for (const m of body.matchAll(/proof(?: verb)?\s+`([a-z][a-z0-9-]*)`/g)) if (!b.verbs.includes(m[1])) b.verbs.push(m[1]);
    delete b.body;
  }
  return out;
}

// "[restart] The clean restart and the reload (7 min)" -> "The clean restart and the reload"
function cleanTitle(s) {
  return s.replace(/\[[^\]]*\]/g, "").replace(/\(\d+ min[^)]*\)\s*$/, "").replace(/\s+/g, " ").trim();
}

function proofVerbs(dir) {
  const verbs = new Set();
  if (!existsSync(dir)) return verbs;
  for (const f of readdirSync(dir)) {
    if (!f.endsWith(".mjs")) continue;
    const m = /\bname:\s*["']([^"']+)["']/.exec(readFileSync(join(dir, f), "utf8"));
    if (m) verbs.add(m[1]);
  }
  return verbs;
}

function beatScripts(dir) {
  if (!existsSync(dir)) return [];
  return readdirSync(dir)
    .filter((f) => f.endsWith(".mjs") && !f.startsWith("_") && !f.endsWith("-setup.mjs"))
    .sort();
}

function runScript(root, dir, file, host, timeoutSec) {
  const args = [join(dir, file)];
  if (host) args.push("--host", host);
  const env = host ? { ...process.env, LF_HOST_DIR: host } : process.env;
  const r = spawnSync(process.execPath, args, { cwd: root, env, encoding: "utf8", timeout: timeoutSec * 1000, windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  const lines = parseBeatLines(r.stdout || "");
  const fallbackBeat = (/^b(\d+)/i.exec(file) || [])[1];
  const beat = fallbackBeat ? `B${fallbackBeat}` : file.replace(/\.mjs$/, "");
  if (r.error) lines.push({ beat: lines[0]?.beat || beat, id: "run", status: "FAIL", reason: `${file} did not finish: ${r.error.code === "ETIMEDOUT" ? `timed out after ${timeoutSec}s` : r.error.message}` });
  else if (r.status !== 0 && !lines.some((l) => l.status === "FAIL")) {
    const tail = String(r.stderr || "").trim().split(/\r?\n/).pop() || "";
    lines.push({ beat: lines[0]?.beat || beat, id: "run", status: "FAIL", reason: `${file} exited ${r.status}${tail ? `: ${tail}` : ""}` });
  } else if (!lines.length) lines.push({ beat, id: "run", status: "FAIL", reason: `${file} printed no BEAT line` });
  return lines;
}

const isoDay = () => {
  const d = new Date();
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
};

export async function main(args, ctx) {
  const root = ctx.root;
  const pick = (name, rel) => resolve(root, val(args, name) || rel);
  const dir = pick("--dir", "tools/playtest/opening");
  const sheetFile = pick("--sheet", "docs/playtest/m4-opening-day.md");
  const proofsDir = pick("--proofs", "agent-box/proofs");
  const date = val(args, "--date") || isoDay();
  const outFile = pick("--out", join("tmp", "playtest", `opening-${date}.md`));
  const host = val(args, "--host");
  const timeoutSec = Number(val(args, "--timeout")) || 600;
  const only = val(args, "--beats") ? new Set(val(args, "--beats").split(",").map((s) => s.trim().toUpperCase()).filter(Boolean)) : null;
  const wanted = (b) => !only || only.has(b);

  // run the scripts
  const byBeat = new Map();
  for (const file of beatScripts(dir)) {
    const lines = runScript(root, dir, file, host, timeoutSec);
    for (const l of lines) {
      if (!wanted(l.beat)) continue;
      if (!byBeat.has(l.beat)) byBeat.set(l.beat, []);
      byBeat.get(l.beat).push(l);
    }
  }

  // the beats: the sheet's, plus any a script reports that the sheet lacks
  const sheet = existsSync(sheetFile) ? sheetBeats(readFileSync(sheetFile, "utf8")) : [];
  const beats = new Map(sheet.filter((b) => wanted(b.beat)).map((b) => [b.beat, b]));
  for (const b of byBeat.keys()) if (!beats.has(b)) beats.set(b, { beat: b, title: "", verbs: [] });
  const onMain = proofVerbs(proofsDir);

  const rows = [];
  const detail = (b, extra) => `- ${b.beat}${b.title ? ` ${b.title}` : ""}: ${extra}`;
  const proofLines = (b) => b.verbs.map((v) => `  - node agent-box/harness.mjs proof ${v}${onMain.has(v) ? "" : " (proof verb not on main yet)"}`);
  const tally = { pass: 0, fail: 0, blocked: 0, notrun: 0 };
  const failing = [];
  for (const b of [...beats.values()].sort((x, y) => beatNum(x.beat) - beatNum(y.beat))) {
    const lines = byBeat.get(b.beat);
    if (!lines) {
      tally.notrun++;
      if (b.verbs.length) rows.push(detail(b, "NOT RUN, game beat: run these on the two-client session"), ...proofLines(b));
      else rows.push(detail(b, "NOT RUN, no script and no proof verb: walk it by hand from the sheet"));
      continue;
    }
    const a = aggregate(lines);
    let row;
    if (a.status === "PASS") {
      tally.pass++;
      row = detail(b, `PASS (${a.total} check${a.total === 1 ? "" : "s"})`);
    } else if (a.status === "BLOCKED") {
      tally.blocked++;
      const nums = blockedBy(lines);
      const why = nums.length ? `by ${nums.map((n) => `#${n}`).join(", ")}` : `: ${lines.find((l) => l.status === "BLOCKED").reason}`;
      row = detail(b, `BLOCKED ${why} (${a.pass} of ${a.total} checks passed, ${a.blocked} blocked)`);
    } else {
      tally.fail++;
      const f = lines.filter((l) => l.status === "FAIL").map((l) => `${l.id}: ${l.reason}`).join("; ");
      row = detail(b, `FAIL ${f}; new card: #`);
      failing.push(row);
    }
    rows.push(row);
    if (b.verbs.length) rows.push(`  - game proof${b.verbs.length > 1 ? "s" : ""} for this beat, on the session:`, ...proofLines(b).map((l) => `  ${l}`));
  }

  const total = rows.filter((r) => r.startsWith("- ")).length;
  const report = [
    `# Opening-day run ${date}`,
    "",
    `Beat sheet: ${sheetFile.startsWith(root) ? sheetFile.slice(root.length + 1).replace(/\\/g, "/") : sheetFile}. Host folder: ${host || "none given"}. ` +
      `${tally.pass} pass, ${tally.fail} fail, ${tally.blocked} blocked, ${tally.notrun} not run.`,
    "",
    ...rows,
    "",
    "A FAIL gets a card: `tools\\cli slice new 1006 --plan <file>`; nothing is fixed inside the run. A beat blocked by a slice is",
    "not a failure. Fill the run's row in docs/playtest/session-playbook.md.",
    "",
  ].join("\n");
  mkdirSync(dirname(outFile), { recursive: true });
  writeFileSync(outFile, report);

  for (const f of failing) console.log(f);
  const ok = tally.fail === 0;
  console.log(`SUMMARY opening ${ok ? "ok" : "fail"} beats=${total} pass=${tally.pass} fail=${tally.fail} blocked=${tally.blocked} notrun=${tally.notrun} report=${outFile}`);
  return ok ? 0 : 1;
}
