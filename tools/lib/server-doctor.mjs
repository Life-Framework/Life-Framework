// The pure part of `tools\cli server-doctor` (#1271): reads one dedicated-server console.log and says which
// mods loaded, which script errors it holds (with the fix from docs/debug-log-triage.md when the table knows
// the line), the first-boot success lines, and a PASS/FAIL verdict. No game needed; the verdict is
// judgeBootLog's (tools/release/boot-check.mjs), so a boot check and the doctor never disagree.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { judgeBootLog, READY_LINE } from "../release/boot-check.mjs";
import { matchKnown, parseTriageTable } from "./log-triage.mjs";
import { checkEffectiveConfig } from "./server-doctor-config.mjs";

const SCRIPT_ERROR = /SCRIPT\s+\(E\)|\bVME\b|Virtual Machine Exception/;
const GPROJ = /gproj:\s*'([^']+)'\s*guid:\s*'([0-9A-Fa-f]{16})'(?:\s*version:\s*'([^']*)')?/;
const SUCCESS = /Entered online game state\.|### Updating player|\[LFDebug:CharacterCreation\] flow start for player|\[LFDebug:\w+\][^\n]*\b(loaded|ready|started|initiali[sz]ed)\b/i;

const clip = (s) => s.trim().slice(0, 240);

// Mods from the "Loaded addons:" block: name (the .gproj file), guid, version when the log prints one.
export function parseMods(lines) {
  const mods = [];
  let inBlock = false;
  for (const l of lines) {
    if (/Loaded addons:/.test(l)) { inBlock = true; continue; }
    if (!inBlock) continue;
    const m = GPROJ.exec(l);
    if (!m) { if (/\S/.test(l)) inBlock = false; continue; }
    const name = m[1].split("/").pop().replace(/\.gproj$/i, "");
    mods.push({ name, guid: m[2].toUpperCase(), version: m[3] ?? null });
  }
  return mods;
}

const TIMESTAMP = /^\s*\d{1,2}:\d{2}:\d{2}(?:\.\d+)?\s+/;

// An error line against the triage table, through the shared matcher (tools/lib/log-triage.mjs, #1385).
// A known line carries the row's verdict and advice ("OPEN: <what to do>"); a new line carries no fix.
function markLine(line, rows, md) {
  const hit = matchKnown(line.replace(TIMESTAMP, "").trimEnd(), rows);
  if (!hit) return { line: clip(line), known: false, fix: null };
  const cells = (md[hit.row.line - 1] ?? "").split(/(?<!\\)\|/).slice(1, -1).map((c) => c.trim());
  const advice = cells[2] ? `${hit.row.verdict}: ${cells[2]}` : hit.row.verdict;
  return { line: clip(line), known: true, fix: advice };
}

const STAGE_FIX = {
  config: "A config.json field is bad: the server drops it and keeps the shipped default. Fix the field named in the effective-config section (docs/config/config.schema.json says the range).",
  mods: "A mod could not load: check the Workshop id in server.json is the one the Workshop page shows, the item is Public or Unlisted, and the host can reach the Workshop.",
  scripts: "A script error stopped the boot: fix the first SCRIPT (E) line below; later ones are usually fallout.",
  ready: "The server never printed the ready line: it stopped or hung during boot; read the last log line quoted here.",
};

// `profile` is the profile folder holding config.json; without it the report has no config section.
export function diagnose(text, { triage = "", profile = null } = {}) {
  const lines = String(text ?? "").split(/\r?\n/);
  const md = String(triage ?? "").split(/\r?\n/);
  const rows = parseTriageTable(String(triage ?? ""));
  const judged = judgeBootLog(text, { client: false });
  const errors = lines.filter((l) => SCRIPT_ERROR.test(l)).map((l) => markLine(l, rows, md));
  const success = lines.filter((l) => SUCCESS.test(l)).slice(0, 8).map(clip);
  const config = profile ? checkEffectiveConfig(profile) : null;
  // a field the server rejects (kind bad) or a config.json it cannot read (kind file) fails the doctor;
  // an unknown key is only ignored, so it is listed but does not fail
  const configFail = !!config && config.findings.some((f) => f.kind === "bad" || f.kind === "file");
  const stage = judged.ok && configFail ? "config" : judged.stage;
  const configLine = config?.findings.find((f) => f.kind === "bad" || f.kind === "file");
  const ok = judged.ok && !configFail;
  return {
    verdict: ok ? "PASS" : "FAIL",
    stage,
    line: judged.ok && configFail ? `${configLine.field ?? "config.json"}: ${configLine.msg}` : judged.line,
    fix: ok ? null : STAGE_FIX[stage] ?? null,
    mods: parseMods(lines),
    errors,
    success,
    config,
    engine: /Initializing engine, version (\S+)/.exec(String(text ?? ""))?.[1] ?? null,
    game: /Creating game instance\([^)]*\), version (\S+)/.exec(String(text ?? ""))?.[1] ?? null,
  };
}

// A profile folder or a console.log: reads the newest log, and the profile's config.json when the target is a
// folder. Null when there is no console.log to read.
export function diagnoseTarget(target, { triage = "" } = {}) {
  const log = findLog(target);
  if (!log) return null;
  const profile = statSync(target).isDirectory() ? target : null;
  return diagnose(readFileSync(log, "utf8"), { triage, profile });
}

function renderConfig(c) {
  const out = [`effective config (${c.present ? "config.json" : "no config.json: shipped defaults"}):`];
  if (!c.findings.length) { out.push("  ok: every field is valid"); return out; }
  for (const f of c.findings) {
    const note = f.kind === "unknown" ? "ignored" : f.fellBack ? "server keeps the shipped default" : "";
    out.push(`  ${f.kind}  ${f.field ?? "config.json"}: ${f.msg}${f.line ? ` (line ${f.line})` : ""}${note ? `  [${note}]` : ""}`);
  }
  return out;
}

export function renderReport(r) {
  const out = [];
  out.push(`engine ${r.engine ?? "?"}, game ${r.game ?? "?"}`);
  out.push(`mods loaded (${r.mods.length}):`);
  for (const m of r.mods) out.push(`  ${m.name}  ${m.guid}${m.version ? `  v${m.version}` : ""}`);
  if (!r.mods.length) out.push("  (none listed: the log has no \"Loaded addons:\" block)");
  out.push(`script errors (${r.errors.length}):`);
  for (const e of r.errors) {
    out.push(`  [${e.known ? "known" : "new"}] ${e.line}`);
    out.push(`    fix: ${e.fix ?? "not in docs/debug-log-triage.md; classify it and add a row"}`);
  }
  out.push(`first-boot success lines (${r.success.length}):`);
  for (const s of r.success) out.push(`  ${s}`);
  if (r.config) out.push(...renderConfig(r.config));
  if (r.verdict === "FAIL") {
    out.push(`stopped at stage "${r.stage}"${r.line ? `: ${r.line}` : ""}`);
    if (r.fix) out.push(`fix: ${r.fix}`);
  }
  out.push(r.verdict);
  return out.join("\n");
}

// A log file, or the newest console.log under a profile folder.
export function findLog(arg) {
  if (!arg || !existsSync(arg)) return null;
  if (!statSync(arg).isDirectory()) return arg;
  let best = null;
  const walk = (d) => {
    for (const e of readdirSync(d, { withFileTypes: true })) {
      const p = join(d, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.name === "console.log" && (!best || statSync(p).mtimeMs > best.t)) best = { p, t: statSync(p).mtimeMs };
    }
  };
  walk(arg);
  return best?.p ?? null;
}

export { READY_LINE };
