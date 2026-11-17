// tools/test/convert-self-register.mjs - make LF_Test_* files register themselves (#2610, slice #2616).
//
//   node tools/test/convert-self-register.mjs <file-or-glob>... [--dry-run]
//
// Appends the LF_TestRegistration subclass (addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistration.c) after the
// test class of each file that lacks one. The tier comes from the file's `// tier:` line, so the registration's
// Tier() matches it (gen-test-registry.ps1 checks that). Idempotent: a file with a registration is left alone.
// Refuses, and does not touch, a file with no `// tier:` line, no test class, or two test classes (one registration
// per file). A glob uses `*`, `?` and `[A-L]` in the last path segment. Exit 0 when nothing was refused, else 1.
// After a batch run `tools\cli regen-tests`: converted files leave the Enforce registry.
import { readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const TIERS = "LOGIC|WORLD|PERSISTENCE|PERF";

function registrationBlock(testClass, tier) {
  const name = testClass.replace(/^LF_Test_/, "");
  return [
    "//------------------------------------------------------------------------------------------------",
    "//! Self-registration (LF_TestRegistration.c): this file is the whole of adding this test.",
    `class LF_TestRegistration_${name} : LF_TestRegistration`,
    "{",
    `\tprotected static bool s_bRegistered = LF_TestManager.Enqueue(new LF_TestRegistration_${name}());`,
    "",
    "\toverride LF_TestTier Tier()",
    "\t{",
    `\t\treturn LF_TestTier.${tier};`,
    "\t}",
    "",
    "\toverride LF_Test Create()",
    "\t{",
    `\t\treturn new ${testClass}();`,
    "\t}",
    "}",
  ].join("\n");
}

// -> { status: "converted"|"already"|"refused", text, reason? }. Pure: no file access.
export function convertText(text, fileName = "file") {
  if (/class\s+\w+\s*:\s*LF_TestRegistration\b/.test(text)) return { status: "already", text };
  const classes = [...text.matchAll(/class\s+(LF_Test_\w+)\s*:\s*LF_Test\b/g)];
  if (classes.length === 0) return { status: "refused", text, reason: `${fileName}: no 'class LF_Test_X : LF_Test'` };
  if (classes.length > 1) {
    return { status: "refused", text, reason: `${fileName}: ${classes.length} test classes (${classes.map((c) => c[1]).join(", ")}); one registration per file` };
  }
  const cls = classes[0];
  const tiers = [...text.slice(0, cls.index).matchAll(/\/\/\s*tier:\s*(LOGIC|WORLD|PERSISTENCE|PERF)\b/g)];
  if (tiers.length === 0) return { status: "refused", text, reason: `${fileName}: no '// tier: ${TIERS}' comment above ${cls[1]}` };
  const tier = tiers[tiers.length - 1][1];

  // The test class closes with a "}" in column 0; the registration goes right after it.
  const close = text.indexOf("\n}", cls.index);
  if (close < 0) return { status: "refused", text, reason: `${fileName}: cannot find the end of ${cls[1]}` };
  const at = close + 2;
  return { status: "converted", text: `${text.slice(0, at)}\n\n${registrationBlock(cls[1], tier)}${text.slice(at)}`, tier };
}

function expand(arg) {
  if (!/[*?[]/.test(basename(arg))) return [arg];
  const dir = dirname(arg);
  let pattern = "";
  let inClass = false;
  for (const ch of basename(arg)) {
    if (inClass) {
      pattern += ch;
      if (ch === "]") inClass = false;
    } else if (ch === "[") {
      pattern += ch;
      inClass = true;
    } else if (ch === "*") pattern += ".*";
    else if (ch === "?") pattern += ".";
    else if (/[A-Za-z0-9_-]/.test(ch)) pattern += ch;
    else pattern += "[" + ch + "]";
  }
  const re = new RegExp(`^${pattern}$`);
  return readdirSync(dir).filter((f) => re.test(f)).sort().map((f) => join(dir, f));
}

function main(argv) {
  const dry = argv.includes("--dry-run");
  const args = argv.filter((a) => a !== "--dry-run");
  if (args.length === 0) {
    console.log("usage: node tools/test/convert-self-register.mjs <file-or-glob>... [--dry-run]");
    return 2;
  }
  const files = args.flatMap(expand).filter((f) => statSync(f).isFile());
  const count = { converted: 0, already: 0, refused: 0 };
  for (const file of files) {
    const r = convertText(readFileSync(file, "utf8"), basename(file));
    count[r.status]++;
    if (r.status === "refused") console.log(`REFUSED ${r.reason}`);
    else if (r.status === "converted" && !dry) writeFileSync(file, r.text);
  }
  console.log(`convert-self-register: files=${files.length} converted=${count.converted} already=${count.already} refused=${count.refused}${dry ? " (dry run)" : ""}`);
  return count.refused > 0 ? 1 : 0;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) process.exit(main(process.argv.slice(2)));
