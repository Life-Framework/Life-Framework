// Opening-day beat B9, wipe with notice (#1492; docs/playtest/m4-opening-day.md "B9").
//
//   node tools/playtest/opening/b9-wipe.mjs [--host <dir under tmp/>] [--keep]
//
// Runs the real `tools\cli host wipe` and `host announce --wipe` against a SCRATCH COPY of a host
// folder, never the real one: the source (default tools/validation/fixtures/opening-b9/host-b9)
// is copied into tmp/opening-b9/run-<stamp>/, `--host` must resolve inside <repo>/tmp/ and any
// other path is refused before anything runs. The announce step writes its next-session file inside
// the copy (LF_ANNOUNCE_SESSIONS), so website/content/sessions.json is never touched. The copy is
// removed at the end unless --keep.
//
// One `CHECK <id> PASS|BLOCKED|FAIL <detail>` line per step, then
// `SUMMARY b9-wipe ok|blocked|fail pass=N blocked=N fail=N`. BLOCKED always names the open issue:
// #1413 is the wipe notice gate (`host wipe` refuses without a notice 24 h old). While it is not
// on main, `host wipe` wipes a stopped host with no notice, so the script probes that on a second
// throwaway copy and marks the notice steps BLOCKED instead of wiping the main copy early.
// Exit 1 only on FAIL (or a refused --host); BLOCKED exits 0.
import { cpSync, existsSync, mkdirSync, readFileSync, readdirSync, realpathSync, writeFileSync } from "node:fs";
import { basename, dirname, join, relative, resolve, isAbsolute } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../../link-safety.mjs";
import { findSavePoints, hostPaths, writeHostState } from "../../host/host.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const FIXTURE = join(ROOT, "tools", "validation", "fixtures", "opening-b9", "host-b9");
export const GATE_ISSUE = "#1413";
export const WIPE_POLICY_URL = "https://life-framework.org/docs/community/wipe-policy";
// Fields a notice timestamp may live in inside the next-session row (`host announce` records the
// session time `at`; the #1413 gate reads when the notice was posted).
const NOTICE_FIELDS = ["announcedAt", "noticeAt", "postedAt"];

// Moves the notice timestamp back `hours` in a scratch sessions file; the session time `at` stays.
// Returns { aged, fields }. Only ever call it on a file inside the scratch copy.
export function ageNotice(file, hours) {
  const data = JSON.parse(readFileSync(file, "utf8"));
  const row = data?.next;
  const fields = [];
  if (row && typeof row === "object") {
    for (const f of NOTICE_FIELDS) {
      const ms = typeof row[f] === "string" ? Date.parse(row[f]) : NaN;
      if (!Number.isNaN(ms)) {
        row[f] = new Date(ms - hours * 3600000).toISOString();
        fields.push(f);
      }
    }
  }
  if (fields.length) writeFileSync(file, JSON.stringify(data, null, 2) + "\n");
  return { aged: fields.length > 0, fields };
}

// A --host must resolve (symlinks followed) to a folder strictly inside <repo>/tmp/.
export function checkScratchSource(host, root = ROOT) {
  const tmp = join(root, "tmp");
  let real;
  try {
    real = realpathSync(resolve(host));
  } catch {
    return `--host ${host} does not exist`;
  }
  const realTmp = existsSync(tmp) ? realpathSync(tmp) : tmp;
  const rel = relative(realTmp, real);
  if (!rel || rel.startsWith("..") || isAbsolute(rel)) return `--host ${host} is outside tmp/ (${realTmp}): B9 only runs on a scratch copy, never a real host folder`;
  return null;
}

const results = [];
const check = (id, status, detail = "") => {
  results.push({ id, status, detail });
  console.log(`CHECK ${id} ${status}${detail ? ` ${detail}` : ""}`);
};

