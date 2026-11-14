// Proves the quorum rule of `promote --confirm` (#575) in a scratch repo with a fake GitHub:
// inputs: tools/**, agent-box/**, .githooks/**
//   1. with the rule off, promote works as before (no confirmations file needed);
//   2. with the rule on, too few roster confirmations refuse and leave stable where it was;
//   3. confirmations from people not on the roster, and a repeat from one tester, do not count;
//   4. one fail from a roster tester blocks the promotion;
//   5. enough distinct roster testers move stable and mark the issue verified;
//   6. an issue whose PR lists no manual steps needs no confirmation.
// red-proof: with the `pass >= size` check in evaluateQuorum changed to `pass >= 1` it printed
// "two confirmations promoted".
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as rg from "../lib/release-gates.mjs";
import { evaluateQuorum } from "../lib/quorum.mjs";
import { cmdPromote } from "../promote.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

Object.assign(process.env, { GIT_AUTHOR_NAME: "t", GIT_AUTHOR_EMAIL: "t@t", GIT_COMMITTER_NAME: "t", GIT_COMMITTER_EMAIL: "t@t" });
const sh = (cwd, ...args) => {
  const r = spawnSync("git", args, { cwd, encoding: "utf8", windowsHide: true });
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${r.stderr}`);
  return r.stdout.trim();
};
const mute = async (fn) => {
  const real = console.log;
  const lines = [];
  console.log = (...a) => lines.push(a.join(" "));
  try {
    return { code: await fn(), out: lines.join("\n") };
  } finally {
    console.log = real;
  }
};

const roster = ["ana", "ben", "cy", "dee"].map((id) => ({ id }));
const res = (issue, tester, result = "pass") => ({ issue, tester, result });

// ---- the pure rule
let v = evaluateQuorum({ issues: [1], results: [res(1, "ana"), res(1, "ben")], roster, size: 3 });
check(!v.ok && v.short.length === 1, "two confirmations promoted");
v = evaluateQuorum({ issues: [1], results: [res(1, "ana"), res(1, "ben"), res(1, "cy")], roster, size: 3 });
check(v.ok, "three confirmations were refused");
v = evaluateQuorum({ issues: [1], results: [res(1, "ana"), res(1, "ana"), res(1, "ben"), res(1, "Stranger"), res(1, "mallory")], roster, size: 3 });
check(!v.ok, "a repeat and two strangers counted");
v = evaluateQuorum({ issues: [1], results: [res(1, "ana"), res(1, "ben"), res(1, "cy"), res(1, "dee", "fail")], roster, size: 3 });
check(!v.ok && v.failed.length === 1, "a fail did not block");
v = evaluateQuorum({ issues: [], results: [], roster, size: 3 });
check(v.ok, "no issues needed confirmation but the rule refused");

// ---- the command
const dir = mkdtempSync(join(tmpdir(), "lf-quorum-"));
const origin = join(dir, "origin.git");
const work = join(dir, "work");
try {
  sh(dir, "init", "--bare", "-b", "main", origin);
  sh(dir, "clone", origin, work);
  writeFileSync(join(work, "base.txt"), "base\n");
  sh(work, "add", "."); sh(work, "commit", "-m", "base"); sh(work, "push", "origin", "HEAD:main");
  sh(work, "fetch", "origin");
  rg.initStable(work, "origin/main");
  const mergePr = (n) => {
    sh(work, "checkout", "-q", "-b", `ws/pr${n}`, "main");
    writeFileSync(join(work, `f${n}.txt`), `${n}\n`);
    sh(work, "add", "."); sh(work, "commit", "-m", `work ${n}`);
    sh(work, "checkout", "-q", "main");
    sh(work, "merge", "--no-ff", "-m", `Merge pull request #${n} from x/ws/pr${n}`, `ws/pr${n}`);
    sh(work, "push", "origin", "main");
    return sh(work, "rev-parse", "HEAD");
  };
  mergePr(31); // docs only, no manual steps
  const m32 = mergePr(32); // manual steps
  sh(work, "fetch", "origin");
  const prs = {
    31: { number: 31, title: "Docs", body: "Refs #301\n", mergedAt: "t" },
    32: { number: 32, title: "Payphone", body: "Refs #302\n\n## Manual testing\n\n**Steps**\n\n1. Dial\n\n**Expected**\n\nA tone\n", mergedAt: "t" },
  };
  const issues = { 301: { state: "open", labels: ["in-dev"] }, 302: { state: "open", labels: ["needs-playtest"] } };
  const gh = {
    async getPr(n) { return prs[n] ?? null; },
    async getIssue(n) { const i = issues[n]; return i ? { number: n, state: i.state, stateReason: "", labels: [...i.labels] } : null; },
    async addLabels(n, l) { for (const x of l) if (!issues[n].labels.includes(x)) issues[n].labels.push(x); },
    async removeLabels(n, l) { issues[n].labels = issues[n].labels.filter((x) => !l.includes(x)); },
    async close(n) { issues[n].state = "closed"; },
    async reopen(n) { issues[n].state = "open"; },
  };
  const stable = () => sh(origin, "rev-parse", "stable");
  const before = stable();
  const writeRule = (enabled) => {
    mkdirSync(join(work, "docs", "playtest"), { recursive: true });
    writeFileSync(join(work, "docs", "playtest", "quorum.json"), JSON.stringify({ enabled, size: 3, roster: "docs/playtest/testers.json" }));
    writeFileSync(join(work, "docs", "playtest", "testers.json"), JSON.stringify({ version: 1, testers: roster }));
  };
  const confFile = (name, results) => {
    const f = join(work, "tmp", name);
    mkdirSync(join(work, "tmp"), { recursive: true });
    writeFileSync(f, JSON.stringify({ sha: m32.slice(0, 8), results }));
    return f;
  };
  const to = m32.slice(0, 12);

  // rule on: no file, too few, strangers and a repeat, a fail
  writeRule(true);
  let r = await mute(() => cmdPromote(work, ["--to", to, "--confirm"], { gh }));
  check(r.code === 1 && stable() === before, `rule on without a confirmations file moved stable: ${r.out}`);
  r = await mute(() => cmdPromote(work, ["--to", to, "--confirm", "--confirmations", confFile("two.json", [res(302, "ana"), res(302, "ben")])], { gh }));
  check(r.code === 1 && /2 of 3/.test(r.out) && stable() === before, `two confirmations promoted: ${r.out}`);
  check(issues[302].state === "open" && !issues[302].labels.includes("verified"), "issue verified without a quorum");
  r = await mute(() => cmdPromote(work, ["--to", to, "--confirm", "--confirmations", confFile("strangers.json", [res(302, "ana"), res(302, "ana"), res(302, "mallory"), res(302, "zed")])], { gh }));
  check(r.code === 1 && stable() === before, `strangers and a repeat promoted: ${r.out}`);
  r = await mute(() => cmdPromote(work, ["--to", to, "--confirm", "--confirmations", confFile("fail.json", [res(302, "ana"), res(302, "ben"), res(302, "cy"), res(302, "dee", "fail")])], { gh }));
  check(r.code === 1 && /failed it/.test(r.out) && stable() === before, `a failed issue promoted: ${r.out}`);

  // enough distinct roster testers
  r = await mute(() => cmdPromote(work, ["--to", to, "--confirm", "--confirmations", confFile("three.json", [res(302, "ana"), res(302, "ben"), res(302, "cy")])], { gh }));
  check(r.code === 0 && /quorum met/.test(r.out), `three confirmations refused: ${r.out}`);
  check(stable() === m32, "stable did not move on a met quorum");
  check(issues[302].state === "closed" && issues[302].labels.includes("verified"), `302 after promotion: ${issues[302].state} ${issues[302].labels}`);
  check(issues[301].state === "closed" && issues[301].labels.includes("verified"), "301 (no manual steps) was not verified with the rest");

  // rule off: the old behaviour
  const m33 = mergePr(33);
  prs[33] = { number: 33, title: "More", body: "Refs #303\n\n## Manual testing\n\nHost and join.\n", mergedAt: "t" };
  issues[303] = { state: "open", labels: ["needs-playtest"] };
  sh(work, "fetch", "origin");
  writeRule(false);
  r = await mute(() => cmdPromote(work, ["--to", m33.slice(0, 12), "--confirm"], { gh }));
  check(r.code === 0 && stable() === m33, `rule off, promote failed: ${r.out}`);
} catch (err) {
  failures.push(`selftest crashed: ${err.stack || err.message}`);
} finally {
  try { rmSync(dir, { recursive: true, force: true }); } catch { /* temp scratch only */ }
}

if (failures.length) {
  console.log("quorum-selftest: FAIL");
  for (const f of failures) console.log("  " + f);
  process.exit(1);
}
console.log("quorum-selftest: OK");
