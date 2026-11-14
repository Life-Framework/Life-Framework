// Proves the test pass cache key (tools/lib/test-cache.mjs) on a throwaway git repo: every input
// that can change a result changes the key, the same tree in a second worktree gives the same key,
// and the store keeps passes only and survives a corrupt file. Does not boot the game.
// inputs: tools/lib/test-cache.mjs, tools/validation/test-cache-selftest.mjs
//
// red-proof: 2026-11-14, each break run through LF_TEST_CACHE_MODULE=<broken copy>:
//   `git add -A` removed from treeHashes -> FAIL on every file case (script byte, .meta, untracked file, deletion);
//   `only` dropped from keyInputs        -> FAIL "a different --only list";
//   `tier` dropped                       -> FAIL "a different tier";
//   `build` dropped                      -> FAIL "a different server build";
//   `rdb` dropped                        -> FAIL "a changed resourceDatabase.rdb";
//   "tools/test" dropped from KEY_PATHS  -> FAIL "a changed test-e2e.ps1";
//   lookup accepting code != 0           -> FAIL "a stored timeout (code 124) is never a hit".
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readdirSync, writeFileSync, appendFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL, fileURLToPath } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const modPath = process.env.LF_TEST_CACHE_MODULE || join(here, "..", "lib", "test-cache.mjs");
const tc = await import(pathToFileURL(modPath).href);

