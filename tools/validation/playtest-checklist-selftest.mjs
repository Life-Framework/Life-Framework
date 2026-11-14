// Proves the playtest checklist and verdict tooling (#402, #403) on fixtures, with no GitHub:
// inputs: tools/**, docs/playtest/smoke-list.md
//   1. manual steps come from the merged PRs of the open issues; a repeated step is shown once (`same steps as #N`);
//   2. issues group by place in route order, and 2-player, restart, JIP, dedicated and desk steps are marked and blocked apart,
//      while "no restart needed" is not a restart step;
//   3. the top-hour cut ranks issues under open chapters and epics first, then big player flows (even from a later milestone), then the rest, polish and
//      tooling last, and does not spend leftover minutes on filler while a flow waits; one line lists every issue one walk proves, and a tick covers all of them (#402);
//      it takes M2 before M3, stays inside the budget (smoke list included), and --all takes everything;
//   4. the fixed smoke list (docs/playtest/smoke-list.md) is always at the top of the checklist (#403);
//   5. every issue line has a pass box and a fail box; the ticked file reads back, and `verdicts` prints the verified
//      or regression gh commands and a comment, never runs them, and reports a failed smoke check.
// red-proof: with `owner.has(k)` forced false in the step placement, "a repeated step is shown once" failed (#12 showed 3
// steps); with the budget test changed to `<= budget + 1000`, "the top hour stays inside its budget" failed (got 136 min).
// Ranking: with the chapter test forced false, "an issue under an open chapter is tier 0" failed (and 8 more); with the polish test forced false, "polish and
// visual-only items are tier 3" failed; with folding disabled, "two issues one walk proves share one line" failed. All restored.
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { main } from "../commands/playtest-checklist.mjs";
import { buildItems, parseSmokeList, parseVerdicts, rankItems, renderChecklist, selectItems, splitSteps, tagsOf, totalMinutes, verdictCommands } from "../lib/playtest-checklist.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const body = (steps, refs) => `Refs ${refs}\n\n## Summary\n\nx\n\n## Manual testing\n\n${steps}\n\n🤖 Generated with [Claude Code](https://claude.com/claude-code)\n`;
const issue = (number, title, milestone = "M2 Living Town", labels = ["needs-playtest"]) => ({ number, title, milestone: { title: milestone }, labels: labels.map((name) => ({ name })) });

const issues = [
  issue(11, "Bank: teller stands on the footing"),
  issue(12, "Bank: alarm flash at dusk"),
  issue(13, "Police: car light bar"),
  issue(14, "Fuel till pays out"),
  issue(15, "Jail work shortens the sentence"),
  issue(16, "Account: two characters per account"),
  issue(17, "Late joiner sees the plot"),
  issue(18, "Outreach: join page", "M2.5 Showcase and Outreach", ["in-dev"]),
  issue(19, "M3 config overlay", "M3 Framework Release"),
  issue(20, "No steps written"),
  issue(21, "Dev tooling film rig", "Development", ["needs-playtest", "track:dev"]),
];
const prs = [
  { number: 101, title: "bank", body: body("1. Walk to the bank (177, 258). Talk to the teller; withdraw 100.\n2. Look at the teller's feet.\n", "#11, #12") },
  { number: 102, title: "bank alarm", body: body("1. Walk to the bank (177, 258). Talk to the teller; withdraw 100.\n2. Drill the vault at dusk: the flash lights the site.\n", "#12") },
  { number: 103, title: "police", body: body("**Steps**\n\n1. Walk to the police station car (243.5, 246).\n2. Check the bar touches the roof. No restart needed.\n", "#13") },
  { number: 104, title: "fuel", body: body("- At the fuel station kiosk, rob the till: it pays 150.\n", "#14") },
  { number: 105, title: "jail", body: body("1. Get arrested in the playtest town and scrub the floor in the jail.\n2. With a second client, check they see you bend over.\n", "#15") },
  { number: 106, title: "accounts", body: body("1. Host a dedicated server and register Ada.\n2. Restart the server with -loadSessionSave and rejoin: Ada keeps her bank.\n", "#16") },
  { number: 107, title: "jip", body: body("1. Walk to the farm (148, 299) and plant a plot.\n2. A late joiner joins while it grows and sees the same stage.\n", "#17") },
  { number: 108, title: "outreach", body: body("1. Deploy preview: open /join on the website and press the Discord button.\n", "#18") },
  { number: 109, title: "config", body: body("1. Walk to the spawn square (119, 243) and check the travel globe still lists every area.\n", "#19") },
  { number: 110, title: "nosteps", body: "Refs #20\n\n## Summary\n\nnothing manual\n" },
  { number: 111, title: "film", body: body("1. Open the storyboard json and read the shot list.\n", "#21") },
];

