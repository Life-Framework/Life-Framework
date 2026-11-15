// The ops pages name only `tools\cli` commands that exist, or mark them *(planned)* (#1502).
// inputs: docs/ops/*.md, docs/playtest/m4-opening-day.md, tools/cli.mjs, tools/commands/*.mjs, tools/host/commands/*.mjs, tools/validation/fixtures/ops-commands/**
//
// "A second person can run a session from the runbook" only holds while every command the pages
// tell them to type exists. A `tools\cli <cmd> [<sub>]` in inline code or a fenced block passes when
// <cmd> is a CLI command (tools/cli.mjs, tools/commands/*.mjs) and, for `host` and `playtest`, <sub>
// is a core verb or a file in tools/host/commands/. A reference to a command that does not exist yet
// passes when the code span is followed or preceded by *(planned)* (a fenced line: `(planned)` anywhere on it).
// The reverse fails too (#1633): a command that exists and is still marked *(planned)* sends an operator to
// a hand-made substitute for a command that ran, so the page is stale. That check is an error even while
// the missing-command check is WARN_ONLY.
//
// On the real tree this WARNS and exits 0 while the wave-4 host commands are still open (WARN_ONLY);
// `--strict` fails on them, and the flip is one constant once they land. `--selftest` runs the
// fixture page (a real command, a planned one, a missing one) and asserts pass, pass, fail.
//
// red-proof (#1633): 2026-11-15, before checkPage took a mode, `--selftest` printed
// `FAIL  fixture: host backup is marked planned but exists, and was not reported` and exited 1.
// red-proof: 2026-11-15, with checkPage a stub returning no findings, `--selftest` printed
// `FAIL fixture: missing command was not reported` and exited 1.
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const WARN_ONLY = true;

const PAGES = ["docs/playtest/m4-opening-day.md"];

// Words in cli.mjs's dispatch chain (`cmd === "x"`) and its `cmds` table, plus one file per command in
// tools/commands/. `sub` holds the verbs of the commands whose second word the pages name.
export function knownCommands(root) {
  const cli = readFileSync(join(root, "tools", "cli.mjs"), "utf8");
  const top = new Set();
  for (const m of cli.matchAll(/\bcmd === "([a-z][a-z0-9-]*)"/g)) top.add(m[1]);
  const table = /^const cmds = \{\n([\s\S]*?)^\};/m.exec(cli);
  for (const m of (table ? table[1] : "").matchAll(/^  (?:"([a-z][a-z0-9-]*)"|([a-z][a-z0-9-]*)):/gm)) top.add(m[1] ?? m[2]);
  for (const f of mjsNames(join(root, "tools", "commands"))) top.add(f);

  const region = (from, to) => {
    const a = cli.indexOf(from);
    const b = cli.indexOf(to, a + 1);
    return a < 0 ? "" : cli.slice(a, b < 0 ? undefined : b);
  };
  const verbs = (text) => new Set([...text.matchAll(/\bsub === "([a-z][a-z0-9-]*)"/g)].map((m) => m[1]));
  const host = verbs(region("function cmdHost(", "// ----------------------------------------------------------------- playtest"));
  for (const f of mjsNames(join(root, "tools", "host", "commands"))) host.add(f);
  host.add("help");
  const playtest = verbs(region("function cmdPlaytest(", "function "));
  return { top, sub: { host, playtest } };
}

function mjsNames(dir) {
  return existsSync(dir) ? readdirSync(dir).filter((n) => /^[a-z][a-z0-9-]*\.mjs$/.test(n)).map((n) => n.slice(0, -4)) : [];
}

