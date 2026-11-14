// tools\cli slice: slice cards for the roadmap pipeline (docs/process/roadmap-pipeline.md, docs/process/slice-card.md).
//
//   slice new <parent> [--plan file] [--dry-run]   no plan: write a template plan to tmp/slices-<parent>.md;
//                                                  with a plan: check it, then create each card as a sub-issue of
//                                                  <parent> in dependency order (--dry-run writes tmp/slices-<parent>.cards.md)
//   slice list <parent> [--ready]                  the DAG, which slices are ready, ready ones by critical path
//   slice brief <issue>                            the agent brief for one card
//   slice record <issue> --model M --outcome O --gate pass|fail --minutes N [--wait N] [--type T]
//   slice stats                                    first-pass rate, rework and median minutes per model and slice type
//
// `--fixture <json>` (list, brief) reads { issues: [...] } instead of gh; `--ledger <file>` moves the ledger, which
// otherwise lives in the git common dir (lf/slice-ledger.jsonl) so every worktree appends to one file. The logic is
// tools/lib/slice.mjs; the proof is tools/validation/slice-selftest.mjs.
import { appendFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { gitCommonDir, ghBin, run } from "../wt.mjs";
import { TEMPLATE, brief, cardBody, checkPlan, ledgerRow, parseCard, parsePlan, renderList, renderStats, suggestModel, topo } from "../lib/slice.mjs";

export const help = "slice new|list|brief|record|stats   slice cards: scaffold a chapter's slices as sub-issues, show the DAG and the ready queue by critical path, print a slice's agent brief, log and report outcomes per model";

const USAGE = `usage: tools\\cli slice new <parent> [--plan file] [--dry-run]
       tools\\cli slice list <parent> [--ready] [--fixture f.json]
       tools\\cli slice brief <issue> [--fixture f.json]
       tools\\cli slice record <issue> --model haiku|sonnet|opus --outcome merged|handoff|split|failed --gate pass|fail --minutes N [--wait N] [--type T]
       tools\\cli slice stats [--json]`;

const val = (args, name) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
};
const num = (s) => Number(String(s || "").replace(/^#/, "")) || null;

function ghRaw(root, args, input) {
  const bin = ghBin();
  if (!bin) throw new Error("the GitHub CLI (gh) is required");
  const r = run(bin, args, { cwd: root, maxBuffer: 64 * 1024 * 1024, ...(input ? { input, stdio: ["pipe", "pipe", "pipe"] } : {}) });
  if (r.status !== 0) throw new Error(`gh ${args.slice(0, 3).join(" ")}: ${(r.stderr || r.stdout).trim().split("\n")[0]}`);
  return r.stdout;
}

const toSlice = (i) => ({
  id: i.number,
  number: i.number,
  title: i.title,
  state: String(i.state || "open").toLowerCase(),
  labels: (i.labels || []).map((l) => (typeof l === "string" ? l : l.name)),
  card: parseCard(i.body),
});

function loadFixture(file) {
  return JSON.parse(readFileSync(resolve(file), "utf8")).issues;
}

function subIssues(root, parent, fixture) {
  if (fixture) return loadFixture(fixture).filter((i) => parseCard(i.body).parent === parent || (i.parent === parent));
  return JSON.parse(ghRaw(root, ["api", `repos/{owner}/{repo}/issues/${parent}/sub_issues`, "--paginate", "--slurp"])).flat();
}

export function ledgerPath(root, args) {
  return resolve(val(args, "--ledger") || join(gitCommonDir(root), "lf", "slice-ledger.jsonl"));
}

export function readLedger(file) {
  if (!existsSync(file)) return [];
  return readFileSync(file, "utf8").split("\n").filter(Boolean).map((l) => JSON.parse(l));
}

export async function main(args, ctx) {
  const root = ctx.root;
  const [sub, target] = args;
  const fixture = val(args, "--fixture");
  try {
    if (sub === "new") return newSlices(root, num(target), args);
    if (sub === "list") {
      const parent = num(target);
      if (!parent) throw new Error("slice list <parent issue>");
      const slices = subIssues(root, parent, fixture).map(toSlice).filter((s) => s.card.isCard);
      if (!slices.length) {
        console.log(`#${parent}: no slice cards under it (tools\\cli slice new ${parent})`);
        return 0;
      }
      console.log(renderList(slices, { readyOnly: args.includes("--ready") }));
      return 0;
    }
    if (sub === "brief") {
      const n = num(target);
      if (!n) throw new Error("slice brief <issue>");
      const issue = fixture ? loadFixture(fixture).find((i) => i.number === n) : JSON.parse(ghRaw(root, ["issue", "view", String(n), "--json", "number,title,body,labels,state"]));
      if (!issue) throw new Error(`#${n} not found`);
      const card = parseCard(issue.body);
      if (!card.isCard) throw new Error(`#${n} has no ## Card section (docs/process/slice-card.md)`);
      console.log(brief(issue, card));
      return 0;
    }
    if (sub === "record") {
      const row = {
        at: new Date().toISOString(),
        issue: num(target),
        model: (val(args, "--model") || "").toLowerCase(),
        type: (val(args, "--type") || "").toLowerCase() || undefined,
        outcome: val(args, "--outcome"),
        gate: val(args, "--gate"),
        minutes: Number(val(args, "--minutes")),
        ...(val(args, "--wait") ? { wait: Number(val(args, "--wait")) } : {}),
      };
      const p = [...(row.issue ? [] : ["<issue>"]), ...ledgerRow(row)];
      if (p.length) throw new Error(`slice record needs ${p.join(", ")}`);
      const file = ledgerPath(root, args);
      mkdirSync(dirname(file), { recursive: true });
      appendFileSync(file, JSON.stringify(row) + "\n");
      console.log(`SUMMARY slice record ok #${row.issue} ${row.model} ${row.outcome} gate=${row.gate} ${row.minutes}m ledger=${file}`);
      return 0;
    }
    if (sub === "stats") {
      const rows = readLedger(ledgerPath(root, args));
      if (args.includes("--json")) console.log(JSON.stringify(rows));
      else console.log(renderStats(rows));
      return 0;
    }
    console.log(USAGE);
    return sub ? 2 : 0;
  } catch (e) {
    console.error(`SUMMARY slice ${sub || ""} fail: ${e.message}`);
    return 1;
  }
}

function newSlices(root, parent, args) {
  if (!parent) throw new Error("slice new <parent issue>");
  const planFile = val(args, "--plan");
  const tmp = join(root, "tmp");
  mkdirSync(tmp, { recursive: true });
  if (!planFile) {
    const out = join(tmp, `slices-${parent}.md`);
    if (existsSync(out)) throw new Error(`${out} exists; edit it, then: tools\\cli slice new ${parent} --plan ${out} --dry-run`);
    writeFileSync(out, TEMPLATE(parent));
    console.log(`SUMMARY slice new ok template=${out}  fill one "# Slice" per slice, then --plan ${out} --dry-run`);
    return 0;
  }
  const slices = parsePlan(readFileSync(resolve(planFile), "utf8"));
  if (!slices.length) throw new Error(`${planFile}: no "# Slice <KEY>: <title>" headings`);
  const problems = checkPlan(slices);
  for (const s of slices) {
    const want = suggestModel(s.card);
    if (s.card.model && want !== s.card.model && want === "opus") problems.push(`${s.id}: risk ${s.card.risk.join(",")} routes to opus, not ${s.card.model}`);
  }
  if (problems.length) {
    console.log(`SUMMARY slice new fail ${problems.length} problem(s) in ${planFile}`);
    for (const p of problems) console.log(`  ${p}`);
    return 1;
  }
  const ordered = topo(slices);
  if (args.includes("--dry-run")) {
    const out = join(tmp, `slices-${parent}.cards.md`);
    writeFileSync(out, ordered.map((s) => `# ${s.title}\n\n${cardBody(s, parent)}`).join("\n---\n\n"));
    console.log(`SUMMARY slice new ok dry-run ${ordered.length} card(s) out=${out}`);
    return 0;
  }
  ghRaw(root, ["label", "create", "slice", "--color", "c5def5", "--description", "A slice card: one 30-45 minute agent job (docs/process/slice-card.md)", "--force"]);
  const numbers = new Map();
  for (const s of ordered) {
    const url = ghRaw(root, ["issue", "create", "--title", s.title, "--label", "slice", "--body-file", "-"], cardBody(s, parent, numbers)).trim();
    const n = num(url.split("/").pop());
    numbers.set(s.id, n);
    const id = ghRaw(root, ["api", `repos/{owner}/{repo}/issues/${n}`, "--jq", ".id"]).trim();
    ghRaw(root, ["api", "-X", "POST", `repos/{owner}/{repo}/issues/${parent}/sub_issues`, "-F", `sub_issue_id=${id}`]);
  }
  console.log(`SUMMARY slice new ok ${ordered.length} sub-issue(s) of #${parent}: ${ordered.map((s) => `${s.id}=#${numbers.get(s.id)}`).join(" ")}`);
  return 0;
}
