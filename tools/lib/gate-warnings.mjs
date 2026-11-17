// Warn-only gate steps on a town-maintainer-* branch (docs/worktrees.md "Maintainer world edits"). A maintainer's
// hand edit is never blocked by layout findings, whatever tier the gate runs: the ci placement audit still runs, its
// findings are logged as WARN, written to tmp/gate-warnings.json, and the ship queue lists them in the PR body.
// Pure apart from the stdout capture and the file helpers; proven by tools/validation/maintainer-gate-selftest.mjs.
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { MAINTAINER_BRANCH } from "./gate-tier.mjs";
import { judgeWorldTests, testWarningLines } from "./world-test-judge.mjs";

export const GATE_WARNINGS_FILE = join("tmp", "gate-warnings.json");
export const GATE_WARNINGS_HEADING = "## Gate warnings (maintainer layout)";
const BEGIN = "<!-- lf-gate-warnings -->";
const END = "<!-- /lf-gate-warnings -->";
const LINES_PER_CHECK = 60;
const LINE_CHARS = 400;
// GitHub rejects a PR body over 65,536 characters; the section is cut well before it can matter.
export const PR_BODY_LIMIT = 65000;

// "  5.  15.5  Market_Tarp_Juice [Village_Dressing.layer, market] @217.5,2.5,233.1: overlaps ..." (place-audit ranked rows).
export function placementFindingLines(text) {
  return String(text).split(/\r?\n/).filter((l) => /^\s*\d+\.\s+[\d.]+\s+\S+ \[/.test(l)).map((l) => l.trim());
}

// Runs fn with process.stdout.write mirrored into a string; the output still reaches the log.
export async function captureStdout(fn) {
  let text = "";
  const write = process.stdout.write.bind(process.stdout);
  process.stdout.write = (chunk, ...rest) => {
    text += typeof chunk === "string" ? chunk : Buffer.from(chunk).toString();
    return write(chunk, ...rest);
  };
  let code;
  try {
    code = await fn();
  } finally {
    process.stdout.write = write;
  }
  return { code, text };
}

// `audit(args)` is the placement audit runner (exit code). On a maintainer branch a failing audit prints a WARN,
// hands its findings to record({ check, lines, code }) and exits 0; elsewhere the runner is returned untouched.
export function demotePlacement(branch, audit, record = () => {}) {
  if (!MAINTAINER_BRANCH.test(branch || "")) return audit;
  return async (args) => {
    const { code, text } = await captureStdout(() => audit(args));
    if (code === 0) return 0;
    const lines = placementFindingLines(text);
    console.log(`WARN  placement audit flagged ${lines.length || "some"} entit${lines.length === 1 ? "y" : "ies"} (exit ${code}): a warning on a maintainer branch, listed in the PR body (tmp/gate-warnings.json)`);
    record({ check: "placement audit", lines, code });
    return 0;
  };
}

// `run(...args)` is a ci test step (exit code). On a maintainer branch a step whose tests FAILED prints a WARN per
// failure, hands them to record({ check: "tests", lines, code }) and exits 0, at every tier (full, slim, T1). A step that
// judged nothing still blocks: a script compile error, or no SUMMARY line (timeout, crash, environment)
// (tools/lib/world-test-judge.mjs). ctx: { fallbackTests, tests } (tests may be a function, read only on a failure).
export function demoteTests(branch, run, record = () => {}, ctx = {}) {
  if (!MAINTAINER_BRANCH.test(branch || "")) return run;
  return async (...args) => {
    const { code, text } = await captureStdout(() => run(...args));
    if (!code) return code ?? 0;
    const tests = typeof ctx.tests === "function" ? ctx.tests() : ctx.tests ?? [];
    const verdict = judgeWorldTests({ status: code, text, fallbackTests: ctx.fallbackTests ?? [], tests });
    if (verdict.blocks) {
      console.log(`FAIL  tests: ${verdict.blocks}`);
      return code;
    }
    const lines = testWarningLines(verdict.warnings);
    for (const l of lines) console.log(`WARN  test failed on a maintainer branch (a warning, not blocking, listed in the PR body): ${l}`);
    record({ check: "tests", lines, code });
    return 0;
  };
}

export function recordGateWarning(root, entry) {
  const file = join(root, GATE_WARNINGS_FILE);
  let all = [];
  try { all = JSON.parse(readFileSync(file, "utf8")); } catch { /* none yet */ }
  all = all.filter((e) => e.check !== entry.check);
  all.push({ check: entry.check, lines: entry.lines ?? [], code: entry.code ?? 1 });
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, JSON.stringify(all, null, 1));
}

export function clearGateWarnings(root) {
  rmSync(join(root, GATE_WARNINGS_FILE), { force: true });
}

export function readGateWarnings(root) {
  const file = join(root, GATE_WARNINGS_FILE);
  if (!existsSync(file)) return [];
  try { return JSON.parse(readFileSync(file, "utf8")); } catch { return []; }
}

// The PR body section for the recorded warnings, "" when there are none.
export function gateWarningsMarkdown(entries) {
  const live = (entries ?? []).filter((e) => e && (e.lines?.length || e.note));
  if (!live.length) return "";
  const L = [BEGIN, GATE_WARNINGS_HEADING, "", "The gate ran these checks on this maintainer branch and they found something. A maintainer's hand edit is never blocked by layout findings, so the PR still merges; look at them when you play the world:", ""];
  for (const e of live) {
    L.push(`**${e.check}**`, "");
    const lines = e.lines ?? [];
    for (const l of lines.slice(0, LINES_PER_CHECK)) L.push(`- ${l.length > LINE_CHARS ? `${l.slice(0, LINE_CHARS)}...` : l}`);
    if (lines.length > LINES_PER_CHECK) L.push(`- +${lines.length - LINES_PER_CHECK} more, see the gate log`);
    L.push("");
  }
  L.push(END);
  return L.join("\n");
}

// body with its gate-warnings section replaced by md (appended when there is none, removed when md is "").
export function applyGateWarnings(body, md) {
  const text = String(body ?? "");
  const i = text.indexOf(BEGIN);
  const j = text.indexOf(END);
  const stripped = i >= 0 && j > i ? `${text.slice(0, i)}${text.slice(j + END.length)}`.replace(/\n{3,}/g, "\n\n").replace(/\s+$/, "\n") : text;
  if (!md) return stripped;
  const out = `${stripped.replace(/\s+$/, "")}\n\n${md}\n`;
  return out.length <= PR_BODY_LIMIT ? out : stripped;
}
