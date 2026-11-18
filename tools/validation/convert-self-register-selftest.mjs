// tools/validation/convert-self-register-selftest.mjs
// inputs: tools/test/convert-self-register.mjs, tools/lib/scratch.mjs
//
// Proves tools/test/convert-self-register.mjs (#2616): it inserts the [LF_TestInfo(LF_TestTier.X)] attribute (#2610 R1b)
// on the line above the class of a test file that lacks one, takes the tier from the file's `// tier:` line, leaves a converted file alone (idempotent),
// handles a file with several test classes (each gets the nearest tier above it), writes pack: true for a pack test (path under a pack or --pack), and refuses a file with no tier or no test class. Cases run on fixture files in a scratch folder, once
// through the library function and once through the command line with a glob.
// red-proof: Run against the #2616 converter (one attribute per file, pack unknown) the two-class and pack checks fail (2026-11-18, #2617).
// red-proof: Observed 2026-11-17 before the converter existed: the import failed (ERR_MODULE_NOT_FOUND) and the run exited 1.
// Rewritten 2026-11-18 for the attribute shape: run against the old class-appending converter it printed FAIL for every attribute check.

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { convertText, isPackPath } from "../test/convert-self-register.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failures = 0;
function check(cond, name) {
  if (cond) return;
  failures++;
  console.log(`FAIL ${name}`);
}

const PLAIN = `// tier: WORLD
//! demo/plain: a plain test.
class LF_Test_Plain : LF_Test
{
	override string GetName()
	{
		return "demo/plain";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(true, "ok");
	}
}
// red-proof: Observed 2026-11-17: asserting false failed; restored.
`;

const NO_TIER = `//! demo/no-tier
class LF_Test_NoTier : LF_Test
{
}
`;

const TWO = `// tier: LOGIC
class LF_Test_One : LF_Test
{
}

// tier: WORLD
//! second class
class LF_Test_Two : LF_Test
{
}
`;

const NO_CLASS = `// tier: LOGIC
class LF_Helper
{
}
`;

// Plain: converted, tier from the comment, attribute on the line directly above the class, nothing else added.
const plain = convertText(PLAIN, "LF_Test_Plain.c");
check(plain.status === "converted", "plain file is converted");
check(plain.text.includes("[LF_TestInfo(LF_TestTier.WORLD)]\nclass LF_Test_Plain : LF_Test\n"), "attribute sits on the line directly above the class");
check(plain.text.includes("// tier: WORLD\n//! demo/plain"), "the // tier: comment stays");
check(!/LF_TestRegistration|static bool|Enqueue/.test(plain.text), "no registration class, no static initializer");
check(plain.text.length === PLAIN.length + "[LF_TestInfo(LF_TestTier.WORLD)]\n".length, "exactly one line is added");
check(plain.text.trimEnd().split("\n").pop().startsWith("// red-proof:"), "the trailing red-proof line stays last");
check(!plain.text.includes("\r"), "output has no CR");

// A `//!` doc comment between the tier line and the class: the attribute still goes right above the class.
check(convertText("// tier: LOGIC\n//! doc\n//! more\nclass LF_Test_Doc : LF_Test\n{\n}\n", "d").text.includes("//! more\n[LF_TestInfo(LF_TestTier.LOGIC)]\nclass LF_Test_Doc"), "doc comment stays above the attribute");
// Tier names: each is taken from the comment.
for (const tier of ["LOGIC", "PERSISTENCE", "PERF"]) {
  check(convertText(`// tier: ${tier}\nclass LF_Test_T : LF_Test\n{\n}\n`, "t").text.includes(`[LF_TestInfo(LF_TestTier.${tier})]`), `tier ${tier} is copied`);
}
// Idempotent: a converted file is left byte-identical.
const again = convertText(plain.text, "LF_Test_Plain.c");
check(again.status === "already", "a converted file is reported already");
check(again.text === plain.text, "a converted file is not changed");

// Refusals.
check(convertText(NO_TIER, "LF_Test_NoTier.c").status === "refused", "a file with no tier is refused");
check(convertText(NO_CLASS, "LF_Test_Helper.c").status === "refused", "a file with no test class is refused");