// ---- steps and tags
check(splitSteps("**Steps**\n\n1. A.\n2. B.\n\nRefs #4\n🤖 Generated with x\n---\n").join("|") === "A.|B.", "splitSteps drops markers, labels, Refs and the footer");
check(!tagsOf("Check it. No restart needed.").restart && tagsOf("Restart the server and rejoin").restart, "`no restart needed` is not a restart step");
check(tagsOf("with a second client").mp && tagsOf("a late joiner joins while it grows").jip && tagsOf("Host a dedicated server").ded, "two-player, JIP and dedicated steps are tagged");

// ---- items, dedupe, places, blocks
const items = buildItems(issues, prs);
const byN = (n) => items.find((i) => i.number === n);
check(byN(11).steps.length === 2 && byN(12).steps.length === 3, "an issue keeps the steps of every merged PR that refs it");
check(byN(20).noSteps && byN(20).prs.join() === "110", "an issue whose PR has no manual steps is listed with none");
check(byN(11).place === "bank" && byN(13).place === "police" && byN(14).place === "fuel" && byN(17).place === "farm" && byN(19).place === "spawn", "places come from the words and the coordinates");
check(byN(15).block === "2p" && byN(16).block === "restart" && byN(17).block === "restart" && byN(18).block === "desk" && byN(21).block === "desk" && byN(13).block === "solo", "blocks: 2-player, restart, JIP, desk (website, dev track) and solo");
check(!byN(13).tags.restart, "`No restart needed` leaves the police car solo");

const all = selectItems(items, { all: true });
const c11 = all.chosen.find((c) => c.number === 11);
const c12 = all.chosen.find((c) => c.number === 12);
check(c11.shownSteps.length === 2, "the first issue shows its own steps");
check(c12.shownSteps.length === 1 && c12.sameAs.join() === "11", `a repeated step is shown once (#12 showed ${c12.shownSteps.length} steps)`);
check(all.chosen.length === issues.length, "--all takes every open issue");

// ---- the top hour
const big = [];
const bigPrs = [];
const words = (n) => String.fromCharCode(97 + (n % 26)) + String.fromCharCode(97 + Math.floor(n / 26)) + "zz";
for (let n = 1; n <= 40; n++) {
  big.push(issue(200 + n, `M2 item ${n} at the bank`, n % 2 ? "M2 Living Town" : "M3 Framework Release"));
  bigPrs.push({ number: 300 + n, title: "x", body: body(`1. Walk to the bank (177, 258) and do check ${words(n)} alpha.\n2. Then beta ${words(n)} gamma.\n3. Then delta ${words(n)} epsilon.\n`, `#${200 + n}`) });
}
const cut = selectItems(buildItems(big, bigPrs), { budget: 60, reserve: 15 });
const mins = totalMinutes(cut.chosen) + 15;
check(mins <= 60, `the top hour stays inside its budget (got ${mins} min)`);
check(cut.chosen.length > 5 && cut.chosen.every((c) => c.milestone === "M2 Living Town"), "the top hour is M2 first: no M3 item before the M2 items run out");
check(cut.skipped.length > 0, "the rest wait for the next pass");

