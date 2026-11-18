// The module-wide static-initializer function stays under the engine's instruction cap (#2756, design #2610).
// inputs: addons/**/*.c, tools/validation/validate-static-init-budget.mjs
//
// Every non-const static field initializer in the `Game` script module, vanilla included, compiles into ONE function,
// and the engine caps its instruction count. Past the cap the whole module stops compiling: `Too many instructions per
// function` is reported against whatever files sit at the tail (usually vanilla ones), then `Can't compile "Game"
// script module!` (docs/debug-log-triage.md). Measured 2026-11-18: 29 more hook-shaped initializers compiled, 30 did
// not, at 359 initializers in addons/, so the limit is about 385 of today's mix.
//
// This counts the initializers under addons/ (`static T s_x = ...;`, not `static const`) and
//   - warns at WARN (200), prints the headroom to FAIL,
//   - fails above CAP (250), naming the files that gained the most since origin/main.
// Fix: initialize lazily in the accessor (`if (!s_x) s_x = new ...;`), or use an LF_StaticResetHook (no initializer).
//
//   node tools/validation/validate-static-init-budget.mjs
//
// LF_STATIC_INIT_ROOT points the scan at another tree (the selftest uses a scratch one).
//
// red-proof: see static-init-budget-selftest.mjs (it fails without this file, and 251 fixture initializers exit 1).
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

export const WARN = 200;
export const CAP = 250;
const STATIC_INIT = /^\s*(?:protected |private )?static (?:ref )?[\w<>, ]+ \w+\s*=/;
const STATIC_CONST = /\bstatic\s+const\b/;

/** Number of non-const static field initializers in one script's text (comments and strings do not count). */
export function countStaticInits(text) {
  return linesOf(text).filter(isStaticInit).length;
}

export function isStaticInit(line) {
  return STATIC_INIT.test(line) && !STATIC_CONST.test(line);
}

// Blank block comments, line comments and string literals so a doc comment cannot count.
function linesOf(text) {
  return text
    .replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, " "))
    .split("\n")
    .map((l) => l.replace(/"(?:[^"\\]|\\.)*"/g, '""').replace(/\/\/.*$/, ""));
}

function walk(dir, out) {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) walk(p, out);
    else if (e.name.endsWith(".c")) out.push(p);
  }
  return out;
}

/** { total, files: Map repo-relative path -> count } for every .c under base/addons. */
export function scanTree(base) {
  const files = new Map();
  let total = 0;
  const addons = join(base, "addons");
  for (const f of existsSync(addons) ? walk(addons, []) : []) {
    const n = countStaticInits(readFileSync(f, "utf8"));
    if (n > 0) files.set(relative(base, f).split("\\").join("/"), n);
    total += n;
  }
  return { total, files };
}

/** Files that gained static initializers against `ref` (added lines of `git diff -U0`), biggest first. Best effort. */
function addedSince(base, ref) {
  let diff;
  try {
    diff = execFileSync("git", ["diff", "-U0", "--no-color", ref, "--", "addons"], { cwd: base, encoding: "utf8", maxBuffer: 1 << 28, stdio: ["ignore", "pipe", "ignore"] });
  } catch {
    return [];
  }
  const gained = new Map();
  let file = "";
  for (const line of diff.split("\n")) {
    const m = /^\+\+\+ b\/(.+)$/.exec(line);
    if (m) file = m[1];
    else if (line.startsWith("+") && !line.startsWith("+++") && file.endsWith(".c") && isStaticInit(line.slice(1))) gained.set(file, (gained.get(file) || 0) + 1);
  }
  return [...gained].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]));
}

if (process.argv[1] && process.argv[1].endsWith("validate-static-init-budget.mjs")) {
  const root = process.env.LF_STATIC_INIT_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const { total, files } = scanTree(root);
  const headroom = CAP - total;
  console.log(`static-init-budget: ${total} static initializer(s) in addons/, cap ${CAP}, warn at ${WARN}, headroom ${headroom >= 0 ? headroom : 0} (engine limit is about 385 of today's mix)`);
  if (total > CAP) {
    const gained = addedSince(root, "origin/main").slice(0, 5);
    const biggest = [...files].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0])).slice(0, 5);
    console.log(`FAIL static-init-budget: ${total} static initializers, ${total - CAP} over the cap of ${CAP}`);
    if (gained.length) console.log("  gained since origin/main: " + gained.map(([f, n]) => `${f} (+${n})`).join(", "));
    console.log("  biggest files: " + biggest.map(([f, n]) => `${f} (${n})`).join(", "));
    console.log("  Every static initializer compiles into one module-wide function the engine caps; past it the Game module stops compiling.");
    console.log("  Initialize lazily in the accessor (`if (!s_x) s_x = new ...;`) or use an LF_StaticResetHook subclass, which needs no initializer.");
    process.exit(1);
  }
  if (total >= WARN) console.log(`WARN static-init-budget: ${total} is at least ${WARN} (80% of the cap); initialize lazily in the accessor instead of adding another.`);
}
