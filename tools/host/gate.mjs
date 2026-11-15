// The #555 opening gate behind `tools\cli host gate` (#1410): the four lines of
// docs/community/opening-gate.md in one run. Each line passes or fails; the gate opens only when all do.
//
//   sessions    the latest SESSIONS finished sessions each peaked at TARGET players or more (host history)
//   save        the last restore drill passed (drill-last.json) and no restore happened after a session
//   regression  no open `regression` issue
//   staff       the roster names an administrator with a real uid (the maintainer; no #pending one)
//
// evaluateGate is pure: the history, drill result, issue list and roster text come in as arguments, so a
// test injects all four. gatherInputs reads the real ones. Opening itself (removing the alpha password)
// stays the maintainer's step.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { hostHistory } from "./history.mjs";
import { readDrillResult } from "./drill.mjs";
import { adminIds, parseRoster } from "../commands/staff.mjs";

export const DEFAULT_TARGET = 40; // "a peak of about 40 players" (docs/community/opening-gate.md)
export const DEFAULT_SESSIONS = 3; // "several sessions in a row"

const RESTORE = /-replaced-by-restore(?:-\d+)?$/;
const STAMP = /^(\d{4})-(\d\d)-(\d\d)_(\d\d)-(\d\d)-(\d\d)/;
// "2026-11-10_18-00-00-x" -> "2026-11-10 18:00:00", the form history.mjs prints session times in.
const stampDisplay = (name) => {
  const m = STAMP.exec(name);
  return m ? `${m[1]}-${m[2]}-${m[3]} ${m[4]}:${m[5]}:${m[6]}` : null;
};

function sessionsLine(history, target, need) {
  // A session that has not ended (still running, or no start recorded) neither counts nor breaks the run.
  const done = (history?.sessions ?? []).filter((s) => s.start && s.end);
  const last = done.slice(-need);
  const low = last.filter((s) => !(s.peakPlayers >= target));
  const facts = last.map((s) => `${s.start.slice(0, 10)} peak ${s.peakPlayers ?? "?"}`).join(", ");
  if (done.length < need) return { id: "sessions", ok: false, detail: `${done.length} finished session(s), need ${need} in a row at ${target}+ players${facts ? ` (${facts})` : ""}` };
  if (low.length) return { id: "sessions", ok: false, detail: `${low.length} of the latest ${need} sessions under ${target} players (or no player count in perf/*.csv): ${facts}` };
  return { id: "sessions", ok: true, detail: `the latest ${need} sessions each peaked at ${target}+ players: ${facts}` };
}

function saveLine(history, drill) {
  const problems = [];
  if (!drill) problems.push("no restore drill result (run `tools\\cli host drill`)");
  else if (!drill.ok) problems.push(`the last restore drill failed${drill.reason ? `: ${drill.reason}` : ""}`);
  const sessions = history?.sessions ?? [];
  const firstStart = sessions.map((s) => s.start).filter(Boolean).sort()[0];
  const restores = [];
  if (firstStart) {
    for (const s of sessions) {
      for (const b of s.backups ?? []) {
        const at = stampDisplay(b);
        if (RESTORE.test(b) && at && at >= firstStart) restores.push(b);
      }
    }
  }
  if (restores.length) problems.push(`a restore after a session: ${[...new Set(restores)].join(", ")}`);
  if (problems.length) return { id: "save", ok: false, detail: problems.join("; ") };
  return { id: "save", ok: true, detail: `drill passed (${drill.backup ?? "?"}, ${drill.accounts} account(s)), no restore after a session` };
}

function regressionLine(issues) {
  if (!Array.isArray(issues)) return { id: "regression", ok: false, detail: "the regression issue list could not be read (gh failed)" };
  if (issues.length) return { id: "regression", ok: false, detail: `${issues.length} open regression issue(s): ${issues.map((i) => `#${i.number}${i.title ? ` ${i.title}` : ""}`).join("; ")}` };
  return { id: "regression", ok: true, detail: "no open regression issue" };
}

function staffLine(rosterText) {
  if (rosterText === null || rosterText === undefined) return { id: "staff", ok: false, detail: "staff roster not found (server/flagship/staff.txt)" };
  const { ids, errors } = adminIds(parseRoster(rosterText));
  if (errors.length) return { id: "staff", ok: false, detail: `roster: ${errors[0]}` };
  if (!ids.length) return { id: "staff", ok: false, detail: "the roster has no administrator with a uid" };
  return { id: "staff", ok: true, detail: `${ids.length} administrator uid(s) filled in` };
}

const TITLES = {
  sessions: "Sessions at the player target",
  save: "No save loss",
  regression: "No open regression",
  staff: "Administrator uid in the roster",
};

// Pure. { history: {sessions}, drill: readDrillResult()|null, issues: [{number,title}]|null,
//         roster: text|null, target?, sessions? } -> { open, lines: [{ id, title, ok, detail }] }
export function evaluateGate({ history, drill, issues, roster, target = DEFAULT_TARGET, sessions = DEFAULT_SESSIONS }) {
  const lines = [sessionsLine(history, target, sessions), saveLine(history, drill), regressionLine(issues), staffLine(roster)]
    .map((l) => ({ ...l, title: TITLES[l.id] }));
  return { open: lines.every((l) => l.ok), lines, target, sessions };
}

export function gateText(g) {
  const out = ["[LFHost:gate] the #555 opening gate (docs/community/opening-gate.md)"];
  for (const l of g.lines) out.push(`${l.ok ? "PASS" : "FAIL"}  ${l.title}: ${l.detail}`);
  out.push(g.open
    ? "the gate is open: removing the alpha password is the maintainer's step"
    : `the gate is closed: ${g.lines.filter((l) => !l.ok).length} of ${g.lines.length} lines fail`);
  return out.join("\n");
}

export function gateSummary(g) {
  const failed = g.lines.filter((l) => !l.ok).map((l) => l.id);
  return `SUMMARY host-gate ${g.open ? "open" : "closed"} passed=${g.lines.length - failed.length}/${g.lines.length}${failed.length ? ` failed=${failed.join(",")}` : ""}`;
}

// Open `regression` issues from GitHub, or null when gh fails. Bounded: a dead link ends the call.
export function ghRegressions(root) {
  const r = spawnSync("gh", ["issue", "list", "--label", "regression", "--state", "open", "--json", "number,title", "--limit", "100"], {
    cwd: root, encoding: "utf8", timeout: 30000, windowsHide: true,
  });
  if (r.status !== 0) return null;
  try {
    const list = JSON.parse(r.stdout);
    return Array.isArray(list) ? list : null;
  } catch {
    return null;
  }
}

// Reads the real inputs. `issuesFile` (a JSON array of { number, title }) and `rosterFile` override gh and
// the repo roster; the command passes them from --issues-file and --roster.
export function gatherInputs({ root, hostDir, issuesFile, rosterFile }) {
  let issues;
  if (issuesFile) {
    try {
      const list = JSON.parse(readFileSync(issuesFile, "utf8"));
      issues = Array.isArray(list) ? list : null;
    } catch {
      issues = null;
    }
  } else issues = ghRegressions(root);
  const roster = rosterFile ?? join(root, "server", "flagship", "staff.txt");
  return {
    history: hostHistory(hostDir),
    drill: readDrillResult(hostDir),
    issues,
    roster: existsSync(roster) ? readFileSync(roster, "utf8") : null,
  };
}
