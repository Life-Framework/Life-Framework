// Proves the safety check `prs heal --watch` puts between a merge and the push, on throwaway repos
// with a bare origin. Never touches this repo, its worktrees, or GitHub.
// inputs: tools/**, .gitattributes
//
//   ws/gen-only    both sides append a string key, a language conf line and a features.md row; the
//                  conf conflicts, the others merge through the drivers -> healed; merged keys are
//                  base + both additions, every row is there.
//   ws/real        both sides edit one script line -> needs-human, HEAD and origin untouched.
//   ws/lost-key    like gen-only but the regenerate step also drops a main key -> refused, merge undone.
//   ws/lost-row    like gen-only but the regenerate step drops main's features.md row -> refused.
//   ws/own-delete  the branch deliberately removes a key; the merge keeps that removal -> healed.
//
// red-proof (2026-11-14): with safetyCheck stubbed to { ok: true } lost-key and lost-row printed
//   "expected refused, got healed".
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { healMerge } from "../prs.mjs";
import { safeRemoveTree } from "../link-safety.mjs";

const failures = [];
const fail = (m) => failures.push(m);
const ST = "addons/LifeFramework/Language/everonlife_localization.st";
const CONF = "addons/LifeFramework/Language/everonlife_localization.en_us.conf";
const FEATURES = "docs/features.md";
const SCRIPT = "addons/LifeFramework/Scripts/Game/Core/LF_Foo.c";

function git(dir, args) {
  return spawnSync("git", args, { cwd: dir, windowsHide: true, encoding: "utf8" });
}
function must(dir, args) {
  const r = git(dir, args);
  if (r.status !== 0) throw new Error(`git ${args.join(" ")} in ${dir}: ${r.stderr || r.stdout}`);
  return r.stdout.trim();
}
function write(dir, rel, text) {
  mkdirSync(dirname(join(dir, rel)), { recursive: true });
  writeFileSync(join(dir, rel), text);
}
function commit(dir, msg) {
  must(dir, ["add", "-A"]);
  must(dir, ["commit", "-q", "-m", msg]);
}

const guid = (k) => createHash("sha1").update(k).digest("hex").slice(0, 16).toUpperCase();
const item = (k) => `  CustomStringTableItem "{${guid(k)}}" {\n   Id "${k}"\n   Target_en_us "${k} text"\n  }`;
const stText = (keys) => `StringTable {\n ItemClassName "CustomStringTableItem"\n Items {\n${keys.map(item).join("\n")}\n }\n}\n`;
const idsOf = (text) => [...text.matchAll(/^\s+Id "([^"]+)"/gm)].map((m) => m[1]);
const featuresText = (rows) => `# Features\n\n| Feature | Does |\n|---|---|\n${rows.map((r) => `| ${r} | does ${r} |`).join("\n")}\n`;

// Stand-in for regen-localization: the conf is one line per string id, in table order.
function regenConf(dir) {
  write(dir, CONF, idsOf(readFileSync(join(dir, ST), "utf8")).map((k) => `${k}=${k} text`).join("\n") + "\n");
}
// Appends a key, its conf line and a features row, as a feature PR does.
function addFeature(dir, key) {
  const keys = [...idsOf(readFileSync(join(dir, ST), "utf8")), key];
  write(dir, ST, stText(keys));
  regenConf(dir);
  const rows = [...readFileSync(join(dir, FEATURES), "utf8").matchAll(/^\| (\w+) \| does/gm)].map((m) => m[1]).filter((r) => r !== "Feature");
  write(dir, FEATURES, featuresText([...rows, `Row${key}`]));
}
function regenWith(tamper) {
  return (dir, kind) => {
    if (kind !== "regen-localization") return { ok: false, error: `unexpected ${kind}` };
    regenConf(dir);
    if (tamper) {
      tamper(dir);
      must(dir, ["add", "-A"]);
    }
    return { ok: true };
  };
}

