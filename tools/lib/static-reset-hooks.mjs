// tools/lib/static-reset-hooks.mjs - world-end static resets are hook classes per feature, found by reflection.
//
// History. Core/LF_StaticReset.c first held one central list (`LF_X.ResetStatics();` per feature) that
// nearly every feature PR appended to, so the ship queue ran those PRs one after another (#1870 moved
// it). Its replacement, one `modded class LF_StaticResetHooks` layer per feature whose Run called
// super.Run, nested one script frame per layer: at about 123 layers Run overflowed the script stack
// (#2034) and took the suite and the world-end reset down with it.
//
// Then each hook registered itself from a static initializer. Every static field initializer in the
// Game module (vanilla included) compiles into one function whose size the engine caps ("Too many
// instructions per function"); the hooks were two thirds of ours and the module was a day from not
// compiling (#2755). Now each class that owns statics adds a small hook class in its own file with no
// registration at all; LF_StaticReset finds every LF_StaticResetHook subclass by reflection
// (typename.GetInheritedTypes) on first use and walks a flat array sorted by owner:
//
//   //------------------------------------------------------------------------------------------------
//   //! World-end reset of LF_X's statics (LF_StaticReset finds it by reflection).
//   class LF_X_StaticResetHook : LF_StaticResetHook
//   {
//   	override string Owner()
//   	{
//   		return "LF_X";
//   	}
//
//   	override void Apply()
//   	{
//   		LF_X.ResetStatics();
//   	}
//   }
//
// Pure helpers here are shared by the codemod (tools/codemods/static-reset-registry.mjs), `tools\cli
// heal-hotspots` (moves a branch's old central-list or modded-layer edit into this form), the feature
// scaffold, and validate-registries.mjs (rejects a central call, a modded layer or a static Register).
// tools/validation/static-reset-hooks-selftest.mjs proves them.

import { execFileSync } from "node:child_process";
import { readFileSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";

export const CENTRAL = "addons/LifeFramework/Scripts/Game/Core/LF_StaticReset.c";
export const SCRIPTS = "addons/LifeFramework/Scripts/Game";

const CALL = /^\s*(LF_[A-Za-z0-9_]+)\.([A-Za-z0-9_]+)\(\);\s*$/;
const MODDED = /^modded\s+class\s+LF_StaticResetHooks\b[\s\S]*?\n\}[^\n]*\n?/gm;
// A registry hook class: header line through its closing brace at column 0.
const HOOK = /^class\s+(LF_[A-Za-z0-9_]+)\s*:\s*LF_StaticResetHook\b[\s\S]*?\n\}/gm;

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
    if (m && m[1] !== "LF_Debug" && m[1] !== "LF_StaticResetHooks") out.push({ cls: m[1], method: m[2], line: i + 1 });
    if (depth < 0 || (depth === 0 && /\}/.test(l))) inReset = false;
  });
  return out;
}

// Calls registered through the old `modded class LF_StaticResetHooks` layers (the form #2034 retired).
export function moddedCalls(text) {
  const out = [];
  for (const block of String(text).match(MODDED) || []) {
    for (const l of block.split("\n")) {
      const m = CALL.exec(l);
      if (m) out.push({ cls: m[1], method: m[2] });
    }
  }
  return out;
}

// Registry hook classes in a file: [{ name, owner, calls: [{cls, method}], block }].
export function registryHooks(text) {
  const out = [];
  for (const m of String(text).matchAll(HOOK)) {
    const block = m[0];
    const owner = /override\s+string\s+Owner\(\)\s*\{\s*return\s+"([^"]+)"\s*;/.exec(block);
    const apply = /override\s+void\s+Apply\(\)\s*\{([\s\S]*?)\n\t\}/.exec(block);
    const calls = [];
    for (const l of (apply ? apply[1] : "").split("\n")) {
      const c = CALL.exec(l);
      if (c) calls.push({ cls: c[1], method: c[2] });
    }
    out.push({ name: m[1], owner: owner ? owner[1] : null, calls, block });
  }
  return out;
}

// Every reset call a file registers, in either form.
export function hookedCalls(text) {
  return [...registryHooks(text).flatMap((h) => h.calls), ...moddedCalls(text)];
}

const key = (c) => `${c.cls}.${c.method}`;

