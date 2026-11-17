// tools/validation/no-lone-cr-selftest.mjs - validate-no-lone-cr.
// inputs: tools/validation/validate-no-lone-cr.mjs, tools/lib/lone-cr.mjs, tools/validation/no-lone-cr-selftest.mjs
//
// Runs the validator over a throwaway tree under the OS temp dir:
//   - an LF-only script passes;
//   - a script whose `//` comment holds a lone CR (the `\return` accident) fails with file:line;
//   - CRLF-only text, a .cmd with CRLF, a `-text` asset and a NUL-bearing file are not flagged.
//
// red-proof: 2026-11-17 with loneCrLines returning [], "a lone CR in a comment fails" printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { loneCrLines, minusTextMatcher } from "../lib/lone-cr.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-no-lone-cr.mjs");
const dir = mkdtempSync(join(tmpdir(), "lf-no-lone-cr-"));
const scripts = join(dir, "addons", "LifeFramework", "Scripts", "Game", "Feature", "Map");
const tools = join(dir, "tools");
for (const d of [scripts, tools]) mkdirSync(d, { recursive: true });
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_LONECR_ROOT: dir }, encoding: "utf8" });

try {
  check(loneCrLines("a\nb\rc\n").join() === "2", "loneCrLines reports the 1-based line");
  check(loneCrLines("a\r\nb\r\n").length === 0, "CRLF pairs are not lone");
  check(loneCrLines("x\r").join() === "1", "a CR at end of file is lone");
  check(loneCrLines("a\r\rb").join() === "1", "two CRs on one line report it once");
  const skip = minusTextMatcher("*.wav -text\naddons/X/Language/*.conf -text\n# c\n*.cmd text eol=crlf\n");
  check(skip("a/b/c.wav") && skip("addons/X/Language/a.conf") && !skip("addons/Y/Language/a.conf") && !skip("a/b.cmd"), "minusTextMatcher follows .gitattributes -text");

  writeFileSync(join(dir, ".gitattributes"), "* text=auto eol=lf\n*.cmd text eol=crlf\n*.wav -text\n");
  writeFileSync(join(scripts, "LF_Clean.c"), "class LF_Clean\n{\n\t//! \\return the row.\n\tvoid Go() {}\n}\n");
  let r = run();
  check(r.status === 0, `LF-only tree passes (${r.stdout.trim()})`);

  writeFileSync(join(scripts, "LF_CrlfOnly.c"), "class LF_CrlfOnly\r\n{\r\n}\r\n");
  writeFileSync(join(tools, "run.cmd"), "@echo off\r\nrem x\r\n");
  writeFileSync(join(tools, "clip.wav"), Buffer.from([82, 73, 70, 70, 13, 1, 2]));
  writeFileSync(join(tools, "blob.dat"), Buffer.from([0, 13, 65]));
  r = run();
  check(r.status === 0, `CRLF text, .cmd, -text asset and NUL file are not flagged (${r.stdout.trim()})`);

  // The accident: "\return" written through a shell that expanded "\r" to a real CR byte.
  writeFileSync(join(scripts, "LF_Planted.c"), "class LF_Planted\n{\n\t//! the row. \return the row.\n\tvoid Go() {}\n}\n");
  r = run();
  check(r.status === 1 && /addons\/LifeFramework\/Scripts\/Game\/Feature\/Map\/LF_Planted\.c:3 /.test(r.stdout), `a lone CR in a comment fails (${r.stdout.trim().split("\n")[0]})`);

  rmSync(join(scripts, "LF_Planted.c"));
  r = run();
  check(r.status === 0, "passes again once the CR is gone");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) {
  console.log(`no-lone-cr-selftest: ${failed} FAIL`);
  process.exit(1);
}
console.log("no-lone-cr-selftest: OK");
