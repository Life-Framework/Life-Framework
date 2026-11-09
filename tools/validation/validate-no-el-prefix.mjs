// No EL_ identifiers in addons/ (AGENTS.md golden rule 5, issue #334).
//
// Every class, method, define and key is LF_; an EL_ name can collide with the real
// EveronLife mod on the same server. The one exception is a resource file whose path is
// a {GUID}path reference in saves and prefabs (addons/.../EL_JailCell.et and its siblings):
// its basename may still appear (never as a class or enum declaration).
//
// red-proof: 2026-11-09, a scratch `class EL_ZZProbe {}` in Scripts/Game/Core/LF_ZZProbe.c
// printed `FAIL addons/LifeFramework/Scripts/Game/Core/LF_ZZProbe.c:1 EL_ZZProbe` and
// exited 1; with the file removed it prints OK.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], {
  cwd: root,
  encoding: "utf8",
  maxBuffer: 1 << 28,
}).split("\0").filter(Boolean);

const BINARY = /\.(png|jpe?g|tif|edds|xob|bin|ico|wav|ogg|mp4|rdb|blob|dds|gif|pdf|lock)$/i;
const resourceBases = new Set();
for (const f of files) {
  const b = basename(f).replace(/\.meta$/i, "");
  if (b.startsWith("EL_") && !b.endsWith(".c")) resourceBases.add(b.replace(/\.[^.]+$/, ""));
}

const TOKEN = /(?<![A-Z0-9])EL_\w*/g;
const bad = [];
for (const f of files) {
  if (BINARY.test(f) || !existsSync(join(root, f))) continue;
  const buf = readFileSync(join(root, f));
  if (buf.includes(0)) continue;
  const lines = buf.toString("utf8").split("\n");
  lines.forEach((line, i) => {
    for (const m of line.matchAll(TOKEN)) {
      if (resourceBases.has(m[0]) && !/\b(class|enum)\s+EL_/.test(line)) continue;
      bad.push(`FAIL ${f}:${i + 1} ${m[0]}`);
    }
  });
  if (/(^|\/)EL_[^/]*\.c$/.test(f)) bad.push(`FAIL ${f}: script file keeps the EL_ prefix`);
}

if (bad.length) {
  console.log(bad.slice(0, 60).join("\n"));
  if (bad.length > 60) console.log(`... ${bad.length - 60} more`);
  console.log(`validate-no-el-prefix: ${bad.length} EL_ identifier(s). Rename to LF_ (AGENTS.md rule 5).`);
  process.exit(1);
}
console.log(`validate-no-el-prefix: OK (${files.length} addon file(s); ${resourceBases.size} EL_-named resource path(s) kept)`);
