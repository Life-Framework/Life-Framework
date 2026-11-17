// tools/validation/features-index-selftest.mjs
// inputs: tools/lib/features-index.mjs, tools/validation/validate-features-index.mjs
//
// Proves the generated docs/features.md index: it regenerates identically, an added area file
// changes it, a ### row added to an area file does not, and a hand-added row in the index fails
// validate-features-index (the red case).
// red-proof: with the comparison in checkIndex disabled the red case printed
// "FAIL hand-added row in the index was accepted" and exited 1 (2026-11-16).
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { renderIndex, splitSections } from "../lib/features-index.mjs";
import { checkIndex } from "./validate-features-index.mjs";

let failed = false;
const fail = (m) => { console.log(`FAIL ${m}`); failed = true; };
const root = mkdtempSync(join(tmpdir(), "lf-features-"));
try {
  mkdirSync(join(root, "docs", "features"), { recursive: true });
  const put = (f, t) => writeFileSync(join(root, f), t);
  put("docs/features/core.md", "# Core\n\n### `LF_A` — row\n- a\n");
  put("docs/features/money-economy.md", "# Money & Economy\n\n### `LF_B` — row\n- b\n");
  const index = renderIndex(root);
  put("docs/features.md", index);
  if (renderIndex(root) !== index) fail("index does not regenerate identically");
  if (checkIndex(root).length) fail("a fresh index failed the validator");

  put("docs/features/core.md", `${readFileSync(join(root, "docs/features/core.md"), "utf8")}\n### \`LF_C\` — new row\n- c\n`);
  if (renderIndex(root) !== index) fail("a ### row in an area file changed the index");
  if (checkIndex(root).length) fail("a ### row in an area file failed the validator");

  put("docs/features/quests.md", "# Quests\n");
  if (renderIndex(root) === index) fail("adding an area file did not change the index");
  if (!checkIndex(root).length) fail("a stale index (new area file) passed the validator");
  put("docs/features.md", renderIndex(root));
  if (checkIndex(root).length) fail("regenerated index failed the validator");

  put("docs/features.md", `${renderIndex(root)}\n### \`LF_Hand\` — row\n- hand\n`);
  const errs = checkIndex(root);
  if (!errs.length) fail("hand-added row in the index was accepted");
  else if (!errs[0].includes("regen-features")) fail("the error does not name tools\cli regen-features");

  const s = splitSections("# T\n\npre\n## A (x)\n\nbody\n## B\nb\n");
  if (s.length !== 2 || s[0].slug !== "a" || s[0].body !== "# A (x)\n\nbody\n" || s[1].body !== "# B\nb\n") fail(`splitSections: ${JSON.stringify(s)}`);
} finally {
  rmSync(root, { recursive: true, force: true });
}
if (failed) process.exit(1);
console.log("features-index-selftest: OK (regenerates, area file changes it, ### rows do not, hand-added row fails)");
