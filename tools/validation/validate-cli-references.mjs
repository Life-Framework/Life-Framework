// The docs and the tool sources name only `tools\cli` commands that exist, and every command file is documented.
// inputs: AGENTS.md, CLAUDE.md, .claude/skills/**/*.md, docs/**/*.md, tools/cli.mjs, tools/commands/*.mjs, tools/host/commands/*.mjs, tools/**/*.mjs, agent-box/**/*.mjs, server/README.md, website/README.md
//
// Three failures (validate-ops-commands.mjs keeps the stricter per-verb check of the ops runbooks):
//   1. `toolscli`: a backslash eaten by a shell or a string literal turned `tools\cli` into one word. In a doc it
//      is a command nobody can paste; in a `console.log("usage: tools\cli x")` it is what the tool prints.
//   2. a `tools\cli <cmd>` in AGENTS.md, CLAUDE.md, a skill or a docs page whose <cmd> is not a CLI command, unless
//      the span is marked *(planned)*. docs/research and docs/roadmap are design history and are skipped.
//   3. a command file in tools/commands/ that no page names after `cli` (or in backticks): a command nobody can find.
// A deleted command leaves its doc lines behind (2); a new one needs a line in the docs that tell an agent it exists (3).
//
// red-proof: 2026-11-27, run on a checkout of origin/main (before the `toolscli` fix, the string-literal fixes and the
// tmp-sweep doc line) it printed `SUMMARY validate-cli-references fail 133 toolscli, 0 unknown command, 1 undocumented` and
// exited 1; `--selftest` feeds in-memory pages to the three rules and asserts one typo, one unknown command, one
// undocumented command, and no finding for a planned marker, a dashed command written with a space, or `tools/cli node`.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { knownCommands } from "./validate-ops-commands.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

