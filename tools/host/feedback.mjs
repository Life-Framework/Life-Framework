// tools/host/feedback.mjs - issue drafts from a session's copied Discord feedback channel.
//
// `tools\cli host feedback <export.txt>` calls this with the text of the export. It reads the
// messages, drops thanks, merges the messages that report the same thing, and writes one issue body
// per report in the layout of the repo's bug or feature template (.github/ISSUE_TEMPLATE/). Discord
// user ids, mentions and author names never reach a draft. Nothing here files an issue or starts a
// process; the command prints the `gh issue create` lines and a person runs them.

import { mkdirSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const TITLE_MAX = 90;

// Header of a message in a copied Discord channel: "Name — Today at 8:02 PM" or "Name — 11/14/2026 8:02 PM".
const HEADER_DASH = /^(.{1,64}?)\s+[—–-]\s+(?:Today|Yesterday|\d{1,4}[/.-]\d{1,2}[/.-]\d{1,4})\b.*$/;
// "[2026-11-14 20:02] Name: text", the form of a channel exporter.
const HEADER_BRACKET = /^\[([^\]]{3,40})\]\s+([^:]{1,64}):\s*(.*)$/;
const NOISE = /^\s*(?:\(edited\)|pinned|new)\s*$/i;

/** The messages of an export: [{ author, text }] in order. Lines before the first header are ignored. */
export function parseExport(text) {
  const out = [];
  let cur = null;
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.replace(/\s+$/, "");
    const dash = HEADER_DASH.exec(line);
    if (dash) {
      cur = { author: dash[1].trim(), lines: [] };
      out.push(cur);
      continue;
    }
    const bracket = HEADER_BRACKET.exec(line);
    if (bracket) {
      cur = { author: bracket[2].trim(), lines: bracket[3] ? [bracket[3]] : [] };
      out.push(cur);
      continue;
    }
    if (cur && line.trim() && !NOISE.test(line)) cur.lines.push(line.trim());
  }
  return out.map((m) => ({ author: m.author, text: m.lines.join("\n") })).filter((m) => m.text);
}