// The hook class for `calls`, which all reset one owning class (calls[0].cls).
export function hookClass(calls) {
  const cls = calls[0].cls;
  const name = `${cls}_StaticResetHook`;
  return [
    "//------------------------------------------------------------------------------------------------",
    `//! World-end reset of ${cls}'s statics (LF_StaticReset finds it by reflection).`,
    `class ${name} : LF_StaticResetHook`,
    "{",
    "\toverride string Owner()",
    "\t{",
    `\t\treturn "${cls}";`,
    "\t}",
    "",
    "\toverride void Apply()",
    "\t{",
    ...calls.map((c) => `\t\t${c.cls}.${c.method}();`),
    "\t}",
    "}",
    "",
  ].join("\n");
}

// Hook classes for `calls` (grouped by owning class, first-seen order).
export function hookBlock(calls) {
  const groups = new Map();
  for (const c of calls) {
    if (!groups.has(c.cls)) groups.set(c.cls, []);
    groups.get(c.cls).push(c);
  }
  return [...groups.values()].map(hookClass).join("\n");
}

// `text` with hook classes for those of `calls` it does not register yet appended (idempotent).
export function addHooks(text, calls) {
  const have = new Set(hookedCalls(text).map(key));
  const todo = [];
  for (const c of calls) if (!have.has(key(c)) && !todo.some((t) => key(t) === key(c))) todo.push(c);
  if (!todo.length) return text;
  const base = String(text).replace(/\s*$/, "\n");
  return `${base}\n${hookBlock(todo)}`;
}

// `text` with every `modded class LF_StaticResetHooks` layer (and the comment header right above it)
// replaced by registry hook classes at the same spot. Returns { text, calls } (calls moved).
export function migrateText(text) {
  const src = String(text);
  const calls = [];
  const out = src.replace(/(?:^\/\/-{20,}\n)?(?:^\/\/![^\n]*\n)*(^modded\s+class\s+LF_StaticResetHooks\b[\s\S]*?\n\}[^\n]*\n?)/gm, (whole, block) => {
    const cs = moddedCalls(block);
    calls.push(...cs);
    if (!cs.length) return "";
    const have = new Set(registryHooks(src).flatMap((h) => h.calls).map(key));
    const todo = cs.filter((c) => !have.has(key(c)));
    return todo.length ? hookBlock(todo) : "";
  });
  return { text: out.replace(/\n{3,}$/, "\n").replace(/\n*$/, "\n"), calls };
}

