// No committed git conflict markers in any tracked text file (AGENTS.md: a rule that keeps getting
// broken becomes a check).
// inputs: every tracked file, .gitattributes
//
// A line that starts with an ours, base or theirs marker (7 `<`, `|` or `>` plus a space) or is
// exactly 7 `=` is what git leaves in a file it could not merge. On 2026-11-18 a site-rent merge
// (33884d14b, #2683) committed a whole block of them in Feature/Sites/Feature.md and
// docs/features/map.md had a stray base marker; validate stayed green because nothing looked.
//
// Scope: all of `git ls-files`. Skipped: files `.gitattributes` marks `-text` (binary assets, the CRLF
// language confs), files with a NUL byte, files over 8 MB. A marker must start the line, so a string
// literal in a selftest that builds one with `+` or an indented example is not a hit. ALLOW lists
// files that legitimately hold such lines at column 0 (a fixture or a doc that shows a marker);
// add a path there with the reason, never to silence a real conflict.
//
// LF_CONFLICTMARKERS_ROOT points the scan at another tree (the selftest uses a temp one; with no
// .git there, a directory walk lists the files instead of `git ls-files`).
//
// red-proof: 2026-11-18, tools/validation/no-conflict-markers-selftest.mjs plants a .md with a full
// ours/base/theirs block and runs this over it: `FAIL docs/x.md:2 ...`, exit 1; the clean tree passes.
import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { allowMatcher, conflictMarkerLines } from "../lib/conflict-markers.mjs";
import { looksBinary, minusTextMatcher } from "../lib/lone-cr.mjs";

const root = process.env.LF_CONFLICTMARKERS_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");

// Files allowed to hold marker lines at column 0. The merge-driver selftests build their markers by
// string concatenation, so nothing needs listing today; these paths are for checked-in fixtures.
const ALLOW = [
  "tools/merge/fixtures/**", // merge-driver selftest inputs, if a fixture file is ever checked in
  "tools/validation/fixtures/conflict-markers/**", // planted examples for a check on markers
];
const isAllowed = allowMatcher(ALLOW);
const MAX_BYTES = 8 * 1024 * 1024;

function walk(dir, rel, out) {
  for (const name of readdirSync(dir)) {
    if (name === ".git" || name === "node_modules") continue;
    const p = join(dir, name);
    const r = rel ? `${rel}/${name}` : name;
    if (statSync(p).isDirectory()) walk(p, r, out);
    else out.push(r);
  }
}

function listFiles() {
  if (existsSync(join(root, ".git"))) {
    const out = execFileSync("git", ["-C", root, "ls-files", "-z"], { encoding: "utf8", maxBuffer: 1 << 28 });
    return out.split("\0").filter(Boolean);
  }
  const files = [];
  walk(root, "", files);
  return files;
}

const attrs = existsSync(join(root, ".gitattributes")) ? readFileSync(join(root, ".gitattributes"), "utf8") : "";
const isBinaryByAttr = minusTextMatcher(attrs);

const bad = [];
let scanned = 0;
for (const f of listFiles()) {
  if (isBinaryByAttr(f) || isAllowed(f)) continue;
  let buf;
  try {
    const p = join(root, f);
    if (statSync(p).size > MAX_BYTES) continue;
    buf = readFileSync(p);
  } catch {
    continue; // tracked but deleted in this checkout
  }
  if (looksBinary(buf)) continue;
  scanned++;
  for (const h of conflictMarkerLines(buf)) bad.push(`FAIL ${f}:${h.line} conflict marker "${h.text}"`);
}

if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-no-conflict-markers: ${bad.length} marker line(s). Resolve the merge by hand (keep the intended side, drop the markers); a file that must show one at column 0 goes in ALLOW with a reason.`);
  process.exit(1);
}
console.log(`validate-no-conflict-markers: OK (${scanned} text file(s))`);
