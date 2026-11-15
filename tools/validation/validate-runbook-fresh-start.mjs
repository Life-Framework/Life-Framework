// The session runbook's "Start" section describes a fresh start as well as a continued one (#1634).
// inputs: docs/ops/session-runbook.md, docs/ops/session-cheatsheet.md, addons/LifeFramework/Scripts/Game/Persistence/LF_PersistenceManagerComponent.c
//
// On an empty host the server logs `save present: false` and `fresh session: no save point exists
// for this mission`; `save loaded:` appears only on the next start. A runbook that says "you want
// `save loaded:`" fails its first real operator. The two fresh-start lines are read from the game
// source, so a reworded log line changes the demand. The Start section must also say that the first
// `host start` in a new worktree refreshes the resource database first, and that the server runs on
// this PC, not on a test machine.
//
// `--selftest` runs the check on a Start section that only knows `save loaded:` and on one that
// covers all four points.
//
// red-proof: 2026-11-15, on the runbook before #1634 `SUMMARY validate-runbook-fresh-start fail 6`,
// first line "runbook Start never names the fresh-start line `save present: false`", exit 1.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

// The text of the "## Start" section, up to the next "## " heading.
export function startSection(md) {
  const m = /^## Start\s*$([\s\S]*?)(?=^## )/m.exec(md);
  return m ? m[1] : "";
}

// The two log lines a fresh start prints, as the game source writes them: a prefix each.
export function freshStartLines(src) {
  const out = [];
  if (/fresh session: no save point exists for this mission/.test(src)) out.push("fresh session:");
  if (/save scan complete, save present: /.test(src)) out.push("save present: false");
  return out;
}

export function checkStart(text, lines, where, full = true) {
  const out = [];
  for (const l of lines) if (!text.includes(l)) out.push(`${where} never names the fresh-start line \`${l}\``);
  if (!full) return out;
  if (!/resource database/i.test(text)) out.push(`${where} never says the first start in a new worktree refreshes the resource database`);
  if (!/this PC|this machine|same machine/i.test(text)) out.push(`${where} never says the server runs on this PC, not a test machine`);
  return out;
}

function selftest() {
  const lines = ["fresh session:", "save present: false"];
  let failed = 0;
  const fail = (m) => {
    console.log(`FAIL  ${m}`);
    failed++;
  };
  const bad = "You want `save loaded:` in the log.";
  const good =
    "A fresh host logs `save present: false` and `fresh session: no save point exists for this mission`; `save loaded:` " +
    "comes on the next start. The first start in a new worktree refreshes the resource database first (about 45 s). " +
    "The server runs on this PC.";
  if (checkStart(bad, lines, "bad").length !== 4) fail(`a Start section with only save loaded: should give 4 findings, got ${checkStart(bad, lines, "bad").length}`);
  if (checkStart(good, lines, "good").length) fail(`a complete Start section was flagged: ${checkStart(good, lines, "good").join("; ")}`);
  if (freshStartLines('"fresh session: no save point exists for this mission" "save scan complete, save present: %1"').length !== 2) fail("freshStartLines misses the source lines");
  if (!startSection("## Start\n\nbody\n\n## During\n").includes("body")) fail("startSection misses the section");
  console.log(failed ? `SUMMARY validate-runbook-fresh-start selftest fail ${failed}` : "SUMMARY validate-runbook-fresh-start selftest ok");
  process.exit(failed ? 1 : 0);
}

if (process.argv.includes("--selftest")) selftest();

const read = (p) => (existsSync(join(ROOT, p)) ? readFileSync(join(ROOT, p), "utf8") : "");
const lines = freshStartLines(read("addons/LifeFramework/Scripts/Game/Persistence/LF_PersistenceManagerComponent.c"));
const findings = [
  ...checkStart(startSection(read("docs/ops/session-runbook.md")), lines, "runbook Start"),
  ...checkStart(startSection(read("docs/ops/session-cheatsheet.md")), lines, "cheatsheet Start", false),
];
for (const f of findings) console.log(`FAIL  ${f}`);
console.log(findings.length ? `SUMMARY validate-runbook-fresh-start fail ${findings.length}` : "SUMMARY validate-runbook-fresh-start ok");
process.exit(findings.length ? 1 : 0);
