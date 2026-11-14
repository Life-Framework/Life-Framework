// tools/release/release.mjs - `tools\cli release <version>` (#369), the human step that
// turns a verified commit into a tagged Workshop release candidate.
//
//   tools\cli release 0.2.0                  dry run (the default): checks, plan, changes nothing
//   tools\cli release 0.2.0 --dry-run        the same, spelled out
//   tools\cli release 0.2.0 --confirm        stamp, gate, tag locally
//   tools\cli release 0.2.0 --confirm --push also push the tag (refs/tags/v0.2.0 only)
//
// It never publishes to the Workshop (that is #370, by hand in Workbench) and never pushes
// without --push. Agents run it with the default dry run only.
//
// The release commit is built with plumbing (write-tree, commit-tree) on top of HEAD and
// reachable only through the tag, so no branch moves; the working tree returns to HEAD.
// Gates run against the stamped tree, so a failed gate leaves nothing behind.
// red-proof: tools/validation/release-cli-selftest.mjs.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { join } from "node:path";

export const ADDON_NAMES = ["LifeFramework", "LF_Everon1989"];
export const KIT_READMES = ["docs/marketing/workshop/README.md", "docs/marketing/workshop/everon-1989/README.md"];
export const SEMVER = /^\d+\.\d+\.\d+$/;

function git(root, args) {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8", windowsHide: true, maxBuffer: 1 << 26 });
  return { code: r.status ?? 1, out: (r.stdout ?? "").trim(), err: (r.stderr ?? "").trim() };
}

// The files this release stamps, relative to the repo root, that exist in this tree.
export function stampFiles(root) {
  const files = ADDON_NAMES.map((n) => `addons/${n}/${n}.gproj`).concat(KIT_READMES);
  return files.filter((f) => existsSync(join(root, f)));
}

// A .gproj gets one top-level `Version "x"` line right after TITLE (replaced when present).
export function stampGproj(text, version) {
  const line = `Version "${version}"`;
  if (/^\s*Version\s+"[^"]*"/m.test(text)) return text.replace(/^(\s*)Version\s+"[^"]*"/m, `$1${line}`);
  if (/^\s*TITLE\s+"[^"]*"/m.test(text)) return text.replace(/^(\s*TITLE\s+"[^"]*")/m, `$1\n ${line}`);
  return null;
}

export function stampReadme(text, version) {
  const re = /^(\|\s*Version\s*\|\s*)`?\d+\.\d+\.\d+`?(\s*\|)/m;
  return re.test(text) ? text.replace(re, `$1\`${version}\`$2`) : null;
}

// Preflight problems; an empty list means the tree may be released.
export function preflight(root, version, g = git) {
  const problems = [];
  if (!SEMVER.test(version ?? "")) problems.push(`version "${version ?? ""}" is not x.y.z`);
  const status = g(root, ["status", "--porcelain"]);
  if (status.code !== 0) problems.push(`not a git repository: ${status.err}`);
  else if (status.out) problems.push(`working tree is dirty (${status.out.split("\n").length} path(s)); commit or stash first`);
  const branch = g(root, ["rev-parse", "--abbrev-ref", "HEAD"]).out;
  if (branch !== "main" && branch !== "stable") {
    // A worktree cannot check out a branch another worktree holds, so a detached HEAD sitting
    // exactly on origin/main or origin/stable counts.
    const head = g(root, ["rev-parse", "HEAD"]).out;
    const onTip = ["origin/main", "origin/stable"].some((r) => {
      const x = g(root, ["rev-parse", "--verify", "-q", r]);
      return x.code === 0 && x.out === head;
    });
    if (!onTip) problems.push(`on "${branch}"; release runs from main or stable (or a detached HEAD at origin/main or origin/stable)`);
  }
  if (SEMVER.test(version ?? "") && g(root, ["rev-parse", "--verify", "-q", `refs/tags/v${version}`]).code === 0) {
    problems.push(`tag v${version} already exists`);
  }
  return problems;
}