// A command a page may leave out: legacy top-level aliases of `mcp <verb>`, and the internal verbs the dispatch
// chain answers that no page needs to teach.
const UNDOCUMENTED_OK = new Set(["install", "update", "verify", "enable", "disable"]);
// Doc folders that are design history: they name commands that were planned, renamed or retired since.
const SKIP_DOCS = [/^docs\/research\//, /^docs\/roadmap\//];
// Text where `toolscli` is the subject, not a typo (this file).
const TYPO = /toolscli/;

// Every .md under dir, root-relative with forward slashes.
function walk(root, dir, exts, out = []) {
  const abs = join(root, dir);
  if (!existsSync(abs)) return out;
  for (const n of readdirSync(abs)) {
    if (n === "node_modules" || n === ".git" || n === "tmp") continue;
    const rel = `${dir}/${n}`;
    const st = statSync(join(root, rel));
    if (st.isDirectory()) walk(root, rel, exts, out);
    else if (exts.some((e) => n.endsWith(e))) out.push(rel);
  }
  return out;
}

// [{ page, text }] for the doc scope and for the source scope.
function load(root, files) {
  return files.filter((f) => existsSync(join(root, f))).map((page) => ({ page, text: readFileSync(join(root, page), "utf8") }));
}

export function docFiles(root) {
  const md = [...["AGENTS.md", "CLAUDE.md", "server/README.md", "website/README.md", "agent-box/README.md", "tools/README.md"], ...walk(root, ".claude", [".md"]), ...walk(root, "docs", [".md"])];
  return md.filter((f) => !SKIP_DOCS.some((re) => re.test(f)));
}

export function sourceFiles(root) {
  return [...walk(root, "tools", [".mjs", ".json", ".ps1", ".cmd"]), ...walk(root, "agent-box", [".mjs", ".json", ".ps1"]), ".gitignore"]
    .filter((f) => !/^tools\/(validation\/fixtures|film)\//.test(f) && f !== "tools/validation/validate-cli-references.mjs");
}

const PLANNED = /\(planned/i;

// Rule 1: lines with `toolscli`: [{ page, line, text }].
export function typoFindings(pages) {
  const out = [];
  for (const { page, text } of pages) text.split(/\r?\n/).forEach((l, i) => { if (TYPO.test(l)) out.push({ page, line: i + 1, text: l.trim().slice(0, 120) }); });
  return out;
}

// Rule 2: `tools\cli <word>` whose word is no command and whose line carries no (planned) marker.
export function unknownFindings(pages, known) {
  const out = [];
  for (const { page, text } of pages) {
    text.split(/\r?\n/).forEach((l, i) => {
      if (PLANNED.test(l)) return;
      // `tools\cli` or `tools/cli.mjs`; a bare `tools/cli node` is a process name in the RAM tables, not a command
      for (const m of l.matchAll(/tools(?:\\+cli|[\\/]cli\.(?:mjs|cmd))\s+(?:--?[\w-]+\s+)*([a-z][a-z0-9-]*)/g)) {
        if (!known.top.has(m[1])) out.push({ page, line: i + 1, ref: m[1], text: l.trim().slice(0, 120) });
      }
    });
  }
  return out;
}

// Rule 3: command files no page names as `cli <cmd>` or `<cmd>`.
export function undocumentedCommands(names, pages) {
  const all = pages.map((p) => p.text).join("\n");
  return names.filter((n) => {
    if (UNDOCUMENTED_OK.has(n)) return false;
    // `playtest-checklist` is documented as `tools\cli playtest checklist`: a dash may be a space
    const esc = n.replace(/[.*+?^${}()|[\]\\]/g, "\\$&").replace(/-/g, "[- ]");
    return !new RegExp("(?:cli(?:\\.mjs|\\.cmd)?\\s+(?:--?[\\w-]+\\s+)*|`)" + esc + "(?![\\w-])").test(all);
  });
}

function selftest() {
  let failed = 0;
  const fail = (m) => { console.log(`FAIL  ${m}`); failed++; };
  const known = { top: new Set(["validate", "wt", "real-cmd"]), sub: {} };
  const typo = typoFindings([{ page: "a.md", text: "run `tools\\cli validate`\nrun `toolscli validate`\n" }]);
  if (typo.length !== 1 || typo[0].line !== 2) fail(`toolscli: expected one finding on line 2, got ${JSON.stringify(typo)}`);
  const pages = [{ page: "b.md", text: "ok `tools\\cli wt new x`\nbad `tools\\cli zz-gone now`\nlater `tools\\cli zz-later` *(planned)*\nfenced tools/cli --verbose real-cmd\n" }];
  const unk = unknownFindings(pages, known);
  if (unk.length !== 1 || unk[0].ref !== "zz-gone") fail(`unknown command: expected only zz-gone, got ${JSON.stringify(unk.map((f) => f.ref))}`);
  const undoc = undocumentedCommands(["real-cmd", "zz-hidden", "install"], [{ page: "c.md", text: "see `real-cmd` and `tools\\cli wt`" }]);
  if (undoc.length !== 1 || undoc[0] !== "zz-hidden") fail(`undocumented: expected only zz-hidden, got ${JSON.stringify(undoc)}`);
  const none = undocumentedCommands(["x-y", "play-list"], [{ page: "d.md", text: "run tools\\cli x-y --now, then `tools\\cli play list`" }]);
  if (none.length) fail(`a documented command was flagged: ${none.join(",")}`);
  const proc = unknownFindings([{ page: "e.md", text: "| tools/cli node (agents' commands) | 32 |" }], known);
  if (proc.length) fail("a bare `tools/cli node` process name in a table was flagged as a command");
  console.log(failed ? `SUMMARY validate-cli-references selftest fail ${failed}` : "SUMMARY validate-cli-references selftest ok");
  process.exit(failed ? 1 : 0);
}

if (process.argv.includes("--selftest")) selftest();

const known = knownCommands(ROOT);
const docs = load(ROOT, docFiles(ROOT));
const sources = load(ROOT, sourceFiles(ROOT));
const names = existsSync(join(ROOT, "tools", "commands")) ? readdirSync(join(ROOT, "tools", "commands")).filter((n) => /^[a-z][a-z0-9-]*\.mjs$/.test(n)).map((n) => n.slice(0, -4)) : [];
const rel = (p) => relative(ROOT, join(ROOT, p)).replace(/\\/g, "/");

const typos = typoFindings([...docs, ...sources]);
const unknown = unknownFindings(docs, known);
const undocumented = undocumentedCommands(names, docs);

for (const f of typos) console.log(`FAIL  ${rel(f.page)}:${f.line} \`toolscli\` is \`tools\\cli\` with the backslash eaten (a string literal needs \`tools\\\\cli\`): ${f.text}`);
for (const f of unknown) console.log(`FAIL  ${rel(f.page)}:${f.line} \`tools\\cli ${f.ref}\` is not a command: fix the name, drop the line, or mark it *(planned)*: ${f.text}`);
for (const n of undocumented) console.log(`FAIL  tools/commands/${n}.mjs: no page under AGENTS.md, docs/ or .claude/ names \`tools\\cli ${n}\`; add it where an agent looks (or UNDOCUMENTED_OK here, with a reason)`);
const total = typos.length + unknown.length + undocumented.length;
console.log(total ? `SUMMARY validate-cli-references fail ${typos.length} toolscli, ${unknown.length} unknown command, ${undocumented.length} undocumented` : `SUMMARY validate-cli-references ok ${docs.length} pages, ${names.length} command files`);
process.exit(total ? 1 : 0);
