// No lone carriage return in a tracked text file under addons/ and tools/ (AGENTS.md golden rule 4).
// inputs: addons/**, tools/**, .gitattributes
//
// A CR byte that is not part of CRLF splits a `//` comment for any tool that reads CR as a line
// break: an agent rewriting LF_MapShares.c and LF_MapMarksSync.c (2026-11-17) turned the rest of a
// doc comment into bare code and broke the compile. Cause both times: `\return` in a doc comment
// written through a shell or script that expanded `\r` into a real CR byte.
//
// Scope: tracked files under addons/ and tools/. Skipped: files `.gitattributes` marks `-text`
// (binary assets, the CRLF language confs), `.cmd`/`.bat` (CRLF by rule), and any file with a NUL
// byte. A CRLF pair alone is not flagged: git normalises it to LF on commit. The check is for the
// CR git cannot normalise (it even classifies such a file as binary, so the damage hides in diffs).
//
// LF_LONECR_ROOT points the scan at another tree (the selftest uses a temp one; with no .git
// there, a directory walk lists the files instead of `git ls-files`).
//
// red-proof: 2026-11-17, tools/validation/no-lone-cr-selftest.mjs plants a .c file whose comment
// holds a CR and runs this over it: `FAIL addons/.../LF_Planted.c:3`, exit 1; LF-only passes.
import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { isCrlfFile, looksBinary, loneCrLines, minusTextMatcher } from "../lib/lone-cr.mjs";

const root = process.env.LF_LONECR_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SCOPES = ["addons", "tools"];

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
    const out = execFileSync("git", ["-C", root, "ls-files", "-z", "--", ...SCOPES], { encoding: "utf8", maxBuffer: 1 << 28 });
    return out.split("\0").filter(Boolean);
  }
  const files = [];
  for (const s of SCOPES) if (existsSync(join(root, s))) walk(join(root, s), s, files);
  return files;
}

const attrs = existsSync(join(root, ".gitattributes")) ? readFileSync(join(root, ".gitattributes"), "utf8") : "";
const isBinaryByAttr = minusTextMatcher(attrs);

const bad = [];
let scanned = 0;
for (const f of listFiles()) {
  if (isCrlfFile(f) || isBinaryByAttr(f)) continue;
  let buf;
  try {
    buf = readFileSync(join(root, f));
  } catch {
    continue; // tracked but deleted in this checkout
  }
  scanned++;
  if (!buf.includes(13) || looksBinary(buf)) continue;
  for (const line of loneCrLines(buf)) bad.push(`FAIL ${f}:${line} lone CR (a carriage return not followed by a line feed)`);
}

if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-no-lone-cr: ${bad.length} lone CR line(s). Delete the byte; the repo is LF-only. A "\\return" in a doc comment must be a backslash and the word, not a CR plus "eturn".`);
  process.exit(1);
}
console.log(`validate-no-lone-cr: OK (${scanned} text file(s))`);
