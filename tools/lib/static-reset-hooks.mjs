// tools/lib/static-reset-hooks.mjs - world-end static resets are registered per feature.
//
// Core/LF_StaticReset.c used to hold one central list (`LF_X.ResetStatics();` per feature) that nearly
// every feature PR appended to, so the ship queue ran those PRs one after another and GitHub marked the
// rest DIRTY. Now each class that owns statics registers its own reset in its own file:
//
//   modded class LF_StaticResetHooks3     (the bucket is bucketOf("LF_X"))
//   {
//   	override void Run(notnull array<string> done, bool apply)
//   	{
//   		super.Run(done, apply);
//   		done.Insert("LF_X");
//   		if (apply)
//   			LF_X.ResetStatics();
//   	}
//   }
//
// One modded chain is too deep: every layer calls super.Run, so N hooks are N nested frames, and the VM
// stack overflows near 120 ("Stack overflow", Class LF_StaticResetHooks, at world teardown; the main guard
// blamed whichever merge added the 120th hook). So the hooks are spread over BUCKETS sibling classes,
// LF_StaticResetHooks, LF_StaticResetHooks1 ... LF_StaticResetHooks7, each its own chain. A feature's file
// uses bucketOf(its first class name); validate-registries caps a bucket at MAX_PER_BUCKET.
//
// Pure helpers here are shared by the migration, `tools\cli heal-hotspots` (rewrites a branch's old
// central-list edit into this form) and validate-registries.mjs (rejects a call in the central file).
// tools/validation/static-reset-hooks-selftest.mjs proves them.

import { execFileSync } from "node:child_process";
import { readFileSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";

export const CENTRAL = "addons/LifeFramework/Scripts/Game/Core/LF_StaticReset.c";
export const SCRIPTS = "addons/LifeFramework/Scripts/Game";

export const BUCKETS = 8;
export const MAX_PER_BUCKET = 40;

// Bucket 0..BUCKETS-1 for a class name (FNV-1a, stable).
export function bucketOf(cls) {
  let h = 0x811c9dc5;
  for (const ch of String(cls)) h = Math.imul(h ^ ch.charCodeAt(0), 0x01000193) >>> 0;
  return h % BUCKETS;
}

export function hookClass(bucket) {
  return bucket === 0 ? "LF_StaticResetHooks" : "LF_StaticResetHooks" + bucket;
}

export function isHookClass(name) {
  return /^LF_StaticResetHooks[0-9]*$/.test(String(name));
}

// Every hook block in a file: [{ cls, text }].
export function hookBlocks(text) {
  const out = [];
  for (const m of String(text).matchAll(/^modded\s+class\s+(LF_StaticResetHooks[0-9]*)\b[\s\S]*?\n\}/gm)) out.push({ cls: m[1], text: m[0] });
  return out;
}

const CALL = /^\s*(LF_[A-Za-z0-9_]+)\.([A-Za-z0-9_]+)\(\);\s*$/;

// Reset calls (`LF_X.Method();`) inside LF_StaticReset.ResetAll, in order. The old central list; the new
// file has none (LF_Debug is the log line, not a reset). Returns [{ cls, method, line }].
export function centralCalls(text) {
  const out = [];
  const lines = String(text).split("\n");
  let inReset = false;
  let depth = 0;
  lines.forEach((l, i) => {
    if (!inReset && /\bstatic\s+void\s+ResetAll\s*\(/.test(l)) {
      inReset = true;
      depth = 0;
      return;
    }
    if (!inReset) return;
    depth += (l.match(/\{/g) || []).length - (l.match(/\}/g) || []).length;
    const m = CALL.exec(l);
    if (m && m[1] !== "LF_Debug" && !isHookClass(m[1])) out.push({ cls: m[1], method: m[2], line: i + 1 });
    if (depth < 0 || (depth === 0 && /\}/.test(l))) inReset = false;
  });
  return out;
}

