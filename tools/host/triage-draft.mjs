// tools/host/triage-draft.mjs - one issue draft per new line group of a triaged server log.
//
// `tools\cli host triage --draft` calls this with the log text and the groups from
// tools/lib/log-triage.mjs. A draft is the issue title and body for a group the triage table does
// not know: count, first time, suspected feature, and a short log excerpt. Nothing here files an
// issue or starts a process; the command prints the `gh issue create` lines and a person runs them.

import { mkdirSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const TIMESTAMP = /^\s*(\d{1,2}:\d{2}:\d{2}(?:\.\d+)?)\s/;
const TAG = /^\[LFDebug:([A-Za-z0-9_]+)\]\s*/;
const FEATURE_PATH = /Scripts\/Game\/Feature\/([A-Za-z0-9_]+)\//;
const TITLE_MAX = 140;
const EXCERPT_CONTEXT = 2;
const EXCERPT_LINE_MAX = 300;

/** The suspected feature of a log message: its `[LFDebug:<Feature>]` tag, else the Feature/<Area> path in it, else "unknown". */
export function suspectFeature(message) {
  const tag = TAG.exec(message);
  if (tag) return { name: tag[1], from: `the [LFDebug:${tag[1]}] tag` };
  const path = FEATURE_PATH.exec(message);
  if (path) return { name: path[1], from: `the Feature/${path[1]} script path` };
  return { name: "unknown", from: "no [LFDebug:<Feature>] tag or Feature path in the line" };
}

function clip(text, max) {
  return text.length > max ? `${text.slice(0, max)}...` : text;
}

/** Drafts for the new (not known) groups, in the order triageLog lists them: [{ title, feature, featureFrom, count, firstTime, firstLine, excerpt, body }]. */
export function draftGroups(logText, groups, logName = "console.log") {
  const lines = logText.split(/\r?\n/);
  const out = [];
  for (const g of groups) {
    if (g.known) continue;
    const feature = suspectFeature(g.sample);
    const first = lines[g.firstLine - 1] ?? "";
    const time = TIMESTAMP.exec(first);
    const firstTime = time ? time[1] : "unknown";
    const from = Math.max(0, g.firstLine - 1 - EXCERPT_CONTEXT);
    const excerpt = lines
      .slice(from, g.firstLine + EXCERPT_CONTEXT)
      .map((l) => clip(l.trimEnd(), EXCERPT_LINE_MAX))
      .join("\n");
    const text = clip(g.key.replace(TAG, ""), TITLE_MAX);
    const title = `Log triage: ${feature.name}: ${text}`;
    const body = [
      `# ${title}`,
      "",
      `- Count: ${g.count}`,
      `- First seen: ${firstTime} (log line ${g.firstLine} of ${logName})`,
      `- Suspected feature: ${feature.name} (from ${feature.from})`,
      `- Kind: ${g.kind}`,
      "",
      "## Log excerpt",
      "",
      "```",
      excerpt,
      "```",
      "",
      "## Next",
      "",
      "Drafted by `tools\\cli host triage --draft` because no row of `docs/debug-log-triage.md` matches this line. Check the page first; if it is a real fault, add the fix and a triage row, if it is expected, add a `BY DESIGN` row.",
      "",
    ].join("\n");
    out.push({ title, feature: feature.name, featureFrom: feature.from, count: g.count, firstTime, firstLine: g.firstLine, excerpt, body });
  }
  return out;
}

/** The `gh issue create` command line for a draft. The title is quoted so bash and PowerShell read it the same. */
export function ghCreateCommand(draft, bodyPath) {
  const title = draft.title
    .replace(/[\r\n]+/g, " ")
    .replace(/"/g, "'")
    .replace(/\\/g, "/")
    .replace(/[$`]/g, " ");
  return `gh issue create --title "${title}" --body-file "${bodyPath}" --label bug`;
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
