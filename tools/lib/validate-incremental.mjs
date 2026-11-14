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
// skipped. The fingerprint hashes path, size and mtime of every matching file
// from `git ls-files --cached --others --exclude-standard`, so edits, adds,
// deletes and renames all change it, and a git checkout only reruns the checks
// whose inputs it touched. Full runs (`validate --full`, `--all`, ci, wt gate,
// wt ship, CI) never consult it.
import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

const STATE_VERSION = 1;
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

// One `git ls-files` + one stat per file for the whole run.
export function snapshotFiles(root) {
  const out = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard"], {
    cwd: root,
    encoding: "utf8",
    maxBuffer: 1 << 28,
  });
  const files = [];
  for (const rel of new Set(out.split("\0").filter(Boolean))) {
    let sig = "gone";
    try {
      const st = statSync(join(root, rel));
      sig = `${st.size}:${Math.round(st.mtimeMs)}`;
    } catch {
      /* deleted in the working tree: stays in the list so a delete changes the fingerprint */
    }
    files.push([rel, sig]);
  }
  files.sort((a, b) => (a[0] < b[0] ? -1 : 1));
  return files;
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

export function loadState(root) {
  try {
    const s = JSON.parse(readFileSync(stateFile(root), "utf8"));
    if (s.version === STATE_VERSION && s.checks) return s;
  } catch {
    /* none yet */
  }
  return { version: STATE_VERSION, checks: {} };
}

export function saveState(root, state) {
  try {
    mkdirSync(dirname(stateFile(root)), { recursive: true });
    writeFileSync(stateFile(root), JSON.stringify(state, null, 1) + "\n");
  } catch {
    /* a read-only tmp just means no caching */
  }
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
    out.set(s, { fp, unchanged: !!prev && prev.fp === fp, why: "inputs unchanged since its last green run" });
  }
  return out;
}

export function incrementalOff(env = process.env) {
  return !!(env.CI || env.GITHUB_ACTIONS || env.LF_VALIDATE_ALL);
}
