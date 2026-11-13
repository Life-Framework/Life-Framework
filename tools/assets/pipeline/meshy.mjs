// Meshy API client for the asset pipeline (docs/asset-pipeline/README.md).
// Endpoints and costs were read from docs.meshy.ai on 2026-11-12; recheck there before changing them.
// The key is read at call time from a file outside the repo and never leaves this module:
// no function returns it, logs it, or puts it in an error message.
import { existsSync, readFileSync, writeFileSync, appendFileSync, mkdirSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";

export const BASE = "https://api.meshy.ai";
export const KEY_FILE = () => process.env.LF_MESHY_KEY_FILE || join(homedir(), ".life-framework", "meshy-key");

// Credits per call (pricing page, 2026-11-12). The task's own consumed_credits wins when it reports one.
export const COST = {
  preview: { "meshy-6-lite": 5, "meshy-t2": 5, "meshy-t2.5": 5, "meshy-6": 20, "meshy-7.1": 20, latest: 20 },
  refine: 10,
  remesh: 5,
};

export function estimate(spec) {
  const model = spec.meshy?.ai_model || "meshy-6-lite";
  return (COST.preview[model] ?? 20) + COST.refine;
}

function key() {
  const file = KEY_FILE();
  if (!existsSync(file)) throw new Error(`no Meshy key file at ${file} (set LF_MESHY_KEY_FILE)`);
  const k = readFileSync(file, "utf8").trim();
  if (!k) throw new Error(`Meshy key file ${file} is empty`);
  return k;
}

// Scrub anything that could carry the key before it reaches a log or an Error.
function scrub(text) {
  let k = "";
  try { k = key(); } catch { /* no key: nothing to scrub */ }
  const s = String(text ?? "");
  return k ? s.split(k).join("<meshy-key>") : s;
}

// One HTTP call, bounded. `fetchImpl` is injectable so the selftest runs offline.
export async function call(method, path, body, { fetchImpl = fetch, timeoutMs = 30000 } = {}) {
  let res;
  try {
    res = await fetchImpl(BASE + path, {
      method,
      headers: { Authorization: `Bearer ${key()}`, ...(body ? { "Content-Type": "application/json" } : {}) },
      body: body ? JSON.stringify(body) : undefined,
      signal: AbortSignal.timeout(timeoutMs),
    });
  } catch (e) {
    throw new Error(`Meshy ${method} ${path}: ${scrub(e.message)}`);
  }
  const text = await res.text();
  let json = null;
  try { json = text ? JSON.parse(text) : null; } catch { /* not json */ }
  if (!res.ok) {
    const why = { 401: "bad or missing key", 402: "out of credits, or the plan has no API access", 429: "rate or concurrency limit" }[res.status] || "";
    throw new Error(`Meshy ${method} ${path}: HTTP ${res.status} ${why} ${scrub(json?.message || text.slice(0, 200))}`.trim());
  }
  return json;
}

export const balance = async (o) => (await call("GET", "/openapi/v1/balance", null, o)).balance;

export function previewBody(spec) {
  const m = spec.meshy || {};
  if (!spec.prompt) throw new Error("spec.prompt is empty");
  if (spec.prompt.length > 800) throw new Error(`spec.prompt is ${spec.prompt.length} characters; Meshy allows 800`);
  return {
    mode: "preview",
    prompt: spec.prompt,
    ai_model: m.ai_model || "meshy-6-lite",
    should_remesh: true,
    topology: "triangle",
    target_polycount: m.target_polycount || 20000,
    target_formats: ["glb"],
  };
}

export function refineBody(spec, previewTaskId) {
  const m = spec.meshy || {};
  const body = { mode: "refine", preview_task_id: previewTaskId, enable_pbr: true, target_formats: ["glb"] };
  if (spec.texture_prompt) {
    if (spec.texture_prompt.length > 800) throw new Error("spec.texture_prompt is over 800 characters");
    body.texture_prompt = spec.texture_prompt;
  }
  if (m.texture_model) body.ai_model = m.texture_model;
  return body;
}

// Poll one task until it ends or the budget runs out. One blocking call with its own bound.
export async function waitTask(id, { budgetS = 900, everyS = 8, onProgress = () => {}, sleep = (ms) => new Promise((r) => setTimeout(r, ms)), ...o } = {}) {
  const until = Date.now() + budgetS * 1000;
  let last = -1;
  for (;;) {
    const t = await call("GET", `/openapi/v2/text-to-3d/${id}`, null, o);
    if (t.progress !== last) { last = t.progress; onProgress(t); }
    if (t.status === "SUCCEEDED") return t;
    if (t.status === "FAILED" || t.status === "CANCELED") {
      throw new Error(`Meshy task ${id} ${t.status}: ${scrub(t.task_error?.message || "no message")}`);
    }
    if (Date.now() > until) throw new Error(`Meshy task ${id} still ${t.status} at ${t.progress}% after ${budgetS}s; rerun the stage to resume the same task`);
    await sleep(everyS * 1000);
  }
}

export async function download(url, file, { fetchImpl = fetch, timeoutMs = 180000 } = {}) {
  const res = await fetchImpl(url, { signal: AbortSignal.timeout(timeoutMs) });
  if (!res.ok) throw new Error(`download ${file}: HTTP ${res.status}`);
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, Buffer.from(await res.arrayBuffer()));
  return file;
}

// Append-only credit ledger: one JSON line per paid call.
export function logCredits(ledgerFile, row) {
  mkdirSync(dirname(ledgerFile), { recursive: true });
  appendFileSync(ledgerFile, JSON.stringify({ at: new Date().toISOString(), ...row }) + "\n");
}

export function spent(ledgerFile) {
  if (!existsSync(ledgerFile)) return 0;
  return readFileSync(ledgerFile, "utf8").split("\n").filter(Boolean).reduce((n, l) => n + (JSON.parse(l).credits || 0), 0);
}
