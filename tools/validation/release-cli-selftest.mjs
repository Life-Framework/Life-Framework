// tools/validation/release-cli-selftest.mjs
// inputs: tools/**, agent-box/**, .githooks/**
//
// Proves `tools\cli release` (tools/release/release.mjs) offline: a scratch git repo
// with a bare "origin" and stubbed gates. Covers the dry run changing nothing, each
// refusal, the real run stamping both addons and the kits, the gate order, a failing
// gate leaving no tag, and --push being the only thing that reaches the remote.
// red-proof: see the PR body for the observed failures (dirty-tree check removed,
// and the push guard removed).

import { mkdirSync, mkdtempSync, readFileSync, writeFileSync, existsSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { runRelease, gateSteps } from "../release/release.mjs";

const failures = [];
const ok = (cond, msg) => { if (!cond) failures.push(msg); };

function sh(cwd, ...args) {
  const r = spawnSync("git", args, { cwd, encoding: "utf8", windowsHide: true });
  return (r.stdout ?? "").trim();
}
function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}
const gproj = (id, guid, title) => `GameProject {\n ID "${id}"\n GUID "${guid}"\n TITLE "${title}"\n Dependencies {\n  "58D0FB3206B6F859"\n }\n}\n`;
const readme = "| Field | Value |\n|---|---|\n| Version | `0.1.0` |\n| Licence | APL |\n";

function scratch() {
  const base = mkdtempSync(join(tmpdir(), "lf-relcli-"));
  const origin = join(base, "origin.git");
  const root = join(base, "repo");
  mkdirSync(origin);
  sh(origin, "init", "--bare", "-q");
  mkdirSync(root);
  sh(root, "init", "-q", "-b", "main");
  sh(root, "config", "user.name", "t");
  sh(root, "config", "user.email", "t@t");
  sh(root, "config", "commit.gpgsign", "false");
  sh(root, "config", "tag.gpgsign", "false");
  sh(root, "remote", "add", "origin", origin);
  put(root, "addons/LifeFramework/LifeFramework.gproj", gproj("LifeFramework", "79636E668EA37AC9", "Life Framework"));
  put(root, "addons/LF_Everon1989/LF_Everon1989.gproj", gproj("LF_Everon1989", "B1E84D5687E11724", "Pack"));
  put(root, "docs/marketing/workshop/README.md", readme);
  put(root, "docs/marketing/workshop/everon-1989/README.md", readme);
  put(root, ".gitignore", "tmp/\n");
  sh(root, "add", "-A");
  sh(root, "commit", "-q", "-m", "init");
  sh(root, "checkout", "-q", "-b", "ws/a");
  sh(root, "commit", "-q", "--allow-empty", "-m", "work");
  sh(root, "checkout", "-q", "main");
  sh(root, "merge", "--no-ff", "-q", "ws/a", "-m", "Merge pull request #7 from x/ws/a", "-m", "Add the thing");
  return { root, origin };
}

function stubs(failAt = null) {
  const calls = [];
  const runner = (label) => { calls.push(label); return label === failAt ? 1 : 0; };
  return { calls, runner };
}
const quiet = { log: () => {} };
const lines = () => { const out = []; return { out, log: (s) => out.push(s) }; };

// 1. Dry run (default) and --dry-run: exit 0, nothing changes, only the cheap gate runs.
for (const flags of [[], ["--dry-run"]]) {
  const { root } = scratch();
  const s = stubs();
  const code = await runRelease(root, ["0.2.0", ...flags], { ...quiet, runner: s.runner });
  ok(code === 0, `dry run ${flags.join(" ") || "(default)"} exits 0, got ${code}`);
  ok(sh(root, "status", "--porcelain") === "", "dry run leaves the tree clean");
  ok(sh(root, "tag") === "", "dry run creates no tag");
  ok(s.calls.length === 1 && /release-ready/.test(s.calls[0]), `dry run runs only the cheap gate, ran: ${s.calls}`);
}