// Calls a file already registers through its `modded class LF_StaticResetHooks` block(s).
export function hookedCalls(text) {
  const out = [];
  for (const { text: block } of hookBlocks(text)) {
    for (const l of block.split("\n")) {
      const m = CALL.exec(l);
      if (m) out.push({ cls: m[1], method: m[2] });
    }
  }
  return out;
}

const key = (c) => `${c.cls}.${c.method}`;

// The hook block registering `calls` (all owned by one file).
export function hookBlock(calls) {
  const cls = hookClass(bucketOf(calls[0].cls));
  const body = calls.map((c) => `\t\tdone.Insert("${c.cls}");\n\t\tif (apply)\n\t\t\t${c.cls}.${c.method}();\n`).join("");
  return [
    "//------------------------------------------------------------------------------------------------",
    "//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).",
    "modded class " + cls,
    "{",
    "\toverride void Run(notnull array<string> done, bool apply)",
    "\t{",
    "\t\tsuper.Run(done, apply);",
    body.replace(/\n$/, ""),
    "\t}",
    "}",
    "",
  ].join("\n");
}

// `text` with a hook block for those of `calls` it does not register yet appended (idempotent).
export function addHooks(text, calls) {
  const have = new Set(hookedCalls(text).map(key));
  const todo = [];
  for (const c of calls) if (!have.has(key(c)) && !todo.some((t) => key(t) === key(c))) todo.push(c);
  if (!todo.length) return text;
  const base = String(text).replace(/\s*$/, "\n");
  return `${base}\n${hookBlock(todo)}`;
}

// The new central file: the hook bucket classes and a ResetAll that runs them. No feature names in it.
export function centralFile() {
  const classes = [];
  const runs = [];
  for (let b = 0; b < BUCKETS; b++) {
    const cls = hookClass(b);
    classes.push(b === 0 ? "" : `class ${cls} : Managed\n{\n\t//! Bucket ${b}: a sibling chain, see the header. Features write \`modded class ${cls}\`.\n\tvoid Run(notnull array<string> done, bool apply)\n\t{\n\t}\n}\n`);
    runs.push(`\t\t${cls} b${b} = new ${cls}();\n\t\tb${b}.Run(done, apply);\n`);
  }
  return `//------------------------------------------------------------------------------------------------
//! One hook that drops process-global (static) feature state when a world ends, so the next
//! world loaded in the same process (Workbench play restart, server mission change) starts clean.
//!
//! Called from the LF_GameModeRoleplay destructor (world teardown), next to
//! LF_PlayerAccountManager.Reset. It is NOT called at game start: placed breach targets, jail
//! cells, garages, and spawn points register in their OnPostInit, which runs before the game
//! mode starts; a start-time reset would wipe them.
//!
//! Nothing is listed here. Each class that owns statics registers its reset in its own file with a
//! \`modded class LF_StaticResetHooks<N>\` (the feature scaffold writes it and picks the bucket N from
//! the class name; template in tools/lib/static-reset-hooks.mjs). A central list made every feature PR
//! touch this file, so the ship queue ran them one after another; validate-registries.mjs rejects a
//! call added here.
//!
//! Why ${BUCKETS} classes: every modded layer calls super.Run, so one class with N hooks is N nested
//! frames, and the script VM overflows its stack near 120 ("Stack overflow" in LF_StaticResetHooks at
//! world teardown, 2026-11-16: the main guard blamed the merge that added the 120th hook). The buckets
//! are sibling classes, so no chain is deeper than one bucket (validate-registries caps a bucket at
//! ${MAX_PER_BUCKET} hooks).
//------------------------------------------------------------------------------------------------
class LF_StaticResetHooks : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Bucket 0. Every feature's modded override calls super.Run first, inserts its class name into
	//! done, and resets its statics only when apply is true (apply false lists the hooks without
	//! running them: LF_Test_StaticResetHooks).
	void Run(notnull array<string> done, bool apply)
	{
	}
}

${classes.filter(Boolean).join("\n")}
//------------------------------------------------------------------------------------------------
class LF_StaticReset
{
	protected static int s_iRuns;

	//------------------------------------------------------------------------------------------------
	//! Runs the buckets one after another (never one chain through another: see the header).
	protected static void RunBuckets(notnull array<string> done, bool apply)
	{
${runs.join("")}	}

	//------------------------------------------------------------------------------------------------
	static void ResetAll()
	{
		array<string> done = new array<string>();
		RunBuckets(done, true);

		s_iRuns = s_iRuns + 1;
		LF_Debug.Log("StaticReset", string.Format("feature statics reset (run %1, %2 hooks)", s_iRuns, done.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! The registered hooks by class name, without running them.
	static array<string> Registered()
	{
		array<string> done = new array<string>();
		RunBuckets(done, false);
		return done;
	}
}
`;
}

