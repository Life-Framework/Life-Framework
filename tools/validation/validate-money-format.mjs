// A player-facing amount is formatted by LF_MoneyFormat, never by hand (money-format, playtest 2026-11-27 round 4).
// inputs: addons/*/Scripts/**/*.c, tools/lib/money-format.mjs
//
// The crane payout toast read "$1040" beside the HUD's "+$1,040" because a caller passed `paid.ToString()` and another
// built "$" + amount. The shared formatter (LF_MoneyFormat.FormatMoney, Feature/Money/LF_MoneyFormat.c) groups the
// thousands, writes the minus outside the symbol and uses the pack's currency symbol. This fails a production script
// line that glues a dollar sign onto an amount:  "$" + amount,  string.Format("$%1", amount).
// A row of the language table that writes {currency:sym}%1 is fine: LF_Terms.Text groups that slot (MoneyArg).
//
// Scope: addons/*/Scripts/** except Tests/ and LF_MoneyFormat.c. Log lines (LF_Debug.*, Print*) are not text a player
// reads. A real exception is marked `// lf-money-format-ok: <why>` on its line or the line above.
//
// LF_MONEYFMT_ROOT points the scan at another tree (the selftest uses it).
//
// red-proof: see money-format-selftest.mjs.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { scanSource } from "../lib/money-format.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = process.env.LF_MONEYFMT_ROOT || join(here, "..", "..");

function walk(dir, out) {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) {
      if (e.name !== "Tests") walk(p, out);
    } else if (e.name.endsWith(".c") && e.name !== "LF_MoneyFormat.c") out.push(p);
  }
  return out;
}

/** Production script files, repo-relative with forward slashes. */
export function productionScripts(base) {
  const addons = join(base, "addons");
  const files = [];
  for (const a of existsSync(addons) ? readdirSync(addons) : []) {
    const scripts = join(addons, a, "Scripts");
    if (existsSync(scripts)) walk(scripts, files);
  }
  return files.map((f) => relative(base, f).split("\\").join("/")).sort();
}

/** Every hand-built money site in the tree: [{ file, line, rule, why, text }]. */
export function scanTree(base) {
  const sites = [];
  for (const f of productionScripts(base)) {
    for (const h of scanSource(readFileSync(join(base, f), "utf8"))) sites.push({ file: f, ...h });
  }
  return sites;
}

if (process.argv[1] && process.argv[1].endsWith("validate-money-format.mjs")) {
  const sites = scanTree(root);
  for (const s of sites) console.log(`FAIL ${s.file}:${s.line} ${s.why}: ${s.text.slice(0, 120)}`);
  console.log(`money-format: ${sites.length} hand-built money site(s)`);
  if (sites.length > 0) {
    console.log("  Format an amount with LF_MoneyFormat.FormatMoney(amount) (\"$1,040\", \"-$150\"), or pass the raw int to a");
    console.log("  {currency:sym}%1 row of LF_Terms.Text. A real exception: `// lf-money-format-ok: <why>` on the line or above.");
    process.exit(1);
  }
}