// 2. Refusals: exit 1, the reason named, no gate run, no tag.
{
  const refuse = async (name, setup, version, expect, extra = []) => {
    const { root } = scratch();
    setup(root);
    const s = stubs();
    const l = lines();
    const code = await runRelease(root, [version, "--confirm", ...extra], { log: l.log, runner: s.runner });
    ok(code === 1, `${name} is refused: expected exit 1, got ${code}`);
    ok(expect.test(l.out.join("\n")), `${name}: output lacks ${expect}`);
    ok(s.calls.length === 0, `${name}: no gate may run`);
    ok(sh(root, "tag") === (name === "existing tag" ? "v0.2.0" : ""), `${name}: no new tag`);
  };
  await refuse("dirty tree", (r) => put(r, "addons/LifeFramework/stray.txt", "x"), "0.2.0", /working tree is dirty/);
  await refuse("branch ws/x", (r) => sh(r, "checkout", "-q", "-b", "ws/x"), "0.2.0", /release runs from main or stable/);
  await refuse("bad version", () => {}, "v2", /not x\.y\.z/);
  await refuse("existing tag", (r) => sh(r, "tag", "v0.2.0"), "0.2.0", /tag v0\.2\.0 already exists/);
  const { root } = scratch();
  const l = lines();
  const code = await runRelease(root, ["0.2.0", "--push"], { log: l.log, runner: stubs().runner });
  ok(code === 1 && /--push needs --confirm/.test(l.out.join("\n")), "--push without --confirm is refused");
}
{
  // A detached HEAD on origin/main is accepted.
  const { root } = scratch();
  sh(root, "push", "-q", "origin", "main");
  sh(root, "fetch", "-q", "origin");
  sh(root, "checkout", "-q", "--detach", "origin/main");
  const code = await runRelease(root, ["0.2.0"], { ...quiet, runner: stubs().runner });
  ok(code === 0, `detached HEAD at origin/main is accepted, got ${code}`);
}

// 3. Real run: both addons and both kits stamped in the tag, gates in order, notes, tag, no push.
{
  const { root, origin } = scratch();
  const s = stubs();
  const code = await runRelease(root, ["0.2.0", "--confirm"], { ...quiet, runner: s.runner });
  ok(code === 0, `real run exits 0, got ${code}`);
  ok(JSON.stringify(s.calls) === JSON.stringify(gateSteps().map((x) => x[0])), `gates run in order, ran: ${s.calls}`);
  ok(sh(root, "tag") === "v0.2.0", "tag v0.2.0 exists");
  for (const f of ["addons/LifeFramework/LifeFramework.gproj", "addons/LF_Everon1989/LF_Everon1989.gproj"]) {
    ok(sh(root, "show", `v0.2.0:${f}`).includes('Version "0.2.0"'), `${f} stamped in the tag`);
    ok(!readFileSync(join(root, f), "utf8").includes("Version"), `${f} restored in the working tree`);
  }
  for (const f of ["docs/marketing/workshop/README.md", "docs/marketing/workshop/everon-1989/README.md"]) {
    ok(sh(root, "show", `v0.2.0:${f}`).includes("| Version | `0.2.0` |"), `${f} stamped in the tag`);
  }
  ok(sh(root, "status", "--porcelain") === "", "tree clean after the release");
  ok(sh(root, "rev-parse", "main") === sh(root, "rev-parse", "v0.2.0^{commit}^"), "main did not move; the tag commit sits on top of it");
  const notes = join(root, "tmp", "release", "notes-0.2.0.md");
  ok(existsSync(notes) && readFileSync(notes, "utf8").includes("- Add the thing (#7)"), "notes list the merged PR");
  ok(sh(origin, "tag") === "", `no --push, remote has no tag: "${sh(origin, "tag")}"`);
  // Stamping twice replaces the line.
  sh(root, "tag", "-d", "v0.2.0");
  await runRelease(root, ["0.3.0", "--confirm"], { ...quiet, runner: stubs().runner });
  const g3 = sh(root, "show", "v0.3.0:addons/LifeFramework/LifeFramework.gproj");
  ok((g3.match(/Version "/g) ?? []).length === 1, "one Version line after stamping");
}

// 4. --push pushes only the tag.
{
  const { root, origin } = scratch();
  const code = await runRelease(root, ["1.0.0", "--confirm", "--push"], { ...quiet, runner: stubs().runner });
  ok(code === 0, `push run exits 0, got ${code}`);
  ok(sh(origin, "tag") === "v1.0.0", `--push puts the tag on the remote: "${sh(origin, "tag")}"`);
  ok(sh(origin, "branch", "--list") === "", "no branch reaches the remote");
}

// 5. A failing gate stops the run: no tag, files restored, later gates skipped.
{
  const { root } = scratch();
  const s = stubs("full suite");
  const l = lines();
  const code = await runRelease(root, ["0.2.0", "--confirm"], { log: l.log, runner: s.runner });
  ok(code === 1, `failing gate exits 1, got ${code}`);
  ok(sh(root, "tag") === "", "failing gate: no tag");
  ok(sh(root, "status", "--porcelain") === "", "failing gate: stamped files restored");
  ok(!s.calls.includes("audit placement --strict"), "gates after the failure do not run");
  ok(/gate "full suite"/.test(l.out.join("\n")), "output names the failed gate");
}

for (const f of failures) console.log(`  FAIL  ${f}`);
if (failures.length) {
  console.log("release-cli-selftest: FAIL");
  process.exit(1);
}
console.log("release-cli-selftest: OK (dry run, refusals, stamp, gates, tag, push, gate failure)");
