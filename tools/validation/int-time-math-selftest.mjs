// tools/validation/int-time-math-selftest.mjs - validate-int-time-math (#2787).
// inputs: tools/validation/validate-int-time-math.mjs, tools/lib/int-time-math.mjs, tools/validation/int-time-math-baseline.json, tools/lib/scratch.mjs
//
// Checks the name heuristic on planted sources (an int seconds/ms/unix argument flagged; float names, counts,
// comments, strings, `// lf-float:` marks and the LF_Utils helpers pass), the baseline ratchet (a listed site passes,
// an unlisted one fails, a fixed one is stale, no reason fails), the command line on a scratch tree, and that the
// real baseline holds only entries with a reason and the real tree is clean against it.
//
// red-proof: 2026-11-18 with the time-word match in isTimeName disabled, "int seconds argument is flagged" printed
// FAIL and the run exited 1; restored, it prints "int-time-math-selftest: OK".
import { spawnSync } from "node:child_process";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { compare, isTimeName, keyHits, scanSource, strip, words } from "../lib/int-time-math.mjs";
import { scratchDir } from "../lib/scratch.mjs";
import { scanTree } from "./validate-int-time-math.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};
const here = dirname(fileURLToPath(import.meta.url));
const wrap = (body) => `class A\n{\n\tstatic int M(int seconds, float fSecs, int nowUnix, int count)\n\t{\n${body}\n\t}\n}\n`;
const hits = (body) => scanSource(wrap(body));

// The name heuristic.
check(words("m_iOnDutySeconds").join() === "on,duty,seconds", `words: m_iOnDutySeconds -> ${words("m_iOnDutySeconds")}`);
check(words("nowMs").join() === "now,ms", `words: nowMs -> ${words("nowMs")}`);
check(isTimeName("nowUnix") && isTimeName("m_iRentIntervalHours") && isTimeName("cooldownSeconds") && isTimeName("deadline"), "isTimeName: unix, rent hours, cooldown, deadline");
check(!isTimeName("count") && !isTimeName("m_iPrice") && !isTimeName("nowhere") && !isTimeName("windowing"), "isTimeName: count, price, nowhere, windowing are not times");

// Planted sources.
check(hits("\t\treturn Math.Max(0, seconds);").length === 1, "int seconds argument is flagged");
check(hits("\t\treturn Math.Min(nowUnix + 5, 2000000000);").length === 1, "a unix-named argument is flagged in Math.Min");
check(hits("\t\treturn Math.Clamp(nowUnix - 5, 0, 99);").length === 1, "a unix-named argument is flagged in Math.Clamp");
check(hits("\t\treturn Math.Max(0, Math.Min(seconds, 9));").length === 2, "a nested pair is two sites");
check(hits("\t\treturn Math.Max(0, cfg.m_iSearchCooldownSeconds);").length === 1, "an int member named for a time is flagged");
check(hits("\t\treturn Math.Max(0, fSecs);").length === 0, "a float parameter named for a time passes");
check(hits("\t\treturn Math.Max(0, cfg.m_fShowSeconds);").length === 0, "an m_f member passes");
check(hits("\t\tfloat left = 2;\n\t\tfloat elapsed = 1;\n\t\treturn Math.Max(0, left - elapsed);").length === 0, "a local declared float passes");
check(hits("\t\treturn Math.Max(count, 1);").length === 0, "a plain count passes");
check(hits("\t\treturn LF_Utils.MaxInt(0, seconds);").length === 0, "the LF_Utils helper passes");
check(hits("\t\t// return Math.Max(0, seconds);").length === 0, "a comment passes");
check(hits('\t\tstring s = "Math.Max(0, seconds)";').length === 0, "a string passes");
check(hits("\t\t// lf-float: the delta is a float frame time\n\t\treturn Math.Max(0, seconds);").length === 0, "lf-float on the line above silences a site");
check(hits("\t\treturn Math.Max(0, seconds); // lf-float: float seconds").length === 0, "lf-float on the line silences a site");
check(hits("\t\treturn Math.Max(0,\n\t\t\tseconds);").length === 1, "a call split over two lines is found");
check(hits("\t\treturn Math.Max(0, seconds);")[0].method === "M", "the method name is found");
check(strip('a // Math.Max(seconds)\n"Math.Min(now)"').split("\n").every((l) => !l.includes("Math")), "strip blanks comments and strings");

