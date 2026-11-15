// opening-run: host=log
// tools/playtest/opening/b11-after.mjs - opening-day beat B11 "After: the log and the session record"
// (docs/playtest/m4-opening-day.md). Runs standalone:
//   node tools/playtest/opening/b11-after.mjs [--log <console.log>] [--triage <table>] [--root <repo>]
// Prints one line per check, `BEAT B11 <check-id> PASS|FAIL|BLOCKED <reason>`, and exits 1 only on a FAIL.
// BLOCKED means the command the check needs is not on main yet (or the host has no log to read); it is never
// a pass and never a failure. Default log: the newest console.log of the host profile.
//
// Checks:
//   triage-groups   `host triage` groups the log into known and new lines (#1269)
//   triage-drafts   `--draft` writes one draft and prints one issue command per new group, files nothing (#1270)
//   doctor-mods     `server-doctor` lists every mod the log loaded (#1271)
//   doctor-boot     `server-doctor` says whether the server booted, as the log shows (#1272)
//   doctor-config   `server-doctor` shows the effective config and the fields that fell back to a default (#1349)
//   history-session `host history` lists the session with its start, stop and restart (#1409)
// The doctor and history checks read the command's text with loose patterns; tighten them to the real output
// once those commands are on main.

import { existsSync, mkdtempSync, readdirSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const BEAT = "B11";
const SKIP_DIRS = new Set(["ArmaReforger", "core", "logs", "profile"]);

export function formatLine(r) {
  return `BEAT ${BEAT} ${r.id} ${r.status} ${String(r.reason).replace(/\s+/g, " ").trim()}`;
}

function cliRunner(root) {
  return (args) => {
    const r = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), ...args], { cwd: root, encoding: "utf8", timeout: 120000, windowsHide: true });
    return { status: r.status ?? 1, out: `${r.stdout ?? ""}${r.stderr ?? ""}` };
  };
}

const res = (id, status, reason) => ({ id, status, reason });

function triageChecks(run, log, triage, outDir, injected) {
  const args = ["host", "triage", log, ...(triage ? ["--triage", triage] : []), "--draft", "--out", outDir];
  const r = run(args);
  const sum = /SUMMARY host-triage ok groups=(\d+) known=(\d+) new=(\d+)(?: drafts=(\d+))?/.exec(r.out);
  if (r.status !== 0 || !sum) {
    const why = `host triage exited ${r.status}${sum ? "" : " without a SUMMARY line"}: ${r.out.trim().split(/\r?\n/).pop()}`;
    return [res("triage-groups", "FAIL", why), res("triage-drafts", "FAIL", "no triage run to read drafts from")];
  }
  const [groups, known, fresh] = [1, 2, 3].map((i) => Number(sum[i]));
  const drafts = Number(sum[4] ?? 0);
  const out = [];
  if (known + fresh !== groups) out.push(res("triage-groups", "FAIL", `groups=${groups} but known=${known} new=${fresh}`));
  else out.push(res("triage-groups", "PASS", `groups=${groups} known=${known} new=${fresh} (a group is known when a docs/debug-log-triage.md row matches)`));

  const commands = r.out.split(/\r?\n/).filter((l) => /^gh\s/.test(l)).length;
  const files = injected ? drafts : readdirSync(outDir).filter((f) => /^\d+\.md$/.test(f)).length;
  const problems = [];
  if (drafts !== fresh) problems.push(`drafts=${drafts}, want one per new group (${fresh})`);
  if (commands !== fresh) problems.push(`${commands} issue command(s) printed, want ${fresh}`);
  if (files !== fresh) problems.push(`${files} draft file(s) on disk, want ${fresh}`);
  if (fresh > 0 && !/nothing is filed/.test(r.out)) problems.push("the run does not say nothing is filed");
  if (problems.length) out.push(res("triage-drafts", "FAIL", problems.join("; ")));
  else out.push(res("triage-drafts", "PASS", `drafts=${drafts} issue-commands=${commands}, printed and not run`));
  return out;
}