// ---- ranking: chapter work, then player flows, then the rest, polish last (#402)
const rankIssues = [
  issue(40, "Fuel till: rob it from the counter"),
  issue(41, "Visual: the moonshine renders as a plain white boiler"),
  issue(42, "Cash icon shows as a backpack"),
  issue(43, "Cuffs: arrest a suspect and walk them to the cell"),
  issue(44, "M3-Packs: bank vault switch"),
  issue(45, "Framework: item catalogue licence check at the outlet", "M5 Settling in"),
  issue(46, "Plain M2 thing at the farm"),
  issue(47, "Visual: the till floats above the counter"),
  issue(48, "Agent proofs leave state behind"),
  issue(49, "Comms epic: payphones and pagers"),
];
const rankPrs = rankIssues.map((it, i) => ({
  number: 400 + i,
  title: "x",
  mergedAt: "2026-11-10T10:00:00Z",
  body: body(`1. Walk to the farm (148, 299) and try ${words(60 + i)} alpha.\n2. Then ${words(60 + i)} beta.\n`, `#${it.number}`),
}));
const chapters = new Map([[1000, [40, 47]], [49, [48]]]);
const rItems = buildItems(rankIssues, rankPrs, { chapters, now: Date.parse("2026-11-14T10:00:00Z") });
const tierOfN = (n) => rItems.find((i) => i.number === n).tier;
check(tierOfN(40) === 0 && tierOfN(47) === 0, "an issue under an open chapter is tier 0, polish or not");
check(tierOfN(49) === 0 && rItems.find((i) => i.number === 49).isEpic, "an open chapter or epic is tier 0 and knows it is one");
check(tierOfN(48) === 0, "an issue under an epic is tier 0 even when it reads like tooling");
check(tierOfN(43) === 1 && tierOfN(45) === 1, "a big player flow in the title is tier 1, whatever its milestone");
check(tierOfN(46) === 2 && tierOfN(44) === 2, "an ordinary issue and M3 framework work are tier 2");
check(tierOfN(41) === 3 && tierOfN(42) === 3, "polish and visual-only items are tier 3");
const order = rankItems(rItems).map((i) => i.number);
const at = (n) => order.indexOf(n);
check(Math.max(at(40), at(47), at(49), at(48)) < Math.min(at(43), at(45)), `chapter work comes before flows (order ${order.join(" ")})`);
check(at(45) < at(46) && at(43) < at(46), "a flow comes before an ordinary issue, even from a later milestone");
check(at(46) < at(41) && at(44) < at(42), "polish comes last");
check(at(49) < at(40), "the epic itself comes before its sub-issues");
const tight = selectItems(rItems, { budget: 14, reserve: 0 });
const tightIds = tight.chosen.map((c) => c.number);
check(tightIds.includes(40) && tightIds.includes(43) && !tightIds.includes(41) && !tightIds.includes(42), `a tight hour takes chapter work and flows, not polish (got ${tightIds.join(",")})`);
const stuck = selectItems(rItems, { budget: 8, reserve: 0 });
check(stuck.chosen.every((c) => c.tier === 0) || !stuck.chosen.some((c) => c.tier >= 2), "leftover minutes are not spent on tier 2 or 3 while a flow waits");

