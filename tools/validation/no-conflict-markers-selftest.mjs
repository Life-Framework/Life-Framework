// tools/validation/no-conflict-markers-selftest.mjs - validate-no-conflict-markers.
// inputs: tools/validation/validate-no-conflict-markers.mjs, tools/lib/conflict-markers.mjs, tools/validation/no-conflict-markers-selftest.mjs
//
// Runs the validator over a throwaway tree under the OS temp dir:
//   - a clean tree passes, also with indented marker examples and a setext-style underline;
//   - a full ours/base/theirs block fails with file:line for each marker;
//   - a lone base marker (the docs/features/map.md leftover) fails;
//   - a CRLF file's separator fails;
//   - a `-text` asset, a NUL-bearing file and an ALLOW fixture path are not flagged.
//
// red-proof: 2026-11-18 with conflictMarkerLines returning [], "a full block fails" printed FAIL.
// 2026-11-23 (#4622) with the old pattern (7 pipes plus a space only): "a bare or unlabeled diff3 base
// marker" and "a bare diff3 base marker (no label) fails" printed FAIL, exit 1; the fix passes both.
import { spawnSync } from "node:child_process";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { scratchDir } from "../lib/scratch.mjs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { allowMatcher, conflictMarkerLines } from "../lib/conflict-markers.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

// Built by repeat() so this file never holds a marker at column 0 itself.
const L = "<".repeat(7);
const B = "|".repeat(7);
const E = "=".repeat(7);
const R = ">".repeat(7);
const block = `a\n${L} ours\nx\n${B} base\ny\n${E}\nz\n${R} theirs\nb\n`;

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-no-conflict-markers.mjs");
const dir = scratchDir("lf-no-conflict-markers-");
const docs = join(dir, "docs");
const fixtures = join(dir, "tools", "merge", "fixtures");
for (const d of [docs, fixtures]) mkdirSync(d, { recursive: true });
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_CONFLICTMARKERS_ROOT: dir }, encoding: "utf8" });

try {
  check(conflictMarkerLines(block).map((h) => h.line).join() === "2,4,6,8", "conflictMarkerLines reports the 1-based lines");
  check(conflictMarkerLines(`${L}\n${E}=\n ${E}\n ${B}\n=== a ===\n`).length === 0, "an ours marker needs its trailing space, a separator exactly 7 =, a base marker column 0");
  check(conflictMarkerLines(`a\n${B}\nb\n${B}x\n${B}|\n`).map((h) => h.line).join() === "2,4,5", "a bare or unlabeled diff3 base marker (7 pipes) fails (#4622)");
  check(conflictMarkerLines(`a\r\n${E}\r\nb\r\n`).length === 1, "a CRLF separator counts");
  const allow = allowMatcher(["tools/merge/fixtures/**", "docs/*.md"]);
  check(allow("tools/merge/fixtures/a/b.txt") && allow("docs/x.md") && !allow("docs/a/x.md") && !allow("tools/merge/x.txt"), "allowMatcher: ** crosses folders, * does not");

  writeFileSync(join(dir, ".gitattributes"), "* text=auto eol=lf\n*.dat -text\n");
  writeFileSync(join(docs, "clean.md"), `# T\n\nIndented example:\n\n    ${L} ours\n    ${E}\n\nTitle\n${E}${E}${E}\n`);
  let r = run();
  check(r.status === 0, `a clean tree passes (${r.stdout.trim()})`);

  writeFileSync(join(docs, "planted.md"), block);
  r = run();
  check(r.status === 1 && [2, 4, 6, 8].every((n) => r.stdout.includes(`planted.md:${n} `)), `a full block fails with every line (${r.stdout.trim().split("\n")[0]})`);

  writeFileSync(join(docs, "planted.md"), `a\n${B} base\nb\n`);
  r = run();
  check(r.status === 1 && r.stdout.includes("planted.md:2 "), "a lone base marker fails");

  writeFileSync(join(docs, "planted.md"), `a\n${B}\nb\n`);
  r = run();
  check(r.status === 1 && r.stdout.includes("planted.md:2 "), "a bare diff3 base marker (no label) fails");

  writeFileSync(join(docs, "planted.md"), `a\r\n${E}\r\nb\r\n`);
  r = run();
  check(r.status === 1 && r.stdout.includes("planted.md:2 "), "a CRLF separator fails");

  writeFileSync(join(docs, "planted.md"), "fine\n");
  writeFileSync(join(dir, "blob.dat"), block);
  writeFileSync(join(dir, "nul.bin"), Buffer.concat([Buffer.from([0]), Buffer.from(block)]));
  writeFileSync(join(fixtures, "case.txt"), block);
  r = run();
  check(r.status === 0, `-text asset, NUL file and ALLOW fixture are not flagged (${r.stdout.trim()})`);
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) {
  console.log(`no-conflict-markers-selftest: ${failed} FAIL`);
  process.exit(1);
}
console.log("no-conflict-markers-selftest: OK");
