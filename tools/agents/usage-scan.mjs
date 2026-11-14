// Per-account usage scan of one Claude Code config dir (~/.claude, ~/.claude-gaming, or the laptop's).
// Standalone on purpose (node builtins only, no import.meta): `usage --accounts` pipes this file's
// source to `node --input-type=module -` over ssh to read the laptop's logs in place.
//
// Output is small whatever the log size: usage bucketed per minute, the rate-limit hits, the count
// of session logs written in the last 10 minutes, and the masked identity from .claude.json.
import { createReadStream, existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { homedir } from "node:os";
import { basename, join, resolve } from "node:path";
import { createInterface } from "node:readline";

// cost-weighted tokens = input + 1.25 x cache write + 0.1 x cache read + 5 x output (usage.mjs re-exports this)
export const cost = (u) =>
  (u.input_tokens || 0) + 1.25 * (u.cache_creation_input_tokens || 0) + 0.1 * (u.cache_read_input_tokens || 0) + 5 * (u.output_tokens || 0);

export const maskEmail = (e) => {
  const m = /^(.)[^@]*@(.+)$/.exec(String(e || ""));
  return m ? `${m[1]}***@${m[2]}` : "";
};

// Claude Code keeps the login in <config dir>/.claude.json, or ~/.claude.json for the default dir.
export function readIdentity(dir, home = homedir()) {
  const cands = [join(dir, ".claude.json")];
  if (resolve(dir) === resolve(join(home, ".claude"))) cands.push(join(home, ".claude.json"));
  for (const p of cands) {
    try {
      const a = JSON.parse(readFileSync(p, "utf8")).oauthAccount;
      if (a && (a.accountUuid || a.emailAddress)) {
        return { uuid: String(a.accountUuid || ""), email: maskEmail(a.emailAddress), tier: String(a.organizationRateLimitTier || ""), org: String(a.organizationType || "") };
      }
    } catch {}
  }
  return null;
}

function jsonlFiles(root, minMtime, out = []) {
  let ents;
  try {
    ents = readdirSync(root, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const e of ents) {
    const p = join(root, e.name);
    if (e.isDirectory()) jsonlFiles(p, minMtime, out);
    else if (e.name.endsWith(".jsonl")) {
      try {
        const m = statSync(p).mtimeMs;
        if (m >= minMtime) out.push({ file: p, mtime: m });
      } catch {}
    }
  }
  return out;
}

// One file: API turns once per message.id (a turn is written as several records), and rate-limit hits.
async function scanFile(file, sinceMs, turns, hits) {
  const seen = new Map();
  const rl = createInterface({ input: createReadStream(file, { encoding: "utf8" }), crlfDelay: Infinity });
  for await (const line of rl) {
    const isErr = line.includes('"isApiErrorMessage":true');
    if (!isErr && !line.includes('"output_tokens"')) continue;
    let o;
    try {
      o = JSON.parse(line);
    } catch {
      continue;
    }
    const ts = o.timestamp ? Date.parse(o.timestamp) : NaN;
    if (!Number.isFinite(ts) || ts < sinceMs) continue;
    if (isErr) {
      if (o.error === "rate_limit" || o.apiError === "usage_limit_reached") {
        const info = (o.quotaLimits && o.quotaLimits.rateLimitType) || (o.apiErrorParams && o.apiErrorParams.rate_limit_info && o.apiErrorParams.rate_limit_info.rateLimitType) || "";
        const resets = (o.quotaLimits && o.quotaLimits.resetsAt) || (o.apiErrorParams && o.apiErrorParams.rate_limit_info && o.apiErrorParams.rate_limit_info.resetsAt) || 0;
        hits.push({ ts, type: String(info), resetsAt: Number(resets) * 1000 || 0 });
      }
      continue;
    }
    const m = o.message;
    if (!m || m.role !== "assistant" || !m.id || !m.usage || m.model === "<synthetic>") continue;
    const prev = seen.get(m.id);
    if (!prev) seen.set(m.id, { ts, usage: m.usage });
    else if ((m.usage.output_tokens || 0) >= (prev.usage.output_tokens || 0)) prev.usage = m.usage;
  }
  for (const t of seen.values()) {
    const k = Math.floor(t.ts / 60000) * 60000;
    const b = (turns[k] ||= [0, 0]);
    b[0] += cost(t.usage);
    b[1] += 1;
  }
}

// opts: { dir, home?, lookbackMs, now?, activeMs? } -> a JSON-able record for this config dir.
export async function scanConfigDir(opts) {
  const now = opts.now ?? Date.now();
  const activeMs = opts.activeMs ?? 10 * 60000;
  const since = now - opts.lookbackMs;
  const out = { dir: basename(opts.dir), identity: readIdentity(opts.dir, opts.home), minutes: {}, hits: [], active: 0, files: 0 };
  const root = join(opts.dir, "projects");
  if (!existsSync(root)) {
    out.missing = true;
    return out;
  }
  // the title-generator throwaway projects are not agent work
  const files = jsonlFiles(root, since).filter((f) => !/claude-title/.test(f.file));
  out.files = files.length;
  out.active = files.filter((f) => f.mtime >= now - activeMs).length;
  for (let i = 0; i < files.length; i += 6) {
    await Promise.all(files.slice(i, i + 6).map((f) => scanFile(f.file, since, out.minutes, out.hits)));
  }
  out.hits.sort((a, b) => a.ts - b.ts);
  return out;
}