// ---- one line for one walk: an issue whose steps another shows, in the same place, rides on that line
const foldIssues = [issue(51, "Bank: teller stands on the footing"), issue(52, "Bank: teller sign faces the street")];
const foldPrs = [{ number: 501, title: "x", body: body("1. Walk to the bank (177, 258). Talk to the teller; withdraw 100.\n2. Look at the teller's feet.\n", "#51, #52") }];
const foldSel = selectItems(buildItems(foldIssues, foldPrs), { all: true });
const foldMd = renderChecklist({ chosen: foldSel.chosen, smoke: [], all: true, date: "2026-11-14" });
check(/^- \[ \] #51 #52 Bank: teller stands on the footing .* fail \[ \]$/m.test(foldMd), "two issues one walk proves share one line that lists both numbers");
check(/^  - also proves #52: Bank: teller sign faces the street$/m.test(foldMd) && !/^- \[ \] #52 /m.test(foldMd), "the folded issue keeps its title under the line and no line of its own");
const foldTicked = parseVerdicts(foldMd.replace("- [ ] #51 ", "- [x] #51 "));
check(foldTicked.length === 2 && foldTicked.every((x) => x.verdict === "pass"), "ticking the shared line passes every issue on it");
check(parseVerdicts(foldMd.replace(/^(- \[ \] #51 #52 .*) fail \[ \]$/m, "$1 fail [x]\n  > note: footing floats")).every((x) => x.verdict === "fail" && x.note === "footing floats"), "failing the shared line fails every issue on it, with the note");

// ---- smoke list and render
const smoke = parseSmokeList(readFileSync(join(ROOT, "docs", "playtest", "smoke-list.md"), "utf8"));
check(smoke.length >= 6 && ["S1", "S2", "S3", "S4", "S5", "S6"].every((id) => smoke.some((s) => s.id === id)), "the smoke list has boot, join, save, restart, reload and late joiner");
const md = renderChecklist({ chosen: all.chosen, skipped: [], smoke, all: true, date: "2026-11-14" });
check(md.indexOf("## Smoke list") !== -1 && md.indexOf("## Smoke list") < md.indexOf("## Solo walk"), "the smoke list comes before the solo walk");
check(smoke.every((s) => md.includes(`- [ ] ${s.id} `)), "every smoke check has a box");
check(issues.every((i) => new RegExp(`^- \\[ \\] #${i.number} .* fail \\[ \\]$`, "m").test(md)), "every issue has a pass box and a fail box");
const route = ["Bank and bank street", "General store", "Fuel station", "Police station", "Farm"].map((n) => md.indexOf(`### ${n}`));
check(route[0] !== -1 && route[0] < route[2] && route[2] < route[3], "places come in route order");
check(/\[2P\]/.test(md) && /\[RESTART\]/.test(md) && /\[JIP\]/.test(md), "two-player, restart and JIP steps are marked");

// ---- verdicts
const ticked = md
  .replace("- [ ] #11 ", "- [x] #11 ")
  .replace(/^(- \[ \] #13 .*) fail \[ \]$/m, "$1 fail [x]\n  > note: bar floats 5 cm above the roof")
  .replace("- [ ] S4 ", "- [x] S4 ")
  .replace(/^(- \[ \] S5 .*) fail \[ \]$/m, "$1 fail [x]");
const v = parseVerdicts(ticked);
const get = (id) => v.find((x) => x.id === id);
check(get("11").verdict === "pass" && get("13").verdict === "fail" && get("12").verdict === "open", "the ticked file reads back: pass, fail, open");
check(get("13").note === "bar floats 5 cm above the roof", "a `> note:` line is the comment");
check(get("S5").verdict === "fail" && get("S5").smoke, "a failed smoke check is read as such");
const { cmds, notes } = verdictCommands(v, { by: "the maintainer", date: "2026-11-14", sha: "abc12345" });
check(cmds.some((c) => /^gh issue edit 11 --add-label verified --remove-label needs-playtest --remove-label in-dev$/.test(c)) && cmds.some((c) => c.startsWith("gh issue close 11 ")), "a pass prints verified and close");
check(cmds.some((c) => /^gh issue edit 13 --add-label regression /.test(c)) && !cmds.some((c) => c.startsWith("gh issue close 13")), "a fail prints regression and does not close");
check(cmds.some((c) => c.startsWith("gh issue comment 13 ") && c.includes("bar floats 5 cm")), "the fail comment carries the note");
check(!cmds.some((c) => / 12( |$)/.test(c)), "an open box prints nothing");
check(notes.length === 1 && /S5 FAILED/.test(notes[0]) && /do not promote/.test(notes[0]), "a failed smoke check blocks promotion in the output");

// ---- the command on fixtures: writes the file, prints a summary, runs nothing
const dir = mkdtempSync(join(tmpdir(), "lf-playtest-"));
mkdirSync(join(dir, "fx"));
writeFileSync(join(dir, "fx", "issues.json"), JSON.stringify(issues));
writeFileSync(join(dir, "fx", "prs.json"), JSON.stringify(prs));
const capture = (fn) => {
  const real = console.log;
  const lines = [];
  console.log = (...a) => lines.push(a.join(" "));
  try {
    return { code: fn(), lines };
  } finally {
    console.log = real;
  }
};
const out = join(dir, "out.md");
const r1 = capture(() => main(["checklist", "--from-json", join(dir, "fx"), "--out", out], { root: ROOT }));
check(r1.code === 0 && /^SUMMARY playtest-checklist ok items=\d+ smoke=6 minutes=\d+/.test(r1.lines[0]) && r1.lines.length <= 4, `checklist prints a short summary (got ${r1.lines.length} lines)`);
check(readFileSync(out, "utf8").includes("## Smoke list"), "the command wrote the checklist with the smoke list");
writeFileSync(join(dir, "ticked.md"), ticked);
const r2 = capture(() => main(["verdicts", join(dir, "ticked.md")], { root: ROOT }));
check(r2.code === 0 && /nothing was run/.test(r2.lines[0]) && r2.lines.some((l) => l.startsWith("gh issue edit 11 ")) && r2.lines.some((l) => l.startsWith("WARN") && /S5/.test(l)), "verdicts prints the commands and the smoke warning and says nothing was run");
check(capture(() => main(["verdicts", join(dir, "nope.md")], { root: ROOT })).code === 1, "verdicts on a missing file fails");

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("playtest-checklist-selftest: OK");
