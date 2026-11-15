// Proves tools/playtest/opening/b3-rules.mjs, the desk half of beat B3 of the opening-day script (#1601): the rules
// `tools\cli rules export` reads from the pack table, the site's website/content/rules.json and the Discord block carry
// the same rule text. The two board lines are BLOCKED by #1280 (the town board is a town-pass card).
// inputs: tools/**, website/content/**, docs/community/**
//
// Fixtures: tools/validation/fixtures/opening-b3/ (a four-row string table, a matching and a changed site file, a
// matching and a changed Discord block). The selftest never writes tmp/ and never reads the live site file.
//   1. matching texts PASS (export, site, Discord), and the board lines print BLOCKED by #1280;
//   2. one changed rule in the site file FAILs naming that rule, and so does one in the Discord block;
//   3. a missing Discord block file is BLOCKED by its card (#232, RULESDISCORD), not a pass and not a fail;
//   4. the first Discord file that exists wins (docs/community/discord-rules.md before tmp/rules-discord.md);
//   5. a table with no rules is a FAIL on the export line.
//
// red-proof: 2026-11-15, run before tools/playtest/opening/b3-rules.mjs existed it printed
// "FAIL cannot load b3-rules.mjs" and exited 1.
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const fix = join(here, "fixtures", "opening-b3");
const f = (name) => join(fix, name);
const fails = [];
const expect = (ok, msg) => {
  if (!ok) fails.push(msg);
};

let b3;
try {
  b3 = await import(pathToFileURL(join(root, "tools", "playtest", "opening", "b3-rules.mjs")).href);
} catch (e) {
  console.log(`FAIL cannot load b3-rules.mjs: ${e.message}`);
  process.exit(1);
}
const { runB3 } = b3;
const run = (o) => runB3({ table: f("table.st"), site: f("site-match.json"), discord: [f("discord-match.md")], ...o });
const line = (lines, id) => lines.find((l) => l.id === id);

// 1. matching texts
const ok = run({});
for (const id of ["rules-export", "rules-site", "rules-discord"]) expect(line(ok, id)?.verdict === "PASS", `${id} should PASS on matching texts: ${JSON.stringify(line(ok, id))}`);
for (const id of ["board-rules", "board-own"]) {
  const l = line(ok, id);
  expect(l?.verdict === "BLOCKED" && l.reason.includes("#1280"), `${id} should be BLOCKED by #1280: ${JSON.stringify(l)}`);
}
expect(!ok.some((l) => l.verdict === "FAIL"), "matching texts should hold no FAIL");

// 2. one changed rule
const siteBad = run({ site: f("site-changed.json") });
const s = line(siteBad, "rules-site");
expect(s?.verdict === "FAIL" && s.reason.includes("rule 2") && s.reason.includes("walk away"), `a changed site rule should FAIL naming rule 2: ${JSON.stringify(s)}`);
expect(line(siteBad, "rules-discord")?.verdict === "PASS", "the Discord block should still PASS beside a changed site file");
const discBad = run({ discord: [f("discord-changed.md")] });
const d = line(discBad, "rules-discord");
expect(d?.verdict === "FAIL" && d.reason.includes("rule 3") && d.reason.includes("Combat logging is fine"), `a changed Discord rule should FAIL naming rule 3: ${JSON.stringify(d)}`);
expect(line(discBad, "rules-site")?.verdict === "PASS", "the site should still PASS beside a changed Discord block");

// 3. a missing Discord block file
const noDisc = run({ discord: [f("no-such-block.md")] });
const nd = line(noDisc, "rules-discord");
expect(nd?.verdict === "BLOCKED" && nd.reason.includes("#232"), `a missing Discord block should be BLOCKED by #232: ${JSON.stringify(nd)}`);
expect(!noDisc.some((l) => l.verdict === "FAIL"), "a missing Discord block is not a FAIL");
expect(line(noDisc, "rules-site")?.verdict === "PASS", "the site check still runs when the Discord block is missing");

// 4. the first existing Discord file wins
const first = run({ discord: [f("no-such-block.md"), f("discord-changed.md"), f("discord-match.md")] });
expect(line(first, "rules-discord")?.verdict === "FAIL", "the first Discord file that exists is the one compared");

// 5. a table with no rules, and a missing site file
const empty = run({ table: f("no-such-table.st") });
expect(line(empty, "rules-export")?.verdict === "FAIL", `a missing table should FAIL rules-export: ${JSON.stringify(line(empty, "rules-export"))}`);
const noSite = run({ site: f("no-such-site.json") });
expect(line(noSite, "rules-site")?.verdict === "FAIL", "a missing site file should FAIL rules-site");

if (fails.length) {
  for (const x of fails) console.log(`FAIL ${x}`);
  console.log(`SUMMARY opening-b3-selftest fail ${fails.length}`);
  process.exit(1);
}
console.log("SUMMARY opening-b3-selftest ok");
