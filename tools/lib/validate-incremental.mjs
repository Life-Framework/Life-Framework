// Incremental `validate`: skip a check whose declared inputs have not changed
// since its last green run in this worktree.
//
// Each check declares its inputs in a header line within its first 30 lines:
//     // inputs: addons/**/*.layer, docs/features.md      (.mjs)
//     # inputs: addons/**/*.layer, docs/features.md        (.ps1)
// Globs are repo-relative: `*` matches within a folder, `**` across folders.
// The check's own file is always an input. `inputs: always`, or no header at
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
// wt ship, CI) never consult it.
import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join } from "node:path";
import { writeFileAtomic } from "./atomic-file.mjs";

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

export function fingerprint(files, globs, selfRel) {
  const res = globs.map(globToRegExp);
  const h = createHash("sha1");
  let n = 0;
  for (const [rel, sig] of files) {
    if (rel === selfRel || res.some((r) => r.test(rel))) {
      h.update(`${rel}\0${sig}\n`);
      n++;
    }
  }
  h.update(`n=${n}`);
  return h.digest("hex");
}

const stateFile = (root) => join(root, "tmp", "validate-state.json");

export function sharedFile(root) {
  try {
    let dir = execFileSync("git", ["rev-parse", "--git-common-dir"], { cwd: root, encoding: "utf8" }).trim();
    if (!isAbsolute(dir)) dir = join(root, dir);
    return join(dir, "lf", "validate-green.json");
  } catch {
    return null;
  }
}

function loadShared(file) {
  try {
    const s = JSON.parse(readFileSync(file, "utf8"));
    if (s.version === STATE_VERSION && s.checks) return s;
  } catch {
    /* none yet, or a torn write: just no sharing */
  }
  return { version: STATE_VERSION, checks: {} };
}

// Merge this run's green fingerprints into the shared file (newest first, SHARED_KEEP per check).
// A lost race with another worktree only costs a cache miss.
export function publishShared(file, state) {
  if (!file) return;
  const shared = loadShared(file);
  for (const [name, e] of Object.entries(state.checks)) {
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

export function loadState(root, shared = sharedFile(root)) {
  let state = { version: STATE_VERSION, checks: {} };
  try {
    const s = JSON.parse(readFileSync(stateFile(root), "utf8"));
    if (s.version === STATE_VERSION && s.checks) state = s;
  } catch {
    /* none yet */
  }
  // Not saved back into tmp/validate-state.json (saveState drops it).
  Object.defineProperty(state, "shared", { value: shared ? loadShared(shared).checks : {}, enumerable: false, writable: true });
  Object.defineProperty(state, "sharedFile", { value: shared, enumerable: false, writable: true });
  return state;
}

export function saveState(root, state) {
  try {
    mkdirSync(dirname(stateFile(root)), { recursive: true });
    writeFileSync(stateFile(root), JSON.stringify(state, null, 1) + "\n");
  } catch {
    /* a read-only tmp just means no caching */
  }
  if (state.sharedFile) publishShared(state.sharedFile, state);
}

// Decide for each script: { name, fp|null, unchanged }.
// `dir` is tools/validation (absolute), `rootRel` its repo-relative prefix.
export function plan(root, dir, scripts, state, files) {
  const rootRel = "tools/validation";
  const out = new Map();
  for (const s of scripts) {
    const full = join(dir, s);
    const globs = existsSync(full) ? parseInputs(readFileSync(full, "utf8")) : null;
    if (!globs) {
      out.set(s, { fp: null, unchanged: false, why: "always runs" });
      continue;
    }
    const prev = state.checks[s];
    const ttl = ttlOf(globs);
    if (ttl) {
      const fresh = !!prev?.at && Date.now() - prev.at < ttl * 1000;
      out.set(s, { fp: "ttl", unchanged: fresh, why: `green ${ttl} s ago window` });
      continue;
    }
    const fp = fingerprint(files, [...globs, ...IMPLICIT], `${rootRel}/${s}`);
    if (prev && prev.fp === fp) out.set(s, { fp, unchanged: true, why: "inputs unchanged since its last green run" });
    else if (state.shared?.[s]?.includes(fp)) out.set(s, { fp, unchanged: true, why: "green on these exact inputs in another worktree" });
    else out.set(s, { fp, unchanged: false, why: "inputs changed" });
  }
  return out;
}

export function incrementalOff(env = process.env) {
  return !!(env.CI || env.GITHUB_ACTIONS || env.LF_VALIDATE_ALL);
}
