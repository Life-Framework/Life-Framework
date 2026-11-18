// Incremental `validate`: skip a check whose declared inputs have not changed
// since its last green run in this worktree.
//
// Each check declares its inputs in a header line within its first 30 lines:
//     // inputs: addons/**/*.layer, docs/features.md      (.mjs)
//     # inputs: addons/**/*.layer, docs/features.md        (.ps1)
// Globs are repo-relative: `*` matches within a folder, `**` across folders.
// The check's own file is always an input, and so is its static import closure (plan()); `inputs: imports`
// says "no data, only my code". `inputs: always`, or no header at
// all, means the check runs every time (the safe default for a check that
// reads machine state such as `git worktree list`).
//
// State lives in <root>/tmp/validate-state.json: { checks: { name: { fp, secs } } }.
// A check's entry is written only when it passes, so a failing check is never
// skipped. The fingerprint hashes path and git blob id of every matching file
// (`git ls-files -s` for clean tracked files, a blob hash of the bytes for
// modified and untracked ones), so edits, adds, deletes and renames all change it,
// and identical content gives the identical fingerprint in every worktree.
//
// Because it is content-addressed, green fingerprints are also shared between
// worktrees in <git common dir>/lf/validate-green.json: a fresh worktree skips every
// check some other worktree already passed on the same inputs. Before this, the
// first validate of each slice ran all ~350 checks (about 6 min under load, 2026-11-17). Full runs (`validate --full`, `--all`, ci, wt gate,
// wt ship) never consult it. CI has its own green set, restored by actions/cache (#3189, ciGreenFile below).
//
// Base-clean skip (2026-11-18, efficiency pass 3): a check whose inputs match no path this branch
// changed since its merge-base with origin/main gives the result it gives on that main commit, which
// GitHub's full validate already ran (CI sets LF_VALIDATE_NO_BASE: a CI run is that validation, so it never trusts it). Before it, ~25 git-heavy
// `tools/**` selftests (40-80 s each under load) re-ran on every slice whenever main's tools/ had
// moved, slices that never touched tools/ included: 58% of all validate CPU on 2026-11-18.
// A `ttl=` check (machine state such as the worktree registry) shares its green time across
// worktrees, so it runs once per window on the machine, not once per worktree.
import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join, resolve } from "node:path";
import { writeFileAtomic } from "./atomic-file.mjs";
import { importClosure } from "./import-closure.mjs";

const STATE_VERSION = 2; // 2: content-addressed fingerprints (1 used mtimes)
const SHARED_KEEP = 12; // green fingerprints kept per check in the shared file
// Every check also depends on its baselines and fixtures beside it.
const IMPLICIT = ["tools/validation/*.json", "tools/validation/fixtures/**"];