function doctorChecks(run, has, logText, log) {
  const ids = ["doctor-mods", "doctor-boot", "doctor-config"];
  let args = null;
  if (has("tools/commands/server-doctor.mjs")) args = ["server-doctor", log];
  else if (has("tools/host/commands/server-doctor.mjs")) args = ["host", "server-doctor", log];
  if (!args) return ids.map((id) => res(id, "BLOCKED", "server-doctor is not on main (#1271, #1272, #1349)"));
  const r = run(args);
  if (r.status !== 0) return ids.map((id) => res(id, "FAIL", `server-doctor exited ${r.status}: ${r.out.trim().split(/\r?\n/).pop()}`));
  const mods = [...new Set([...logText.matchAll(/under name (\w+)/g)].map((m) => m[1]).filter((n) => !SKIP_DIRS.has(n)))];
  const missing = mods.filter((m) => !r.out.includes(m));
  const ready = /Entered online game state/.test(logText);
  const saysNot = /not boot|did not boot|failed to boot|never booted/i.test(r.out);
  const saysBoot = /\bboot(ed|s)?\b/i.test(r.out);
  const bootOk = ready ? saysBoot && !saysNot : saysNot;
  const modsRes = res("doctor-mods", "PASS", `lists ${mods.join(", ")}`);
  if (!mods.length) Object.assign(modsRes, res("doctor-mods", "FAIL", "the log names no loaded mod to compare with"));
  else if (missing.length) Object.assign(modsRes, res("doctor-mods", "FAIL", `server-doctor does not list ${missing.join(", ")}`));
  return [
    modsRes,
    bootOk ? res("doctor-boot", "PASS", ready ? "says the server booted" : "says the server did not boot")
      : res("doctor-boot", "FAIL", `the log ${ready ? "shows" : "lacks"} the ready line but server-doctor says otherwise`),
    /config/i.test(r.out) && /default/i.test(r.out) ? res("doctor-config", "PASS", "shows the effective config and the fields on a default")
      : res("doctor-config", "FAIL", "output lacks the effective config or its fallbacks to a default"),
  ];
}

function historyCheck(run, has) {
  if (!has("tools/host/commands/history.mjs")) return res("history-session", "BLOCKED", "host history is not on main (#1409)");
  const r = run(["host", "history"]);
  if (r.status !== 0) return res("history-session", "FAIL", `host history exited ${r.status}: ${r.out.trim().split(/\r?\n/).pop()}`);
  const lacks = ["start", "stop", "restart"].filter((w) => !new RegExp(w, "i").test(r.out));
  if (lacks.length) return res("history-session", "FAIL", `the listing has no ${lacks.join(", ")}`);
  return res("history-session", "PASS", "lists the session with start, stop and restart");
}

/** The six checks of B11. `has(rel)` says whether a repo file exists and `run(args)` runs the repo CLI: both injectable. */
export async function runBeat({ root = ROOT, log, triage, has, run } = {}) {
  const ids = ["triage-groups", "triage-drafts", "doctor-mods", "doctor-boot", "doctor-config", "history-session"];
  if (!log || !existsSync(log)) return ids.map((id) => res(id, "BLOCKED", "no host console.log to read (the host has not run; pass --log)"));
  const injected = !!run;
  const runCli = run ?? cliRunner(root);
  const exists = has ?? ((rel) => existsSync(join(root, rel)));
  const outDir = mkdtempSync(join(tmpdir(), "lf-b11-drafts-"));
  const logText = readFileSync(log, "utf8");
  return [...triageChecks(runCli, log, triage, outDir, injected), ...doctorChecks(runCli, exists, logText, log), historyCheck(runCli, exists)];
}

async function main(argv) {
  const flag = (n) => (argv.includes(n) ? argv[argv.indexOf(n) + 1] : undefined);
  const root = flag("--root") ? resolve(flag("--root")) : ROOT;
  let log = flag("--log");
  if (!log) {
    const { hostDir, hostPaths, newestConsoleLog } = await import("../../host/host.mjs");
    log = newestConsoleLog(hostPaths(hostDir()).profile);
  }
  const results = await runBeat({ root, log: log ? resolve(log) : null, triage: flag("--triage") ? resolve(flag("--triage")) : undefined });
  for (const r of results) console.log(formatLine(r));
  return results.some((r) => r.status === "FAIL") ? 1 : 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  process.exitCode = await main(process.argv.slice(2));
}
