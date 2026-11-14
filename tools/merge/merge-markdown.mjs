// Git merge driver for append-mostly docs (docs/features.md,
// docs/debug-log-triage.md).
//
// It is git's own 3-way line merge (`git merge-file --diff3`) plus one rule:
// a conflict whose base side is empty means both sides only inserted lines at
// the same spot (two new rows at the end of a section or table). Those keep
// ours, then theirs. Every other conflict stays a conflict: the file gets
// standard markers and the driver exits 1, so git stops for a hand merge.
// Nothing is keyed by heading, so nothing is duplicated or dropped.
//
//   node tools/merge/merge-markdown.mjs %O %A %B %P

import { spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const OURS_LABEL = "ours";
const BASE_LABEL = "base";
const THEIRS_LABEL = "theirs";

function runMergeFile(oursPath, basePath, theirsPath) {
  const result = spawnSync(
    "git",
    ["merge-file", "-p", "--diff3", "-L", OURS_LABEL, "-L", BASE_LABEL, "-L", THEIRS_LABEL, oursPath, basePath, theirsPath],
    { windowsHide: true, encoding: "utf8", maxBuffer: 64 * 1024 * 1024 },
  );
  if (result.error || result.status === null || result.status < 0) {
    throw new Error(`git merge-file failed: ${result.error ? result.error.message : result.stderr}`);
  }
  return result.stdout;
}

// Splits text into lines that keep their own line ending.
function splitKeep(text) {
  const lines = text.match(/[^\n]*\n|[^\n]+$/g);
  return lines || [];
}

function isMarker(line, mark, label) {
  const bare = line.replace(/\r?\n$/, "");
  return bare === `${mark} ${label}` || (label === null && bare === mark);
}

// Resolves pure-insertion conflicts in merge-file --diff3 output.
// Returns { text, conflicts } where conflicts counts the hunks left in place.
export function resolveInsertions(diff3Text) {
  const lines = splitKeep(diff3Text);
  const out = [];
  let conflicts = 0;
  let i = 0;
  while (i < lines.length) {
    if (!isMarker(lines[i], "<<<<<<<", OURS_LABEL)) {
      out.push(lines[i]);
      i++;
      continue;
    }
    const ours = [];
    const base = [];
    const theirs = [];
    let part = ours;
    let j = i + 1;
    let closed = false;
    for (; j < lines.length; j++) {
      if (part === ours && isMarker(lines[j], "|||||||", BASE_LABEL)) part = base;
      else if (part !== theirs && isMarker(lines[j], "=======", null)) part = theirs;
      else if (part === theirs && isMarker(lines[j], ">>>>>>>", THEIRS_LABEL)) {
        closed = true;
        break;
      } else part.push(lines[j]);
    }
    if (!closed) {
      out.push(...lines.slice(i));
      conflicts++;
      break;
    }
    if (base.length === 0 && ours.join("") === theirs.join("")) {
      out.push(...ours);
    } else if (base.length === 0) {
      out.push(...withEnding(ours, theirs.length > 0), ...theirs);
    } else {
      out.push(...lines.slice(i, j + 1));
      conflicts++;
    }
    i = j + 1;
  }
  return { text: out.join(""), conflicts };
}

// When ours ends without a newline and theirs follows, give ours one so the
// two inserted blocks do not fuse onto one line.
function withEnding(block, followed) {
  if (!followed || block.length === 0) return block;
  const last = block[block.length - 1];
  if (last.endsWith("\n")) return block;
  const eol = block.some((line) => line.endsWith("\r\n")) ? "\r\n" : "\n";
  return [...block.slice(0, -1), last + eol];
}

export function mergeLines(baseText, oursText, theirsText) {
  const dir = mkdtempSync(join(tmpdir(), "lf-merge-md-"));
  const write = (name, text) => {
    const path = join(dir, name);
    writeFileSync(path, text ?? "");
    return path;
  };
  let diff3;
  try {
    diff3 = runMergeFile(write("ours", oursText), write("base", baseText), write("theirs", theirsText));
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
  const resolved = resolveInsertions(diff3);
  const text = resolved.conflicts === 0 ? dropDoubleAdds(baseText, oursText, theirsText, resolved.text) : resolved.text;
  return { ok: resolved.conflicts === 0, conflicts: resolved.conflicts, text };
}

// A line both sides added (new to the base) is one row, even when the two
// sides put it at different spots: a branch that copied main's rows by hand,
// then merged main, got the #1026 and #1111 features.md sections twice
// (2026-11-14). Keeps the first copy. Short lines (blank, `---`, `|---|`) are
// left alone, since two sections may each add one.
function dropDoubleAdds(baseText, oursText, theirsText, mergedText) {
  const key = (line) => line.replace(/\r?\n$/, "").trimEnd();
  const counts = (text) => {
    const map = new Map();
    for (const line of splitKeep(text ?? "")) map.set(key(line), (map.get(key(line)) || 0) + 1);
    return map;
  };
  const [base, ours, theirs] = [baseText, oursText, theirsText].map(counts);
  const lines = splitKeep(mergedText);
  const seen = new Map();
  const out = [];
  for (const line of lines) {
    const k = key(line);
    const n = (seen.get(k) || 0) + 1;
    seen.set(k, n);
    const allowed = Math.max(ours.get(k) || 0, theirs.get(k) || 0);
    const doubleAdd = k.trim().length >= 12 && !base.has(k) && ours.has(k) && theirs.has(k);
    if (doubleAdd && n > allowed) continue;
    out.push(line);
  }
  return out.join("");
}

function runDriver() {
  const [basePath, oursPath, theirsPath, path] = process.argv.slice(2);
  if (!basePath || !oursPath || !theirsPath) {
    console.error("usage: merge-markdown.mjs <base> <ours> <theirs> [path]");
    process.exit(2);
  }
  const merged = mergeLines(
    readFileSync(basePath, "utf8"),
    readFileSync(oursPath, "utf8"),
    readFileSync(theirsPath, "utf8"),
  );
  writeFileSync(oursPath, merged.text);
  if (!merged.ok) {
    console.error(`lf-markdown: ${merged.conflicts} conflict(s) in ${path || oursPath}; both sides changed the same lines`);
    process.exit(1);
  }
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").endsWith("tools/merge/merge-markdown.mjs");
if (invoked) runDriver();