/** Text with Discord ids, mentions and the given author names removed. */
export function sanitize(text, authors = []) {
  let t = text
    .replace(/<@[!&]?\d+>/g, "")
    .replace(/<#\d+>/g, "#channel")
    .replace(/\b\d{17,20}\b/g, "")
    .replace(/\(\s*(?:my\s+)?(?:discord\s+)?(?:user\s+)?id(?:\s+is)?\s*\)/gi, "");
  for (const a of authors) {
    const name = a.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    t = t.replace(new RegExp(`@?\\b${name}\\b`, "g"), "another player");
  }
  return t
    .replace(/\(\s*\)/g, "")
    .replace(/[ \t]+/g, " ")
    .replace(/ +([,.!?])/g, "$1")
    .replace(/\n{2,}/g, "\n")
    .trim();
}

const THANKS_CORE = new Set(["thanks", "thank", "thx", "ty", "cheers", "gg", "wp", "love", "loved", "awesome", "fun", "great", "amazing"]);
const THANKS_FILLER = new Set(
  ("you for the a an so much all guys everyone team devs dev host hosting hosted session game job work had have was it that really very lol good nice time " +
    "see next bye night to this and").split(" "),
);

/** True for a message that is only thanks or praise ("thanks for hosting, had a great time!"). */
export function isThanks(text) {
  const words = text.toLowerCase().replace(/[^a-z0-9'\s]/g, " ").split(/\s+/).filter(Boolean);
  if (!words.length || words.length > 12) return false;
  if (!words.every((w) => THANKS_CORE.has(w) || THANKS_FILLER.has(w))) return false;
  return words.some((w) => THANKS_CORE.has(w));
}

const FEATURE =
  /\b(?:would be (?:nice|cool|great|good|awesome)|it'?d be (?:nice|cool|great)|please add|can we (?:get|have|add)|could we (?:get|have|add)|could you add|suggest\w*|feature request|i wish|wish (?:we|there|you)|should (?:have|add)|add an? )\b/i;

/** "feature" for a suggestion, otherwise "bug". */
export function classify(text) {
  return FEATURE.test(text) ? "feature" : "bug";
}

const STOP = new Set(
  ("this that with from have been were what when they them then than there their about after before into just also very much some more only over only " +
    "nobody cant can't won't dont don't didnt didn't does doesnt doesn't would could should still again here same like even").split(" "),
);

function tokens(text) {
  const set = new Set();
  for (const w of text.toLowerCase().match(/[a-z0-9']+/g) ?? []) {
    if (w.length >= 4 && !STOP.has(w)) set.add(w);
  }
  return set;
}

function sameReport(a, b) {
  if (a.kind !== b.kind) return false;
  let shared = 0;
  for (const t of a.tokens) if (b.tokens.has(t)) shared++;
  const smaller = Math.min(a.tokens.size, b.tokens.size);
  return shared >= 2 && smaller > 0 && shared / smaller >= 0.6;
}

function firstSentence(text) {
  const piece = text.split(/[.!?\n]/).map((s) => s.trim()).find(Boolean) ?? text.trim();
  const clipped = piece.length > TITLE_MAX ? `${piece.slice(0, TITLE_MAX).replace(/\s+\S*$/, "")}...` : piece;
  return clipped.charAt(0).toUpperCase() + clipped.slice(1);
}

function players(n) {
  return n === 1 ? "1 player" : `${n} players`;
}

function quoted(text) {
  return text.split("\n").map((l) => `> ${l}`).join("\n");
}

/**
 * The newest `host summary` report of `date` (YYYY-MM-DD) in `sessionsDir`, as "sessions/<name>.md",
 * or null when there is none. Reports are named `<stamp>.md` (tools/host/host.mjs `stamp`).
 */
export function findSessionReport(sessionsDir, date) {
  let names = [];
  try {
    names = readdirSync(sessionsDir);
  } catch {
    return null;
  }
  const hit = names.filter((f) => f.startsWith(`${date}_`) && f.endsWith(".md")).sort().pop();
  return hit ? `sessions/${hit}` : null;
}

/** The lines that tie a draft to its session and to the other drafts of that session. */
function linkLines(session, siblings) {
  const out = [];
  if (session?.date) {
    const report = session.report
      ? `its \`tools\\cli host summary\` report is \`${session.report}\` in the host folder.`
      : "no `tools\\cli host summary` report was found for it; run that command and name the report here.";
    out.push(`Session: ${session.date}; ${report}`, "");
  }
  if (session?.date) {
    out.push(
      siblings.length ? "Other drafts from this session:" : "Other drafts from this session: none",
      ...siblings.map((t) => `- ${t}`),
      "",
    );
  }
  return out;
}

function bodyFor(kind, reports, reporters, links = []) {
  const first = reports[0];
  const more = reports.slice(1);
  const asked = [
    first,
    ...(more.length ? ["", "Also reported:", ...more.map((r) => quoted(r))] : []),
  ].join("\n");
  const foot = [
    "---",
    ...links,
    `Reported by ${players(reporters)} in the session's feedback channel. Drafted by \`tools\\cli host feedback\`; no author names or Discord ids are kept. Check it against the session's server log before filing (\`tools\\cli host triage\`).`,
    "",
  ].join("\n");
  if (kind === "feature") {
    return [
      "### Problem or opportunity",
      "",
      asked,
      "",
      "### Proposed solution",
      "",
      "Not worked out in the feedback; the words above are the idea.",
      "",
      "### Alternatives considered",
      "",
      "None given.",
      "",
      foot,
    ].join("\n");
  }
  return [
    "### What happened?",
    "",
    asked,
    "",
    "### Steps to reproduce",
    "",
    "Not given in the feedback; ask the reporter.",
    "",
    "### Expected behavior",
    "",
    "Not given in the feedback.",
    "",
    "### Logs or screenshots",
    "",
    "None attached.",
    "",
    "### Environment",
    "",
    "A live session of the Life Framework server; build and client as in the session's announcement.",
    "",
    foot,
  ].join("\n");
}

/**
 * Drafts for an export's messages: { drafts, thanks, duplicates }. A draft is
 * { kind, label, title, reporters, reports, body }, in the order the reports first appeared.
 * `session` ({ date, report }, optional) adds the session line and the sibling list to each body.
 */
export function draftFeedback(messages, { session = null } = {}) {
  const authors = [...new Set(messages.map((m) => m.author))];
  // Consecutive messages of one author are one report.
  const merged = [];
  for (const m of messages) {
    const last = merged[merged.length - 1];
    if (last && last.author === m.author) last.text += `\n${m.text}`;
    else merged.push({ author: m.author, text: m.text });
  }
  let thanks = 0;
  const groups = [];
  let duplicates = 0;
  for (const m of merged) {
    const text = sanitize(m.text, authors);
    if (!text) continue;
    if (isThanks(text)) {
      thanks++;
      continue;
    }
    const entry = { kind: classify(text), text, tokens: tokens(text), authors: new Set([m.author]) };
    const into = groups.find((g) => sameReport(g.head, entry));
    if (into) {
      into.reports.push(text);
      into.authors.add(m.author);
      duplicates++;
    } else {
      groups.push({ head: entry, reports: [text], authors: new Set([m.author]) });
    }
  }
  const titles = groups.map((g) => `${g.head.kind === "feature" ? "[Feature]: " : "[Bug]: "}${firstSentence(g.reports[0])}`);
  const drafts = groups.map((g, i) => {
    const kind = g.head.kind;
    const links = linkLines(session, titles.filter((_, j) => j !== i));
    return {
      kind,
      label: kind === "feature" ? "enhancement" : "bug",
      title: titles[i],
      reporters: g.authors.size,
      reports: g.reports,
      body: bodyFor(kind, g.reports, g.authors.size, links),
    };
  });
  return { drafts, thanks, duplicates };
}

/** The `gh issue create` command line for a draft. The title is quoted so bash and PowerShell read it the same. */
export function ghCreateCommand(draft, bodyPath) {
  const title = draft.title
    .replace(/[\r\n]+/g, " ")
    .replace(/"/g, "'")
    .replace(/\\/g, "/")
    .replace(/[$`]/g, " ");
  return `gh issue create --title "${title}" --body-file "${bodyPath}" --label ${draft.label}`;
}

/** Write `<n>.md` per draft into `dir`, first removing the numbered drafts of an earlier run. Returns the written file names. */
export function writeDrafts(drafts, dir) {
  mkdirSync(dir, { recursive: true });
  for (const f of readdirSync(dir)) {
    if (/^\d+\.md$/.test(f)) rmSync(join(dir, f));
  }
  return drafts.map((d, i) => {
    const name = `${i + 1}.md`;
    writeFileSync(join(dir, name), d.body);
    return name;
  });
}