function cli(args, hostDir, extraEnv = {}) {
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", ...args], {
    cwd: ROOT,
    encoding: "utf8",
    timeout: 120000,
    windowsHide: true,
    env: { ...process.env, LF_HOST_DIR: hostDir, ...extraEnv },
  });
  return { status: r.status, out: `${r.stdout ?? ""}${r.stderr ?? ""}` };
}

const saves = (dir) => findSavePoints(hostPaths(dir).profile).length;

export async function runB9({ source = FIXTURE, keep = false } = {}) {
  const stamp = new Date().toISOString().replace(/[-:T]/g, "").slice(0, 14) + "-" + process.pid;
  const runDir = join(ROOT, "tmp", "opening-b9", `run-${stamp}`);
  const name = basename(source);
  const main = join(runDir, name);
  const probe = join(runDir, "probe", name);
  const sessions = join(main, "sessions.json");
  const env = { LF_ANNOUNCE_SESSIONS: sessions };
  mkdirSync(runDir, { recursive: true });
  cpSync(source, main, { recursive: true });
  cpSync(source, probe, { recursive: true });
  try {
    // 1. The host runs (this process stands in for it): the wipe refuses and touches nothing.
    writeHostState(main, { pid: process.pid });
    const running = cli(["wipe", "--confirm", name], main, env);
    if (running.status !== 0 && /running/i.test(running.out) && saves(main) > 0) check("refuses-while-running", "PASS", "exit " + running.status + ", saves untouched");
    else check("refuses-while-running", "FAIL", `exit ${running.status}, saves=${saves(main)}: ${running.out.trim().slice(0, 300)}`);
    writeHostState(main, { pid: 0 });

    // 2. No notice, host stopped. The probe copy takes the hit if the gate is not on main.
    const probed = cli(["wipe", "--confirm", name], probe, { LF_ANNOUNCE_SESSIONS: join(probe, "sessions.json") });
    const gateAbsent = probed.status === 0 && saves(probe) === 0;
    if (gateAbsent) {
      check("refuses-without-notice", "BLOCKED", `${GATE_ISSUE}: host wipe has no notice gate on main; it wiped a stopped probe copy with no notice`);
    } else if (probed.status !== 0 && /24/.test(probed.out) && /notice/i.test(probed.out) && saves(probe) > 0) {
      check("refuses-without-notice", "PASS", "refused, saying a notice 24 hours old is needed");
    } else {
      check("refuses-without-notice", "FAIL", `exit ${probed.status}, probe saves=${saves(probe)}: ${probed.out.trim().slice(0, 300)}`);
    }

    // 3. Announce a wipe two days out. The default draft carries no wipe-policy link yet, so the
    // policy URL goes through --rules; the check still reads the draft text.
    const at = new Date(Date.now() + 49 * 3600000).toISOString();
    const ann = cli(["announce", "--wipe", "--at", at, "--rules", WIPE_POLICY_URL], main, env);
    let row = null;
    try {
      row = JSON.parse(readFileSync(sessions, "utf8")).next;
    } catch {}
    const okDraft = ann.status === 0 && ann.out.includes(WIPE_POLICY_URL) && /24 hours/.test(ann.out) && /<t:\d+:F>/.test(ann.out) && /WIPE/.test(ann.out);
    if (okDraft && row && row.wipe === true && row.announced === false) check("announce-draft", "PASS", "draft has the wipe time, 24 hours and the wipe-policy link; recorded in the scratch sessions.json");
    else check("announce-draft", "FAIL", `exit ${ann.status}, row=${JSON.stringify(row)}: ${ann.out.trim().slice(0, 300)}`);

    // 4. A notice posted just now is not 24 hours old: the wipe still refuses. Skipped when the
    // gate is absent, because the wipe would go through.
    if (gateAbsent) {
      check("refuses-fresh-notice", "BLOCKED", `${GATE_ISSUE}: no notice gate on main, so a fresh notice cannot be refused`);
    } else {
      const fresh = cli(["wipe", "--confirm", name], main, env);
      if (fresh.status !== 0 && /24/.test(fresh.out) && saves(main) > 0) check("refuses-fresh-notice", "PASS", "refused until the notice is 24 hours old");
      else check("refuses-fresh-notice", "FAIL", `exit ${fresh.status}, saves=${saves(main)}: ${fresh.out.trim().slice(0, 300)}`);
    }

    // 5. Age the notice in the scratch copy only. Nothing to age, or nothing reading it, while the gate is absent.
    if (gateAbsent) {
      check("aged-notice", "BLOCKED", `${GATE_ISSUE}: no notice gate on main, so there is no notice timestamp to age`);
    } else {
      const aged = ageNotice(sessions, 25);
      if (aged.aged) check("aged-notice", "PASS", `${aged.fields.join(", ")} moved back 25 h in the scratch sessions.json`);
      else check("aged-notice", "FAIL", `no notice timestamp in the scratch sessions.json (looked for ${NOTICE_FIELDS.join(", ")})`);
    }

    // 6. The wipe: backup tagged wipe, saves moved aside, a row in wipes.log. These steps exist
    // today (#1274), so they run with or without the notice gate.
    const before = saves(main);
    const wipe = cli(["wipe", "--confirm", name], main, env);
    const { backups } = hostPaths(main);
    const dirs = existsSync(backups) ? readdirSync(backups, { withFileTypes: true }).filter((e) => e.isDirectory()).map((e) => e.name) : [];
    const backupName = dirs.find((d) => /-wipe(-\d+)?$/.test(d));
    const asideName = dirs.find((d) => /-wiped-saves/.test(d));
    if (wipe.status !== 0 || !before) {
      const why = `exit ${wipe.status}, saves before=${before}: ${wipe.out.trim().slice(0, 300)}`;
      for (const id of ["wipe-backup", "wipe-moved-aside", "wipe-logged"]) check(id, "FAIL", why);
    } else {
      if (backupName && findSavePoints(join(backups, backupName)).length) check("wipe-backup", "PASS", backupName);
      else check("wipe-backup", "FAIL", `no wipe-tagged backup with a save point in ${dirs.join(", ")}`);
      if (asideName && findSavePoints(join(backups, asideName)).length && saves(main) === 0) check("wipe-moved-aside", "PASS", `${asideName}, nothing left under the profile`);
      else check("wipe-moved-aside", "FAIL", `aside=${asideName}, saves left under the profile=${saves(main)}`);
      const log = join(backups, "wipes.log");
      const rows = existsSync(log) ? readFileSync(log, "utf8").split("\n").filter(Boolean) : [];
      if (rows.length === 1 && backupName && rows[0].includes(backupName)) check("wipe-logged", "PASS", rows[0].slice(0, 160));
      else check("wipe-logged", "FAIL", `wipes.log rows: ${JSON.stringify(rows)}`);
    }
  } finally {
    if (!keep) safeRemoveTree(runDir);
    else console.log(`[b9] scratch copy kept at ${runDir}`);
  }
  const n = (s) => results.filter((r) => r.status === s).length;
  const verdict = n("FAIL") ? "fail" : n("BLOCKED") ? "blocked" : "ok";
  console.log(`SUMMARY b9-wipe ${verdict} pass=${n("PASS")} blocked=${n("BLOCKED")} fail=${n("FAIL")}`);
  return n("FAIL") ? 1 : 0;
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const args = process.argv.slice(2);
  const hi = args.indexOf("--host");
  let source = FIXTURE;
  if (hi >= 0) {
    const host = args[hi + 1];
    const err = host ? checkScratchSource(host) : "--host needs a folder";
    if (err) {
      console.error(`[b9] refused: ${err}`);
      process.exit(2);
    }
    source = resolve(host);
  }
  process.exit(await runB9({ source, keep: args.includes("--keep") }));
}
