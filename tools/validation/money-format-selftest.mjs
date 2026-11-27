// tools/validation/money-format-selftest.mjs - validate-money-format.
// inputs: tools/validation/validate-money-format.mjs, tools/lib/money-format.mjs, tools/lib/scratch.mjs
//
// Checks the scan on planted sources (a "$" glued to an amount, a "$%1" format and a hand-written {currency:sym}%1
// are flagged; FormatMoney, comments, log lines, the escape mark and a "$" that is not before a placeholder pass), the
// command line on a scratch tree (Tests/ and LF_MoneyFormat.c are skipped), and that the real tree is clean.
//
// red-proof: 2026-11-27 with the dollar-format rule removed from RULES, "string.Format $%1 is flagged" printed FAIL and
// the run exited 1; restored, it prints "money-format-selftest: OK".
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { code, scanSource } from "../lib/money-format.mjs";
import { scratchDir } from "../lib/scratch.mjs";
import { scanTree } from "./validate-money-format.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};
const here = dirname(fileURLToPath(import.meta.url));
const wrap = (body) => `class A\n{\n\tstatic string M(int amount)\n\t{\n${body}\n\t}\n}\n`;
const hits = (body) => scanSource(wrap(body));

// Hand-built money is red.
check(hits('\t\treturn "$" + amount;').length === 1, 'a "$" + amount is flagged');
check(hits('\t\treturn amount.ToString() + "$";').length === 1, 'a trailing + "$" is flagged too');
check(hits('\t\treturn "Paid: " + "$" + amount;').length === 1, 'a "$" in the middle of a concatenation is flagged');
check(hits('\t\treturn string.Format("$%1", amount);').length === 1, "string.Format $%1 is flagged");
check(hits('\t\treturn string.Format("You paid $%2 for %1", name, amount);').length === 1, "a $%2 inside a longer format is flagged");
check(hits('\t\treturn string.Format("Fee {currency:sym}%1", amount);').length === 1, "a hand-written {currency:sym}%1 is flagged");
check(hits('\t\treturn WidgetManager.Translate("#K", "$" + amount);').length === 1, "a Translate argument built by hand is flagged");
check(hits('\t\treturn "$" +\n\t\t\tamount;').length === 1, 'a "$" + at a line end is flagged on its own line');

// The shared formatter, comments, log lines and the mark are green.
check(hits("\t\treturn LF_MoneyFormat.FormatMoney(amount);").length === 0, "FormatMoney passes");
check(hits('\t\treturn Fee("$" );').length === 0, 'a lone "$" argument that is not concatenated passes');
check(hits('\t\t// return "$" + amount;').length === 0, "a comment passes");
check(hits('\t\t/* string.Format("$%1", amount) */').length === 0, "a one-line block comment passes");
check(hits('\t\t/*\n\t\treturn "$" + amount;\n\t\t*/\n\t\treturn 1;').length === 0, "a block comment over lines passes");
check(hits('\t\tLF_Debug.Info("Shop", string.Format("took $%1", amount));').length === 0, "an LF_Debug line passes (a log is not player text)");
check(hits('\t\tPrint("fee $%1");').length === 0, "a Print line passes");
check(hits('\t\treturn "$" + amount; // lf-money-format-ok: a fixture literal, never shown').length === 0, "the mark on the line silences a site");
check(hits('\t\t// lf-money-format-ok: the search pattern, not an amount\n\t\treturn "$" + amount;').length === 0, "the mark on the line above silences a site");
check(hits('\t\treturn "$" + amount; // lf-money-format-ok:').length === 1, "a mark with no reason does not silence a site");
check(hits('\t\treturn "Costs 5$ each";').length === 0, "a dollar not before a placeholder passes");
check(hits('\t\treturn "50% off $ sale";').length === 0, "a dollar with no placeholder passes");
check(hits('\t\treturn "$" + amount;')[0].line === 5, "the line number is the source line");
check(code('a "b // c" // d').trim() === 'a "b // c"', "code() cuts a comment but not a // inside a string");

// The command line, on a scratch tree.
const scratch = scratchDir("lf-moneyfmt-");
const scripts = join(scratch, "addons", "LifeFramework", "Scripts", "Game");
mkdirSync(join(scripts, "Tests"), { recursive: true });
mkdirSync(join(scripts, "Feature", "Money"), { recursive: true });
writeFileSync(join(scripts, "LF_Bad.c"), wrap('\t\treturn "$" + amount;'));
writeFileSync(join(scripts, "Tests", "LF_Test_Skipped.c"), wrap('\t\treturn "$" + amount;'));
writeFileSync(join(scripts, "Feature", "Money", "LF_MoneyFormat.c"), wrap('\t\treturn "$" + amount;'));
const run = () => spawnSync(process.execPath, [join(here, "validate-money-format.mjs")], {
  env: { ...process.env, LF_MONEYFMT_ROOT: scratch },
  encoding: "utf8",
});
const bad = run();
check(bad.status === 1 && /FAIL addons\/LifeFramework\/Scripts\/Game\/LF_Bad\.c:\d+ /.test(bad.stdout), "cli: the scratch tree fails, naming file:line");
check(!/Skipped/.test(bad.stdout) && !/LF_MoneyFormat\.c/.test(bad.stdout), "cli: Tests/ and the formatter itself are not scanned");
writeFileSync(join(scripts, "LF_Bad.c"), wrap("\t\treturn LF_MoneyFormat.FormatMoney(amount);"));
const good = run();
check(good.status === 0, `cli: the scratch tree passes once the site uses the formatter (${good.status})`);

// The real tree.
const repo = scanTree(join(here, "..", ".."));
check(repo.length === 0, `repo: no hand-built money site (${repo.map((s) => `${s.file}:${s.line}`)})`);

if (failed) {
  console.log(`money-format-selftest: ${failed} check(s) failed`);
  process.exit(1);
}
console.log("money-format-selftest: OK (hand-built money red, formatter, logs and the mark green)");