export function parseInputs(text) {
  const head = text.split(/\r?\n/).slice(0, 30);
  for (const line of head) {
    const m = /^\s*(?:\/\/|#)\s*inputs:\s*(.+?)\s*$/.exec(line);
    if (!m) continue;
    const globs = m[1].split(",").map((g) => g.trim()).filter(Boolean);
    if (globs.length === 0 || globs.includes("always")) return null;
    return globs;
  }
  return null;
}

// `inputs: ttl=300`: machine state with no repo inputs (the worktree registry). A green run
// stands for 300 s, then the check runs again.
export function ttlOf(globs) {
  const t = globs?.find((g) => /^ttl=[0-9]+$/.test(g));
  return t ? Number(t.slice(4)) : 0;
}

export function globToRegExp(glob) {
  let re = "";
  for (let i = 0; i < glob.length; i++) {
    const c = glob[i];
    if (c === "*") {
      if (glob[i + 1] === "*") {
        i++;
        if (glob[i + 1] === "/") {
          i++;
          re += "(?:.*/)?";
        } else re += ".*";
      } else re += "[^/]*";
    } else if (c === "?") re += "[^/]";
    else re += c.replace(/[.+^${}()|[\]\\]/g, "\\$&");
  }
  return new RegExp(`^${re}$`);
}

export function blobId(buf) {
  return createHash("sha1").update(`blob ${buf.length}\0`).update(buf).digest("hex");
}

const lsz = (root, args) =>
  execFileSync("git", ["ls-files", "-z", ...args], { cwd: root, encoding: "utf8", maxBuffer: 1 << 28 }).split("\0").filter(Boolean);

// Three `git ls-files` calls for the whole run; only modified and untracked files are read.
export function snapshotFiles(root) {
  const sig = new Map();
  for (const rec of lsz(root, ["-s", "--cached"])) {
    const tab = rec.indexOf("\t");
    const id = rec.slice(0, tab).split(" ")[1];
    sig.set(rec.slice(tab + 1), id);
  }
  const dirty = new Set([...lsz(root, ["-m"]), ...lsz(root, ["--others", "--exclude-standard"])]);
  for (const rel of dirty) {
    let id = "gone";
    try {
      id = blobId(readFileSync(join(root, rel)));
    } catch {
      /* deleted in the working tree: stays in the list so a delete changes the fingerprint */
    }
    sig.set(rel, id);
  }
  return [...sig.entries()].sort((x, y) => (x[0] < y[0] ? -1 : 1));
}

// `exact`: repo paths that always count (the check's import closure), looked up without a regexp each.
export function fingerprint(files, globs, selfRel, exact = null) {
  const res = globs.map(globToRegExp);
  const h = createHash("sha1");
  let n = 0;
  for (const [rel, sig] of files) {
    if (rel === selfRel || exact?.has(rel) || res.some((r) => r.test(rel))) {
      h.update(`${rel}\0${sig}\n`);
      n++;
    }
  }
  h.update(`n=${n}`);
  return h.digest("hex");
}

const stateFile = (root) => join(root, "tmp", "validate-state.json");

// CI green set (#3189). GitHub's runner has no tmp/ or git common dir that outlives the job, so the workflow
// restores one file with actions/cache and names it in LF_VALIDATE_GREEN_FILE. Only then does CI skip checks,
// and only a check whose fingerprint (its inputs' content, its own file, its import closure) is listed in the
// file. The file carries the hash of this module family: a file written by a different validate tool is
// ignored whole, so a change to the skip rules never inherits an old verdict. Machine-state `ttl` greens and
// the base-clean skip (which trusts main's CI) are not used in CI: a runner is a new machine every time.
const TOOL_FILES = ["tools/lib/validate-incremental.mjs", "tools/lib/import-closure.mjs", "tools/lib/atomic-file.mjs"];

export function toolHash(root) {
  const h = createHash("sha1");
  for (const rel of TOOL_FILES) {
    try {
      h.update(`${rel}\0`).update(readFileSync(join(root, rel)).toString("utf8").replace(/\r\n/g, "\n")).update("\n");
    } catch {
      h.update(`${rel}\0missing\n`);
    }
  }
  return h.digest("hex");
}

export function isCi(env = process.env) {
  return !!(env.CI || env.GITHUB_ACTIONS);
}

// The CI green-set path, or null outside CI or when the workflow did not name one.
// With a `root` the file applies only to the runner's workspace: a selftest that runs `validate` on a scratch repo
// inherits the env and must neither read nor rewrite the real green set.
export function ciGreenFile(env = process.env, root = null) {
  if (!isCi(env) || !env.LF_VALIDATE_GREEN_FILE) return null;
  if (root && env.GITHUB_WORKSPACE && resolve(root) !== resolve(env.GITHUB_WORKSPACE)) return null;
  return env.LF_VALIDATE_GREEN_FILE;
}

export function sharedFile(root, env = process.env) {
  const ci = ciGreenFile(env, root);
  if (ci) return ci;
  try {
    let dir = execFileSync("git", ["rev-parse", "--git-common-dir"], { cwd: root, encoding: "utf8" }).trim();
    if (!isAbsolute(dir)) dir = join(root, dir);
    return join(dir, "lf", "validate-green.json");
  } catch {
    return null;
  }
}

// `tool` is the toolHash the file must carry (CI sets it, restoring a file another tool version wrote runs
// everything); null trusts any file of this STATE_VERSION (the local per-machine file). `useTtl` is false in CI.
// Every entry is checked for shape: a corrupt file (a string where a list belongs) must not match by substring.
function loadShared(file, { tool = null, useTtl = true } = {}) {
  const empty = { version: STATE_VERSION, checks: {}, ttl: {} };
  try {
    const s = JSON.parse(readFileSync(file, "utf8"));
    if (!s || s.version !== STATE_VERSION || !s.checks || typeof s.checks !== "object" || Array.isArray(s.checks)) return empty;
    if (tool && s.tool !== tool) return empty;
    const checks = {};
    for (const [name, list] of Object.entries(s.checks)) {
      if (Array.isArray(list)) checks[name] = list.filter((fp) => typeof fp === "string" && /^[0-9a-f]{40}$/.test(fp));
    }
    const ttl = {};
    if (useTtl && s.ttl && typeof s.ttl === "object") {
      for (const [name, at] of Object.entries(s.ttl)) if (Number.isFinite(at)) ttl[name] = at;
    }
    return { ...s, checks, ttl };
  } catch {
    /* none yet, or a torn write: just no sharing */
  }
  return empty;
}

// Paths this worktree changed since its merge-base with `ref` (committed, staged, modified,
// deleted, untracked), or null when there is no such ref (then nothing is skipped this way).
export function baseChanges(root, ref = "origin/main") {
  try {
    const git = (args) => execFileSync("git", args, { cwd: root, encoding: "utf8", maxBuffer: 1 << 28, stdio: ["ignore", "pipe", "ignore"] });
    const mb = git(["merge-base", "HEAD", ref]).trim();
    if (!/^[0-9a-f]{40}$/.test(mb)) return null;
    const out = new Set(git(["diff", "--name-only", "--no-renames", "-z", mb]).split("\0").filter(Boolean));
    for (const rel of lsz(root, ["--others", "--exclude-standard"])) out.add(rel);
    return [...out];
  } catch {
    return null;
  }
}

// Merge this run's green fingerprints into the shared file (newest first, SHARED_KEEP per check).
// A lost race with another worktree only costs a cache miss.
export function publishShared(file, state, opts = {}) {
  if (!file) return;
  const shared = loadShared(file, opts);
  if (opts.tool) shared.tool = opts.tool;
  for (const [name, e] of Object.entries(state.checks)) {
    if (opts.useTtl !== false && e?.fp === "ttl" && e.at) shared.ttl[name] = Math.max(e.at, shared.ttl[name] ?? 0);
    if (!e?.fp || e.fp === "ttl") continue;
    const list = (shared.checks[name] ?? []).filter((fp) => fp !== e.fp);
    shared.checks[name] = [e.fp, ...list].slice(0, SHARED_KEEP);
  }
  try {
    mkdirSync(dirname(file), { recursive: true });
    writeFileAtomic(file, JSON.stringify(shared) + "\n");
  } catch {
    /* read-only: no sharing */
  }
}

export function loadState(root, shared = sharedFile(root), env = process.env) {
  let state = { version: STATE_VERSION, checks: {} };
  try {
    const s = JSON.parse(readFileSync(stateFile(root), "utf8"));
    if (s.version === STATE_VERSION && s.checks) state = s;
  } catch {
    /* none yet */
  }
  // Not saved back into tmp/validate-state.json (saveState drops it).
  const ci = !!shared && shared === ciGreenFile(env, root);
  const opts = ci ? { tool: toolHash(root), useTtl: false } : {};
  const sh = shared ? loadShared(shared, opts) : { checks: {}, ttl: {} };
  Object.defineProperty(state, "shared", { value: sh.checks, enumerable: false, writable: true });
  Object.defineProperty(state, "sharedTtl", { value: sh.ttl, enumerable: false, writable: true });
  Object.defineProperty(state, "sharedFile", { value: shared, enumerable: false, writable: true });
  Object.defineProperty(state, "sharedOpts", { value: opts, enumerable: false, writable: true });
  return state;
}

export function saveState(root, state) {
  try {
    mkdirSync(dirname(stateFile(root)), { recursive: true });
    writeFileSync(stateFile(root), JSON.stringify(state, null, 1) + "\n");
  } catch {
    /* a read-only tmp just means no caching */
  }
  if (state.sharedFile) publishShared(state.sharedFile, state, state.sharedOpts ?? {});
}

// Decide for each script: { name, fp|null, unchanged }.
// `dir` is tools/validation (absolute), `rootRel` its repo-relative prefix.
// `changed` (from baseChanges) turns on the base-clean skip; null leaves it off.
//
// A .mjs check's static import closure (tools/lib/import-closure.mjs) is always among its inputs, whatever
// its header lists: a header names the data a check reads, and the code it loads is never optional (before
// this, 159 of 337 declared checks missed a module they import, so editing tools/lib/scratch.mjs re-ran none
// of its users). The token `imports` in a header means "no data inputs, only my own code".
export function plan(root, dir, scripts, state, files, changed = null) {
  const rootRel = "tools/validation";
  const out = new Map();
  const importCache = new Map();
  for (const s of scripts) {
    const full = join(dir, s);
    const text = existsSync(full) ? readFileSync(full, "utf8") : "";
    const declared = text ? parseInputs(text) : null;
    if (!declared) {
      out.set(s, { fp: null, unchanged: false, why: "always runs" });
      continue;
    }
    const globs = declared.filter((g) => g !== "imports");
    let exact = null;
    if (/\.(mjs|js|cjs)$/.test(s) && !ttlOf(declared)) {
      try {
        exact = new Set(importClosure(root, `${rootRel}/${s}`, importCache));
      } catch {
        // Could not read an imported file: its inputs are unknown, so the check runs every time.
        out.set(s, { fp: null, unchanged: false, why: "always runs (import closure unreadable)" });
        continue;
      }
    }
    const prev = state.checks[s];
    const ttl = ttlOf(globs);
    if (ttl) {
      const at = Math.max(prev?.at ?? 0, state.sharedTtl?.[s] ?? 0);
      const fresh = at > 0 && Date.now() - at < ttl * 1000;
      out.set(s, { fp: "ttl", unchanged: fresh, why: `green within the last ${ttl} s on this machine` });
      continue;
    }
    const fp = fingerprint(files, [...globs, ...IMPLICIT], `${rootRel}/${s}`, exact);
    if (prev && prev.fp === fp) out.set(s, { fp, unchanged: true, why: "inputs unchanged since its last green run" });
    else if (state.shared?.[s]?.includes(fp)) out.set(s, { fp, unchanged: true, why: "green on these exact inputs in another worktree" });
    else if (changed && !touches(changed, globs, `${rootRel}/${s}`, text, exact)) out.set(s, { fp, unchanged: true, base: true, why: "inputs as on origin/main (merge-base), which CI validated" });
    else out.set(s, { fp, unchanged: false, why: "inputs changed" });
  }
  return out;
}

// Data folders that slices edit all the time (town claims and kits, grid cells, dialogue lines,
// feature docs, PR pictures). For the base skip, a change there counts against a check's broad glob
// (`tools/**`, `docs/**`, ...) only when the check's source names the folder; a narrower glob that
// matches it always counts. Without this every town draft re-ran the ~25 git-heavy tools selftests.
// [folder, the text a check that reads it names]
export const DATA_DIRS = [
  ["tools/grid/cells/", "grid/cells"],
  ["tools/town/claims/", "town/claims"],
  ["tools/town/kits/", "town/kits"],
  ["tools/assets/dialogue/", "dialogue"],
  ["docs/features/", "docs/features"],
  ["website/public/docs-images/", "docs-images"],
];
const BROAD = new Set(["**", "tools/**", "docs/**", "website/**", "agent-box/**"]);

// Does any changed path count as an input of this check (for the base skip)? A baseline beside the
// checks (tools/validation/<name>.json, which every check fingerprints) counts only for the checks
// whose source names it: record-shapes.json changes in every persistence slice.
export function touches(changed, globs, selfRel, text = "", exact = null) {
  const res = globs.map((g) => [g, globToRegExp(g)]);
  return changed.some((rel) => {
    if (rel === selfRel || exact?.has(rel)) return true;
    const hits = res.filter(([, r]) => r.test(rel)).map(([g]) => g);
    if (hits.some((g) => !BROAD.has(g))) return true;
    if (rel.startsWith("tools/validation/fixtures/")) return true;
    if (/^tools\/validation\/[^/]+$/.test(rel) && !/\.(mjs|ps1)$/.test(rel)) {
      return text.includes(rel.split("/").pop().replace(/\.[^.]+$/, ""));
    }
    if (!hits.length) return false;
    const data = DATA_DIRS.find(([d]) => rel.startsWith(d));
    return !data || text.includes(data[1]);
  });
}

// CI skips only when the workflow restored a green set and named it (ciGreenFile); LF_VALIDATE_ALL always wins.
export function incrementalOff(env = process.env) {
  if (env.LF_VALIDATE_ALL) return true;
  return isCi(env) && !ciGreenFile(env);
}

