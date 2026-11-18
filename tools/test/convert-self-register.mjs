// tools/test/convert-self-register.mjs - make LF_Test_* files register themselves (#2610, slice #2616).
//
//   node tools/test/convert-self-register.mjs <file-or-glob>... [--dry-run] [--pack]
//
// Inserts `[LF_TestInfo(LF_TestTier.<tier>)]` on the line directly above each `class LF_Test_X : LF_Test`
// that lacks one (Tests/LF_TestInfo.c: LF_TestManager finds the class by reflection, no static initializer). The tier
// comes from the nearest `// tier:` line above that class (a multi-class file has one per class), which stays
// (gen-test-registry.ps1 fails when the two disagree). Idempotent: a class that already has an attribute is left alone.
// A file under a pack's Scripts/Game/Tests (or with `--pack`) gets `pack: true`: the pack leg runs it and the core never
// names it (#2617). Refuses, and does not touch, a file with no test class or a class with no `// tier:` line above it.
// A glob uses `*`, `?` and `[A-L]` in the last
// path segment. Exit 0 when nothing was refused, else 1.
// After a batch run `tools\cli regen-tests`: converted files leave the Enforce registry (a comment row remains).
import { readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const TIERS = "LOGIC|WORLD|PERSISTENCE|PERF";

function attributeLine(tier, pack) {
  return `[LF_TestInfo(LF_TestTier.${tier}${pack ? ", pack: true" : ""})]`;
}

// A pack's tests sit under addons/<pack>/Scripts/Game/Tests; the core's are under addons/LifeFramework.
export function isPackPath(file) {
  const p = String(file).replace(/\\/g, "/");
  return /addons\/(?!LifeFramework\/)[^/]+\/Scripts\/Game\/Tests\//.test(p);
}

// -> { status: "converted"|"already"|"refused", text, reason? }. Pure: no file access.
// Every `class LF_Test_X : LF_Test` gets its own attribute with the nearest `// tier:` above it (the generator's rule);
// a class that already has one is left alone. `pack` adds `pack: true`.
export function convertText(text, fileName = "file", { pack = false } = {}) {
  const classes = [...text.matchAll(/class\s+(LF_Test_\w+)\s*:\s*LF_Test\b/g)];
  if (classes.length === 0) return { status: "refused", text, reason: `${fileName}: no 'class LF_Test_X : LF_Test'` };
  const inserts = [];
  const tiers = [];
  for (const cls of classes) {
    const at = text.lastIndexOf("\n", cls.index) + 1;
    // Already carries one: the nearest non-blank, non-comment line above the class is an LF_TestInfo attribute.
    const above = text.slice(0, at).split("\n").reverse().find((l) => l.trim() !== "" && !/^\s*\/\//.test(l));
    if (above && /^\s*\[LF_TestInfo\(/.test(above)) continue;
    const found = [...text.slice(0, cls.index).matchAll(/\/\/\s*tier:\s*(LOGIC|WORLD|PERSISTENCE|PERF)\b/g)];
    if (found.length === 0) return { status: "refused", text, reason: `${fileName}: no '// tier: ${TIERS}' comment above ${cls[1]}` };
    const tier = found[found.length - 1][1];
    tiers.push(tier);
    inserts.push({ at, line: attributeLine(tier, pack) });
  }
  if (inserts.length === 0) return { status: "already", text };
  let out = text;
  for (const ins of inserts.reverse()) out = `${out.slice(0, ins.at)}${ins.line}\n${out.slice(ins.at)}`;
  return { status: "converted", text: out, tier: tiers[0], count: inserts.length };
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
  const forcePack = argv.includes("--pack");
  const args = argv.filter((a) => a !== "--dry-run" && a !== "--pack");
  if (args.length === 0) {
    console.log("usage: node tools/test/convert-self-register.mjs <file-or-glob>... [--dry-run] [--pack]");
    return 2;
  }
  const files = args.flatMap(expand).filter((f) => statSync(f).isFile());
  const count = { converted: 0, already: 0, refused: 0 };
  for (const file of files) {
    const r = convertText(readFileSync(file, "utf8"), basename(file), { pack: forcePack || isPackPath(file) });
    count[r.status]++;
    if (r.status === "refused") console.log(`REFUSED ${r.reason}`);
    else if (r.status === "converted" && !dry) writeFileSync(file, r.text);
  }
  console.log(`convert-self-register: files=${files.length} converted=${count.converted} already=${count.already} refused=${count.refused}${dry ? " (dry run)" : ""}`);
  return count.refused > 0 ? 1 : 0;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) process.exit(main(process.argv.slice(2)));