// The ratchet.
const found = keyHits("F.c", hits("\t\ta = Math.Max(0, seconds);\n\t\tb = Math.Min(seconds, 9);"), new Map());
const k = "F.c::M";
check(found.get(k) === 2, "keyHits counts two sites under file::method");
const full = { [k]: { count: 2, reason: "a fixture site kept on purpose" } };
check(compare(found, full).fresh.length === 0 && compare(found, full).stale.length === 0, "ratchet: a listed site passes");
check(compare(found, {}).fresh.length === 1, "ratchet: an unlisted site fails");
check(compare(found, { [k]: { count: 1, reason: "one of two" } }).fresh.length === 1, "ratchet: more sites than allowed fails");
check(compare(found, { [k]: { count: 3, reason: "one fixed since" } }).stale.length === 1, "ratchet: a fixed site is stale");
check(compare(found, { ...full, "Gone.c::Old": { count: 1, reason: "file deleted since" } }).stale.length === 1, "ratchet: an entry for a vanished site is stale");
check(compare(found, { [k]: { count: 2, reason: "" } }).noReason.length === 1, "ratchet: an entry with no reason fails");

// The command line, on a scratch tree.
const scratch = scratchDir("lf-inttime-");
const scripts = join(scratch, "addons", "LifeFramework", "Scripts", "Game");
mkdirSync(join(scripts, "Tests"), { recursive: true });
writeFileSync(join(scripts, "LF_Bad.c"), wrap("\t\treturn Math.Max(0, seconds);"));
writeFileSync(join(scripts, "Tests", "LF_Test_Skipped.c"), wrap("\t\treturn Math.Max(0, seconds);"));
const baselineFull = join(scratch, "full.json");
const baselineEmpty = join(scratch, "empty.json");
writeFileSync(baselineEmpty, "{}");
writeFileSync(baselineFull, JSON.stringify({ "LifeFramework/Scripts/Game/LF_Bad.c::M": { count: 1, reason: "a fixture site kept on purpose" } }));
const run = (baseline) => spawnSync(process.execPath, [join(here, "validate-int-time-math.mjs")], {
  env: { ...process.env, LF_INTTIME_ROOT: scratch, LF_INTTIME_BASELINE: baseline },
  encoding: "utf8",
});
const empty = run(baselineEmpty);
check(empty.status === 1 && /FAIL addons\/LifeFramework\/Scripts\/Game\/LF_Bad\.c:\d+ M Math\.Max/.test(empty.stdout), "cli: the scratch tree fails an empty baseline, naming file:line");
check(!/Skipped/.test(empty.stdout), "cli: Tests/ is not scanned");
const ok = run(baselineFull);
check(ok.status === 0, `cli: the scratch tree passes its full baseline (${ok.status}: ${ok.stdout.trim().split("\n").pop()})`);

// The real baseline and tree.
const real = JSON.parse(readFileSync(join(here, "int-time-math-baseline.json"), "utf8"));
const bare = Object.entries(real).filter(([, b]) => typeof b.reason !== "string" || b.reason.trim().length < 12).map(([key]) => key);
check(bare.length === 0, `baseline: every entry has a reason (${bare})`);
const repo = scanTree(join(here, "..", ".."));
const verdict = compare(repo.found, real);
check(verdict.fresh.length === 0, `repo: no site outside the baseline (${verdict.fresh.map((f) => f.key)})`);
check(verdict.stale.length === 0, `repo: no stale baseline entry (${verdict.stale.map((f) => f.key)})`);

if (failed) {
  console.log(`int-time-math-selftest: ${failed} check(s) failed`);
  process.exit(1);
}
console.log("int-time-math-selftest: OK (planted int time sites red, float and helper forms green, ratchet holds)");
