// Proves docs/owner/log-lines.md (#1417): the success lines an owner reads in the first boot log, and the
// table of the fifteen commonest error lines with what each means and the fix.
//   - every success line passes judgeBootLog (tools/release/boot-check.mjs), alone (no mod or script
//     failure) and together (the whole set reaches "Entered online game state." and a join);
//   - the page names the pack's mission, by the GUID of its .conf.meta;
//   - the error table has exactly fifteen rows, each with a meaning and a fix, and each row's pattern (the
//     first backticked text of its first cell) appears verbatim in docs/debug-log-triage.md;
//   - docs/owner/install.md links the page.
// inputs: docs/owner/log-lines.md, docs/owner/install.md, docs/debug-log-triage.md, tools/release/boot-check.mjs, addons/LF_Everon1989/Missions/LF_Everon1989.conf.meta
// red-proof: before docs/owner/log-lines.md existed the run printed "FAIL docs/owner/log-lines.md exists";
// with one success line changed to "SCRIPT (E): ..." it printed "FAIL success line 5 ... stops at scripts".
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { judgeBootLog } from "../release/boot-check.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const pagePath = join(root, "docs", "owner", "log-lines.md");
expect(existsSync(pagePath), "docs/owner/log-lines.md exists");
const page = existsSync(pagePath) ? readFileSync(pagePath, "utf8") : "";
const triage = readFileSync(join(root, "docs", "debug-log-triage.md"), "utf8");
const install = readFileSync(join(root, "docs", "owner", "install.md"), "utf8");

// The text under a "## <heading>" up to the next "## ".
function section(md, heading) {
  const m = md.match(new RegExp(`^## ${heading}[^\\n]*\\n([\\s\\S]*?)(?=^## |$(?![\\s\\S]))`, "m"));
  return m ? m[1] : "";
}

// Success lines: the first ```text block under "## The lines of a good boot", one log line per row.
const good = section(page, "The lines of a good boot");
const block = good.match(/```text\n([\s\S]*?)```/);
const success = block ? block[1].split("\n").map((l) => l.trim()).filter(Boolean) : [];
expect(success.length >= 6, `the good-boot block lists at least six lines (found ${success.length})`);

const stamp = (i, line) => {
  const ch = /Entered online game state/.test(line) ? "NETWORK      :" : /^Config load/.test(line) ? "DEFAULT      :" : "SCRIPT       :";
  return `10:00:${String(i).padStart(2, "0")}.000 ${ch} ${line}`;
};
success.forEach((line, i) => {
  const r = judgeBootLog(stamp(i, line), { client: false });
  expect(r.stage !== "mods" && r.stage !== "scripts", `success line ${i + 1} (${line.slice(0, 50)}) stops at ${r.stage}: ${r.line}`);
});
const whole = success.map((l, i) => stamp(i, l)).join("\n");
const readyOnly = judgeBootLog(whole, { client: false });
expect(readyOnly.ok, `the success lines together pass judgeBootLog without a client: stage ${readyOnly.stage}`);
const joined = judgeBootLog(whole, { client: true });
expect(joined.ok, `the success lines together pass judgeBootLog with a client: stage ${joined.stage}`);
expect(success.some((l) => /Entered online game state\./.test(l)), "the block has the server-up line");
expect(success.some((l) => /\[LFDebug:Persistence\] system state SETUP -> ACTIVE/.test(l)), "the block has the framework line");
const packLine = success.find((l) => /^Config load @"\{[0-9A-F]{16}\}Missions\/LF_Everon1989\.conf"/.test(l));
expect(!!packLine, "the block has the pack mission line");
if (packLine) {
  const guid = packLine.match(/\{([0-9A-F]{16})\}/)[1];
  const meta = readFileSync(join(root, "addons", "LF_Everon1989", "Missions", "LF_Everon1989.conf.meta"), "utf8");
  expect(meta.includes(`{${guid}}Missions/LF_Everon1989.conf`), `the pack mission GUID ${guid} is the pack header's`);
}
// A line that would fail the boot must still be caught: the judge is the page's oracle.
expect(judgeBootLog(whole + "\n10:01:00.000 SCRIPT    (E): Unknown type 'LF_Foo'", { client: false }).stage === "scripts", "the oracle still catches a SCRIPT (E) line");

// Error table: "## The fifteen commonest error lines", rows `| <pattern> | <meaning> | <fix> |`.
const errs = section(page, "The fifteen commonest error lines");
const rows = errs.split("\n").filter((l) => /^\|/.test(l)).filter((l) => !/^\|\s*-/.test(l)).slice(1);
expect(rows.length === 15, `the error table has fifteen rows (found ${rows.length})`);
const seen = new Set();
rows.forEach((row, i) => {
  const cells = row.split(/(?<!\\)\|/).slice(1, -1).map((c) => c.trim());
  expect(cells.length === 3, `error row ${i + 1} has three cells (line, meaning, fix)`);
  const [line, meaning, fix] = cells;
  const pat = line?.match(/`([^`]+)`/)?.[1];
  expect(!!pat, `error row ${i + 1} opens with a backticked log pattern`);
  if (pat) {
    expect(triage.includes(pat), `error row ${i + 1} pattern is not in docs/debug-log-triage.md: ${pat}`);
    expect(!seen.has(pat), `error row ${i + 1} repeats a pattern: ${pat}`);
    seen.add(pat);
  }
  expect((meaning ?? "").length >= 20, `error row ${i + 1} says what the line means`);
  expect((fix ?? "").length >= 20, `error row ${i + 1} gives the fix`);
});

expect(/\]\(log-lines\.md\)/.test(install), "docs/owner/install.md links log-lines.md");

for (const f of fails) console.log(`  FAIL  ${f}`);
if (fails.length) { console.log("owner-log-lines-selftest: FAIL"); process.exit(1); }
console.log("owner-log-lines-selftest: ok");
