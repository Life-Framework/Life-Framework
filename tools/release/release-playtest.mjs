// tools/release/release-playtest.mjs - `tools\cli playtest release [--tag playtest-m2-2]`.
//
// Cuts a playtest download from this worktree's HEAD, which must be origin/main:
//   1. refreshes resourceDatabase.rdb (headless Workbench; only it writes the file,
//      and the game refuses every resource it does not register),
//   2. creates the tag's GitHub release as a pre-release with that rdb attached,
//   3. starts .github/workflows/playtest-release.yml for the tag, which packs
//      LifeFramework-Playtest.zip from a clean checkout and makes the release latest.
import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { describeMissing, rdbFreshness } from "../lib/rdb-fresh.mjs";
import { addonDir, rdbAddons } from "../lib/addons.mjs";
import { REPO } from "./playtest-server.mjs";

const TAG_RE = /^playtest-m(\d+)-(\d+)$/;

function run(cmd, args, cwd) {
  const r = spawnSync(cmd, args, { cwd, encoding: "utf8" });
  return { ok: r.status === 0, out: (r.stdout || "").trim(), err: (r.stderr || "").trim() };
}

// The tag after the newest playtest-m<M>-<n> (same milestone, n + 1), or playtest-m2-1.
export function nextTag(tags) {
  const parsed = tags.map((t) => TAG_RE.exec(t)).filter(Boolean).map((m) => ({ m: Number(m[1]), n: Number(m[2]) }));
  if (!parsed.length) return "playtest-m2-1";
  parsed.sort((a, b) => a.m - b.m || a.n - b.n);
  const last = parsed[parsed.length - 1];
  return `playtest-m${last.m}-${last.n + 1}`;
}

export function releaseName(tag) {
  const m = TAG_RE.exec(tag);
  return m ? `Playtest M${m[1]}, build ${m[2]}` : `Playtest ${tag}`;
}

export function cmdPlaytestRelease(root, args, { ensureRdb, isMain }) {
  if (isMain) {
    console.log("FAIL  playtest release runs in a worktree at origin/main (tools\\cli wt new playtest-release), not the main checkout");
    return 1;
  }
  const ti = args.indexOf("--tag");
  run("git", ["fetch", "--quiet", "--tags", "origin", "main"], root);
  const head = run("git", ["rev-parse", "HEAD"], root).out;
  const main = run("git", ["rev-parse", "origin/main"], root).out;
  if (head !== main && !args.includes("--any-commit")) {
    console.log(`FAIL  HEAD ${head.slice(0, 8)} is not origin/main ${main.slice(0, 8)}; check out origin/main (or pass --any-commit)`);
    return 1;
  }
  if (run("git", ["status", "--porcelain", "--untracked-files=no"], root).out) {
    console.log("FAIL  the worktree has uncommitted changes; the release is built from the commit, not the files");
    return 1;
  }
  const tags = run("git", ["tag", "-l", "playtest-*"], root).out.split(/\r?\n/).filter(Boolean);
  const tag = ti >= 0 ? args[ti + 1] : nextTag(tags);
  if (!/^playtest-[A-Za-z0-9.-]+$/.test(tag || "")) {
    console.log(`FAIL  "${tag}" is not a playtest-* tag`);
    return 1;
  }
  if (tags.includes(tag)) {
    console.log(`FAIL  ${tag} exists; pass --tag with a new one`);
    return 1;
  }

  console.log(`playtest release: ${tag} (${releaseName(tag)}) from ${head.slice(0, 8)}`);
  ensureRdb(root);
  const addon = join(root, "addons", "LifeFramework");
  const fresh = rdbFreshness(addon);
  if (!fresh.exists || fresh.missing.length) {
    console.log(`FAIL  resourceDatabase.rdb is not fresh (${fresh.exists ? describeMissing(fresh.missing) : "missing"}); the zip would not load`);
    return 1;
  }
  console.log(`  rdb: ${fresh.total} resources registered`);
  // Any other addon that declares resources must have a fresh rdb too (the zip and the attached rdb are the core's).
  for (const name of rdbAddons(root).slice(1)) {
    const other = rdbFreshness(addonDir(root, name));
    if (!other.exists || other.missing.length) {
      console.log(`FAIL  ${name} resourceDatabase.rdb is not fresh (${other.exists ? describeMissing(other.missing) : "missing"})`);
      return 1;
    }
    console.log(`  rdb [${name}]: ${other.total} resources registered`);
  }

  const notes = "Packing the download; this release becomes the latest when the playtest-release workflow finishes.";
  const create = run("gh", ["release", "create", tag, fresh.rdbPath, "--repo", REPO, "--target", head, "--title", releaseName(tag), "--notes", notes, "--prerelease"], root);
  if (!create.ok) {
    console.log(`FAIL  gh release create: ${create.err}`);
    return 1;
  }
  console.log(`  release ${create.out} (pre-release, rdb attached)`);
  const wf = run("gh", ["workflow", "run", "playtest-release.yml", "--repo", REPO, "--ref", "main", "-f", `tag=${tag}`], root);
  if (!wf.ok) {
    console.log(`FAIL  gh workflow run: ${wf.err}; start it by hand: gh workflow run playtest-release.yml -f tag=${tag}`);
    return 1;
  }
  console.log("  packing on GitHub: gh run list --workflow playtest-release.yml --limit 1");
  console.log(`  players get it from https://github.com/${REPO}/releases/latest/download/LifeFramework-Playtest.zip once it finishes`);
  return 0;
}

export function selftestRelease() {
  const failures = [];
  if (nextTag([]) !== "playtest-m2-1") failures.push("first tag is not playtest-m2-1");
  if (nextTag(["playtest-m2-1", "playtest-m2-10", "playtest-m2-9"]) !== "playtest-m2-11") failures.push("next tag does not sort numerically");
  if (nextTag(["playtest-m2-4", "playtest-m3-1", "playtest-probe"]) !== "playtest-m3-2") failures.push("next tag ignores the newest milestone");
  if (releaseName("playtest-m2-3") !== "Playtest M2, build 3") failures.push("release name");
  if (releaseName("playtest-x") !== "Playtest playtest-x") failures.push("odd tag release name");
  return failures;
}
