// tools/test/convert-self-register.mjs - make LF_Test_* files register themselves (#2610, slice #2616).
//
//   node tools/test/convert-self-register.mjs <file-or-glob>... [--dry-run]
//
// Inserts `[LF_TestInfo(LF_TestTier.<tier>)]` on the line directly above the `class LF_Test_X : LF_Test` of each file
// that lacks one (Tests/LF_TestInfo.c: LF_TestManager finds the class by reflection, no static initializer). The tier
// comes from the file's `// tier:` line, which stays (gen-test-registry.ps1 fails when the two disagree). Idempotent: a
// file that already has an attribute is left alone. Refuses, and does not touch, a file with no `// tier:` line, no
// test class, or two test classes (multi-class and pack files are #2617). A glob uses `*`, `?` and `[A-L]` in the last
// path segment. Exit 0 when nothing was refused, else 1.
// After a batch run `tools\cli regen-tests`: converted files leave the Enforce registry (a comment row remains).
import { readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const TIERS = "LOGIC|WORLD|PERSISTENCE|PERF";

function attributeLine(tier) {
  return `[LF_TestInfo(LF_TestTier.${tier})]`;
}

// -> { status: "converted"|"already"|"refused", text, reason? }. Pure: no file access.
export function convertText(text, fileName = "file") {
  if (/\[LF_TestInfo\(/.test(text)) return { status: "already", text };
  const classes = [...text.matchAll(/class\s+(LF_Test_\w+)\s*:\s*LF_Test\b/g)];
  if (classes.length === 0) return { status: "refused", text, reason: `${fileName}: no 'class LF_Test_X : LF_Test'` };
  if (classes.length > 1) {
    return { status: "refused", text, reason: `${fileName}: ${classes.length} test classes (${classes.map((c) => c[1]).join(", ")}); one attribute per file, multi-class files are #2617` };
  }
  const cls = classes[0];
  const tiers = [...text.slice(0, cls.index).matchAll(/\/\/\s*tier:\s*(LOGIC|WORLD|PERSISTENCE|PERF)\b/g)];
  if (tiers.length === 0) return { status: "refused", text, reason: `${fileName}: no '// tier: ${TIERS}' comment above ${cls[1]}` };
  const tier = tiers[tiers.length - 1][1];

  // The attribute goes on its own line right above the class declaration.
  const at = text.lastIndexOf("\n", cls.index) + 1;
  return { status: "converted", text: `${text.slice(0, at)}${attributeLine(tier)}\n${text.slice(at)}`, tier };
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
