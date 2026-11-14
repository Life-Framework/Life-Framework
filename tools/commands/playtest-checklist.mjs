// tools\cli playtest checklist | verdicts: one playtest session that clears many needs-playtest issues (#402, #403).
//
//   tools\cli playtest checklist [--all] [--budget 60] [--steps] [--out file] [--from-json dir]
//   tools\cli playtest verdicts <ticked-checklist.md> [--repo owner/name]
//
// `checklist` reads the open needs-playtest and in-dev issues and the `## Manual testing` steps of the merged PRs that
// ref them (gh), drops repeated steps, groups the rest by place in a walking route, marks two-player, restart and
// late-joiner steps, estimates minutes, and writes a markdown checklist (default tmp/playtest/checklist-<date>.md)
// with a pass/fail box per issue. The default is the top hour, M2 first; `--all` lists everything. The fixed smoke list
// (docs/playtest/smoke-list.md) always comes first. `verdicts` reads the ticked file back and PRINTS the gh commands
// (verified or regression label, comment, close on pass); it never runs them. The logic is tools/lib/playtest-checklist.mjs.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { ghBin, git, run } from "../wt.mjs";
import { buildItems, parseSmokeList, parseVerdicts, renderChecklist, selectItems, totalMinutes, verdictCommands } from "../lib/playtest-checklist.mjs";

export const help = "playtest checklist [--all] [--budget N] | verdicts <file>   one playtest session's checklist from the merged PRs' manual steps; the gh commands for its verdicts (printed, not run)";

const USAGE = `usage: tools\\cli playtest checklist [--all] [--budget 60] [--steps] [--out file]
       tools\\cli playtest verdicts <ticked-checklist.md> [--repo owner/name]`;

const val = (args, name) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
};

function gh(root, args) {
  const bin = ghBin();
  if (!bin) throw new Error("the GitHub CLI (gh) is required");
  const r = run(bin, args, { cwd: root, maxBuffer: 256 * 1024 * 1024 });
  if (r.status !== 0) throw new Error(`gh ${args.slice(0, 3).join(" ")}: ${(r.stderr || r.stdout).trim().split("\n")[0]}`);
  return JSON.parse(r.stdout || "[]");
}

// Open issues in either state label, deduplicated by number.
function fetchIssues(root) {
  const seen = new Map();
  for (const label of ["needs-playtest", "in-dev"]) {
    for (const i of gh(root, ["issue", "list", "--state", "open", "--label", label, "--limit", "500", "--json", "number,title,labels,milestone"])) seen.set(i.number, i);
  }
  return [...seen.values()];
}

function fetchPrs(root) {
  return gh(root, ["pr", "list", "--state", "merged", "--limit", "2000", "--json", "number,title,body,url"]);
}

export function readSmoke(root) {
  const file = join(root, "docs", "playtest", "smoke-list.md");
  return existsSync(file) ? parseSmokeList(readFileSync(file, "utf8")) : [];
}

function checklistCmd(args, root) {
  const all = args.includes("--all");
  const budget = Number(val(args, "--budget") ?? 60);
  if (!Number.isFinite(budget) || budget <= 0) throw new Error("--budget takes minutes, e.g. --budget 60");
  const fixtures = val(args, "--from-json");
  let issues;
  let prs;
  if (fixtures) {
    issues = JSON.parse(readFileSync(join(fixtures, "issues.json"), "utf8"));
    prs = JSON.parse(readFileSync(join(fixtures, "prs.json"), "utf8"));
  } else {
    issues = fetchIssues(root);
    prs = fetchPrs(root);
  }
  const items = buildItems(issues, prs);
  const smoke = readSmoke(root);
  const { chosen, skipped } = selectItems(items, { all, budget, reserve: smoke.reduce((n, x) => n + x.minutes, 0) });
  const sha = fixtures ? "" : git(root, ["rev-parse", "--short=8", "origin/main"]).stdout.trim();
  const date = new Date().toISOString().slice(0, 10);
  const text = renderChecklist({ chosen, skipped, smoke, steps: args.includes("--steps"), all, budget, sha, date, openCount: issues.length });
  const out = resolve(root, val(args, "--out") ?? join("tmp", "playtest", `checklist-${date}${all ? "-all" : ""}.md`));
  mkdirSync(dirname(out), { recursive: true });
  writeFileSync(out, text);
  const minutes = totalMinutes(chosen) + smoke.reduce((n, s) => n + s.minutes, 0);
  const noSteps = chosen.filter((c) => c.noSteps).length;
  console.log(`SUMMARY playtest-checklist ok items=${chosen.length} smoke=${smoke.length} minutes=${minutes} of ${issues.length} open${noSteps ? `, ${noSteps} without steps` : ""}`);
  console.log(`checklist: ${out}`);
  console.log("after ticking: tools\\cli playtest verdicts " + out);
  return 0;
}

function verdictsCmd(args, root) {
  const file = args.find((a) => !a.startsWith("--") && a !== val(args, "--repo"));
  if (!file) throw new Error(USAGE);
  const path = resolve(root, file);
  if (!existsSync(path)) throw new Error(`no such file: ${path}`);
  const verdicts = parseVerdicts(readFileSync(path, "utf8"));
  const date = new Date().toISOString().slice(0, 10);
  const { cmds, notes } = verdictCommands(verdicts, { date, repo: val(args, "--repo") ?? "" });
  const pass = verdicts.filter((v) => v.verdict === "pass" && !v.smoke).length;
  const fail = verdicts.filter((v) => v.verdict === "fail" && !v.smoke).length;
  const open = verdicts.filter((v) => v.verdict === "open").length;
  console.log(`SUMMARY playtest-verdicts ok pass=${pass} fail=${fail} open=${open} smoke_fail=${notes.length}; nothing was run`);
  for (const n of notes) console.log(`WARN  ${n}`);
  if (cmds.length) {
    console.log("# review, then run these (the maintainer or lead; verified closes the issue):");
    for (const c of cmds) console.log(c);
  }
  return 0;
}

export function main(args, { root }) {
  const [sub, ...rest] = args;
  try {
    if (sub === "checklist") return checklistCmd(rest, root);
    if (sub === "verdicts") return verdictsCmd(rest, root);
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
  console.log(USAGE);
  return 1;
}
