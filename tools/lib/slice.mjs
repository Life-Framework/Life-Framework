// Slice cards (docs/process/slice-card.md): parse, check, order and brief the slices a planner cuts from a chapter.
// Pure functions; tools/commands/slice.mjs does the gh and file work, tools/validation/slice-selftest.mjs proves them.

export const MARKER = "<!-- slice-card v1 -->";
export const TIERS = ["T0", "T1", "T2"];
export const MODELS = ["haiku", "sonnet", "opus"];
export const TYPES = ["capability", "system", "data", "visual", "tooling", "docs", "town"];
export const RISKS = ["persistence", "replication", "money", "inventory"];
export const DONE_LABELS = ["in-dev", "needs-playtest", "verified"];
export const RUNNING_LABEL = "agent:working";
export const BUDGET = { minutes: 45, calls: 60 };
// Files with merge drivers (tools/merge/) or generated tables: any slice may append to them.
export const SHARED = [
  "docs/features.md",
  "docs/debug-log-triage.md",
  "addons/LifeFramework/Language/**",
  "addons/LF_Everon1989/Language/**",
  "addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistry*",
  "tools/validation/enforce-lint-baseline.json",
  "docs/config/config.schema.json",
  "docs/config/config.example.json",
  "addons/LifeFramework/Scripts/Game/Core/LF_StaticReset.c",
];
// The routing table (docs/process/roadmap-pipeline.md "Model routing"). Risky work goes to Opus whatever its type.
export const ROUTING = {
  docs: "haiku",
  tooling: "sonnet",
  data: "sonnet",
  visual: "sonnet",
  capability: "sonnet",
  system: "sonnet",
  // The chapter-close town-placement pass (docs/architecture/world-zones.md).
  town: "sonnet",
};
export const AGENT_OF = { haiku: "lf-scout", sonnet: "lf-builder", opus: "lf-hard" };

const list = (v) => String(v || "").split(",").map((s) => s.trim().replace(/^`|`$/g, "")).filter((s) => s && !/^none$/i.test(s));

export function suggestModel(card) {
  if (card.risk.length) return "opus";
  return ROUTING[card.type] || "sonnet";
}

