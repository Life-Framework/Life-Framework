// Token usage per agent run, read from the Claude Code session logs.
//
// Each subagent is <projects>/<project>/<session>/subagents/agent-<id>.jsonl with a .meta.json
// beside it (agentType, description); a lead thread is <projects>/<project>/<session>.jsonl.
// One API turn is written as several `assistant` records that share message.id and repeat the
// same usage, so turns are counted once per id. `<synthetic>` records are not API calls.
//
// cost-weighted tokens = input + 1.25 x cache write + 0.1 x cache read + 5 x output
// context per turn     = input + cache write + cache read
//
// Tool shares split each turn's cost evenly over the tool calls that turn made (a turn with no
// call counts as "text"), so a share says which kind of step the money went on. Result tokens
// are the tool output (chars / 4, 1600 per image) that then rides along in every later turn.
import { createReadStream, existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { homedir } from "node:os";
import { basename, dirname, join } from "node:path";
import { createInterface } from "node:readline";
import { fileURLToPath } from "node:url";

export const cost = (u) =>
  (u.input_tokens || 0) + 1.25 * (u.cache_creation_input_tokens || 0) + 0.1 * (u.cache_read_input_tokens || 0) + 5 * (u.output_tokens || 0);
export const HANDOFF_CTX = 150000;
const HANDOFF_STATE = 5000; // the tmp/ state file a fresh agent starts from
const HANDOFF_REREAD = 30000; // what it re-reads to pick the work back up
export const context = (u) => (u.input_tokens || 0) + (u.cache_creation_input_tokens || 0) + (u.cache_read_input_tokens || 0);

// What a shell command is for. The whole line is checked for the repo verbs first, so
// `cd x && tools\cli test ...` is a test, not a cd.
export function bashKind(cmd) {
  const c = String(cmd || "");
  const cli = /tools[\\/]+cli(?:\.cmd|\.mjs)?\s+(\S+)(?:\s+(\S+))?/.exec(c);
  if (/\b(?:Start-Sleep|sleep)\b|\buntil\b.*\bdo\b/.test(c)) return "bash:wait";
  // a status check of something still running: a progress file, a process list, pool/session status
  if (/^\s*date\b/.test(c) || /\b(?:pool|session(?:\.mjs)?)\s+status\b|\bps\s+-ef\b|Get-Process\b|\btasklist\b/.test(c)) return "bash:poll";
  if (/session\.mjs|harness\.mjs/.test(c) || (cli && /^(session|shots|look|film|journey|host|serve)$/.test(cli[1]))) return "bash:session";
  if (cli && /^(test|e2e|dev)$/.test(cli[1])) return "bash:test";
  if (cli && cli[1] === "wt" && /^(ship|gate)$/.test(cli[2] || "")) return "bash:ship";
  if (cli && /^(validate|ci)$/.test(cli[1])) return "bash:validate";
  if (cli && cli[1] === "call") return "bash:mcp-call";
  if ((cli && cli[1] === "logs") || /logs_\d|[\\/]logs[\\/]|[\\/]console\.log\b/.test(c)) return "bash:log";
  if (/ffmpeg|ffprobe|\bFF=/.test(c)) return "bash:media";
  if (/\bsed\s+-i\b|\bcat\s*>|\bSet-Content\b|\bOut-File\b/.test(c)) return "bash:edit";
  const first = c
    .split(/&&|\|\||;|\n/)
    .map((s) => s.trim().replace(/^(?:do|then|else)\s+/, "").replace(/^(?:\w+=\S+\s+|timeout\s+\d+\S*\s+)+/, "").trim())
    .find((s) => s && !/^(?:cd|Set-Location|pushd|date|echo|mkdir|export|set|for|while|if|done|fi)\b/.test(s) && !/^\$?\w+\s*=/.test(s)) || "";
  const w = first.split(/\s+/)[0].toLowerCase();
  if (w === "git") return "bash:git";
  if (w === "gh") return "bash:gh";
  if (/^(cat|head|tail|sed|awk|get-content|type|less|more|jq)$/.test(w)) return "bash:read";
  if (/^(grep|rg|select-string|find|ls|dir|get-childitem|wc|tree|du)$/.test(w)) return "bash:search";
  if (/^(node|npm|npx|pnpm|python|python3|py)$/.test(w) || w === "tools\\cli" || w === "tools/cli") return "bash:script";
  return "bash:other";
}

export function toolCategory(name, input) {
  if (name === "Read") return "read";
  if (name === "Grep" || name === "Glob") return "search";
  if (name === "Edit" || name === "Write" || name === "NotebookEdit" || name === "MultiEdit") return "edit";
  if (name === "Bash" || name === "PowerShell") return bashKind(input && input.command);
  if (name === "Agent" || name === "Task" || name === "SendMessage") return "agent";
  if (name === "WebFetch" || name === "WebSearch") return "web";
  if (name.startsWith("mcp__")) return "mcp";
  return name.toLowerCase();
}

// Two calls are the same read when they ask for the same bytes again.
function readKey(name, input) {
  if (!input) return null;
  if (name === "Read") return `R ${input.file_path}|${input.offset || ""}|${input.limit || ""}`;
  if (name === "Grep") return `G ${input.pattern}|${input.path || ""}|${input.glob || ""}`;
  if ((name === "Bash" || name === "PowerShell") && /^bash:(read|search|log)$/.test(bashKind(input.command))) return `B ${input.command}`;
  return null;
}

function resultTokens(content) {
  if (typeof content === "string") return Math.ceil(content.length / 4);
  if (!Array.isArray(content)) return 0;
  let t = 0;
  for (const c of content) {
    if (c.type === "image") t += 1600;
    else if (typeof c.text === "string") t += Math.ceil(c.text.length / 4);
  }
  return t;
}

// Parse one jsonl into a run summary. Only user/assistant lines are decoded; request blobs are
// skipped by a cheap prefix test because they are most of the bytes.
export async function analyzeFile(file, meta = {}) {
  const turns = new Map(); // message.id -> turn
  const calls = new Map(); // tool_use id -> call
  let first = null;
  let last = null;
  const rl = createInterface({ input: createReadStream(file, { encoding: "utf8" }), crlfDelay: Infinity });
  for await (const line of rl) {
    const head = line.slice(0, 600);
    if (!head.includes('"role":"assistant"') && !head.includes('"role":"user"') && !head.includes('"type":"user"')) continue;
    let o;
    try {
      o = JSON.parse(line);
    } catch {
      continue;
    }
    const ts = o.timestamp ? Date.parse(o.timestamp) : NaN;
    if (Number.isFinite(ts)) {
      if (first === null || ts < first) first = ts;
      if (last === null || ts > last) last = ts;
    }
    const m = o.message;
    if (!m) continue;
    if (o.type === "assistant" || m.role === "assistant") {
      if (!m.id || !m.usage || m.model === "<synthetic>") continue;
      let t = turns.get(m.id);
      if (!t) {
        t = { ts, model: m.model, usage: m.usage, tools: [] };
        turns.set(m.id, t);
      }
      if ((m.usage.output_tokens || 0) >= (t.usage.output_tokens || 0)) t.usage = m.usage;
      for (const c of m.content || []) {
        if (c.type !== "tool_use") continue;
        const call = { name: c.name, input: c.input, cat: toolCategory(c.name, c.input), ts, turn: t, result: 0, dur: 0 };
        t.tools.push(call);
        calls.set(c.id, call);
      }
    } else if (Array.isArray(m.content)) {
      for (const c of m.content) {
        if (c.type !== "tool_result") continue;
        const call = calls.get(c.tool_use_id);
        if (!call) continue;
        call.result = resultTokens(c.content);
        if (Number.isFinite(ts) && Number.isFinite(call.ts)) call.dur = Math.max(0, ts - call.ts);
      }
    }
  }
  return summarize(file, meta, [...turns.values()].sort((a, b) => a.ts - b.ts), first, last);
}

function summarize(file, meta, turns, first, last) {
  const run = {
    id: basename(file, ".jsonl").replace(/^agent-/, ""),
    session: meta.session || "",
    source: meta.source || "",
    type: meta.agentType || "lead",
    description: meta.description || "",
    models: [...new Set(turns.map((t) => t.model))],
    start: first,
    turns: turns.length,
    peakCtx: 0,
    avgCtx: 0,
    cost: 0,
    outTokens: 0, // the model's own output (thinking, commands, edits); it stays in context too
    resultTokens: 0,
    hotCost: 0, // cost of the turns past 150k context: what a handoff to a fresh agent saves
    wallMin: first !== null && last !== null ? (last - first) / 60000 : 0,
    tools: {}, // category -> { calls, cost, result, durMin }
    compactions: 0,
    repeatReads: 0,
    reads: 0,
    biggest: { tokens: 0, cat: "" },
    bigResults: 0,
    waitMin: 0,
    waitCtx: 0,
    waits: 0,
  };
  const seen = new Set();
  let ctxSum = 0;
  let prev = 0;
  // The same run under the handoff rule: past HANDOFF_CTX a fresh agent starts from the brief plus
  // a state file and spends HANDOFF_REREAD re-reading what it needs. Every later turn then reads
  // `offset` fewer cached tokens.
  const base = turns.length ? context(turns[0].usage) + HANDOFF_STATE : 0;
  let offset = 0;
  run.handoffs = 0;
  run.handoffCost = 0;
  for (const t of turns) {
    const ctx = context(t.usage);
    const c = cost(t.usage);
    ctxSum += ctx;
    run.cost += c;
    run.outTokens += t.usage.output_tokens || 0;
    if (ctx > HANDOFF_CTX) run.hotCost += c;
    if (ctx - offset < base) offset = Math.max(0, ctx - base); // the session compacted on its own
    if (ctx - offset > HANDOFF_CTX) {
      offset = ctx - base;
      run.handoffs++;
      run.handoffCost += 1.25 * (base + HANDOFF_REREAD);
    }
    run.handoffCost += Math.max(0, c - 0.1 * offset);
    if (ctx > run.peakCtx) run.peakCtx = ctx;
    if (prev > 100000 && ctx < prev * 0.6) run.compactions++;
    prev = ctx;
    const share = t.tools.length ? c / t.tools.length : c;
    if (!t.tools.length) bump(run.tools, "text", { cost: c });
    for (const call of t.tools) {
      run.resultTokens += call.result;
      bump(run.tools, call.cat, { calls: 1, cost: share, result: call.result, durMin: call.dur / 60000 });
      const k = readKey(call.name, call.input);
      if (k) {
        run.reads++;
        if (seen.has(k)) run.repeatReads++;
        seen.add(k);
      }
      if (call.result > run.biggest.tokens) run.biggest = { tokens: call.result, cat: call.cat };
      if (call.result >= 10000) run.bigResults++;
      if (call.dur >= 60000) {
        run.waits++;
        run.waitMin += call.dur / 60000;
        run.waitCtx += ctx;
      }
    }
  }
  run.avgCtx = turns.length ? ctxSum / turns.length : 0;
  run.startCtx = turns.length ? context(turns[0].usage) : 0;
  run.waitCtx = run.waits ? run.waitCtx / run.waits : 0;
  run.flags = diagnose(run);
  return run;
}

function bump(map, k, add) {
  const e = (map[k] ||= { calls: 0, cost: 0, result: 0, durMin: 0 });
  for (const f in add) e[f] += add[f];
}

// Why a run was expensive, from the numbers alone. The thresholds are where the M2 runs that
// later needed a restart or a second agent separated from the ones that did not.
export function diagnose(r) {
  const f = [];
  const k = (n) => `${Math.round(n / 1000)}k`;
  if (r.turns > 250) f.push(`scope too big: ${r.turns} turns`);
  if (r.avgCtx > 200000) f.push(`carried ${k(r.avgCtx)} context per turn`);
  if (r.repeatReads >= 15 && r.repeatReads / Math.max(1, r.reads) >= 0.15) f.push(`re-read loop: ${r.repeatReads} of ${r.reads} reads repeated`);
  if (r.biggest.tokens >= 20000) f.push(`huge dump: one ${r.biggest.cat} result of ${k(r.biggest.tokens)} tokens`);
  else if (r.bigResults >= 5) f.push(`${r.bigResults} tool results over 10k tokens`);
  if (r.waitMin >= 20 && r.waitCtx >= 120000) f.push(`waited ${Math.round(r.waitMin)} min in ${r.waits} calls holding ${k(r.waitCtx)} context`);
  if (r.compactions) f.push(`compacted ${r.compactions}x`);
  const top = Object.entries(r.tools).sort((a, b) => b[1].cost - a[1].cost)[0];
  if (top && top[1].cost / Math.max(1, r.cost) >= 0.4) f.push(`${Math.round((100 * top[1].cost) / r.cost)}% of cost in ${top[0]} turns`);
  return f;
}

// Main checkout path -> Claude's project folder name (every non-alphanumeric becomes '-').
export function projectName(root) {
  return root.replace(/[^A-Za-z0-9]/g, "-");
}

function mainRoot() {
  const here = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const git = join(here, ".git");
  if (existsSync(git) && statSync(git).isFile()) {
    // a worktree: .git is "gitdir: <main>/.git/worktrees/<name>"
    const m = /gitdir:\s*(.+)/.exec(readFileSync(git, "utf8"));
    if (m) return join(m[1].trim(), "..", "..", "..");
  }
  return here;
}

export function defaultDirs() {
  const name = projectName(mainRoot());
  return [".claude", ".claude-gaming"].map((p) => join(homedir(), p, "projects", name)).filter(existsSync);
}

// Every run under the project dirs: { file, meta } for subagents, and leads when asked.
export function listRuns(dirs, { leads = false, sinceMs = 0 } = {}) {
  const out = [];
  for (const dir of dirs) {
    const source = basename(dirname(dirname(dir)));
    for (const ent of readdirSync(dir, { withFileTypes: true })) {
      const p = join(dir, ent.name);
      if (ent.isFile() && ent.name.endsWith(".jsonl")) {
        if (leads && statSync(p).mtimeMs >= sinceMs) out.push({ file: p, meta: { source, session: ent.name.slice(0, -6), agentType: "lead" } });
        continue;
      }
      if (!ent.isDirectory()) continue;
      const sub = join(p, "subagents");
      if (!existsSync(sub)) continue;
      for (const f of readdirSync(sub)) {
        if (!/^agent-.*\.jsonl$/.test(f)) continue;
        const file = join(sub, f);
        if (statSync(file).mtimeMs < sinceMs) continue;
        let meta = {};
        try {
          meta = JSON.parse(readFileSync(file.replace(/\.jsonl$/, ".meta.json"), "utf8"));
        } catch {}
        out.push({ file, meta: { ...meta, source, session: ent.name } });
      }
    }
  }
  return out;
}

export function parseSince(s, now = Date.now()) {
  if (!s) return 0;
  const rel = /^(\d+)([hdw])$/.exec(s);
  if (rel) return now - Number(rel[1]) * { h: 3600e3, d: 86400e3, w: 604800e3 }[rel[2]];
  const t = Date.parse(s);
  if (!Number.isFinite(t)) throw new Error(`--since: not a date or 7d/12h/2w: ${s}`);
  return t;
}

export async function collect(dirs, { leads = false, since = 0, type = null } = {}) {
  const runs = [];
  const list = listRuns(dirs, { leads, sinceMs: since }).filter((r) => !type || r.meta.agentType === type);
  // a few files at a time: some logs are hundreds of MB
  for (let i = 0; i < list.length; i += 8) {
    const batch = await Promise.all(list.slice(i, i + 8).map((r) => analyzeFile(r.file, r.meta)));
    for (const r of batch) if (r.turns && (r.start === null || r.start >= since)) runs.push(r);
  }
  return runs;
}

const M = (n) => (n >= 1e6 ? `${(n / 1e6).toFixed(1)}M` : `${Math.round(n / 1000)}k`);
const median = (a) => {
  const s = [...a].sort((x, y) => x - y);
  return s.length ? s[Math.floor(s.length / 2)] : 0;
};

export function byType(runs) {
  const g = {};
  for (const r of runs) (g[r.type] ||= []).push(r);
  const total = runs.reduce((s, r) => s + r.cost, 0) || 1;
  return Object.entries(g)
    .map(([type, rs]) => {
      const turns = rs.reduce((s, r) => s + r.turns, 0);
      const cost = rs.reduce((s, r) => s + r.cost, 0);
      return {
        type,
        runs: rs.length,
        turns,
        avgCtx: rs.reduce((s, r) => s + r.avgCtx * r.turns, 0) / Math.max(1, turns),
        medianCost: median(rs.map((r) => r.cost)),
        medianTurns: median(rs.map((r) => r.turns)),
        cost,
        share: cost / total,
        over150k: rs.filter((r) => r.peakCtx > HANDOFF_CTX).length,
        hotShare: rs.reduce((s, r) => s + r.hotCost, 0) / Math.max(1, cost),
        handoffCost: rs.reduce((s, r) => s + r.handoffCost, 0),
      };
    })
    .sort((a, b) => b.cost - a.cost);
}

function toolLine(r, n = 4) {
  return Object.entries(r.tools)
    .sort((a, b) => b[1].cost - a[1].cost)
    .slice(0, n)
    .map(([k, v]) => `${k} ${Math.round((100 * v.cost) / Math.max(1, r.cost))}%`)
    .join(", ");
}

export function report(runs, { top = 15, detail = false } = {}) {
  const lines = [];
  const total = runs.reduce((s, r) => s + r.cost, 0);
  const sorted = [...runs].sort((a, b) => b.cost - a.cost);
  const topCost = sorted.slice(0, 10).reduce((s, r) => s + r.cost, 0);
  lines.push(`${runs.length} runs, ${M(total)} cost-weighted tokens; median run ${M(median(runs.map((r) => r.cost)))}; top 10 = ${Math.round((100 * topCost) / Math.max(1, total))}%`);
  const ho = runs.reduce((s, r) => s + r.handoffCost, 0);
  lines.push(`with a fresh agent at ${HANDOFF_CTX / 1000}k context (handoff rule, simulated): ${M(ho)}, saves ${Math.round(100 - (100 * ho) / Math.max(1, total))}%`);
  lines.push("");
  lines.push("type              runs  turns  avg ctx  median turns  median cost  share  peak>150k  past 150k  handoff saves");
  for (const t of byType(runs)) {
    lines.push(
      `${t.type.padEnd(17)} ${String(t.runs).padStart(4)} ${String(t.turns).padStart(6)} ${M(t.avgCtx).padStart(8)} ${String(t.medianTurns).padStart(13)} ${M(t.medianCost).padStart(12)} ${`${Math.round(100 * t.share)}%`.padStart(6)} ${String(t.over150k).padStart(10)} ${`${Math.round(100 * t.hotShare)}%`.padStart(10)} ${`${Math.round(100 - (100 * t.handoffCost) / Math.max(1, t.cost))}%`.padStart(14)}`,
    );
  }
  lines.push("");
  lines.push(`top ${Math.min(top, sorted.length)} by cost`);
  sorted.slice(0, top).forEach((r, i) => {
    const date = r.start ? new Date(r.start).toISOString().slice(0, 10) : "?";
    lines.push(
      `${String(i + 1).padStart(2)}. ${M(r.cost).padStart(6)}  ${r.type} "${r.description.slice(0, 50)}" ${date} ${r.source}/${r.session.slice(0, 8)}/${r.id}`,
    );
    lines.push(
      `    ${r.turns} turns, ctx start ${M(r.startCtx)} avg ${M(r.avgCtx)} peak ${M(r.peakCtx)}, wall ${Math.round(r.wallMin)} min, ${Math.round((100 * r.hotCost) / Math.max(1, r.cost))}% past 150k (handoff: ${r.handoffs}x, ${M(r.handoffCost)}), ${r.repeatReads}/${r.reads} reads repeated, ${M(r.resultTokens)} result + ${M(r.outTokens)} output tok, ${r.models.join("+")}`,
    );
    lines.push(`    tools: ${toolLine(r, detail ? 20 : 5)}`);
    if (detail) {
      for (const [k, v] of Object.entries(r.tools).sort((a, b) => b[1].cost - a[1].cost)) {
        lines.push(`      ${k.padEnd(14)} ${String(v.calls).padStart(4)} calls ${M(v.cost).padStart(6)} ${M(v.result).padStart(6)} result tok ${Math.round(v.durMin)} min`);
      }
    }
    if (r.flags.length) lines.push(`    why: ${r.flags.join("; ")}`);
  });
  return lines.join("\n");
}

const HELP = `usage: tools\\cli usage [--since 2026-11-01|7d] [--top N] [--type lf-hard] [--leads] [--detail] [--json] [--dir <projects/<name>>]...
  Reads the Claude Code session logs (~/.claude and ~/.claude-gaming) and prints per agent run:
  turns, peak and average context, cost-weighted tokens, wall time, the cost share per tool kind,
  and why the expensive runs were expensive. --leads adds the lead threads.`;

export async function main(args = process.argv.slice(2)) {
  const opt = { top: 15, dirs: [], leads: false, detail: false, json: false, since: 0, type: null };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--top") opt.top = Number(args[++i]);
    else if (a === "--since") opt.since = parseSince(args[++i]);
    else if (a === "--type") opt.type = args[++i];
    else if (a === "--dir") opt.dirs.push(args[++i]);
    else if (a === "--leads") opt.leads = true;
    else if (a === "--detail") opt.detail = true;
    else if (a === "--json") opt.json = true;
    else if (a === "-h" || a === "--help" || a === "help") {
      console.log(HELP);
      return 0;
    } else {
      console.error(`unknown argument ${a}\n${HELP}`);
      return 2;
    }
  }
  const dirs = opt.dirs.length ? opt.dirs : defaultDirs();
  if (!dirs.length) {
    console.error("no Claude Code project logs found for this repo");
    return 1;
  }
  const runs = await collect(dirs, opt);
  if (opt.json) console.log(JSON.stringify(runs.sort((a, b) => b.cost - a.cost).slice(0, opt.top), null, 1));
  else console.log(report(runs, opt));
  return 0;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  main().then((c) => process.exit(c));
}
