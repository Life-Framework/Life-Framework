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
// Speed (2026-11-27): the five scenes share nothing, so each is a section run in its own process in parallel
// (tools/lib/selftest-sections.mjs). A scene's origin is built with one `git fast-import` (the same base, five branches
// and main's move as before, tools/lib/selftest-git.mjs FastStream) and cloned onto the branch under test, instead of a
// seed checkout with a commit per branch and a worktree add per scene. Every assertion still runs once.
//
// red-proof (2026-11-27, the sectioned file): the expected keys of ws/gen-only inverted printed "gen-only: merged keys ..." and "section 'ws/gen-only' exited 1"; restored.
// red-proof (2026-11-14): with safetyCheck stubbed to { ok: true } lost-key and lost-row printed
//   "expected refused, got healed".
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { createHash } from "node:crypto";
import { gitSync } from "../lib/git-retry.mjs";
import { healMerge } from "../prs.mjs";
import { describeRun, FastStream, fastImport, removeScratch, runGit, useHermeticGit } from "../lib/selftest-git.mjs";
import { sections } from "../lib/selftest-sections.mjs";
import { scratchDir } from "../lib/scratch.mjs";

useHermeticGit(); // no machine git config, no auto-gc beside the scratch repos (tools/lib/selftest-git.mjs)

let failed = 0;
const fail = (m) => {
  failed++;
  console.log(`FAIL  ${m}`);
};
const ST = "addons/LifeFramework/Language/everonlife_localization.st";
const CONF = "addons/LifeFramework/Language/everonlife_localization.en_us.conf";
const FEATURES = "docs/features.md";
const SCRIPT = "addons/LifeFramework/Scripts/Game/Core/LF_Foo.c";

function git(dir, args) {
  return gitSync(args, { cwd: dir, windowsHide: true, encoding: "utf8" });
}
function must(dir, args) {
  const r = runGit(dir, args); // a probe through git() may fail quietly on purpose; a must() may not
  if (r.status !== 0) throw new Error(`git ${args.join(" ")} in ${dir}: ${describeRun(r)}`);
  return r.stdout.trim();
}
function write(dir, rel, text) {
  mkdirSync(dirname(join(dir, rel)), { recursive: true });
  writeFileSync(join(dir, rel), text);
}

