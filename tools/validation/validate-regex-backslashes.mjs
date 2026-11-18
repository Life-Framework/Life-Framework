// tools/validation/validate-regex-backslashes.mjs
// inputs: tools/**/*.mjs, tools/**/*.js, tools/**/*.ps1, tools/validation/validate-regex-backslashes.mjs
//
// A regex whose backslashes were eaten still parses and silently matches something else. Agents
// writing code through a shell lose them (the lone-CR cause, #2583): main-guard's compile-error
// pattern read SCRIPTs*(E) for SCRIPT\s*\(E\) and never matched (#2759), and its host name cleanup
// used /s+/g, which turns every run of the letter "s" into "-" (#2764).
//
// Heuristic, over the regex literals in tools/**/*.mjs|js, the string given to `new RegExp(` /
// `RegExp(`, and the pattern strings after -match/-replace/-split in tools/**/*.ps1. Inside a
// pattern (escapes and [classes] removed) it flags:
//   - a lone s, d or w (any case) carrying a quantifier: s+  d*  w{2}  where \s \d \w was meant;
//   - s, d or w with a quantifier followed by a group paren: s*(  where \s*\( was meant.
// It also flags a raw control byte (backspace, form feed, vertical tab...) in any scanned file: a
// `\b` or `\f` a shell expanded, which silently broke LF_ConfigList\b in pack-check-selftest.
// A real hit is fixed. A pattern that really means the letter is allowlisted on the same line with
// `// lf-regex-ok: <why>` (`# lf-regex-ok: <why>` in .ps1); the reason is required.
//
//   node tools/validation/validate-regex-backslashes.mjs
//
// LF_REGEXBS_ROOT points the scan at another tree (the selftest uses a temp one).
//
// red-proof: tools/validation/regex-backslashes-selftest.mjs plants /s+/g, SCRIPTs*(E), 'w+' in a
// RegExp string and a ps1 -replace 's+' and asserts each is flagged, while the escaped forms and
// a marked line pass.

import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const MARK = /lf-regex-ok:\s*\S/;

// The body of a pattern with escapes and character classes blanked, so only live regex syntax is left.
function liveSyntax(body) {
  let out = "";
  let inClass = false;
  for (let i = 0; i < body.length; i++) {
    const c = body[i];
    if (c === "\\") {
      out += "  ";
      i++;
    } else if (inClass) {
      if (c === "]") inClass = false;
      out += " ";
    } else if (c === "[") {
      inClass = true;
      out += " ";
    } else out += c;
  }
  return out;
}