// ---------------------------------------------------------------- repo helpers

function walk(dir, out = []) {
  for (const n of readdirSync(dir)) {
    const p = join(dir, n);
    const st = statSync(p);
    if (st.isDirectory()) walk(p, out);
    else if (n.endsWith(".c")) out.push(p);
  }
  return out;
}

// Map class name -> repo-relative file that declares it (`class LF_X`, also `sealed class`).
export function classFiles(root) {
  const map = new Map();
  for (const abs of walk(join(root, SCRIPTS))) {
    const rel = abs.slice(root.length + 1).replace(/\\/g, "/");
    const text = readFileSync(abs, "utf8");
    for (const m of text.matchAll(/^(?:sealed\s+)?class\s+(LF_[A-Za-z0-9_]+)\b/gm)) if (!map.has(m[1])) map.set(m[1], rel);
  }
  return map;
}

// Group calls by declaring file. Returns { byFile: Map<rel, calls[]>, missing: calls[] }.
export function placeCalls(calls, files) {
  const byFile = new Map();
  const missing = [];
  for (const c of calls) {
    const f = files.get(c.cls);
    if (!f) {
      missing.push(c);
      continue;
    }
    if (!byFile.has(f)) byFile.set(f, []);
    byFile.get(f).push(c);
  }
  return { byFile, missing };
}

export function git(cwd, args, opts = {}) {
  return execFileSync("git", args, { cwd, encoding: "utf8", windowsHide: true, maxBuffer: 1 << 28, stdio: ["ignore", "pipe", "pipe"], ...opts });
}

export function gitShow(cwd, rev, path) {
  try {
    return git(cwd, ["show", `${rev}:${path}`]);
  } catch {
    return null;
  }
}

// Calls a branch added to the old central list since `base` (the merge base with main).
export function addedCalls(baseText, headText) {
  const before = new Set(centralCalls(baseText ?? "").map(key));
  return centralCalls(headText ?? "").filter((c) => !before.has(key(c)));
}

// Problems in one file's hook blocks: a block that skips super.Run drops every hook compiled before it.
export function staticResetProblems(text) {
  const problems = [];
  let hooks = 0;
  const layers = {};
  for (const { cls, text: block } of hookBlocks(text)) {
    hooks += hookedCalls(block).length;
    layers[cls] = (layers[cls] || 0) + 1;
    if (!/super\.Run\(\s*done\s*,\s*apply\s*\)\s*;/.test(block)) problems.push(`modded class ${cls} must call super.Run(done, apply) first, or every hook before it is lost`);
    if (!/override\s+void\s+Run\(\s*notnull\s+array<string>\s+done\s*,\s*bool\s+apply\s*\)/.test(block)) problems.push(`modded class ${cls} overrides Run(notnull array<string> done, bool apply)`);
  }
  return { problems, hooks, layers };
}

// Problems in the whole tree's hook layers ({ class: count }): a chain longer than MAX_PER_BUCKET overflows the VM stack.
export function bucketCapProblems(layers) {
  const out = [];
  for (const [cls, n] of Object.entries(layers))
    if (n > MAX_PER_BUCKET) out.push(`${cls}: ${n} modded layers, the cap is ${MAX_PER_BUCKET} (a longer super.Run chain overflows the script VM stack); put new hooks in another LF_StaticResetHooks<N> bucket`);
  return out;
}