// One card from an issue body or a plan section. Fields come from the `## Card` bullet list.
export function parseCard(body, extra = {}) {
  const text = String(body || "").replace(/\r\n/g, "\n");
  const field = {};
  const cardSec = section(text, "Card");
  for (const line of cardSec.split("\n")) {
    const m = /^\s*[-*]\s*([a-z-]+)\s*:\s*(.*?)\s*$/i.exec(line);
    if (m) field[m[1].toLowerCase()] = m[2];
  }
  const budget = /(\d+)\s*min(?:[^\d]+(\d+)\s*calls)?/i.exec(field.budget || "");
  return {
    key: field.key || "",
    parent: Number((/#?(\d+)/.exec(field.parent || "") || [])[1]) || null,
    type: (field.type || "").toLowerCase(),
    tier: (field.tier || "").toUpperCase(),
    model: (field.model || "").toLowerCase(),
    machines: list(field.machines),
    minutes: budget ? Number(budget[1]) : null,
    calls: budget && budget[2] ? Number(budget[2]) : null,
    owns: list(field.owns),
    depends: list(field.depends).map((d) => (/^#?\d+$/.test(d) ? Number(d.replace("#", "")) : d)),
    acceptance: field.acceptance || "",
    risk: list(field.risk).map((r) => r.toLowerCase()),
    blocker: /^(yes|true)$/i.test(field["release-blocker"] || ""),
    capability: section(text, "Capability").trim(),
    acceptanceText: section(text, "Acceptance").trim(),
    notes: section(text, "Notes").trim(),
    isCard: text.includes(MARKER) || Boolean(cardSec.trim()),
    ...extra,
  };
}

// The body of `## <name>...` up to the next `## ` heading.
function section(text, name) {
  const re = new RegExp(`^##\\s+${name}\\b.*$`, "im");
  const m = re.exec(text);
  if (!m) return "";
  const rest = text.slice(m.index + m[0].length);
  const end = /^##?\s/m.exec(rest);
  return end ? rest.slice(0, end.index) : rest;
}

// Problems with one card, as strings. An empty array is a good card.
export function checkCard(card) {
  const p = [];
  if (!TYPES.includes(card.type)) p.push(`type must be one of ${TYPES.join("|")}`);
  if (!TIERS.includes(card.tier)) p.push(`tier must be one of ${TIERS.join("|")}`);
  if (!MODELS.includes(card.model)) p.push(`model must be one of ${MODELS.join("|")}`);
  if (!card.minutes || card.minutes > BUDGET.minutes) p.push(`budget must be at most ${BUDGET.minutes} min (split the slice)`);
  if (card.calls && card.calls > BUDGET.calls) p.push(`budget must be at most ${BUDGET.calls} calls (split the slice)`);
  if (!card.owns.length) p.push("owns: list the files or globs this slice changes");
  if (!card.acceptance) p.push("acceptance: name the test, proof verb or config fixture, written first");
  if (!card.capability) p.push("## Capability: one player-visible capability or one system change, one sentence");
  if (card.risk.length && card.tier !== "T2") p.push(`risk ${card.risk.join(",")} needs tier T2`);
  return [...new Set(p)];
}

// --- the DAG ---------------------------------------------------------------------------------------------------------

// slices: [{ id, title, card, state: "open"|"closed", labels: [names] }]. Dependencies name ids.
export function statusOf(s) {
  if (s.state === "closed" || s.labels.some((l) => DONE_LABELS.includes(l))) return "merged";
  if (s.labels.includes(RUNNING_LABEL)) return "running";
  return "open";
}

export function graph(slices) {
  const byId = new Map(slices.map((s) => [s.id, s]));
  const children = new Map(slices.map((s) => [s.id, []]));
  const missing = [];
  for (const s of slices) {
    for (const d of s.card.depends) {
      if (byId.has(d)) children.get(d).push(s.id);
      else missing.push(`${label(s)} depends on ${typeof d === "number" ? "#" + d : d}, which is not a slice of this parent`);
    }
  }
  return { byId, children, missing };
}

export function findCycle(slices) {
  const { byId } = graph(slices);
  const mark = new Map();
  const walk = (id, path) => {
    if (mark.get(id) === 1) return [...path.slice(path.indexOf(id)), id];
    if (mark.get(id) === 2) return null;
    mark.set(id, 1);
    for (const d of byId.get(id)?.card.depends || []) {
      if (!byId.has(d)) continue;
      const c = walk(d, [...path, id]);
      if (c) return c;
    }
    mark.set(id, 2);
    return null;
  };
  for (const s of slices) {
    const c = walk(s.id, []);
    if (c) return c;
  }
  return null;
}

function ancestors(id, byId, seen = new Set()) {
  for (const d of byId.get(id)?.card.depends || []) {
    if (byId.has(d) && !seen.has(d)) {
      seen.add(d);
      ancestors(d, byId, seen);
    }
  }
  return seen;
}

// The literal part of a glob before its first wildcard.
function stem(glob) {
  const i = glob.search(/[*?[{]/);
  return (i === -1 ? glob : glob.slice(0, i)).replace(/\\/g, "/");
}
const isShared = (g) => SHARED.some((s) => stem(s) && (stem(g).startsWith(stem(s)) || g === s));

export function globsOverlap(a, b) {
  const sa = stem(a);
  const sb = stem(b);
  const exactA = sa === a;
  const exactB = sb === b;
  if (exactA && exactB) return sa === sb;
  if (exactA) return sa.startsWith(sb);
  if (exactB) return sb.startsWith(sa);
  return sa.startsWith(sb) || sb.startsWith(sa);
}

// Pairs of slices that may run at once (neither depends on the other) and claim the same files. Merged slices are skipped.
export function conflicts(slices) {
  const { byId } = graph(slices);
  const live = slices.filter((s) => statusOf(s) !== "merged");
  const out = [];
  for (let i = 0; i < live.length; i++) {
    for (let j = i + 1; j < live.length; j++) {
      const a = live[i];
      const b = live[j];
      if (ancestors(a.id, byId).has(b.id) || ancestors(b.id, byId).has(a.id)) continue;
      for (const ga of a.card.owns.filter((g) => !isShared(g))) {
        const gb = b.card.owns.find((g) => !isShared(g) && globsOverlap(ga, g));
        if (gb) {
          out.push(`${label(a)} and ${label(b)} can run at once but both own ${ga === gb ? ga : `${ga} / ${gb}`}`);
          break;
        }
      }
    }
  }
  return out;
}

// Critical path: the longest chain of unmerged minutes from a slice to the end of the chapter, the slice included.
export function criticalPaths(slices) {
  const { byId, children } = graph(slices);
  const memo = new Map();
  const len = (id) => {
    if (memo.has(id)) return memo.get(id);
    memo.set(id, 0);
    const s = byId.get(id);
    const own = statusOf(s) === "merged" ? 0 : s.card.minutes || BUDGET.minutes;
    let best = 0;
    for (const c of children.get(id)) best = Math.max(best, len(c));
    memo.set(id, own + best);
    return own + best;
  };
  const blocks = new Map();
  const blocker = (id) => {
    if (blocks.has(id)) return blocks.get(id);
    blocks.set(id, false);
    const v = byId.get(id).card.blocker || children.get(id).some(blocker);
    blocks.set(id, v);
    return v;
  };
  return new Map(slices.map((s) => [s.id, { path: len(s.id), blocker: blocker(s.id) }]));
}

// The ready slices, most schedule-critical first: longest remaining chain, then release blockers (feeding one, then being one),
// then lowest number.
export function readyOrder(slices) {
  const { byId } = graph(slices);
  const cp = criticalPaths(slices);
  const ready = slices.filter((s) => statusOf(s) === "open" && s.card.depends.every((d) => byId.has(d) && statusOf(byId.get(d)) === "merged"));
  return ready
    .map((s) => ({ ...s, ...cp.get(s.id) }))
    .sort((a, b) => b.path - a.path || Number(b.blocker) - Number(a.blocker) || Number(b.card.blocker) - Number(a.card.blocker) || String(a.id).localeCompare(String(b.id), undefined, { numeric: true }));
}

export function label(s) {
  return typeof s.id === "number" ? `#${s.id}` : s.id;
}

// The list view: every slice with its state, deps and chain, then the ready queue and the ready:running ratio.
export function renderList(slices, { readyOnly = false } = {}) {
  const { byId } = graph(slices);
  const cp = criticalPaths(slices);
  const ready = readyOrder(slices);
  const readyIds = new Set(ready.map((s) => s.id));
  const running = slices.filter((s) => statusOf(s) === "running").length;
  const lines = [];
  const row = (s) => {
    const st = readyIds.has(s.id) ? "ready" : statusOf(s) === "open" ? "blocked" : statusOf(s);
    const c = cp.get(s.id);
    const deps = s.card.depends.map((d) => (byId.has(d) ? label(byId.get(d)) : `${typeof d === "number" ? "#" + d : d}?`));
    return `${label(s).padEnd(6)} ${st.padEnd(7)} ${s.card.tier || "T?"} ${(s.card.model || "?").padEnd(6)} path ${String(c.path).padStart(3)}m${s.card.blocker ? " BLOCKER" : c.blocker ? " feeds-BLOCKER" : ""}  ${s.title}${deps.length ? `  <- ${deps.join(", ")}` : ""}`;
  };
  if (!readyOnly) {
    for (const s of topo(slices)) lines.push(row(s));
    lines.push("");
  }
  lines.push(`ready ${ready.length}, running ${running}${running && ready.length < running * 2 ? `  (keep ready >= 2x running: plan ${running * 2 - ready.length} more)` : ""}`);
  ready.forEach((s, i) => lines.push(`${String(i + 1).padStart(2)}. ${row(s)}`));
  return lines.join("\n");
}

export function topo(slices) {
  const { byId } = graph(slices);
  const out = [];
  const seen = new Set();
  const visit = (s) => {
    if (seen.has(s.id)) return;
    seen.add(s.id);
    for (const d of s.card.depends) if (byId.has(d)) visit(byId.get(d));
    out.push(s);
  };
  for (const s of slices) visit(s);
  return out;
}

// --- plan files ------------------------------------------------------------------------------------------------------

// A plan is one markdown file: `# Slice <KEY>: <title>` headings, each followed by a card body.
export function parsePlan(text) {
  const parts = String(text).replace(/\r\n/g, "\n").split(/^# Slice\s+/m).slice(1);
  return parts.map((p) => {
    const nl = p.indexOf("\n");
    const head = (nl === -1 ? p : p.slice(0, nl)).trim();
    const m = /^([A-Za-z0-9_-]+)\s*:\s*(.+)$/.exec(head);
    const body = nl === -1 ? "" : p.slice(nl + 1);
    const card = parseCard(body);
    const key = m ? m[1] : card.key || head;
    return { id: key, title: m ? m[2].trim() : head, body, card: { ...card, key }, state: "open", labels: [] };
  });
}

export function checkPlan(slices) {
  const problems = [];
  for (const s of slices) for (const p of checkCard(s.card)) problems.push(`${label(s)}: ${p}`);
  const { missing } = graph(slices);
  problems.push(...missing);
  const cyc = findCycle(slices);
  if (cyc) problems.push(`dependency cycle: ${cyc.join(" -> ")}`);
  else problems.push(...conflicts(slices));
  return problems;
}

// The issue body for one plan slice, its local dependency keys replaced by issue numbers.
export function cardBody(s, parent, numbers = new Map()) {
  const c = s.card;
  const deps = c.depends.map((d) => (numbers.has(d) ? `#${numbers.get(d)}` : typeof d === "number" ? `#${d}` : d));
  return [
    MARKER,
    `**Slice** of #${parent}. Template and rules: \`docs/process/slice-card.md\`. Brief: \`tools\\cli slice brief <this issue>\`.`,
    "",
    "## Card",
    `- key: ${c.key}`,
    `- parent: #${parent}`,
    `- type: ${c.type}`,
    `- tier: ${c.tier}`,
    `- model: ${c.model}`,
    `- machines: ${c.machines.join(", ") || "none"}`,
    `- budget: ${c.minutes} min / ${c.calls || BUDGET.calls} calls`,
    `- owns: ${c.owns.map((g) => "`" + g + "`").join(", ")}`,
    `- depends: ${deps.join(", ") || "none"}`,
    `- acceptance: ${c.acceptance}`,
    `- risk: ${c.risk.join(", ") || "none"}`,
    `- release-blocker: ${c.blocker ? "yes" : "no"}`,
    "",
    "## Capability",
    c.capability,
    "",
    "## Acceptance",
    c.acceptanceText || `Written first: ${c.acceptance}. It fails before the change and passes after.`,
    ...(c.notes ? ["", "## Notes", c.notes] : []),
    "",
  ].join("\n");
}

export const TEMPLATE = (parent) => `# Slices for #${parent}
<!-- One "# Slice <KEY>: <title>" heading per slice. Rules: docs/process/slice-card.md.
     Check: tools\\cli slice new ${parent} --plan <this file> --dry-run   Create: drop --dry-run. -->

# Slice S1: <one player-visible capability or one system change>
## Card
- type: capability
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 60 calls
- owns: \`addons/LifeFramework/Scripts/Game/Feature/<Area>/**\`
- depends: none
- acceptance: test LF_Test_<Name>
- risk: none
- release-blocker: no

## Capability
<one sentence a player or owner would notice>

## Acceptance
<the test, proof verb or config fixture; what it asserts; written and seen red before the code>
`;

// --- the brief -------------------------------------------------------------------------------------------------------

export function brief(issue, card, worktreeSlug) {
  // A town-placement pass runs in a town- worktree: that slug is what validate-world-zones lets edit town layers.
  const town = card.type === "town";
  const slug = worktreeSlug || (town ? `town-${issue.number}` : `slice-${issue.number}`);
  const gate = {
    T0: "validate only (wt ship runs just validate when addons/ is untouched)",
    T1: "light gate: validate plus the targeted test; wt ship still runs the full gate for an addons/ change",
    T2: "full gate (wt ship: validate --full, build, suite)",
  }[card.tier] || "wt ship";
  const agent = AGENT_OF[card.model] || "lf-builder";
  const lines = [
    `Slice #${issue.number}: ${issue.title}  (agent ${agent}, model ${card.model || "?"}, tier ${card.tier || "?"}, parent #${card.parent || "?"})`,
    "",
    `Goal: ${card.capability}`,
    `Acceptance, written first: ${card.acceptance}${card.acceptanceText ? `\n  ${card.acceptanceText.split("\n").join("\n  ")}` : ""}`,
    `Owns only: ${card.owns.join(", ")}. Shared append-only files (features.md, string tables, triage) are fine. Anything else: stop and report.`,
    `Timebox: ${card.minutes || BUDGET.minutes} min / ${card.calls || BUDGET.calls} calls, hard. At the limit ship what passes, write tmp/handoff-${slug}.md, and say SPLIT with the rest as new cards; never extend.`,
    `Machines: ${card.machines.join(", ") || "none"}.`,
    town
      ? "Town pass: place this chapter's systems in town to the polish bar (the polish skill, docs/architecture/world-zones.md); look <area> before and after, audit placement --area <area> --strict clean."
      : `World props: a grid cell only (tools\\cli grid place ${slug} <prefab>...); never edit a town layer, validate-world-zones refuses it (docs/architecture/world-zones.md).`,
    "",
    "Loop:",
    `1. tools\\cli wt new ${slug} --wait; gh issue edit ${issue.number} --add-label ${RUNNING_LABEL}.`,
    "2. Red: write the acceptance check, run it, see it fail (tools\\cli test --no-build --only <LF_Test_X>, or the proof verb/fixture).",
    "3. Implement the smallest change inside owns; run the targeted test until green.",
    `4. Gate ${card.tier}: ${gate}.${card.risk.length ? ` Risk ${card.risk.join(", ")}: an independent reviewer (lf-hard, interrogate) reads the diff before merge; findings in tmp/review-${slug}.md.` : ""}`,
    `5. Ship: tools\\cli wt ship ${slug} --refs ${issue.number}. Visible change: one look picture; polish nitpicks become follow-up issues, not blockers.`,
    `6. Record: tools\\cli slice record ${issue.number} --model ${card.model || "sonnet"} --outcome merged|handoff|split|failed --gate pass|fail --minutes N.`,
    "Stay in your lane: do not re-verify other slices, start no helper agents, no wait loops. Report in five lines; say merged, in-dev.",
  ];
  if (card.notes) lines.push("", "Notes:", card.notes);
  return lines.join("\n");
}

// --- the ledger ------------------------------------------------------------------------------------------------------

export const OUTCOMES = ["merged", "handoff", "split", "failed"];

export function ledgerRow(o) {
  const p = [];
  if (!MODELS.includes(o.model)) p.push(`--model ${MODELS.join("|")}`);
  if (!OUTCOMES.includes(o.outcome)) p.push(`--outcome ${OUTCOMES.join("|")}`);
  if (!["pass", "fail"].includes(o.gate)) p.push("--gate pass|fail (the first gate run)");
  if (!(o.minutes > 0)) p.push("--minutes N");
  return p;
}

const median = (xs) => {
  if (!xs.length) return null;
  const s = [...xs].sort((a, b) => a - b);
  const m = Math.floor(s.length / 2);
  return s.length % 2 ? s[m] : (s[m - 1] + s[m]) / 2;
};

// Rows: { issue, model, type, outcome, gate, minutes, wait? }. Groups by model and by type.
export function stats(rows) {
  const group = (key) => {
    const g = new Map();
    for (const r of rows) {
      const k = r[key] || "?";
      if (!g.has(k)) g.set(k, []);
      g.get(k).push(r);
    }
    return [...g.entries()].sort().map(([k, rs]) => ({
      key: k,
      n: rs.length,
      firstPass: rs.filter((r) => r.gate === "pass").length / rs.length,
      rework: rs.filter((r) => r.outcome !== "merged").length / rs.length,
      minutes: median(rs.map((r) => r.minutes)),
      wait: median(rs.filter((r) => r.wait >= 0).map((r) => r.wait)),
    }));
  };
  return { all: group("all"), model: group("model"), type: group("type") };
}

export function renderStats(rows) {
  if (!rows.length) return "no slices recorded yet (tools\\cli slice record <issue> ...)";
  const s = stats(rows.map((r) => ({ ...r, all: "all" })));
  const pct = (x) => `${Math.round(x * 100)}%`.padStart(4);
  const line = (g) => `  ${g.key.padEnd(11)} n ${String(g.n).padStart(3)}  first-pass ${pct(g.firstPass)}  rework ${pct(g.rework)}  median ${g.minutes}m${g.wait === null ? "" : `  wait ${g.wait}m`}`;
  return ["all slices", ...s.all.map(line), "by model", ...s.model.map(line), "by type", ...s.type.map(line)].join("\n");
}