// Two classes: each gets its own attribute from the nearest tier above it.
const count = (s) => (s.match(/\[LF_TestInfo\(/g) || []).length;
const two = convertText(TWO, "LF_Test_One.c");
check(two.status === "converted", "a two-class file is converted");
check(two.text.includes("[LF_TestInfo(LF_TestTier.LOGIC)]\nclass LF_Test_One : LF_Test"), "first class gets its own tier");
check(two.text.includes("//! second class\n[LF_TestInfo(LF_TestTier.WORLD)]\nclass LF_Test_Two : LF_Test"), "second class gets the nearest tier above it");
check(count(two.text) === 2, "exactly two attributes");
check(convertText(two.text, "x").status === "already", "a converted two-class file is already");
// Half done: one class has its attribute, the other gets one.
const half = convertText(two.text.replace("[LF_TestInfo(LF_TestTier.WORLD)]\n", ""), "x");
check(half.status === "converted" && count(half.text) === 2, "a class missing its attribute is added, the other left alone");

// Pack: pack: true on every class; a pack path is recognised.
const pack = convertText(TWO, "LF_Test_EveronX.c", { pack: true });
check(pack.text.includes("[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]\nclass LF_Test_One") && pack.text.includes("[LF_TestInfo(LF_TestTier.WORLD, pack: true)]\nclass LF_Test_Two"), "pack: true on every class");
check(isPackPath("addons/LF_Everon1989/Scripts/Game/Tests/LF_Test_EveronX.c") && isPackPath(["C:", "lf", "addons", "LF_Other", "Scripts", "Game", "Tests", "a.c"].join("\\")), "a pack path is a pack path");
check(!isPackPath("addons/LifeFramework/Scripts/Game/Tests/LF_Test_A.c"), "a core path is not");

// Command line: glob, in-place write, exit code.
const dir = scratchDir("lf-convert-selftest-");
mkdirSync(dir, { recursive: true });
writeFileSync(join(dir, "LF_Test_Plain.c"), PLAIN);
writeFileSync(join(dir, "LF_Test_Done.c"), plain.text.replace(/Plain/g, "Done"));
const bin = fileURLToPath(new URL("../test/convert-self-register.mjs", import.meta.url));
const glob = join(dir, "LF_Test_*.c");
const run = spawnSync(process.execPath, [bin, glob], { encoding: "utf8" });
check(run.status === 0, `command line exits 0 on a clean batch (got ${run.status}: ${run.stdout}${run.stderr})`);
check(/\[LF_TestInfo\(LF_TestTier\.WORLD\)\]\nclass LF_Test_Plain/.test(readFileSync(join(dir, "LF_Test_Plain.c"), "utf8")), "command line converts the matched file");
check(/converted=1 already=1 refused=0/.test(run.stdout), `command line summary counts (got ${run.stdout.trim()})`);

// A character class in the glob selects by first letter (the batch uses LF_Test_[A-L]*.c).
writeFileSync(join(dir, "LF_Test_Zed.c"), PLAIN.replace(/Plain/g, "Zed"));
const ranged = spawnSync(process.execPath, [bin, join(dir, "LF_Test_[A-L]*.c")], { encoding: "utf8" });
check(/files=1 /.test(ranged.stdout), `a [A-L] glob matches Done only, not Plain or Zed (got ${ranged.stdout.trim()})`);
check(!/LF_TestInfo/.test(readFileSync(join(dir, "LF_Test_Zed.c"), "utf8")), "a file outside the glob is not converted");

writeFileSync(join(dir, "LF_Test_NoTier.c"), NO_TIER);
const bad = spawnSync(process.execPath, [bin, glob], { encoding: "utf8" });
check(bad.status === 1, "command line exits 1 when a file is refused");
check(readFileSync(join(dir, "LF_Test_NoTier.c"), "utf8") === NO_TIER, "a refused file is not written");

if (failures > 0) {
  console.log(`convert-self-register-selftest: FAILED (${failures})`);
  process.exit(1);
}
console.log("convert-self-register-selftest: OK");
