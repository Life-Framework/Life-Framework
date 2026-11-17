// tools/validation/static-reset-hooks-selftest.mjs - the static-reset registry helpers and codemod (#2034).
// inputs: tools/lib/static-reset-hooks.mjs, tools/codemods/static-reset-registry.mjs, tools/validation/static-reset-hooks-selftest.mjs
//
// The modded `LF_StaticResetHooks` chain nested one super.Run frame per feature and overflowed the
// script stack at about 123 layers. This proves, on a throwaway tree under the OS temp dir:
//   - staticResetProblems flags a modded layer (what validate-registries fails on) and passes a hook class;
//   - the codemod turns every layer into an LF_StaticResetHook subclass (no registration line, #2755), keeps the
//     reset calls, rewrites the central file to the flat registry, and a second run changes nothing;
//   - addHooks (the scaffold and heal-hotspots) writes the new form and is idempotent.
//
// red-proof: 2026-11-17 (#2755) with the STATIC_REGISTER loop removed this printed FAIL "a hook that registers
// from a static initializer is a problem (#2755)". 2026-11-16 with the MODDED check removed from staticResetProblems this printed
// FAIL "a modded layer is a problem (validate-registries fails it)".
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { CENTRAL, addHooks, centralFile, hookedCalls, registryHooks, staticResetProblems } from "../lib/static-reset-hooks.mjs";
import { migrateRepo } from "../codemods/static-reset-registry.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const GAME = "addons/LifeFramework/Scripts/Game";
const cls = (name) => `class ${name}\n{\n\tprotected static int s_i;\n\n\tstatic void ResetStatics()\n\t{\n\t\ts_i = 0;\n\t}\n}\n`;
const layer = (names) =>
  `\n//------------------------------------------------------------------------------------------------\n//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).\nmodded class LF_StaticResetHooks\n{\n\toverride void Run(notnull array<string> done, bool apply)\n\t{\n\t\tsuper.Run(done, apply);\n${names
    .map((n) => `\t\tdone.Insert("${n}");\n\t\tif (apply)\n\t\t\t${n}.ResetStatics();\n`)
    .join("")}\t}\n}\n`;

const dir = scratchDir("lf-static-reset-selftest-");
const put = (rel, text) => {
  mkdirSync(dirname(join(dir, rel)), { recursive: true });
  writeFileSync(join(dir, rel), text);
};
const get = (rel) => readFileSync(join(dir, rel), "utf8");

try {
  const one = `${GAME}/Feature/Alpha/LF_Alpha.c`;
  const two = `${GAME}/Feature/Beta/LF_Beta.c`;
  const tail = `${GAME}/Feature/Gamma/LF_Gamma.c`;
  put(CENTRAL, "class LF_StaticResetHooks : Managed\n{\n\tvoid Run(notnull array<string> done, bool apply)\n\t{\n\t}\n}\n");
  put(one, cls("LF_Alpha") + layer(["LF_Alpha"]));
  put(two, cls("LF_Beta") + cls("LF_BetaCache") + layer(["LF_Beta", "LF_BetaCache"]));
  put(tail, cls("LF_Gamma") + layer(["LF_Gamma"]) + "\n" + cls("LF_GammaAfter"));

  check(staticResetProblems(get(one)).problems.length === 1, "a modded layer is a problem (validate-registries fails it)");

  const r = migrateRepo(dir);
  check(r.files.length === 3 && r.central && r.dupes.length === 0, `codemod: 3 files converted, central rewritten (${r.files.map((f) => f.file).join(", ")})`);
  for (const f of [one, two, tail]) {
    const t = get(f);
    check(!/modded\s+class\s+LF_StaticResetHooks/.test(t) && !/super\.Run/.test(t), `codemod: no modded layer left in ${f}`);
    const p = staticResetProblems(t);
    check(p.problems.length === 0 && p.hooks > 0, `codemod: ${f} hook classes are well formed (${p.problems.join("; ")})`);
  }
  check(registryHooks(get(two)).map((h) => `${h.name}=${h.owner}`).join() === "LF_Beta_StaticResetHook=LF_Beta,LF_BetaCache_StaticResetHook=LF_BetaCache", "codemod: a two-class layer becomes one hook per owner");
  check(/class LF_GammaAfter\b/.test(get(tail)) && /LF_Gamma\.ResetStatics\(\);/.test(get(tail)), "codemod: code after the layer is kept");
  check(!/World-end reset of this file's statics/.test(get(one)), "codemod: the old layer's comment header goes with it");
  check(get(CENTRAL) === centralFile() && /static bool Register\(LF_StaticResetHook hook\)/.test(get(CENTRAL)) && !/\bRun\(/.test(get(CENTRAL)), "codemod: central file is the flat registry, no Run chain");
  const before = [one, two, tail].map(get).join("\n");
  const again = migrateRepo(dir);
  check(again.files.length === 0 && !again.central && [one, two, tail].map(get).join("\n") === before, "codemod: second run changes nothing");

  const scaffold = addHooks(cls("LF_Delta"), [{ cls: "LF_Delta", method: "ResetStatics" }]);
  check(/class LF_Delta_StaticResetHook : LF_StaticResetHook/.test(scaffold) && !/s_bRegistered|LF_StaticReset\.Register/.test(scaffold) && staticResetProblems(scaffold).problems.length === 0, "addHooks: writes a hook class with no static registration (#2755)");
  check(addHooks(scaffold, [{ cls: "LF_Delta", method: "ResetStatics" }]) === scaffold && hookedCalls(scaffold).length === 1, "addHooks: idempotent");
  const registering = scaffold.replace("{\n\toverride string Owner()", "{\n\tprotected static bool s_bRegistered = LF_StaticReset.Register(new LF_Delta_StaticResetHook());\n\n\toverride string Owner()");
  check(registering !== scaffold && staticResetProblems(registering).problems.some((m) => /static initializer calls LF_StaticReset\.Register/.test(m)), "a hook that registers from a static initializer is a problem (#2755)");
  check(staticResetProblems("class LF_Z\n{\n\tstatic bool s_b = LF_StaticReset.Register(new LF_Y_StaticResetHook());\n}\n").problems.length === 1, "a static Register initializer outside a hook class is a problem too");
  check(staticResetProblems("class LF_Z\n{\n\tvoid Go()\n\t{\n\t\tLF_StaticReset.Register(new LF_Y_StaticResetHook());\n\t}\n}\n").problems.length === 0, "Register inside a function is fine");
} catch (e) {
  check(false, `selftest threw: ${e.stack || e.message}`);
} finally {
  rmSync(dir, { recursive: true, force: true });
}

console.log(failed ? `static-reset-hooks-selftest: ${failed} FAILED` : "static-reset-hooks-selftest: all passed");
process.exit(failed ? 1 : 0);