// The central file: the hook base class and the flat registry. No feature names in it.
export function centralFile() {
  return `//------------------------------------------------------------------------------------------------
//! Drops process-global (static) feature state when a world ends, so the next world loaded in the
//! same process (Workbench play restart, server mission change) starts clean.
//!
//! Called from the LF_GameModeRoleplay destructor (world teardown), next to
//! LF_PlayerAccountManager.Reset. It is NOT called at game start: placed breach targets, jail
//! cells, garages, and spawn points register in their OnPostInit, which runs before the game
//! mode starts; a start-time reset would wipe them.
//!
//! Nothing is listed here and nothing registers. Each class that owns statics adds an
//! LF_StaticResetHook subclass in its own file (the feature scaffold writes it; template in
//! tools/lib/static-reset-hooks.mjs); the first ResetAll or Registered finds every subclass by
//! reflection (typename.GetInheritedTypes), spawns each once and sorts them by owner (#2755).
//! A static initializer per hook ate the Game module's one static-init function, which the engine
//! caps ("Too many instructions per function"); a central list made every feature PR touch this
//! file; a \`modded class\` layer per feature overflowed the script stack at about 123 (#2034).
//! validate-registries.mjs rejects all three.
//------------------------------------------------------------------------------------------------
class LF_StaticResetHook : Managed
{
	//------------------------------------------------------------------------------------------------
	//! The class whose statics this hook drops (LF_StaticReset.Registered lists it).
	string Owner()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Drop the owner's statics.
	void Apply()
	{
	}
}

//------------------------------------------------------------------------------------------------
class LF_StaticReset
{
	protected static int s_iRuns;
	protected static bool s_bDiscovered;
	protected static ref array<ref LF_StaticResetHook> s_aHooks;

	//------------------------------------------------------------------------------------------------
	//! Add a hook (once per owner). Discovery adds every LF_StaticResetHook subclass already; this is
	//! for a hook object built at run time. Never call it from a static initializer (#2755).
	static bool Register(LF_StaticResetHook hook)
	{
		if (!hook)
			return false;

		if (!s_aHooks)
			s_aHooks = new array<ref LF_StaticResetHook>();

		string owner = hook.Owner();
		foreach (LF_StaticResetHook h : s_aHooks)
		{
			if (h && h.Owner() == owner)
				return true;
		}

		s_aHooks.Insert(hook);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Find every LF_StaticResetHook subclass once, spawn it and add it, sorted by owner.
	protected static void EnsureDiscovered()
	{
		if (s_bDiscovered)
			return;

		s_bDiscovered = true;
		array<typename> types = {};
		typename base = LF_StaticResetHook;
		base.GetInheritedTypes(types);

		map<string, ref LF_StaticResetHook> byOwner = new map<string, ref LF_StaticResetHook>();
		array<string> owners = {};
		foreach (typename t : types)
		{
			if (t == base)
				continue;

			LF_StaticResetHook hook = LF_StaticResetHook.Cast(t.Spawn());
			if (!hook)
			{
				LF_Debug.Error("StaticReset", "could not spawn reset hook " + t.ToString());
				continue;
			}

			string owner = hook.Owner();
			if (owner.IsEmpty())
			{
				LF_Debug.Error("StaticReset", "reset hook " + t.ToString() + " names no owner; skipped");
				continue;
			}

			if (byOwner.Contains(owner))
				continue;

			byOwner.Insert(owner, hook);
			owners.Insert(owner);
		}

		owners.Sort();
		foreach (string o : owners)
		{
			Register(byOwner.Get(o));
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetAll()
	{
		EnsureDiscovered();
		int count;
		if (s_aHooks)
		{
			foreach (LF_StaticResetHook hook : s_aHooks)
			{
				if (!hook)
					continue;

				hook.Apply();
				count++;
			}
		}

		s_iRuns = s_iRuns + 1;
		LF_Debug.Log("StaticReset", string.Format("feature statics reset (run %1, %2 hooks)", s_iRuns, count));
	}

	//------------------------------------------------------------------------------------------------
	//! The hooks by owner class name, without running them.
	static array<string> Registered()
	{
		EnsureDiscovered();
		array<string> done = new array<string>();
		if (!s_aHooks)
			return done;

		foreach (LF_StaticResetHook hook : s_aHooks)
		{
			if (hook)
				done.Insert(hook.Owner());
		}

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

// Every .c file under the script root, repo-relative with forward slashes.
export function scriptFiles(root) {
  return walk(join(root, SCRIPTS)).map((abs) => abs.slice(root.length + 1).replace(/\\/g, "/"));
}

// Map class name -> repo-relative file that declares it (`class LF_X`, also `sealed class`).
export function classFiles(root) {
  const map = new Map();
  for (const rel of scriptFiles(root)) {
    const text = readFileSync(join(root, rel), "utf8");
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

// A static field initializer that calls LF_StaticReset.Register: each one costs a share of the Game
// module's single, size-capped static-init function (#2755). Discovery finds the hook without it.
const STATIC_REGISTER = /^[ \t]*(?:(?:protected|private|const)\s+)*static\b[^;\n(]*=\s*LF_StaticReset\.Register\(/gm;

// Problems in one file's static-reset hooks: any modded layer (the chain overflows, #2034), a static
// initializer that registers (#2755), and a hook class with no owner or no Apply.
export function staticResetProblems(text) {
  const problems = [];
  const src = String(text);
  for (const _ of src.match(MODDED) || [])
    problems.push("modded class LF_StaticResetHooks is retired (#2034: one nested super.Run per feature overflowed the stack); add an LF_StaticResetHook subclass (tools/codemods/static-reset-registry.mjs converts it)");
  const hooks = registryHooks(src);
  for (const m of src.matchAll(STATIC_REGISTER)) {
    const line = src.slice(0, m.index).split("\n").length;
    problems.push(`line ${line}: a static initializer calls LF_StaticReset.Register; delete it, LF_StaticReset finds every LF_StaticResetHook subclass by reflection (#2755: all static initializers share one size-capped function)`);
  }
  for (const h of hooks) {
    if (!h.owner) problems.push(`${h.name} overrides string Owner() returning the class it resets`);
    if (!/override\s+void\s+Apply\(\)/.test(h.block)) problems.push(`${h.name} overrides void Apply() with the reset call`);
  }
  return { problems, hooks: hooks.length };
}