// Why a pattern looks like it lost its backslashes, or null.
export function suspicious(body) {
  const live = liveSyntax(body);
  const lone = /(?<![\\\w])([sdwSDW])([+*]|\{\d)/.exec(live);
  if (lone) return `'${lone[1]}${lone[2]}' where '\\${lone[1]}${lone[2]}' was meant`;
  const group = /([sdwSDW])[+*]\(/.exec(live);
  if (group) return `'${group[1]}${live[group.index + 1]}(' where '\\${group[1]}${live[group.index + 1]}\\(' was meant`;
  return null;
}

// What a quoted or template string source means once JavaScript has read its escapes: `\\` is one
// backslash, `\s` (not an escape) is just `s`, `\n` a newline, `${...}` an interpolation (a placeholder).
export function cookString(src) {
  let out = "";
  for (let i = 0; i < src.length; i++) {
    const c = src[i];
    if (c === "\\") {
      const n = src[++i] ?? "";
      if (n === "n") out += "\n";
      else if (n === "t") out += "\t";
      else if (n === "r") out += "\r";
      else if (n === "0") out += "\0";
      else if (n === "x" || n === "u") out += n + "_"; // hex / unicode escape: not a letter class
      else out += n;
    } else if (c === "$" && src[i + 1] === "{") {
      let depth = 0;
      for (; i < src.length; i++) {
        if (src[i] === "{") depth++;
        else if (src[i] === "}" && --depth === 0) break;
      }
      out += "_";
    } else out += c;
  }
  return out;
}

// Regex literal and RegExp-string patterns in JS source: [{ line, body }]. Strings, template
// literals and comments are skipped over; a `/` starts a regex only where an expression may begin.
export function jsPatterns(text) {
  const out = [];
  let line = 1;
  let prev = ""; // last significant token char or word
  const lineOf = () => line;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === "\n") {
      line++;
      continue;
    }
    if (c === "/" && text[i + 1] === "/") {
      while (i < text.length && text[i] !== "\n") i++;
      line++;
      continue;
    }
    if (c === "/" && text[i + 1] === "*") {
      i += 2;
      while (i < text.length && !(text[i] === "*" && text[i + 1] === "/")) {
        if (text[i] === "\n") line++;
        i++;
      }
      i++;
      continue;
    }
    if (c === '"' || c === "'" || c === "`") {
      const start = i;
      const startLine = line;
      i++;
      while (i < text.length && text[i] !== c) {
        if (text[i] === "\\") i++;
        else if (text[i] === "\n") line++;
        i++;
      }
      const body = text.slice(start + 1, i);
      // The string handed to RegExp( is a pattern.
      const before = text.slice(Math.max(0, start - 30), start);
      if (/\bRegExp\s*\(\s*$/.test(before)) out.push({ line: startLine, body: cookString(body) });
      else if (/\bRegExp\s*\(\s*String\.raw\s*$/.test(before)) out.push({ line: startLine, body: body.replace(/\$\{[^}]*\}/g, "_") });
      prev = '"';
      continue;
    }
    if (c === "/") {
      const regexAllowed = prev === "" || /[(,=:[!&|?{};+\-*%<>~^]/.test(prev) || /^(return|typeof|case|in|of|else|do|void|delete|new|throw|yield|await)$/.test(prev);
      if (regexAllowed) {
        const start = i;
        let inClass = false;
        i++;
        while (i < text.length && text[i] !== "\n") {
          if (text[i] === "\\") i++;
          else if (text[i] === "[") inClass = true;
          else if (text[i] === "]") inClass = false;
          else if (text[i] === "/" && !inClass) break;
          i++;
        }
        out.push({ line: lineOf(), body: text.slice(start + 1, i) });
        while (/[a-z]/.test(text[i + 1] ?? "")) i++;
        prev = "/";
        continue;
      }
      prev = "/";
      continue;
    }
    if (/\s/.test(c)) continue;
    if (/[A-Za-z_$]/.test(c)) {
      let j = i;
      while (j < text.length && /[\w$]/.test(text[j])) j++;
      prev = text.slice(i, j);
      i = j - 1;
      continue;
    }
    if (/\d/.test(c)) {
      while (/[\w.]/.test(text[i + 1] ?? "")) i++;
      prev = "0";
      continue;
    }
    prev = c;
    if (c === ")" || c === "]" || c === "}") prev = c === "}" ? "}" : "0";
  }
  return out;
}

// Pattern strings in PowerShell after -match/-replace/-split (and their i/c variants): [{ line, body }].
export function ps1Patterns(text) {
  const out = [];
  text.split("\n").forEach((src, idx) => {
    for (const m of src.matchAll(/-[ic]?(?:not)?(?:match|replace|split)\s+(?:'((?:[^']|'')*)'|"((?:[^"`]|`.)*)")/gi)) {
      out.push({ line: idx + 1, body: m[1] ?? m[2] });
    }
  });
  return out;
}

// A control byte other than tab, LF and CR in a source file: the shell turned a `\b`, `\f` or `\v`
// into a real backspace, form feed or vertical tab (`LF_ConfigList\b` became `LF_ConfigList` + BS, so
// pack-check-selftest never matched anything). [{ line, code }]
export function controlBytes(text) {
  const out = [];
  text.split("\n").forEach((src, idx) => {
    const m = /[\x00-\x08\x0b\x0c\x0e-\x1f]/.exec(src);
    if (m) out.push({ line: idx + 1, code: m[0].charCodeAt(0) });
  });
  return out;
}

export function scanFile(file, text) {
  const lines = text.split("\n");
  const hits = controlBytes(text).map((c) => ({ file, line: c.line, why: `control byte 0x${c.code.toString(16).padStart(2, "0")} (an escape like \\b or \\f was expanded by a shell)`, body: lines[c.line - 1].slice(0, 80) }));
  const patterns = file.endsWith(".ps1") ? ps1Patterns(text) : jsPatterns(text);
  for (const p of patterns) {
    const why = suspicious(p.body);
    if (!why || MARK.test(lines[p.line - 1] ?? "")) continue;
    hits.push({ file, line: p.line, why, body: p.body });
  }
  return hits;
}

const root = process.env.LF_REGEXBS_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");

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
  let files;
  if (existsSync(join(root, ".git"))) {
    files = execFileSync("git", ["-C", root, "ls-files", "-z", "--", "tools"], { encoding: "utf8", maxBuffer: 1 << 28 }).split("\0").filter(Boolean);
  } else {
    files = [];
    if (existsSync(join(root, "tools"))) walk(join(root, "tools"), "tools", files);
  }
  return files.filter((f) => /\.(mjs|js|ps1)$/.test(f));
}

function main() {
  const hits = [];
  let scanned = 0;
  for (const f of listFiles()) {
    let text;
    try {
      text = readFileSync(join(root, f), "utf8");
    } catch {
      continue; // tracked but deleted in this checkout
    }
    scanned++;
    hits.push(...scanFile(f, text));
  }
  for (const h of hits) console.log(`FAIL ${h.file}:${h.line}: ${h.why} in pattern ${JSON.stringify(h.body)} (a backslash was lost; if the letter is meant, mark the line "lf-regex-ok: <why>")`);
  if (hits.length) process.exit(1);
  console.log(`regex backslashes: ${scanned} file(s), no pattern lost its backslashes`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