const failures = [];
const git = (cwd, args) => {
  const r = spawnSync("git", args, { cwd, encoding: "utf8", windowsHide: true });
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${r.stderr}`);
  return r.stdout.trim();
};
const write = (root, rel, text) => {
  const f = join(root, rel);
  mkdirSync(join(f, ".."), { recursive: true });
  writeFileSync(f, text);
};

const base = mkdtempSync(join(tmpdir(), "lf-test-cache-"));
const repo = join(base, "main");
const other = join(base, "other");
try {
  mkdirSync(repo);
  git(repo, ["init", "-q", "-b", "main"]);
  git(repo, ["config", "user.email", "selftest@localhost"]);
  git(repo, ["config", "user.name", "selftest"]);
  git(repo, ["config", "core.autocrlf", "false"]);
  write(repo, ".gitignore", "*.rdb\ntmp/\n");
  write(repo, "addons/LifeFramework/Scripts/Game/LF_A.c", "class LF_A {}\n");
  write(repo, "addons/LifeFramework/Scripts/Game/LF_A.c.meta", "MetaFileClass { Name \"{1111111111111111}LF_A.c\" }\n");
  write(repo, "tools/test/test-e2e.ps1", "exit 0\n");
  write(repo, "tools/lib/addons.mjs", "export {};\n");
  write(repo, "docs/readme.md", "docs\n");
  git(repo, ["add", "-A"]);
  git(repo, ["commit", "-q", "-m", "init"]);
  write(repo, "addons/LifeFramework/resourceDatabase.rdb", "rdb-1");

  const sel = { tier: "all", only: [], world: null, coreOnly: false, build: "100", addons: ["LifeFramework"] };
  const keyOf = (root, over = {}) => tc.cacheKey(tc.keyInputs(root, { ...sel, ...over }));
  const k0 = keyOf(repo);
  if (keyOf(repo) !== k0) failures.push("the key is not stable on an unchanged tree");

  // Each change must move the key, and undoing it must bring the key back.
  const cases = [
    ["one changed byte in a script", "addons/LifeFramework/Scripts/Game/LF_A.c", "class LF_A {}\n", "class LF_B {}\n"],
    ["a changed .meta", "addons/LifeFramework/Scripts/Game/LF_A.c.meta", "MetaFileClass { Name \"{1111111111111111}LF_A.c\" }\n", "MetaFileClass { Name \"{2222222222222222}LF_A.c\" }\n"],
    ["a changed test-e2e.ps1", "tools/test/test-e2e.ps1", "exit 0\n", "exit 1\n"],
    ["a changed resourceDatabase.rdb", "addons/LifeFramework/resourceDatabase.rdb", "rdb-1", "rdb-2"],
  ];
  for (const [name, rel, before, after] of cases) {
    write(repo, rel, after);
    if (keyOf(repo) === k0) failures.push(`${name} did not change the key`);
    write(repo, rel, before);
    if (keyOf(repo) !== k0) failures.push(`${name}: restoring it did not restore the key`);
  }
  write(repo, "addons/LifeFramework/Scripts/Game/LF_New.c", "class LF_New {}\n");
  const kNew = keyOf(repo);
  if (kNew === k0) failures.push("a new untracked file did not change the key");
  write(repo, "addons/LifeFramework/Scripts/Game/LF_New.c.meta", "meta\n");
  if (keyOf(repo) === kNew) failures.push("an untracked .meta did not change the key");
  rmSync(join(repo, "addons/LifeFramework/Scripts/Game/LF_New.c"));
  rmSync(join(repo, "addons/LifeFramework/Scripts/Game/LF_New.c.meta"));
  if (keyOf(repo) !== k0) failures.push("removing the untracked files did not restore the key");
  rmSync(join(repo, "addons/LifeFramework/Scripts/Game/LF_A.c"));
  if (keyOf(repo) === k0) failures.push("a deleted tracked script did not change the key");
  git(repo, ["checkout", "--", "addons/LifeFramework/Scripts/Game/LF_A.c"]);
  write(repo, "docs/readme.md", "docs changed\n");
  if (keyOf(repo) !== k0) failures.push("a docs change outside the key paths changed the key");
  write(repo, "docs/readme.md", "docs\n");

  const sels = [
    ["a different tier", { tier: "fast" }],
    ["a different --only list", { only: ["LF_Test_A"] }],
    ["a different --only order", { only: ["b", "a"] }, { only: ["a", "b"] }],
    ["a different server build", { build: "101" }],
    ["--core-only", { coreOnly: true }],
    ["a different --world", { world: "Worlds/X/X.ent" }],
    ["a different addon list", { addons: ["LifeFramework", "LF_Everon1989"] }],
  ];
  const fixed = { trees: tc.treeHashes(repo), rdb: tc.rdbHashes(repo, sel.addons) };
  for (const [name, over, against] of sels) {
    if (keyOf(repo, { ...fixed, ...over }) === keyOf(repo, { ...fixed, ...(against ?? {}) })) failures.push(`${name} did not change the key`);
  }

  // The real index must not be touched by the key.
  if (git(repo, ["status", "--porcelain"]) !== "") failures.push("computing the key changed the real index or tree");

  // The same tree in a second worktree, with the same untracked file, gives the same key.
  git(repo, ["worktree", "add", "-q", other, "HEAD"]);
  write(repo, "addons/LifeFramework/Scripts/Game/LF_Untracked.c", "x\n");
  write(other, "addons/LifeFramework/Scripts/Game/LF_Untracked.c", "x\n");
  write(other, "addons/LifeFramework/resourceDatabase.rdb", "rdb-1");
  if (keyOf(repo) !== keyOf(other)) failures.push("an identical tree in a second worktree gave a different key");

  // Store and lookup: passes only, atomic, corrupt files are a miss.
  const cacheRoot = join(base, "cache-main");
  const kPass = keyOf(repo);
  if (tc.lookup(cacheRoot, kPass) !== null) failures.push("an empty cache returned a hit");
  if (tc.store(cacheRoot, "k-fail", { code: 1, failed: 2 })) failures.push("a failure was stored");
  writeFileSync(join(base, "x"), "");
  mkdirSync(tc.cacheDir(cacheRoot), { recursive: true });
  writeFileSync(join(tc.cacheDir(cacheRoot), "k-bad.json"), JSON.stringify({ key: "k-bad", code: 124, failed: 0 }));
  if (tc.lookup(cacheRoot, "k-bad") !== null) failures.push("a stored timeout (code 124) is never a hit");
  if (!tc.store(cacheRoot, kPass, { code: 0, failed: 0, passed: 5, total: 5, at: "t", tier: "all" })) failures.push("a pass was not stored");
  const hit = tc.lookup(cacheRoot, kPass);
  if (!hit || hit.passed !== 5) failures.push("a stored pass was not found");
  if (readdirSync(tc.cacheDir(cacheRoot)).some((n) => n.endsWith(".tmp"))) failures.push("store left a temp file behind");
  appendFileSync(join(tc.cacheDir(cacheRoot), `${kPass}.json`), "garbage{");
  if (tc.lookup(cacheRoot, kPass) !== null) failures.push("a corrupt cache file was a hit");
  if (!/CACHED pass/.test(tc.reuseLine({ ...hit, key: kPass, machine: "pc", worktree: "w" }))) failures.push("reuseLine lost its marker");
  if (tc.cachedSummary(hit, "abc").cached !== true) failures.push("the reused summary is not marked cached");

  // Runs that never use the cache.
  const skips = [
    [{ tier: "persistence" }, "persistence tier"],
    [{ tier: "all", opts: { noCache: true } }, "--no-cache"],
    [{ tier: "all", opts: { red: true } }, "--red"],
    [{ tier: "all", opts: { dev: true } }, "dev"],
    [{ tier: "all", env: { LF_TEST_CACHE: "0" } }, "LF_TEST_CACHE=0"],
    [{ tier: "all", env: { LF_PLACE_AUDIT_IN: "x" } }, "placement audit"],
  ];
  for (const [arg, name] of skips) if (!tc.skipReason({ env: {}, ...arg })) failures.push(`${name} did not turn the cache off`);
  if (tc.skipReason({ tier: "all", env: {} })) failures.push("a plain run turned the cache off");
} catch (e) {
  failures.push(`threw: ${e.message}`);
} finally {
  // A throwaway repo of plain files under the OS temp dir (no junctions): the whole base goes.
  rmSync(base, { recursive: true, force: true });
}

if (failures.length) {
  console.log("test-cache-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("test-cache-selftest: OK (script byte, untracked file, .meta, rdb, test-e2e.ps1, tier, --only, build, core-only, world, addons each move the key; second worktree matches; passes only)");
