// tools\cli rules export: the published town rules, one source for the town board, the site and Discord (#1281).
//
// Reads the pack's `LF-Sign_Rules_*` strings (English), the keys the rules sign card shows (#1279), and writes
//   website/content/rules.json   the /rules page reads this
//   tmp/rules-discord.md         the Discord-ready block (also on stdout with --print); pinning it is the maintainer's step
// Only those keys are read. The orientation quiz (`LF-QuizEveron_*`) is a separate subset written by hand
// (decision 2026-11-14) and is never read or generated here. The proof is tools/validation/rules-export-selftest.mjs.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

export const help = "rules export [--table f] [--json f] [--md f] [--print]   write website/content/rules.json and a Discord-ready markdown block from the pack's published-rules strings";

export const DEFAULT_TABLE = "addons/LF_Everon1989/Language/lf_everon1989_localization.st";
export const DEFAULT_JSON = "website/content/rules.json";
export const DEFAULT_MD = "tmp/rules-discord.md";

const KEYS = {
  title: "LF-Sign_Rules_Title",
  intro: "LF-Sign_Rules_What",
  rules: "LF-Sign_Rules_Rules",
  next: "LF-Sign_Rules_Next",
};

const unescapeValue = (s) => s.replace(/\\(.)/g, (_, c) => (c === "n" ? "\n" : c));

// id -> English text for the rows of a string table.
function readTable(text) {
  const out = new Map();
  for (const m of text.matchAll(/Id "([^"]+)"\s+Target_en_us "((?:[^"\\]|\\.)*)"/g)) out.set(m[1], unescapeValue(m[2]));
  return out;
}

// { ok: true, rules: { title, intro, rules[], next } } or { ok: false, error }.
export function buildRules(tableText) {
  const text = readTable(tableText);
  const get = (key) => (text.get(key) || "").trim();
  const title = get(KEYS.title);
  const rules = get(KEYS.rules)
    .split("|")
    .map((r) => r.trim())
    .filter(Boolean);
  if (!title) return { ok: false, error: `${KEYS.title} has no English text in the table` };
  if (!rules.length) return { ok: false, error: `${KEYS.rules} has no English text in the table` };
  return { ok: true, rules: { title, intro: get(KEYS.intro), rules, next: get(KEYS.next) } };
}

// The Discord block: bold title, the intro, a numbered list, the closing note. One message is 2000 characters.
export function renderMarkdown(r) {
  const lines = [`**${r.title}**`];
  if (r.intro) lines.push(r.intro);
  lines.push("", ...r.rules.map((rule, i) => `${i + 1}. ${rule}`));
  if (r.next) lines.push("", r.next);
  return lines.join("\n") + "\n";
}

const val = (args, name) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
};

function write(file, body) {
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, body);
}

export async function main(args, ctx) {
  if (args[0] !== "export") {
    console.log("usage: tools\\cli rules export [--table f] [--json f] [--md f] [--print]");
    return args.length ? 1 : 0;
  }
  const root = ctx.root;
  const table = resolve(root, val(args, "--table") || DEFAULT_TABLE);
  const jsonFile = resolve(root, val(args, "--json") || DEFAULT_JSON);
  const mdFile = resolve(root, val(args, "--md") || DEFAULT_MD);
  if (!existsSync(table)) {
    console.error(`SUMMARY rules export fail table not found: ${table}`);
    return 1;
  }
  const built = buildRules(readFileSync(table, "utf8"));
  if (!built.ok) {
    console.error(`SUMMARY rules export fail ${built.error}`);
    return 1;
  }
  const md = renderMarkdown(built.rules);
  write(jsonFile, JSON.stringify(built.rules, null, 2) + "\n");
  write(mdFile, md);
  if (args.includes("--print")) process.stdout.write(md);
  const warn = md.length > 2000 ? ` WARNING discord block is ${md.length} chars, over one 2000-character message` : "";
  console.log(`SUMMARY rules export ok rules=${built.rules.rules.length} json=${jsonFile} discord=${mdFile} (${md.length} chars)${warn}`);
  return 0;
}
