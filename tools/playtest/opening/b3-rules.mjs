// Beat B3 of the opening-day script (#1601, chapter #1006): the rules match everywhere. The desk half.
//   node tools/playtest/opening/b3-rules.mjs
// `tools\cli rules export` reads the pack's published-rules strings; the site's website/content/rules.json and the Discord
// block must carry the same rule text. One line per checkbox of the beat in docs/playtest/m4-opening-day.md:
//   BEAT B3 <check-id> PASS|FAIL|BLOCKED <reason>
//   rules-export    the export builds the rules from the pack table
//   rules-site      website/content/rules.json holds the exported title, intro, every rule and the closing note
//   rules-discord   the Discord block holds the same; read from docs/community/discord-rules.md when it exists
//                   (RULESDISCORD, #232), else from tmp/rules-discord.md, which the export just wrote
//   board-rules     BLOCKED by #1280: the town board is a town-pass card, out of this wave
//   board-own       BLOCKED by #1280, the same
// A missing Discord block (neither file) is BLOCKED by #232, not a FAIL. A changed rule is a FAIL naming the rule.
// Exit 1 only when a line is FAIL. The proof is tools/validation/opening-b3-selftest.mjs. ODRUN finds this by glob.
import { existsSync, readFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { buildRules, DEFAULT_JSON, DEFAULT_MD, DEFAULT_TABLE } from "../../commands/rules.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const ROOT = join(here, "..", "..", "..");

const norm = (s) => String(s).replace(/\s+/g, " ").trim();
const clip = (s) => (norm(s).length > 70 ? `${norm(s).slice(0, 67)}...` : norm(s));
const rel = (p) => relative(ROOT, p).replace(/\\/g, "/") || p;

// The Discord block's parts: **title**, intro, "N. rule" lines, closing note (the shape rules.mjs renderMarkdown writes).
export function parseBlock(text) {
  const lines = String(text).split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
  const rules = [];
  const rest = [];
  let title = "";
  for (const l of lines) {
    const t = /^\*\*(.+)\*\*$/.exec(l);
    const r = /^\d+\.\s+(.*)$/.exec(l);
    if (t && !title) title = t[1];
    else if (r) rules.push(r[1]);
    else rest.push(l);
  }
  return { title, rules, rest };
}

// Differences between the exported rules and what a published copy carries, each naming the rule. `copy` is
// { title, intro, rules[], next } (the site) or parseBlock output merged with the text lines (Discord).
function diff(want, got, where) {
  const out = [];
  if (norm(want.title) !== norm(got.title)) out.push(`${where} title is "${clip(got.title)}", the export says "${clip(want.title)}"`);
  const n = Math.max(want.rules.length, got.rules.length);
  for (let i = 0; i < n; i++) {
    const w = want.rules[i];
    const g = got.rules[i];
    if (w === undefined) out.push(`${where} has an extra rule ${i + 1}: "${clip(g)}"`);
    else if (g === undefined) out.push(`${where} lacks rule ${i + 1}: "${clip(w)}"`);
    else if (norm(w) !== norm(g)) out.push(`${where} rule ${i + 1} reads "${clip(g)}", the export says "${clip(w)}"`);
  }
  return out;
}

const readJson = (file) => {
  try {
    return { ok: true, value: JSON.parse(readFileSync(file, "utf8")) };
  } catch (e) {
    return { ok: false, error: e.message };
  }
};

/**
 * The checks of B3. `table` is the pack string table, `site` the rules.json, `discord` the Discord block files in
 * order of preference (the first that exists is read). Returns [{ id, verdict, reason }].
 */
export function runB3({ table, site, discord }) {
  const out = [];
  const line = (id, verdict, reason) => out.push({ id, verdict, reason });
  const board = () => {
    line("board-rules", "BLOCKED", "the rules board at the plaza is placed by #1280, a town-pass card, out of this wave");
    line("board-own", "BLOCKED", "the run-your-own board beside it is placed by #1280, a town-pass card, out of this wave");
  };

  // the export
  if (!existsSync(table)) {
    line("rules-export", "FAIL", `the pack string table is missing: ${rel(table)}`);
    board();
    return out;
  }
  const built = buildRules(readFileSync(table, "utf8"));
  if (!built.ok) {
    line("rules-export", "FAIL", built.error);
    board();
    return out;
  }
  const want = built.rules;
  line("rules-export", "PASS", `${want.rules.length} rules from ${rel(table)}`);

  // the site
  const s = existsSync(site) ? readJson(site) : { ok: false, error: "file not found" };
  if (!s.ok) line("rules-site", "FAIL", `${rel(site)} cannot be read: ${s.error}`);
  else {
    const v = s.value || {};
    const got = { title: v.title ?? "", rules: Array.isArray(v.rules) ? v.rules : [] };
    const bad = diff(want, got, "the site");
    if (norm(want.intro) !== norm(v.intro ?? "")) bad.push(`the site intro reads "${clip(v.intro ?? "")}", the export says "${clip(want.intro)}"`);
    if (norm(want.next) !== norm(v.next ?? "")) bad.push(`the site closing note reads "${clip(v.next ?? "")}", the export says "${clip(want.next)}"`);
    if (bad.length) line("rules-site", "FAIL", `${bad.join("; ")} (run tools\\cli rules export)`);
    else line("rules-site", "PASS", `${rel(site)} carries the title, intro, ${want.rules.length} rules and closing note`);
  }

  // the Discord block
  const file = discord.find((p) => existsSync(p));
  if (!file) line("rules-discord", "BLOCKED", `no Discord block: ${discord.map(rel).join(" or ")} does not exist; the block is RULESDISCORD #232 (tools\\cli rules export writes tmp/rules-discord.md)`);
  else {
    const b = parseBlock(readFileSync(file, "utf8"));
    const bad = diff(want, b, "the Discord block");
    const text = b.rest.map(norm);
    if (want.intro && !text.includes(norm(want.intro))) bad.push(`the Discord block lacks the intro "${clip(want.intro)}"`);
    if (want.next && !text.includes(norm(want.next))) bad.push(`the Discord block lacks the closing note "${clip(want.next)}"`);
    if (bad.length) line("rules-discord", "FAIL", bad.join("; "));
    else line("rules-discord", "PASS", `${rel(file)} carries the title, intro, ${want.rules.length} rules and closing note`);
  }

  board();
  return out;
}

function main() {
  // Write tmp/rules-discord.md (the fallback block) the way the beat sheet does; the site file is read, never rewritten.
  const exp = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "rules", "export", "--json", join(ROOT, "tmp", "b3-rules-export.json")], {
    cwd: ROOT,
    encoding: "utf8",
    timeout: 60000,
    windowsHide: true,
  });
  if (exp.status !== 0) console.error(`rules export exited ${exp.status}: ${(exp.stderr || exp.stdout || "").trim().split(/\r?\n/).pop()}`);
  const lines = runB3({
    table: resolve(ROOT, DEFAULT_TABLE),
    site: resolve(ROOT, DEFAULT_JSON),
    discord: [join(ROOT, "docs", "community", "discord-rules.md"), resolve(ROOT, DEFAULT_MD)],
  });
  for (const l of lines) console.log(`BEAT B3 ${l.id} ${l.verdict} ${l.reason}`);
  return lines.some((l) => l.verdict === "FAIL") ? 1 : 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(main());