const REF = /tools[/\\]cli\b([^`\n]*)/;

// Each `tools\cli ...` reference in inline code or a fenced block that names a missing command and is
// not marked planned: [{ line, ref, text }]. With mode "stale", the references that name an existing command
// but are marked planned (a bare family name such as `tools\cli host` never counts).
export function checkPage(md, known, mode = "missing") {
  const out = [];
  let fenced = false;
  md.split(/\r?\n/).forEach((line, i) => {
    if (/^\s*(```|~~~)/.test(line)) {
      fenced = !fenced;
      return;
    }
    const spans = [];
    if (fenced) {
      if (REF.test(line)) spans.push({ code: line, planned: /\(planned(?:, #\d+)?\)/i.test(line) });
    } else {
      for (const m of line.matchAll(/(`+)([^`]*?)\1/g)) {
        if (!REF.test(m[2])) continue;
        const after = line.slice(m.index + m[0].length);
        const before = line.slice(0, m.index);
        spans.push({ code: m[2], planned: /^\s*\*?_?\(planned(?:, #\d+)?\)/i.test(after) || /\(planned(?:, #\d+)?\)\*?_?\s*$/i.test(before) });
      }
    }
    for (const { code, planned } of spans) {
      const words = (REF.exec(code)[1].trim().match(/\S+/g) ?? []);
      const word = (w) => (w && /^[a-z][a-z0-9-]*$/.test(w) ? w : null);
      const cmd = word(words[0]);
      if (!cmd) continue;
      let ref = null;
      let exists = known.top.has(cmd);
      let named = cmd;
      if (!exists) ref = cmd;
      else if (known.sub[cmd]) {
        const sub = word(words[1]);
        exists = Boolean(sub) && known.sub[cmd].has(sub);
        if (sub) named = `${cmd} ${sub}`;
        if (sub && !exists) ref = named;
      }
      if (mode === "stale") {
        if (exists && planned) out.push({ line: i + 1, ref: named, text: code.trim() });
      } else if (ref && !planned) out.push({ line: i + 1, ref, text: code.trim() });
    }
  });
  return out;
}

async function selftest() {
  const root = ROOT;
  const page = join(root, "tools", "validation", "fixtures", "ops-commands", "page.md");
  const text = readFileSync(page, "utf8");
  const real = knownCommands(root);
  let failed = 0;
  const fail = (m) => { console.log(`FAIL  ${m}`); failed++; };
  for (const w of ["status", "validate", "logs", "wt", "config-check", "slice", "host"]) if (!real.top.has(w)) fail(`known top-level commands lack "${w}"`);
  for (const w of ["start", "status", "restore", "backup", "drill", "watch"]) if (!real.sub.host.has(w)) fail(`known host verbs lack "${w}"`);
  const found = checkPage(text, real).map((f) => f.ref);
  // real command: no finding; planned one: no finding; missing one: finding
  if (found.includes("host status")) fail("fixture: a real command was flagged");
  if (found.includes("host zz-planned-after") || found.includes("host zz-planned-before")) fail("fixture: a missing command marked *(planned)* was flagged");
  if (!found.includes("host zz-not-a-command")) fail("fixture: missing command was not reported");
  if (!found.includes("zz-fixture-missing")) fail("fixture: missing top-level command in a fenced block was not reported");
  if (found.some((r) => r === "host commands")) fail("fixture: a bare `tools\\cli host` family name was flagged");
  if (found.length !== 2) fail(`fixture: expected exactly 2 findings, got ${found.length}: ${found.join(", ")}`);
  // a real command still marked *(planned)*, the marker after or before the code span or on a fenced line (#1633)
  const stale = checkPage(text, real, "stale").map((f) => f.ref);
  for (const r of ["host backup", "host restore", "host start"]) if (!stale.includes(r)) fail(`fixture: ${r} is marked planned but exists, and was not reported`);
  if (stale.length !== 3) fail(`fixture: expected exactly 3 stale findings, got ${stale.length}: ${stale.join(", ")}`);
  console.log(failed ? `SUMMARY validate-ops-commands selftest fail ${failed}` : "SUMMARY validate-ops-commands selftest ok");
  process.exit(failed ? 1 : 0);
}

function pagesOf(root) {
  const dir = join(root, "docs", "ops");
  const ops = existsSync(dir) ? readdirSync(dir).filter((n) => n.endsWith(".md")).sort().map((n) => `docs/ops/${n}`) : [];
  return [...ops, ...PAGES.filter((p) => existsSync(join(root, p)))];
}

if (process.argv.includes("--selftest")) await selftest();

const strict = process.argv.includes("--strict");
const known = knownCommands(ROOT);
const pages = pagesOf(ROOT);
const findings = [];
for (const p of pages) {
  for (const f of checkPage(readFileSync(join(ROOT, p), "utf8"), known)) findings.push({ ...f, page: p });
}
const stale = [];
for (const p of pages) {
  for (const f of checkPage(readFileSync(join(ROOT, p), "utf8"), known, "stale")) stale.push({ ...f, page: p });
}
for (const f of stale) console.log(`FAIL  ${f.page}:${f.line} \`tools\\cli ${f.ref}\` exists but is marked *(planned)*: drop the marker and say what it does (${f.text})`);
const level = strict || !WARN_ONLY ? "FAIL" : "WARN";
for (const f of findings) console.log(`${level}  ${f.page}:${f.line} \`tools\\cli ${f.ref}\` is not a command: write it as *(planned)* or add it (${f.text})`);
if (stale.length || (findings.length && level === "FAIL")) {
  const missing = level === "FAIL" ? findings.length : 0;
  console.log(`SUMMARY validate-ops-commands fail ${missing} missing, ${stale.length} stale *(planned)* in ${pages.length} pages`);
  process.exit(1);
}
console.log(findings.length ? `SUMMARY validate-ops-commands warn ${findings.length} unmarked missing in ${pages.length} pages (warn-only until the wave-4 commands land)` : `SUMMARY validate-ops-commands ok ${pages.length} pages`);