const guid = (k) => createHash("sha1").update(k).digest("hex").slice(0, 16).toUpperCase();
const item = (k) => `  CustomStringTableItem "{${guid(k)}}" {\n   Id "${k}"\n   Target_en_us "${k} text"\n  }`;
const stText = (keys) => `StringTable {\n ItemClassName "CustomStringTableItem"\n Items {\n${keys.map(item).join("\n")}\n }\n}\n`;
const idsOf = (text) => [...text.matchAll(/^\s+Id "([^"]+)"/gm)].map((m) => m[1]);
const featuresText = (rows) => `# Features\n\n| Feature | Does |\n|---|---|\n${rows.map((r) => `| ${r} | does ${r} |`).join("\n")}\n`;
// Stand-in for regen-localization: the conf is one line per string id, in table order.
const confText = (keys) => keys.map((k) => `${k}=${k} text`).join("\n") + "\n";
const scriptText = (x) => `void Foo()\n{\n\tint x = ${x};\n}\n`;
function regenConf(dir) {
  write(dir, CONF, confText(idsOf(readFileSync(join(dir, ST), "utf8"))));
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

// The files of a commit that appends a key, its conf line and a features row to `base` (as a feature PR does).
const base = { keys: ["K1", "K2"], rows: ["Base"], script: 1 };
const filesOf = (s) => ({ [ST]: stText(s.keys), [CONF]: confText(s.keys), [FEATURES]: featuresText(s.rows), [SCRIPT]: scriptText(s.script) });
const withFeature = (s, key, script = s.script) => ({ keys: [...s.keys, key], rows: [...s.rows, `Row${key}`], script });

// A scene's repos: the bare origin with main = base + "main moves" and every ws/* branch from the base, and a clone
// of it with `name` checked out (the branch under test). The branch commit adds nothing but its own change.
function sceneRepos(temp, name) {
  const origin = join(temp, "origin.git");
  const dir = join(temp, name.replace("/", "-"));
  // Settings by environment, not `git config` (five processes) and not repo config: every git this test and
  // healMerge start inherits them.
  const settings = [["user.email", "heal@example.com"], ["user.name", "heal"], ["core.autocrlf", "false"], ["commit.gpgsign", "false"], ["core.hooksPath", join(temp, "no-hooks")]];
  process.env.GIT_CONFIG_COUNT = String(settings.length);
  settings.forEach(([k, v], i) => {
    process.env[`GIT_CONFIG_KEY_${i}`] = k;
    process.env[`GIT_CONFIG_VALUE_${i}`] = v;
  });
  must(temp, ["init", "-q", "--bare", "-b", "main", origin]);
  const fs = new FastStream();
  fs.commit("refs/heads/main", { mark: 1, message: "base", files: filesOf(base) });
  const branches = {
    "ws/gen-only": filesOf(withFeature(base, "KB")),
    "ws/real": filesOf(withFeature(base, "KR", 2)),
    "ws/lost-key": filesOf(withFeature(base, "KD")),
    "ws/lost-row": filesOf(withFeature(base, "KE")),
    "ws/own-delete": filesOf({ ...base, keys: ["K1"] }), // deliberately removes K2
  };
  let mark = 1;
  for (const [b, files] of Object.entries(branches)) fs.commit(`refs/heads/${b}`, { mark: ++mark, from: 1, message: b, files });
  fs.commit("refs/heads/main", { mark: ++mark, from: 1, message: "main moves", files: filesOf(withFeature(base, "KC", 3)) });
  fastImport(origin, fs);
  must(temp, ["clone", "-q", "-b", name, origin, dir]);
  // The bare origin's branch tip, read from its ref file (fast-import writes it loose): no ls-remote process.
  const remote = (n) => readFileSync(join(origin, "refs", "heads", ...n.split("/")), "utf8").trim();
  return { dir, remote };
}

const S = sections(import.meta.url);
const scene = (name, body) => {
  S.add(name, async () => {
    const temp = scratchDir("lf-heal-safety-");
    try {
      await body(sceneRepos(temp, name));
    } catch (e) {
      fail(`scenario threw in '${name}': ${e.stack || e.message}`);
    } finally {
      removeScratch(temp);
    }
  });
};

// Generated-only conflict heals; keys and rows are exactly main's plus the branch's.
scene("ws/gen-only", ({ dir }) => {
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
});

// A branch's own removal is not a mismatch.
scene("ws/own-delete", ({ dir }) => {
  const r = healMerge(dir, { regenerate: regenWith(null) });
  if (r.status !== "healed" && r.status !== "merged") fail(`own-delete: expected healed or merged, got ${r.status} (${r.reason ?? ""})`);
  else {
    const keys = idsOf(readFileSync(join(dir, ST), "utf8")).sort().join(",");
    if (keys !== "K1,KC") fail(`own-delete: merged keys ${keys}, expected K1,KC`);
  }
});

// A real conflict is skipped untouched.
scene("ws/real", ({ dir, remote }) => {
  const before = must(dir, ["rev-parse", "HEAD"]);
  const remoteBefore = remote("ws/real");
  const r = healMerge(dir, { regenerate: regenWith(null) });
  if (r.status !== "needs-human") fail(`real: expected needs-human, got ${r.status}`);
  if (!r.conflicted.includes(SCRIPT)) fail(`real: conflicted ${r.conflicted.join(", ")} misses ${SCRIPT}`);
  if (r.conflicted.includes(CONF)) fail("real: reported the generated conf as needing a human");
  if (must(dir, ["rev-parse", "HEAD"]) !== before) fail("real: HEAD moved");
  if (git(dir, ["status", "--porcelain"]).stdout.trim()) fail("real: worktree left dirty");
  if (remote("ws/real") !== remoteBefore) fail("real: origin changed");
});

// A merge that lost a key or a row is refused and undone.
const refusals = [
  ["ws/lost-key", "key count", (dir) => write(dir, ST, stText(idsOf(readFileSync(join(dir, ST), "utf8")).filter((k) => k !== "KC")))],
  ["ws/lost-row", "line(s) from origin/main missing", (dir) => write(dir, FEATURES, readFileSync(join(dir, FEATURES), "utf8").replace(/^\| RowKC .*\n/m, ""))],
];
for (const [name, expectText, tamper] of refusals) {
  scene(name, ({ dir, remote }) => {
    const before = must(dir, ["rev-parse", "HEAD"]);
    const remoteBefore = remote(name);
    const label = name.slice(3);
    const r = healMerge(dir, { regenerate: regenWith(tamper) });
    if (r.status !== "refused") fail(`${label}: expected refused, got ${r.status}`);
    else if (!String(r.reason).includes(expectText)) fail(`${label}: reason lacks "${expectText}": ${r.reason}`);
    if (must(dir, ["rev-parse", "HEAD"]) !== before) fail(`${label}: HEAD not reset to the pre-merge commit`);
    if (git(dir, ["status", "--porcelain"]).stdout.trim()) fail(`${label}: worktree dirty after refusing`);
    if (remote(name) !== remoteBefore) fail(`${label}: origin changed`);
  });
}

process.exitCode = await S.run({
  failed: () => failed,
  summary: (n) => (n ? "prs-heal-safety-selftest: FAIL" : "prs-heal-safety-selftest: OK (generated-only conflict healed with exact keys and rows; branch's own removal kept; real conflict skipped untouched; lost key and lost row refused and undone)"),
});