const temp = mkdtempSync(join(tmpdir(), "lf-heal-safety-"));
try {
  const origin = join(temp, "origin.git");
  const seed = join(temp, "seed");
  must(temp, ["init", "-q", "--bare", "-b", "main", origin]);
  must(temp, ["clone", "-q", origin, seed]);
  for (const [k, v] of [["user.email", "heal@example.com"], ["user.name", "heal"], ["core.autocrlf", "false"], ["commit.gpgsign", "false"], ["core.hooksPath", join(temp, "no-hooks")]]) {
    must(seed, ["config", k, v]);
  }
  must(seed, ["checkout", "-q", "-b", "main"]);
  write(seed, ST, stText(["K1", "K2"]));
  regenConf(seed);
  write(seed, FEATURES, featuresText(["Base"]));
  write(seed, SCRIPT, "void Foo()\n{\n\tint x = 1;\n}\n");
  commit(seed, "base");
  must(seed, ["push", "-q", "origin", "main"]);

  const branch = (name, fn) => {
    must(seed, ["checkout", "-q", "-b", name, "main"]);
    fn();
    commit(seed, name);
    must(seed, ["push", "-q", "origin", name]);
  };
  branch("ws/gen-only", () => addFeature(seed, "KB"));
  branch("ws/real", () => {
    addFeature(seed, "KR");
    write(seed, SCRIPT, "void Foo()\n{\n\tint x = 2;\n}\n");
  });
  branch("ws/lost-key", () => addFeature(seed, "KD"));
  branch("ws/lost-row", () => addFeature(seed, "KE"));
  branch("ws/own-delete", () => {
    write(seed, ST, stText(["K1"])); // deliberately removes K2
    regenConf(seed);
  });
  must(seed, ["checkout", "-q", "main"]);
  addFeature(seed, "KC");
  write(seed, SCRIPT, "void Foo()\n{\n\tint x = 3;\n}\n");
  commit(seed, "main moves");
  must(seed, ["push", "-q", "origin", "main"]);
  must(seed, ["fetch", "-q", "origin"]);

  const wtOf = (name) => {
    const dir = join(temp, name.replace("/", "-"));
    must(seed, ["worktree", "add", "-q", dir, name]);
    return dir;
  };
  const remote = (name) => must(seed, ["ls-remote", origin, `refs/heads/${name}`]);

  // Generated-only conflict heals; keys and rows are exactly main's plus the branch's.
  {
    const dir = wtOf("ws/gen-only");
    const r = healMerge(dir, { regenerate: regenWith(null) });
    if (r.status !== "healed") fail(`gen-only: expected healed, got ${r.status} (${r.reason ?? ""})`);
    else {
      const keys = idsOf(readFileSync(join(dir, ST), "utf8")).sort().join(",");
      if (keys !== "K1,K2,KB,KC") fail(`gen-only: merged keys ${keys}, expected K1,K2,KB,KC`);
      const feat = readFileSync(join(dir, FEATURES), "utf8");
      for (const row of ["Base", "RowKB", "RowKC"]) if (!feat.includes(`| ${row} |`)) fail(`gen-only: features.md lost ${row}`);
      if (/<{7}|>{7}/.test(readFileSync(join(dir, CONF), "utf8"))) fail("gen-only: conf has conflict markers");
      if (git(dir, ["status", "--porcelain"]).stdout.trim()) fail("gen-only: worktree dirty");
    }
  }

  // A branch's own removal is not a mismatch.
  {
    const dir = wtOf("ws/own-delete");
    const r = healMerge(dir, { regenerate: regenWith(null) });
    if (r.status !== "healed" && r.status !== "merged") fail(`own-delete: expected healed or merged, got ${r.status} (${r.reason ?? ""})`);
    else {
      const keys = idsOf(readFileSync(join(dir, ST), "utf8")).sort().join(",");
      if (keys !== "K1,KC") fail(`own-delete: merged keys ${keys}, expected K1,KC`);
    }
  }

  // A real conflict is skipped untouched.
  {
    const dir = wtOf("ws/real");
    const before = must(dir, ["rev-parse", "HEAD"]);
    const remoteBefore = remote("ws/real");
    const r = healMerge(dir, { regenerate: regenWith(null) });
    if (r.status !== "needs-human") fail(`real: expected needs-human, got ${r.status}`);
    if (!r.conflicted.includes(SCRIPT)) fail(`real: conflicted ${r.conflicted.join(", ")} misses ${SCRIPT}`);
    if (r.conflicted.includes(CONF)) fail("real: reported the generated conf as needing a human");
    if (must(dir, ["rev-parse", "HEAD"]) !== before) fail("real: HEAD moved");
    if (git(dir, ["status", "--porcelain"]).stdout.trim()) fail("real: worktree left dirty");
    if (remote("ws/real") !== remoteBefore) fail("real: origin changed");
  }

  // A merge that lost a key or a row is refused and undone.
  const refusals = [
    ["ws/lost-key", "key count", (dir) => write(dir, ST, stText(idsOf(readFileSync(join(dir, ST), "utf8")).filter((k) => k !== "KC")))],
    ["ws/lost-row", "line(s) from origin/main missing", (dir) => write(dir, FEATURES, readFileSync(join(dir, FEATURES), "utf8").replace(/^\| RowKC .*\n/m, ""))],
  ];
  for (const [name, expectText, tamper] of refusals) {
    const dir = wtOf(name);
    const before = must(dir, ["rev-parse", "HEAD"]);
    const remoteBefore = remote(name);
    const label = name.slice(3);
    const r = healMerge(dir, { regenerate: regenWith(tamper) });
    if (r.status !== "refused") fail(`${label}: expected refused, got ${r.status}`);
    else if (!String(r.reason).includes(expectText)) fail(`${label}: reason lacks "${expectText}": ${r.reason}`);
    if (must(dir, ["rev-parse", "HEAD"]) !== before) fail(`${label}: HEAD not reset to the pre-merge commit`);
    if (git(dir, ["status", "--porcelain"]).stdout.trim()) fail(`${label}: worktree dirty after refusing`);
    if (remote(name) !== remoteBefore) fail(`${label}: origin changed`);
  }
} catch (e) {
  fail(`scenario threw: ${e.stack || e.message}`);
} finally {
  safeRemoveTree(temp);
}

if (failures.length) {
  console.log("prs-heal-safety-selftest: FAIL");
  for (const f of failures) console.log(`  - ${f}`);
  process.exit(1);
}
console.log("prs-heal-safety-selftest: OK (generated-only conflict healed with exact keys and rows; branch's own removal kept; real conflict skipped untouched; lost key and lost row refused and undone)");
