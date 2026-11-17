// The runbook, the hotfix page and the rented-box checklist name only host commands that exist (#1350).
// inputs: docs/ops/session-runbook.md, docs/ops/hotfix.md, docs/ops/rented-box.md, tools/cli.mjs, tools/commands/*.mjs, tools/host/**
//
// Finds every `tools\cli host <sub>` and `tools\cli <cmd>` in the three pages and takes the names
// `tools\cli help` and `tools\cli host help` print as the truth. Two failures:
//   - a command the help does not list and the page does not mark *(planned)*;
//   - a command the help DOES list that the page still marks *(planned)* (a stale mark).
// A command that is not merged yet may stay marked *(planned)*; the mark leaves with the merge.
//
// red-proof: 2026-11-15, before the page edits: `FAIL docs/ops/hotfix.md:77 `tools\cli host announce`
// is merged but still marked planned` (and 15 more of the same kind) and exit 1.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PAGES = ["docs/ops/session-runbook.md", "docs/ops/hotfix.md", "docs/ops/rented-box.md"];

function cliOut(args) {
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), ...args], { cwd: ROOT, encoding: "utf8", windowsHide: true, timeout: 60000, env: { ...process.env, LF_OUTPUT: "log" } });
  return `${r.stdout ?? ""}\n${r.stderr ?? ""}`;
}

// Top-level command names from `tools\cli help`, host verbs from `tools\cli host help`.
export function parseHelp(topHelp, hostHelp) {
  const top = new Set();
  for (const line of topHelp.split(/\r?\n/)) {
    if (/^\s{2}\S/.test(line) && !/^\s+full usage/.test(line)) for (const w of line.trim().split(/\s+/)) top.add(w);
  }
  const host = new Set();
  const usage = /usage: tools\\cli host ([^\[\n]*)/.exec(hostHelp);
  for (const v of (usage ? usage[1] : "").trim().split("|")) if (/^[a-z][a-z0-9-]*$/.test(v.trim().split(/\s/)[0])) host.add(v.trim().split(/\s/)[0]);
  host.add("help");
  for (const m of hostHelp.matchAll(/^ {2}([a-z][a-z0-9-]*)\b/gm)) host.add(m[1]);
  return { top, host };
}

// [{ line, ref, planned }] for each `tools\cli ...` code span in a page.
export function refsOf(md) {
  const out = [];
  let fenced = false;
  md.split(/\r?\n/).forEach((line, i) => {
    if (/^\s*(```|~~~)/.test(line)) { fenced = !fenced; return; }
    const spans = [];
    if (fenced) {
      if (/tools[/\\]cli\b/.test(line)) spans.push({ code: line, planned: /\(planned\)/i.test(line) });
    } else {
      for (const m of line.matchAll(/(`+)([^`]*?)\1/g)) {
        if (!/tools[/\\]cli\b/.test(m[2])) continue;
        const before = /\(planned[^)]*\)\*?_?\s*$/i.test(line.slice(0, m.index));
        spans.push({ code: m[2], planned: before || /^\s*\*?_?\(planned/i.test(line.slice(m.index + m[0].length)) });
      }
    }
    for (const { code, planned } of spans) {
      const words = (/tools[/\\]cli\b([^`\n]*)/.exec(code)[1].trim().match(/\S+/g) ?? []);
      const ok = (w) => w && /^[a-z][a-z0-9-]*$/.test(w);
      if (!ok(words[0])) continue;
      out.push({ line: i + 1, cmd: words[0], sub: words[0] === "host" && ok(words[1]) ? words[1] : null, planned });
    }
  });
  return out;
}

export function check(md, help, page = "page") {
  const fails = [];
  for (const r of refsOf(md)) {
    const exists = help.top.has(r.cmd) && (r.sub === null || r.cmd !== "host" || help.host.has(r.sub));
    const name = r.sub ? `${r.cmd} ${r.sub}` : r.cmd;
    if (!exists && !r.planned) fails.push(`${page}:${r.line} \`tools\\cli ${name}\` is not in the help and not marked planned`);
    if (exists && r.planned) fails.push(`${page}:${r.line} \`tools\\cli ${name}\` is merged but still marked planned`);
  }
  return fails;
}

// The logic itself, on a fixture: a merged command marked planned and a missing unmarked one both fail.
function logicSelfcheck() {
  const help = { top: new Set(["host", "config-check"]), host: new Set(["start", "drill"]) };
  const md = "Run `tools\\cli host drill` *(planned)* then `tools\\cli host zz-missing`, `tools\\cli host start` and `tools\\cli host nope` *(planned)*.\n";
  const got = check(md, help);
  return got.length === 2 && /drill/.test(got[0]) && /zz-missing/.test(got[1]);
}

if (!logicSelfcheck()) {
  console.log("FAIL  selftest logic: expected exactly a stale planned mark and an unlisted command");
  process.exit(1);
}
const help = parseHelp(cliOut(["help"]), cliOut(["host", "help"]));
if (!help.top.has("host") || !help.host.has("start") || !help.host.has("announce")) {
  console.log(`FAIL  could not read the help: top=${help.top.size} host=${help.host.size}`);
  process.exit(1);
}
const fails = [];
for (const p of PAGES) fails.push(...check(readFileSync(join(ROOT, p), "utf8"), help, p));
for (const f of fails) console.log(`FAIL  ${f}`);
console.log(fails.length ? `SUMMARY ops-doc-commands-selftest fail ${fails.length}` : `SUMMARY ops-doc-commands-selftest ok ${PAGES.length} pages, ${help.host.size} host verbs`);
process.exit(fails.length ? 1 : 0);