// Merged PR titles since the previous v* tag, newest first. Merge commits carry the PR title in the body.
export function releaseNotes(root, version, g = git) {
  const tags = g(root, ["tag", "--list", "v*", "--sort=-v:refname"]).out.split("\n").filter((t) => /^v\d+\.\d+\.\d+$/.test(t));
  const prev = tags.find((t) => t !== `v${version}`) ?? null;
  const range = prev ? [`${prev}..HEAD`] : ["HEAD"];
  const RS = String.fromCharCode(30);
  const US = String.fromCharCode(31);
  const log = g(root, ["log", "--first-parent", "--merges", `--format=%s%x1f%b%x1e`, ...range]).out;
  const items = [];
  for (const rec of log.split(RS)) {
    const [subject, body] = rec.trim().split(US);
    if (!subject) continue;
    const pr = (subject.match(/#(\d+)/) ?? [])[1];
    const title = (body ?? "").split("\n").find((l) => l.trim()) ?? subject;
    items.push(`- ${title.trim()}${pr ? ` (#${pr})` : ""}`);
  }
  const head = `# Life Framework ${version}\n\n${prev ? `Merged since ${prev}:` : "Merged to date:"}\n\n`;
  return head + (items.length ? items.join("\n") : "- (no merged PRs)") + "\n";
}

// The gate list, in order: [label, node args run from the repo root].
export function gateSteps() {
  return [
    ["validate --full", ["tools/cli.mjs", "validate", "--full"]],
    ["release-ready --strict", ["tools/validation/validate-release-ready.mjs", "--strict"]],
    ["full suite", ["tools/cli.mjs", "test", "--no-build", "--tier", "all"]],
    ["config harness (static + sessions)", ["tools/cli.mjs", "config-gate", "--full"]],
    ["audit placement --strict", ["tools/cli.mjs", "audit", "placement", "--strict"]],
    ["pack LifeFramework", ["tools/cli.mjs", "pack", "LifeFramework", "--wait"]],
    ["pack LF_Everon1989", ["tools/cli.mjs", "pack", "LF_Everon1989", "--wait"]],
  ];
}

function realRunner(root) {
  return (label, args) => spawnSync(process.execPath, args, { cwd: root, stdio: "inherit", windowsHide: true }).status ?? 1;
}

function restore(root, files, g) {
  g(root, ["checkout", "HEAD", "--", ...files]);
}

const IDENT = ["-c", "user.name=release", "-c", "user.email=release@localhost"];

export async function runRelease(root, args, opts = {}) {
  const log = opts.log ?? ((s) => console.log(s));
  const g = opts.git ?? git;
  const run = opts.runner ?? realRunner(root);
  const version = args.find((a) => !a.startsWith("--"));
  const confirm = args.includes("--confirm") && !args.includes("--dry-run");
  const push = args.includes("--push");
  if (!version) {
    log("usage: tools\\cli release <x.y.z> [--dry-run | --confirm [--push]]");
    return 1;
  }
  if (push && !confirm) {
    log("FAIL  --push needs --confirm; a dry run changes nothing");
    return 1;
  }

  const problems = preflight(root, version, g);
  const files = stampFiles(root);
  const steps = gateSteps();
  const mode = confirm ? `REAL RUN (tags locally${push ? ", then pushes the tag" : ", no push"})` : "dry run, nothing changes";
  log(`release ${version}: ${mode}`);
  log("plan:");
  log(`  1. preflight: clean tree, on main/stable, tag v${version} free`);
  log(`  2. stamp version ${version} in ${files.join(", ")}`);
  steps.forEach(([name], i) => log(`  ${i + 3}. gate: ${name}`));
  log(`  ${steps.length + 3}. write release notes to tmp/release/notes-${version}.md`);
  log(`  ${steps.length + 4}. tag v${version} on a release commit (no branch moves)`);
  log(`  ${steps.length + 5}. ${push ? `push refs/tags/v${version}` : "no push (add --push); the Workshop upload is by hand (#370)"}`);

  if (problems.length) {
    for (const p of problems) log(`${confirm ? "FAIL" : "WOULD REFUSE"}  ${p}`);
    return 1;
  }
  if (!confirm) {
    // The cheap gate runs even on a dry run, so it reports what a real run would hit.
    const code = run("release-ready --strict (dry-run check)", steps[1][1]);
    log(code === 0 ? "dry run OK: preflight clean and release-ready passes; heavy gates run only with --confirm" : "dry run: release-ready --strict fails; fix it before --confirm");
    return code === 0 ? 0 : 1;
  }

  for (const f of files) {
    const p = join(root, f);
    const before = readFileSync(p, "utf8");
    const after = f.endsWith(".gproj") ? stampGproj(before, version) : stampReadme(before, version);
    if (after === null) {
      log(`FAIL  cannot stamp ${f}: no ${f.endsWith(".gproj") ? "TITLE line" : "Version row"}`);
      restore(root, files, g);
      return 1;
    }
    writeFileSync(p, after);
  }
  for (const [name, a] of steps) {
    log(`=== release gate: ${name} ===`);
    if (run(name, a) !== 0) {
      log(`FAIL  gate "${name}"; nothing tagged, stamped files restored`);
      restore(root, files, g);
      return 1;
    }
  }

  const notes = releaseNotes(root, version, g);
  const notesDir = join(root, "tmp", "release");
  mkdirSync(notesDir, { recursive: true });
  const notesFile = join(notesDir, `notes-${version}.md`);
  writeFileSync(notesFile, notes);
  g(root, ["add", "--", ...files]);
  const tree = g(root, ["write-tree"]);
  const commit = tree.code === 0 ? g(root, [...IDENT, "commit-tree", tree.out, "-p", "HEAD", "-m", `release: ${version}`]) : tree;
  const tag = commit.code === 0 ? g(root, [...IDENT, "tag", "-a", `v${version}`, commit.out, "-m", `Life Framework ${version}`]) : commit;
  restore(root, files, g);
  if (tag.code !== 0) {
    log(`FAIL  could not tag: ${tag.err}`);
    return 1;
  }
  log(`tagged v${version} at ${commit.out.slice(0, 8)}; notes: ${notesFile}`);
  if (push) {
    const p = g(root, ["push", "origin", `refs/tags/v${version}`]);
    if (p.code !== 0) {
      log(`FAIL  push: ${p.err}`);
      return 1;
    }
    log(`pushed refs/tags/v${version}`);
  } else {
    log(`not pushed. To push: git push origin refs/tags/v${version}`);
  }
  log("next: upload to the Workshop by hand in Workbench (#370). This tool never publishes.");
  return 0;
}
