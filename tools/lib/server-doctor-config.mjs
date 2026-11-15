// server-doctor, effective config (#1272, O13): what the server will actually run with.
//   checkEffectiveConfig(profileDir) -> { ok, present, findings, badFields, fellBack, log }
// Two sources, both read-only:
//   1. <profileDir>/config.json, judged by tools/config/check.mjs (checkText) against
//      docs/config/config.schema.json. The rules live there; this file only reshapes the answer.
//   2. the [LFDebug:Config] lines of the newest <profileDir>/logs/logs_*/console.log (what
//      LF_ConfigService printed at the last boot): a warning or error the file check did not
//      already explain (a record the file check cannot see, a section the server refused) is added.
// A finding is { kind: "bad"|"unknown"|"warn"|"file", field, msg, level, source: "config"|"log",
// fellBack, line?, col? }. `fellBack` is true when the server drops the value and keeps the shipped
// default (every error the loader prints says "keeping ...", "skipped" or "uses the shipped
// defaults"); an unknown key is only ignored. ok is true when there is no finding at all.
// Profit loops (check.mjs's third check) are left out: they are an economy verdict, not a field.
// O13W wires this into the server-doctor report. Red-proof: tools/validation/server-doctor-config-selftest.mjs.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { checkText } from "../config/check.mjs";

const CONFIG_LINE = /\[LFDebug:Config\]\s*(.*)$/;

/** Dotted field a loader message is about, or null when it is about the whole file. */
function fieldOf(msg) {
  if (/not valid JSON|cannot open|cannot write/.test(msg)) return null;
  const top = /unknown top-level key (\S+?);/.exec(msg) ?? /section (\S+) is not an object/.exec(msg);
  if (top) return top[1];
  return /^(?:\S+?: )?([A-Za-z_]\w*(?:\.[^\s=]+)*) (?:=|is not)/.exec(msg)?.[1] ?? null;
}

function kindOf(level, msg, field) {
  if (field === null) return "file";
  if (level === "error") return "bad";
  return /not a known key|unknown top-level key/.test(msg) ? "unknown" : "warn";
}

/** Newest console.log under the profile (logs/logs_<stamp>/console.log, or a console.log beside config.json). */
function newestLog(profileDir) {
  const logs = join(profileDir, "logs");
  if (existsSync(logs)) {
    const dirs = readdirSync(logs).filter((d) => d.startsWith("logs_")).sort().reverse();
    for (const d of dirs) if (existsSync(join(logs, d, "console.log"))) return join(logs, d, "console.log");
  }
  const flat = join(profileDir, "console.log");
  return existsSync(flat) ? flat : null;
}

function fromFile(profileDir) {
  const file = join(profileDir, "config.json");
  if (!existsSync(file)) return { present: false, findings: [] };
  const { problems } = checkText(readFileSync(file, "utf8"), { source: "config.json", loops: false });
  const findings = problems.map((p) => {
    const field = p.path ? p.path.join(".") : fieldOf(p.msg);
    return { kind: kindOf(p.level, p.msg, field), field, msg: p.msg, level: p.level === "error" ? "error" : "warn", source: "config", fellBack: p.level === "error", line: p.line, col: p.col };
  });
  return { present: true, findings };
}

function fromLog(profileDir, known) {
  const path = newestLog(profileDir);
  if (!path) return { log: null, findings: [] };
  const lines = readFileSync(path, "utf8").split(/\r?\n/);
  const config = [];
  for (const l of lines) {
    const m = CONFIG_LINE.exec(l);
    if (m) config.push({ raw: l, msg: m[1].trim(), level: /\(E\)/.test(l) ? "error" : /\(W\)/.test(l) ? "warn" : "info" });
  }
  const knownFields = new Set(known.map((f) => f.field));
  const knownFile = known.some((f) => f.kind === "file");
  const findings = [];
  for (const c of config) {
    if (c.level === "info") continue;
    const field = fieldOf(c.msg);
    if (field === null ? knownFile : knownFields.has(field)) continue; // the file check already said it
    findings.push({ kind: kindOf(c.level, c.msg, field), field, msg: c.msg, level: c.level, source: "log", fellBack: c.level === "error" });
  }
  return { log: { path, lines: config.length, loaded: config.some((c) => /owner config loaded from/.test(c.msg)) }, findings };
}

/** What the server runs with: bad fields, unknown fields, and the fields that fell back to shipped defaults. */
export function checkEffectiveConfig(profileDir) {
  const file = fromFile(profileDir);
  const log = fromLog(profileDir, file.findings);
  const findings = [...file.findings, ...log.findings];
  const uniq = (list) => [...new Set(list)];
  return {
    ok: findings.length === 0,
    present: file.present,
    findings,
    badFields: uniq(findings.filter((f) => f.kind === "bad" && f.field).map((f) => f.field)),
    fellBack: uniq(findings.filter((f) => f.fellBack && f.field).map((f) => f.field)),
    log: log.log,
  };
}
