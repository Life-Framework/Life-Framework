// The pure part of `tools\cli server-doctor` (#1271): reads one dedicated-server console.log and says which
// mods loaded, which script errors it holds (with the fix from docs/debug-log-triage.md when the table knows
// the line), the first-boot success lines, and a PASS/FAIL verdict. No game needed; the verdict is
// judgeBootLog's (tools/release/boot-check.mjs), so a boot check and the doctor never disagree.
import { existsSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";
import { judgeBootLog, READY_LINE } from "../release/boot-check.mjs";

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

// The triage table rows: pattern fragments (the backticked spans, split at "...") and the advice text.
export function parseTriage(md) {
  const rows = [];
  for (const l of String(md ?? "").split(/\r?\n/)) {
    if (!l.startsWith("|")) continue;
    const cells = l.split(/(?<!\\)\|/).slice(1, -1).map((c) => c.trim());
    if (cells.length < 3 || /^-+$/.test(cells[0]) || /^Message/.test(cells[0])) continue;
    const spans = [...cells[0].matchAll(/`([^`]+)`/g)].map((m) => m[1].split("...").map((f) => f.trim()).filter(Boolean));
    const usable = spans.filter((fr) => fr.length && fr.join("").length >= 12).sort((a, b) => b.join("").length - a.join("").length);
    // a row is keyed by its most specific span; a short generic span ("NULL pointer to instance") would match unrelated lines
    if (usable.length) rows.push({ spans: [usable[0]], fix: `${cells[1].replace(/\*/g, "")}: ${cells[2]}` });
  }
  return rows;
}

export function triageFix(line, rows) {
  const hit = rows.find((r) => r.spans.some((fr) => fr.every((f) => line.includes(f))));
  return hit ? hit.fix : null;
}

const STAGE_FIX = {
  mods: "A mod could not load: check the Workshop id in server.json is the one the Workshop page shows, the item is Public or Unlisted, and the host can reach the Workshop.",
  scripts: "A script error stopped the boot: fix the first SCRIPT (E) line below; later ones are usually fallout.",
  ready: "The server never printed the ready line: it stopped or hung during boot; read the last log line quoted here.",
};

export function diagnose(text, { triage = "" } = {}) {
  const lines = String(text ?? "").split(/\r?\n/);
  const rows = parseTriage(triage);
  const judged = judgeBootLog(text, { client: false });
  const errors = lines.filter((l) => SCRIPT_ERROR.test(l)).map((l) => ({ line: clip(l), fix: triageFix(l, rows) }));
  const success = lines.filter((l) => SUCCESS.test(l)).slice(0, 8).map(clip);
  return {
    verdict: judged.ok ? "PASS" : "FAIL",
    stage: judged.stage,
    line: judged.line,
    fix: judged.ok ? null : STAGE_FIX[judged.stage] ?? null,
    mods: parseMods(lines),
    errors,
    success,
    engine: /Initializing engine, version (\S+)/.exec(String(text ?? ""))?.[1] ?? null,
    game: /Creating game instance\([^)]*\), version (\S+)/.exec(String(text ?? ""))?.[1] ?? null,
  };
}

export function renderReport(r) {
  const out = [];
  out.push(`engine ${r.engine ?? "?"}, game ${r.game ?? "?"}`);
  out.push(`mods loaded (${r.mods.length}):`);
  for (const m of r.mods) out.push(`  ${m.name}  ${m.guid}${m.version ? `  v${m.version}` : ""}`);
  if (!r.mods.length) out.push("  (none listed: the log has no \"Loaded addons:\" block)");
  out.push(`script errors (${r.errors.length}):`);
  for (const e of r.errors) {
    out.push(`  ${e.line}`);
    out.push(`    fix: ${e.fix ?? "not in docs/debug-log-triage.md; classify it and add a row"}`);
  }
  out.push(`first-boot success lines (${r.success.length}):`);
  for (const s of r.success) out.push(`  ${s}`);
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
