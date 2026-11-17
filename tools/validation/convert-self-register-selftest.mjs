// tools/validation/convert-self-register-selftest.mjs
// inputs: tools/test/convert-self-register.mjs, tools/lib/scratch.mjs
//
// Proves tools/test/convert-self-register.mjs (#2616): it inserts the [LF_TestInfo(LF_TestTier.X)] attribute (#2610 R1b)
// on the line above the class of a test file that lacks one, takes the tier from the file's `// tier:` line, leaves a converted file alone (idempotent),
// and refuses a file with no tier or with two test classes. Cases run on fixture files in a scratch folder, once
// through the library function and once through the command line with a glob.
// red-proof: Observed 2026-11-17 before the converter existed: the import failed (ERR_MODULE_NOT_FOUND) and the run exited 1.
// Rewritten 2026-11-18 for the attribute shape: run against the old class-appending converter it printed FAIL for every attribute check.

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { convertText } from "../test/convert-self-register.mjs";
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
check(convertText(TWO, "LF_Test_One.c").status === "refused", "a file with two test classes is refused");
check(convertText(NO_CLASS, "LF_Test_Helper.c").status === "refused", "a file with no test class is refused");

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
